#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // mat4, vec3, vec4 (core types)
#include <glm/gtc/matrix_transform.hpp> // lookAt, perspective (transformations)
#include <glm/gtc/type_ptr.hpp>         // value_ptr (convert to OpenGL pointer)

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/trigonometric.hpp"
#include "rendering/camera.h"
#include "rendering/shader.h"
#include "simulation/terrain.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// anon namespace instead of static functions
namespace {
constexpr char WINDOW_TITLE[] = "Excavation Simulator";
constexpr std::size_t METRIC_WINDOW = 240;
constexpr float BENCHMARK_SIMULATION_DT = 1.0f / 120.0f;

struct AppOptions {
  bool benchmarkMode = false;
  bool disableVsync = false;
  bool writeCsv = false;
  std::size_t benchmarkFrames = 3000;
  std::string csvPath;
};

struct MetricSummary {
  double average = 0.0;
  double p95 = 0.0;
  double maximum = 0.0;
};

struct RollingMetric {
  std::array<double, METRIC_WINDOW> values{};
  std::size_t count = 0;
  std::size_t next = 0;
  double sum = 0.0;

  void add(double value) {
    if (count < values.size()) {
      values[next] = value;
      sum += value;
      ++count;
      next = (next + 1) % values.size();
      return;
    }

    sum -= values[next];
    values[next] = value;
    sum += value;
    next = (next + 1) % values.size();
  }

  bool empty() const { return count == 0; }

  double average() const {
    if (count == 0) {
      return 0.0;
    }
    return sum / static_cast<double>(count);
  }

  double percentile(double percentileValue) const {
    if (count == 0) {
      return 0.0;
    }

    std::vector<double> samples(values.begin(), values.begin() + static_cast<long>(count));
    std::sort(samples.begin(), samples.end());
    const double clamped = std::clamp(percentileValue, 0.0, 1.0);
    const std::size_t index =
        static_cast<std::size_t>(clamped * static_cast<double>(samples.size() - 1));
    return samples[index];
  }
};

std::string formatBytes(double bytes) {
  std::ostringstream formatted;
  formatted << std::fixed;

  if (bytes >= 1024.0 * 1024.0) {
    formatted << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MiB";
    return formatted.str();
  }

  if (bytes >= 1024.0) {
    formatted << std::setprecision(1) << (bytes / 1024.0) << " KiB";
    return formatted.str();
  }

  formatted << std::setprecision(0) << bytes << " B";
  return formatted.str();
}

void accumulateTerrainStats(TerrainUpdateStats &aggregate, const TerrainUpdateStats &sample) {
  if (!sample.updated) {
    return;
  }

  aggregate.updated = true;
  aggregate.cpuMs += sample.cpuMs;
  aggregate.dirtyVertices += sample.dirtyVertices;
  aggregate.uploadBytes += sample.uploadBytes;
  aggregate.stabilizationPasses += sample.stabilizationPasses;
}

struct RuntimeTelemetry {
  RollingMetric frameMs;
  RollingMetric terrainMs;
  RollingMetric dirtyVertices;
  RollingMetric uploadBytes;
  RollingMetric stabilizationPasses;
  bool captureHistory = false;
  std::vector<double> frameHistory;
  std::vector<double> terrainHistory;
  std::vector<double> dirtyVertexHistory;
  std::vector<double> uploadByteHistory;
  std::vector<double> stabilizationPassHistory;
  std::size_t framesSinceTitleUpdate = 0;
  double lastTitleUpdateTime = 0.0;

  void enableHistory(std::size_t expectedFrames) {
    captureHistory = true;
    frameHistory.reserve(expectedFrames);
    terrainHistory.reserve(expectedFrames);
    dirtyVertexHistory.reserve(expectedFrames);
    uploadByteHistory.reserve(expectedFrames);
    stabilizationPassHistory.reserve(expectedFrames);
  }

  void recordFrame(double sampleMs) {
    frameMs.add(sampleMs);
    ++framesSinceTitleUpdate;
    if (captureHistory) {
      frameHistory.push_back(sampleMs);
    }
  }

  void recordTerrainUpdate(const TerrainUpdateStats &stats) {
    if (!stats.updated) {
      return;
    }

    terrainMs.add(stats.cpuMs);
    dirtyVertices.add(static_cast<double>(stats.dirtyVertices));
    uploadBytes.add(static_cast<double>(stats.uploadBytes));
    stabilizationPasses.add(static_cast<double>(stats.stabilizationPasses));
    if (captureHistory) {
      terrainHistory.push_back(stats.cpuMs);
      dirtyVertexHistory.push_back(static_cast<double>(stats.dirtyVertices));
      uploadByteHistory.push_back(static_cast<double>(stats.uploadBytes));
      stabilizationPassHistory.push_back(static_cast<double>(stats.stabilizationPasses));
    }
  }

