# VoxelEngine

A C++20 voxel terrain viewer using OpenGL 4.1, GLFW, GLEW, and GLM. It generates a finite terrain world, renders exposed voxel faces, and supports flying around and editing blocks.

## Build and run

Install a C++20 compiler and CMake 3.20 or newer, plus the graphics dependencies.

macOS with Homebrew:

```sh
brew install cmake glfw glew glm
```

Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake libglfw3-dev libglew-dev libglm-dev libgl1-mesa-dev
```

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/VoxelEngine
```

The viewer needs a display and a driver supporting OpenGL 4.1 core.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Fly forward / left / backward / right |
| Mouse | Look around while the cursor is captured |
| Space / Left Ctrl | Move up / down |
| Left Shift | Boost movement speed |
| Left click | Remove the targeted block |
| Right click | Place the selected material beside the targeted block |
| 1 / 2 / 3 / 4 | Select Grass / Dirt / Stone / Sand |
| F | Toggle wireframe |
| Escape | Release the cursor |
| Tab | Toggle cursor capture |
| Q | Quit |

Movement uses a flying camera with no collision or gravity. Editing works while the cursor is captured, within eight blocks of the camera. The targeted block has a yellow outline; placement cannot fill the camera's current cell. Block edits are held in memory and are lost when the application exits.

## Terrain and command-line options

The world contains a fixed 5 by 5 grid of chunks. Each chunk holds 16 by 16 by 16 blocks. World coordinates span `[-32, 47]` on X and Z, and `[0, 15]` on Y. Coordinates outside this volume are empty and cannot be edited.

Terrain generation is deterministic for a given seed. The default seed is `1337`:

```sh
./build/VoxelEngine --seed 42
```

For a graphics smoke test, create a hidden window, check shader and buffer error handling, render three frames, verify terrain in the depth buffer, and exit:

```sh
./build/VoxelEngine --smoke-test
```

Write a rendered frame as a PPM image; this also enables smoke-test mode:

```sh
./build/VoxelEngine --seed 42 --screenshot build/terrain.ppm
```

Hidden-window tests still require a functioning OpenGL context. On a Linux machine without a display, install `xvfb`, `xauth`, and Mesa, then run:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/VoxelEngine --smoke-test
```

## Tests

Tests are enabled by default and run without opening a window:

```sh
ctest --test-dir build --output-on-failure
```

The core library and tests can be built without GLFW, GLEW, or OpenGL. This configuration needs only a C++20 compiler, CMake, and GLM:

```sh
cmake -S . -B build-core -DVOXEL_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

For additional memory and undefined-behavior checks with Clang or GCC:

```sh
cmake -S . -B build-sanitize \
  -DVOXEL_BUILD_APP=OFF \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

GitHub Actions builds Debug and Release configurations on Linux, runs the core tests, and exercises the viewer with Mesa through Xvfb.

## Code layout

- `src/main.cpp` manages the application loop, input, rendering, and command-line options.
- `src/core/` contains the fly camera and GLFW window lifetime management. Camera math has no graphics-context dependency.
- `src/voxel/` contains block materials, chunk storage, terrain generation, mesh construction, and world access. World meshing samples adjacent chunks to hide shared faces. Editing marks the affected chunks for rebuilding.
- `src/render/` contains shader-program and GPU-mesh ownership. These objects are created and destroyed while the OpenGL context is alive.
- `tests/` contains standalone CTest executables for the core behavior.

The `VoxelCore` CMake target is independent of the viewer's OpenGL dependencies. Terrain is finite; chunk streaming, collision, and save files are not implemented.
