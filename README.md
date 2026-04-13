# Excavation Simulator

Real-time terrain deformation simulator built with C++ and OpenGL. An excavator bucket digs into deformable soil, and the terrain responds with local material removal, accumulation, and stabilization behavior.

This project is positioned as a small systems-focused graphics/simulation project: it emphasizes data movement, incremental updates, and measurable runtime behavior rather than just rendering a scene.

## Highlights

- Real-time deformable `64x64` terrain mesh (`4,096` vertices, `7,938` triangles)
- Local soil stabilization pass that relaxes steep height differences toward an angle-of-repose style constraint
- Dirty-region vertex rebuilds and partial GPU buffer uploads via `glBufferSubData` instead of full terrain re-uploads
- Live runtime telemetry for FPS, average and p95 frame time, terrain update cost, dirty vertices, upload bytes, and stabilization passes
- Deterministic benchmark mode with fixed-step simulation, scripted dig/dump workload, stdout summary, and optional CSV export

## Performance and Systems Focus

- Terrain edits are applied on the CPU, then only affected vertices and neighboring normals are rebuilt
- GPU traffic is bounded by uploading only the touched vertex range instead of replacing the full terrain buffer each update
- Runtime telemetry makes frame pacing and terrain-update cost visible during interactive use
- Benchmark mode makes performance claims reproducible instead of anecdotal

## Runtime Telemetry

During normal interactive runs, the window title updates once per second with:

- FPS
- Average frame time
- P95 frame time
- Average terrain update time
- Average dirty vertices per terrain update
- Average upload size per terrain update
- Average stabilization passes per terrain update

Example title:

```text
Excavation Simulator | 144.2 FPS | frame 6.9 ms avg / 8.4 p95 | terrain 0.2 ms | dirty 42 | upload 1.0 KiB | passes 3.4
```

## Benchmark Mode

Benchmark mode runs a deterministic scripted workload and prints a summary to stdout at the end of the run.

Supported flags:

- `--benchmark`
- `--frames=N`
- `--no-vsync`
- `--csv=PATH`

Example:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/excavation-sim --benchmark --frames=5000 --no-vsync --csv=benchmarks/run.csv
```

Benchmark output includes:

- Average, p95, and max frame time
- Average, p95, and max terrain update time
- Average, p95, and max dirty vertices per update
- Average, p95, and max upload bytes per update
- Average, p95, and max stabilization passes per update

## Build

Requires CMake `3.20+` and a C++17 compiler. Dependencies (`GLFW`, `GLM`) are fetched automatically.

Interactive build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Release build for publishable benchmark numbers:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

Interactive run:

```bash
./run.sh
```

Or run the executable directly:

```bash
./build/excavation-sim
```

## Controls

- Mouse: look around
- `W A S D`: move camera
- `Space` / `Left Shift`: move camera up / down
- Arrow keys: move bucket
- `E`: dig
- `Q`: dump
- `Esc`: quit

## Why This Project Matters

This is not just an OpenGL demo. It shows:

- Real-time simulation updates tied to rendering
- Explicit CPU-to-GPU data movement decisions
- Incremental update strategies instead of brute-force full rebuilds
- Built-in instrumentation and benchmark tooling to support performance discussion on a resume
