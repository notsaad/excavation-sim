# Learning Notes

## OpenGL Fundamentals

### Everything is Triangles
- The GPU only rasterizes triangles — all geometry (quads, cubes, terrain, characters) is broken into triangles before rendering
- 3 points are always coplanar (on the same flat plane), making rasterization math simple and fast
- 4+ points might not be coplanar, creating ambiguity in how to fill the surface

### Vertex Attributes
- Attributes are inputs to the vertex shader (e.g. position, normal)
- `glVertexAttribPointer` tells OpenGL how to read data from a buffer and feed it into shader inputs
- `layout (location = N)` in the shader links to `glVertexAttribPointer(N, ...)`
- Stride = bytes to skip to reach the next vertex; Offset = where in each vertex this attribute starts

### Vertices vs Indices
- **Vertices**: raw data for each point (position, normal, etc.), uploaded to GPU via VBO
- **Indices**: tell OpenGL how to connect vertices into triangles, avoiding data duplication (stored in EBO)
- Example: a quad uses 4 vertices + 6 indices (2 triangles) instead of 6 duplicate vertices

### Normals
- A normal is the direction a surface is "facing"
- Used for lighting: `dot(normal, lightDir)` determines brightness
- Flat surfaces (cube faces) share one normal per face
- Curved surfaces (terrain) compute normals from neighboring heights using cross product
- GPU interpolates normals between vertices across triangle faces (Gouraud shading) — this is why lighting looks smooth rather than faceted

### VBO, VAO, EBO
- **VBO (Vertex Buffer Object)** — raw chunk of GPU memory holding vertex data (positions, normals, etc.). Just a blob of floats.
- **VAO (Vertex Array Object)** — describes how to read the VBO. Stores `glVertexAttribPointer` config (stride, offset, types). Bind a VAO and OpenGL remembers all the layout.
- **EBO (Element Buffer Object)** — holds index data (which vertices connect into triangles). Avoids duplicating vertex data for shared corners.
- Together: VBO = "here's the data", VAO = "here's how to interpret it", EBO = "here's which ones to connect"
- Switching objects (e.g. terrain vs bucket) is just binding a different VAO — each VAO remembers its own VBO, EBO, and attribute layout

### CPU ↔ GPU Data Flow
- Changing data in CPU memory (e.g. height array) does NOT update what's on screen
- Must re-upload to GPU via `glBufferData` to replace the old vertex data
- `GL_STATIC_DRAW` — data set once, used many times (e.g. cube mesh)
- `GL_DYNAMIC_DRAW` — data updated frequently (e.g. terrain that gets modified by digging)

### Uniforms
- Values passed from C++ to shaders, constant for all vertices/fragments in a draw call
- Declared outside `main()` in shader code: `uniform vec3 objectColour;`
- Set from C++ via `glUniform*` functions (e.g. `glUniform3fv` for vec3, `glUniformMatrix4fv` for mat4)
- Used for per-object data like color, MVP matrix, light direction

### GLFW Input: Callbacks vs Polling
- **Callbacks** (`glfwSetCursorPosCallback`, `glfwSetKeyCallback`) — fire once per event. Good for discrete actions.
- **Polling** (`glfwGetKey`) — checked every frame in the render loop. Good for continuous input (movement, holding to dig).
- `glfwSetWindowUserPointer` / `glfwGetWindowUserPointer` lets callbacks access objects (like camera) without globals

## C++ Concepts

### Anonymous Namespaces
- Makes contents have internal linkage — only visible within that translation unit (.cpp file)
- Modern C++ equivalent of marking functions `static`
- Prevents name collisions across .cpp files

### `static` Inside Functions
- Variable is initialized once and persists for the lifetime of the program
- Not destroyed when the function returns — next call sees the same value
- Different from `static` at file scope (which controls linkage)

### `std::array` vs `std::vector`
- `std::array`: fixed size, stack-allocated, no heap overhead — use when size is known at compile time
- `std::vector`: dynamic size, heap-allocated — use when size can change at runtime

### C++ Cast Types
- **`static_cast`** — standard conversions (int↔float, base↔derived pointers). Use 95% of the time. Checked at compile time.
- **`dynamic_cast`** — safe downcast with runtime check (polymorphic classes). Returns nullptr if cast fails.
- **`const_cast`** — add/remove `const`. Rarely needed.
- **`reinterpret_cast`** — raw bit reinterpretation. Dangerous, almost never needed.
- Avoid C-style casts `(int)x` — they can silently do any of the above without making intent clear.
