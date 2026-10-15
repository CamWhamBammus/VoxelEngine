# VoxelEngine

A C++20 voxel terrain editor for macOS and Linux using OpenGL 4.1, GLFW, GLEW, and GLM. It generates a finite terrain world, supports undoable block editing and world saves, and uses greedy meshing and view-frustum culling to reduce rendering work.

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
| Scroll | Zoom by adjusting the field of view |
| R | Reset camera position, direction, and zoom |
| Space / Left Ctrl | Move up / down |
| Left Shift | Boost movement speed |
| Left click | Remove the targeted block |
| Right click | Place the selected material beside the targeted block |
| Middle click | Pick the targeted block's material |
| 1 / 2 / 3 / 4 | Select Grass / Dirt / Stone / Sand |
| F | Toggle wireframe |
| Ctrl/Cmd + Z | Undo the last edit |
| Ctrl/Cmd + Y or Ctrl/Cmd + Shift + Z | Redo an edit |
| F5 / F9 | Save / reload the current world file |
| H | Show or hide the expanded controls overlay |
| Escape | Release the cursor |
| Tab | Toggle cursor capture |
| Q | Quit |

Movement uses a flying camera with no collision or gravity. Editing works while the cursor is captured, within eight blocks of the camera. The targeted block has a yellow outline; placement cannot fill the camera's current cell. Undo history keeps the latest 256 edits for this session. A new edit after undo clears the redo branch.

The HUD shows the selected material, seed, camera position, zoom, undo/redo counts, visible chunks, submitted/total triangles, and average frame rate. `UNSAVED` and a title-bar `*` indicate changes from the loaded or last-saved contents; undoing back to that state clears them. FPS appears after the first sampling interval.

## Save and reload

By default, startup loads `world.vxl` from the current working directory if it exists, or creates new terrain. Press **F5 before quitting** to keep edits; there is no automatic save on exit. Press **F9** to reload the file, discarding unsaved edits and clearing undo history only if loading succeeds. File errors appear in the HUD and terminal, and leave the current session available.

Choose another save file:

```sh
./build/VoxelEngine --world my-world.vxl
```

A missing file starts a new world and is created on the first F5 save. Its parent directory must already exist. Saves store the seed and every block; camera state and edit history are not saved. The versioned format validates dimensions, materials, and a checksum before replacing the active world. Saving writes and flushes a temporary sibling file, then atomically replaces the destination on macOS/Linux. Failed writes do not delete the old save. This is not a backup system: keep copies of worlds you want to preserve.

World files (`*.vxl`) are ignored by Git. To start fresh from a seed without automatically loading the default save:

```sh
./build/VoxelEngine --seed 42
```

If both `--world` and `--seed` are provided, an existing world's saved seed takes precedence; a missing world uses the requested seed. F5 writes to the chosen path, including `world.vxl` when no path was provided.

## Terrain and command-line options

The world contains a fixed 5 by 5 grid of chunks. Each chunk holds 16 by 16 by 16 blocks. World coordinates span `[-32, 47]` on X and Z, and `[0, 15]` on Y. Coordinates outside this volume are empty and cannot be edited.

Terrain generation is deterministic for a given seed. The default seed is `1337`. Greedy meshing merges adjacent exposed faces with the same material and normal. The default world's mesh contains 3,286 triangles, compared with 34,808 before merging; this is a geometry count, not an FPS benchmark. Chunks outside the camera's view are skipped during drawing, and edits rebuild affected chunks and seam neighbors.

For a graphics smoke test, create a hidden window, check shader/buffer error handling and render transitions (editing, undo/redo, reload, wireframe, and culling), then render three final frames, verify terrain in the depth buffer, and exit:

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

Smoke tests never implicitly load or save `world.vxl`. Add `--world path.vxl` to explicitly render an existing save. A fully empty saved world fails the terrain-presence check. Screenshots include the HUD and use portable binary PPM format. Run `--help` to list options.

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

Tests cover storage bounds, material validation, camera math, command-line parsing, ray traversal, edit history and key transitions, frustum planes, corrupted saves, atomic write failures, and a complete edit/save/reload session. Greedy meshes are expanded into unit faces and compared with a simple reference across random and structured chunks to detect holes, overlaps, winding errors, and incorrect material merging.

GitHub Actions builds Debug and Release on Linux, runs core and sanitizer tests, and exercises graphics transitions and screenshot capture with Mesa through Xvfb.

## Code layout

- `src/main.cpp` manages the session, file actions, application loop, and status text.
- `src/core/` contains the camera, frustum, options, keyboard/mouse handling, and GLFW window lifetime. Camera and frustum math have no graphics-context dependency.
- `src/voxel/` contains materials, chunks, terrain, greedy meshes, raycasting, history, and persistence. World meshing samples adjacent chunks to hide shared faces. Editing marks affected chunks for rebuilding.
- `src/render/` contains shader/GPU ownership, the terrain renderer, selection overlay, bitmap HUD, and graphics diagnostics. GPU objects are destroyed while the OpenGL context is alive.
- `tests/` contains standalone CTest executables for the core behavior.

The `VoxelCore` CMake target is independent of the viewer's OpenGL dependencies. Terrain is finite; chunk streaming, collision, and gravity are not implemented.