  void updateWindowTitle(GLFWwindow *window, double now, std::string_view titlePrefix) {
    if (lastTitleUpdateTime == 0.0) {
      lastTitleUpdateTime = now;
      return;
    }

    const double elapsed = now - lastTitleUpdateTime;
    if (elapsed < 1.0 || framesSinceTitleUpdate == 0) {
      return;
    }

    const double fps = static_cast<double>(framesSinceTitleUpdate) / elapsed;
    std::ostringstream title;
    title << std::fixed << std::setprecision(1);
    title << titlePrefix << " | " << fps << " FPS";
    title << " | frame " << frameMs.average() << " ms avg / " << frameMs.percentile(0.95)
          << " p95";

    if (terrainMs.empty()) {
      title << " | terrain idle";
    } else {
      title << " | terrain " << terrainMs.average() << " ms";
      title << " | dirty " << std::setprecision(0) << dirtyVertices.average();
      title << " | upload " << formatBytes(uploadBytes.average());
      title << " | passes " << std::setprecision(1) << stabilizationPasses.average();
    }

    glfwSetWindowTitle(window, title.str().c_str());
    lastTitleUpdateTime = now;
    framesSinceTitleUpdate = 0;
  }
};

enum class ParseResult { Continue, ExitSuccess, ExitFailure };

enum class TerrainAction { Dig, Dump };

void printUsage(const char *programName) {
  std::cout << "Usage: " << programName
            << " [--benchmark] [--frames=N] [--no-vsync] [--csv=PATH]\n";
}

bool parsePositiveSize(std::string_view value, std::size_t &parsed) {
  if (value.empty()) {
    return false;
  }

  char *end = nullptr;
  const unsigned long long raw = std::strtoull(value.data(), &end, 10);
  if (end == value.data() || (end != nullptr && *end != '\0') || raw == 0) {
    return false;
  }

  parsed = static_cast<std::size_t>(raw);
  return true;
}

ParseResult parseArguments(int argc, char **argv, AppOptions &options) {
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--help") {
      printUsage(argv[0]);
      return ParseResult::ExitSuccess;
    }

    if (argument == "--benchmark") {
      options.benchmarkMode = true;
      continue;
    }

    if (argument == "--no-vsync") {
      options.disableVsync = true;
      continue;
    }

    if (argument.rfind("--frames=", 0) == 0) {
      const std::string value = argument.substr(9);
      if (!parsePositiveSize(value, options.benchmarkFrames)) {
        std::cerr << "Invalid --frames value: " << value << "\n";
        printUsage(argv[0]);
        return ParseResult::ExitFailure;
      }
      continue;
    }

    if (argument.rfind("--csv=", 0) == 0) {
      options.csvPath = argument.substr(6);
      options.writeCsv = !options.csvPath.empty();
      if (!options.writeCsv) {
        std::cerr << "Invalid --csv value\n";
        printUsage(argv[0]);
        return ParseResult::ExitFailure;
      }
      continue;
    }

    std::cerr << "Unknown argument: " << argument << "\n";
    printUsage(argv[0]);
    return ParseResult::ExitFailure;
  }

  return ParseResult::Continue;
}

MetricSummary summarizeSamples(const std::vector<double> &samples) {
  MetricSummary summary;
  if (samples.empty()) {
    return summary;
  }

  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  summary.average = sum / static_cast<double>(samples.size());

  std::vector<double> sortedSamples = samples;
  std::sort(sortedSamples.begin(), sortedSamples.end());
  const std::size_t percentileIndex =
      static_cast<std::size_t>(0.95 * static_cast<double>(sortedSamples.size() - 1));
  summary.p95 = sortedSamples[percentileIndex];
  summary.maximum = sortedSamples.back();
  return summary;
}

glm::vec2 benchmarkBucketPosition(std::size_t frameIndex) {
  constexpr float margin = Terrain::spacing() * 6.0f;
  const float traversableSpan = std::max(Terrain::spacing(), Terrain::worldExtent() - (2.0f * margin));
  const float t = static_cast<float>(frameIndex);

  const float x = margin + (0.5f + 0.5f * std::sin(t * 0.021f)) * traversableSpan;
  const float z = margin + (0.5f + 0.5f * std::sin((t * 0.013f) + 1.1f)) * traversableSpan;
  return {x, z};
}

