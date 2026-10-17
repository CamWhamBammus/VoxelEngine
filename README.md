# VoxelEngine

A C++20 voxel terrain editor for macOS and Linux using OpenGL 4.1, GLFW, GLEW, and GLM. It generates a finite terrain world, supports undoable block editing and world saves, and uses greedy meshing and view-frustum culling to reduce rendering work.

## Understand the code

Start with [src/main.cpp](src/main.cpp). It opens the window and repeats three steps: **read input → update the game → draw the frame**.

Each module has a `.h` file that declares its types and functions, and a matching `.cpp` file that implements them. Related code stays together:

```text
src/
  main.cpp          Startup and the main loop
  game.h/.cpp       Game state, player actions, and status text
  input.h/.cpp      Keyboard and mouse controls
  camera.h/.cpp     Camera movement, view math, and visibility checks
  world.h/.cpp      Blocks, chunks, world storage, and terrain generation
  mesh.h/.cpp       Turn blocks into triangles
  editing.h/.cpp    Find the block under the cursor; place, remove, undo, redo
  save.h/.cpp       Read and write world files
  graphics/
    window.h/.cpp   Open and close the GLFW window
    opengl.h/.cpp   Compile shaders and upload mesh data to the GPU
    renderer.h/.cpp Draw terrain and the selected block's outline
    hud.h/.cpp      Draw text on the screen
tests/              Behavior tests and graphics smoke checks
CMakeLists.txt      List the source files and build targets
```

Read these first:

1. [main.cpp](src/main.cpp) shows how everything connects.
2. [game.h](src/game.h) names the current world, camera, selected material, and edit history. [Game::update in game.cpp](src/game.cpp) handles the player's actions in order.
3. [world.h](src/world.h) explains the data: a `Block` is a material, a `Chunk` stores a 16×16×16 cube of blocks, and a `World` holds the chunks.
4. [editing.cpp](src/editing.cpp) shows how aiming and editing work. `raycast` follows the camera's viewing direction through blocks to find a hit.

For one frame, `Input::update` moves the camera and fills an `InputActions` value. `Game::update` uses those actions to save, load, undo, or edit. `WorldRenderer::render` rebuilds meshes for changed chunks and draws visible chunks. The outline and HUD are drawn last. World storage does not depend on the renderer.

Each renderer remembers the chunk contents it last drew. It rebuilds changed chunks and their neighbors, so switching worlds or opening another view cannot lose mesh updates. This uses a small world snapshot and straightforward comparisons; no global cache or event system is needed.

Use this map when changing something:

| I want to change… | Start here |
| --- | --- |
| Keys, mouse sensitivity, or movement speed | [input.cpp](src/input.cpp) |
| Starting camera position and direction | [camera.h](src/camera.h) |
| Zoom limits | [camera.cpp](src/camera.cpp) |
| Block colors and names | [world.h](src/world.h) |
| Terrain shape | `terrainHeight` and `generateTerrain` in [world.cpp](src/world.cpp) |
| Default editing reach | `Options` in [game.h](src/game.h), or the `--reach` flag |
| The order of player actions | [game.cpp](src/game.cpp) |
| Placement rules or undo behavior | [editing.cpp](src/editing.cpp) |
| HUD words and controls text | `Game::statusText` in [game.cpp](src/game.cpp) |
| Lighting or the selection outline | [graphics/renderer.cpp](src/graphics/renderer.cpp) |

You can learn the game logic before reading `graphics/`, the greedy meshing algorithm, or the binary save format. Those details are contained in their own modules. The `VoxelCore` build target and its tests work without an OpenGL window.

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

Run `./build/VoxelEngine --help` (or `-h`) for options, startup rules, and examples. Help works without a display and does not read or write world files.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Fly forward / left / backward / right |
| Mouse | Look around while the cursor is captured |
| Scroll | Zoom by adjusting the field of view |
| R | Reset camera position, direction, and zoom |
| Space / Left Ctrl | Move up / down |
| Left Shift | Boost movement speed |
| Left Alt / Option | Slow movement for precise editing (takes priority over Shift) |
| Left click | Remove the targeted block |
| Right click | Place the selected material beside the targeted block |
| Middle click | Pick the targeted block's material |
| 1 / 2 / 3 / 4 | Select Grass / Dirt / Stone / Sand |
| [ / ] | Cycle to the previous / next material |
| F | Toggle wireframe |
| Ctrl/Cmd + Z | Undo the last edit |
| Ctrl/Cmd + Y or Ctrl/Cmd + Shift + Z | Redo an edit |
| F5 / F9 | Save / reload the current world file |
| H | Show or hide the expanded controls overlay |
| Escape | Release the cursor |
| Tab | Toggle cursor capture |
| Q | Quit |

Movement uses a flying camera with no collision or gravity. Editing works while the cursor is captured, within eight blocks of the camera by default. Use `--reach N` to choose a whole-number distance from 1 to 64 blocks, such as `./build/VoxelEngine --reach 16`. Reach is a session setting; it does not change the save format. The targeted block has a yellow outline; placement cannot fill the camera's current cell. Undo history keeps the latest 256 edits for this session. A new edit after undo clears the redo branch.

The HUD shows the selected material, seed, camera position, zoom, undo/redo counts, visible chunks, submitted/total triangles, and average frame rate. `UNSAVED` and a title-bar `*` indicate changes from the loaded or last-saved contents; undoing back to that state clears them. FPS appears after the first sampling interval.

The target row identifies the block under the crosshair, its world coordinates, and its distance compared with your editing reach. It shows `TARGET NONE` when nothing is in range and `TARGET PAUSED` while the mouse is released.

Editing messages confirm the material placed, removed, or picked. If placement fails, the HUD explains why—for example, the destination is outside the world or contains the camera. Rejected edits do not create undo entries.

The crosshair and HUD scale with the display. The crosshair has a dark border so it remains visible over bright terrain and sky.

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

Tests cover storage bounds, material validation, camera math, command-line parsing, ray traversal, edit history and key transitions, frustum planes, corrupted saves, atomic write failures, and complete edit/save/reload sessions through both the world helpers and `Game::update`. Greedy meshes are expanded into unit faces and compared with a simple reference across random and structured chunks to detect holes, overlaps, winding errors, and incorrect material merging.

GitHub Actions builds Debug and Release on Linux, runs core and sanitizer tests, and exercises graphics transitions and screenshot capture with Mesa through Xvfb.

Terrain is finite; chunk streaming, collision, and gravity are not implemented.
