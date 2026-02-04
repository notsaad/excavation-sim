# Excavation Simulator

A real-time terrain deformation simulator built with C++ and OpenGL. An excavator bucket digs into deformable terrain, demonstrating soil mechanics relevant to autonomous earthmoving — including material removal, accumulation, angle of repose, and different soil types.

## Building

Requires CMake 3.20+ and a C++17 compiler. Dependencies (GLFW, GLM) are fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Running

```bash
./build/excavation-sim
```

Press Escape to close the window.