TerrainAction benchmarkActionForFrame(std::size_t frameIndex) {
  constexpr std::size_t actionWindow = 240;
  return ((frameIndex / actionWindow) % 2 == 0) ? TerrainAction::Dig : TerrainAction::Dump;
}

std::string buildWindowTitlePrefix(const AppOptions &options, std::size_t completedFrames) {
  if (!options.benchmarkMode) {
    return WINDOW_TITLE;
  }

  std::ostringstream prefix;
  prefix << WINDOW_TITLE << " [benchmark " << completedFrames << "/" << options.benchmarkFrames
         << "]";
  return prefix.str();
}

void printBenchmarkSummary(const RuntimeTelemetry &telemetry, double wallSeconds,
                           std::size_t completedFrames) {
  const MetricSummary frameSummary = summarizeSamples(telemetry.frameHistory);
  const MetricSummary terrainSummary = summarizeSamples(telemetry.terrainHistory);
  const MetricSummary dirtySummary = summarizeSamples(telemetry.dirtyVertexHistory);
  const MetricSummary uploadSummary = summarizeSamples(telemetry.uploadByteHistory);
  const MetricSummary passSummary = summarizeSamples(telemetry.stabilizationPassHistory);
  const double averageFps =
      wallSeconds > 0.0 ? static_cast<double>(completedFrames) / wallSeconds : 0.0;

  std::cout << "\nBenchmark Summary\n";
  std::cout << "Frames: " << completedFrames << '\n';
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Wall time: " << wallSeconds << " s\n";
  std::cout << "Average FPS: " << averageFps << '\n';
  std::cout << "Frame time: avg " << frameSummary.average << " ms | p95 " << frameSummary.p95
            << " ms | max " << frameSummary.maximum << " ms\n";
  std::cout << "Terrain update: avg " << terrainSummary.average << " ms | p95 "
            << terrainSummary.p95 << " ms | max " << terrainSummary.maximum << " ms\n";
  std::cout << "Dirty vertices/update: avg " << dirtySummary.average << " | p95 "
            << dirtySummary.p95 << " | max " << dirtySummary.maximum << '\n';
  std::cout << "Upload bytes/update: avg " << uploadSummary.average << " | p95 "
            << uploadSummary.p95 << " | max " << uploadSummary.maximum << '\n';
  std::cout << "Stabilization passes/update: avg " << passSummary.average << " | p95 "
            << passSummary.p95 << " | max " << passSummary.maximum << "\n";
}

bool writeBenchmarkCsv(const AppOptions &options, const RuntimeTelemetry &telemetry, double wallSeconds,
                       std::size_t completedFrames) {
  const std::filesystem::path csvPath(options.csvPath);
  if (!csvPath.parent_path().empty()) {
    std::filesystem::create_directories(csvPath.parent_path());
  }

  const bool needsHeader = !std::filesystem::exists(csvPath) || std::filesystem::file_size(csvPath) == 0;
  std::ofstream output(csvPath, std::ios::app);
  if (!output.is_open()) {
    std::cerr << "Failed to open CSV output: " << options.csvPath << "\n";
    return false;
  }

  const MetricSummary frameSummary = summarizeSamples(telemetry.frameHistory);
  const MetricSummary terrainSummary = summarizeSamples(telemetry.terrainHistory);
  const MetricSummary dirtySummary = summarizeSamples(telemetry.dirtyVertexHistory);
  const MetricSummary uploadSummary = summarizeSamples(telemetry.uploadByteHistory);
  const MetricSummary passSummary = summarizeSamples(telemetry.stabilizationPassHistory);
  const double averageFps =
      wallSeconds > 0.0 ? static_cast<double>(completedFrames) / wallSeconds : 0.0;

  if (needsHeader) {
    output << "frames,wall_seconds,avg_fps,avg_frame_ms,p95_frame_ms,max_frame_ms,"
              "avg_terrain_ms,p95_terrain_ms,max_terrain_ms,"
              "avg_dirty_vertices,p95_dirty_vertices,max_dirty_vertices,"
              "avg_upload_bytes,p95_upload_bytes,max_upload_bytes,"
              "avg_stabilization_passes,p95_stabilization_passes,max_stabilization_passes\n";
  }

  output << completedFrames << ',' << wallSeconds << ',' << averageFps << ','
         << frameSummary.average << ',' << frameSummary.p95 << ',' << frameSummary.maximum << ','
         << terrainSummary.average << ',' << terrainSummary.p95 << ',' << terrainSummary.maximum
         << ',' << dirtySummary.average << ',' << dirtySummary.p95 << ',' << dirtySummary.maximum
         << ',' << uploadSummary.average << ',' << uploadSummary.p95 << ','
         << uploadSummary.maximum << ',' << passSummary.average << ',' << passSummary.p95 << ','
         << passSummary.maximum << '\n';
  return true;
}

// just called on errors
void errorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << "\n";
}

// called on keyboard presses
void keyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// called on window resizes
void framebufferSizeCallback(GLFWwindow * /*window*/, int width, int height) {
  glViewport(0, 0, width, height);
}

void cursorPosCallBack(GLFWwindow *window, double xpos, double ypos) {
  // static means variables persist across calls without being global
  static bool firstMouseMovement = 1;
  static double lastX = 0.0;
  static double lastY = 0.0;

  // this is because the first xpos/ypos could be anywhere on the screen
  // and we dont want crazy drastic movement to start
  if (firstMouseMovement) {
    lastX = xpos;
    lastY = ypos;
    firstMouseMovement = 0;
    return;
  }

  float xOffset = xpos - lastX;
  float yOffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  Camera *camera = static_cast<Camera *>(glfwGetWindowUserPointer(window));
  camera->look(xOffset, yOffset);
}
} // namespace

int main(int argc, char **argv) {
  AppOptions options;
  const ParseResult parseResult = parseArguments(argc, argv, options);
  if (parseResult == ParseResult::ExitSuccess) {
    return EXIT_SUCCESS;
  }
  if (parseResult == ParseResult::ExitFailure) {
    return EXIT_FAILURE;
  }

  glfwSetErrorCallback(errorCallback);
  constexpr float BUCKET_SPEED = 2.0f;

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return EXIT_FAILURE;
  }

  // Request OpenGL 3.3 core profile
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(1280, 720, WINDOW_TITLE, nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  const int swapInterval = (options.disableVsync || options.benchmarkMode) ? 0 : 1;
  glfwSwapInterval(swapInterval);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

  Camera camera;
  // so that you can access the camera in the cursor pos callback
  glfwSetWindowUserPointer(window, &camera);
  if (options.benchmarkMode) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  } else {
    // hides cursor and locks it to window (for camera movement)
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, cursorPosCallBack);
  }

  // Load OpenGL functions
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0) {
    std::cerr << "Failed to initialize OpenGL context\n";
    glfwTerminate();
    return EXIT_FAILURE;
  }

  std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version)
            << '\n';
  std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

  glEnable(GL_DEPTH_TEST);

  // set background colour
  glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

  // clang-format off
  // each vertex: x, y, z, nx, ny, nz
  std::array<float, 144> cubeVertices = {
    // front face (+Z)
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    // back face (-Z)
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    // top face (+Y)
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    // bottom face (-Y)
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    // right face (+X)
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
    // left face (-X)
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
  };

  std::array<unsigned int, 36> cubeIndices = {
      0,  1,  2,  0,  2,  3,  // front
      4,  5,  6,  4,  6,  7,  // back
      8,  9,  10, 8,  10, 11, // top
      12, 13, 14, 12, 14, 15, // bottom
      16, 17, 18, 16, 18, 19, // right
      20, 21, 22, 20, 22, 23, // left
  };
  // clang-format on

  GLuint bucketVAO, bucketVBO, bucketEBO;

  glGenVertexArrays(1, &bucketVAO);
  glBindVertexArray(bucketVAO);

  glGenBuffers(1, &bucketEBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bucketEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, cubeIndices.size() * sizeof(unsigned int),
               cubeIndices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &bucketVBO);
  glBindBuffer(GL_ARRAY_BUFFER, bucketVBO);
  glBufferData(GL_ARRAY_BUFFER, cubeVertices.size() * sizeof(float), cubeVertices.data(),
               GL_DYNAMIC_DRAW);

  // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // normals
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // using shader class and giving path to shader code
  Shader basic_shader("shaders/basic.vert", "shaders/basic.frag");

  Terrain terrain;

  // this includes the view matrix
  // perspective matrix deals with converting 3d coordinates to 2d output (to screen)
  glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
  // perspective takes in fov, aspect ratio, when to clip a close object, when to clip a far object

  glm::vec3 bucketPos(1.5f, 0.5f, -1.5f);

  RuntimeTelemetry telemetry;
  if (options.benchmarkMode) {
    telemetry.enableHistory(options.benchmarkFrames);
  }

  const auto benchmarkStart = std::chrono::steady_clock::now();
  std::size_t benchmarkFramesCompleted = 0;
  double lastTime = glfwGetTime();

  // this is the main render loop that runs 60 times per second (60FPS)
  while (!glfwWindowShouldClose(window)) {
    const auto frameStart = std::chrono::steady_clock::now();

    // implement delta time so movements aren't frame dependent
    const double currentTime = glfwGetTime();
    const float realDeltaTime = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;
    const float deltaTime = options.benchmarkMode ? BENCHMARK_SIMULATION_DT : realDeltaTime;
    TerrainUpdateStats frameTerrainStats;

    if (options.benchmarkMode) {
      const glm::vec2 scriptedBucketPosition = benchmarkBucketPosition(benchmarkFramesCompleted);
      bucketPos.x = scriptedBucketPosition.x;
      bucketPos.z = scriptedBucketPosition.y;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // sets the background to chose colour

    if (!options.benchmarkMode) {
      // camera movement checks (wasd/shift/space)
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.moveForward(deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.moveLeft(deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.moveBackward(deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.moveRight(deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera.moveUp(deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera.moveDown(deltaTime);
      }

      // bucket movement checks (arrow keys)
      if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        bucketPos.x -= (BUCKET_SPEED * deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        bucketPos.x += (BUCKET_SPEED * deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        bucketPos.z += (BUCKET_SPEED * deltaTime);
      }
      if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        bucketPos.z -= (BUCKET_SPEED * deltaTime);
      }
    }

    basic_shader.use();

    // terrain
    terrain.draw(basic_shader, perspective * camera.getViewMatrix());

    // bucket
    auto [bucketRow, bucketCol] = terrain.worldToGrid(bucketPos.x, bucketPos.z);

    // also set the bucket's height to the current terrain height
    // to make it look like its 'digging'
    bucketPos.y = terrain.getHeight(bucketRow, bucketCol).value_or(0.0f);
    glm::mat4 bucketModel =
        glm::translate(glm::mat4(1.0f), bucketPos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
    glm::mat4 bucketMvp = perspective * camera.getViewMatrix() * bucketModel;
    basic_shader.uniformInfo("mvp", bucketMvp);
    basic_shader.uniformInfo("objectColour", glm::vec3(0.9f, 0.75, 0.2f));
    glBindVertexArray(bucketVAO);
    glDrawElements(GL_TRIANGLES, cubeIndices.size(), GL_UNSIGNED_INT, 0);

    if (options.benchmarkMode) {
      const TerrainAction action = benchmarkActionForFrame(benchmarkFramesCompleted);
      const bool dig = action == TerrainAction::Dig;
      accumulateTerrainStats(frameTerrainStats, terrain.modify(bucketRow, bucketCol, dig, deltaTime));
    } else {
      // button to dig
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        accumulateTerrainStats(frameTerrainStats,
                               terrain.modify(bucketRow, bucketCol, true, deltaTime));
      }

      // button to dump
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        accumulateTerrainStats(frameTerrainStats,
                               terrain.modify(bucketRow, bucketCol, false, deltaTime));
      }
    }

    glfwSwapBuffers(window); // shows new frame
    glfwPollEvents();        // polls for actions

    const auto frameEnd = std::chrono::steady_clock::now();
    const double frameDurationMs =
        std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
    telemetry.recordFrame(frameDurationMs);
    telemetry.recordTerrainUpdate(frameTerrainStats);

    std::size_t completedFrames = benchmarkFramesCompleted;
    if (options.benchmarkMode) {
      ++benchmarkFramesCompleted;
      completedFrames = benchmarkFramesCompleted;
    }

    telemetry.updateWindowTitle(window, glfwGetTime(),
                                buildWindowTitlePrefix(options, completedFrames));

    if (options.benchmarkMode && benchmarkFramesCompleted >= options.benchmarkFrames) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  }

  if (options.benchmarkMode) {
    const auto benchmarkEnd = std::chrono::steady_clock::now();
    const double wallSeconds =
        std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();
    printBenchmarkSummary(telemetry, wallSeconds, benchmarkFramesCompleted);
    if (options.writeCsv) {
      writeBenchmarkCsv(options, telemetry, wallSeconds, benchmarkFramesCompleted);
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
