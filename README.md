# VoxelEngine

A small C++20 voxel game for macOS and Linux using OpenGL 4.1, GLFW, GLEW, and GLM. The direction is an **entirely underground expedition game**: leave a starting village, find resources, return, and upgrade the settlement.

This version is an explorable **underground hub** with 28 buildings, a giant central tree, lakes, bridges, waterfalls, and an expedition departure hall. The buildings and departure area are physical spaces only; services, village upgrades, villagers, and expedition transitions are not implemented yet.

## Explore the underground hub

Launch normally on the arrival avenue inside a large enclosed cavern. Four bridges connect the central tree island to waterfront paths and neighborhoods of cottages, halls, workshops, and storehouses. Every building has an accessible interior. Follow the northern avenue to the expedition departure hall, or explore the waterfalls, mineral outcrops, and crystal gardens. Shallow shoreline steps provide a way out of the basins.

The cave palette includes **stone, pale limestone, dark slate, coal ore, copper ore, iron ore, and glowing crystal**. Buildings use **timber, planks, brick, roof tiles, and lanterns**. **Leaves and glowing leaves** form the tree canopy and planted borders. **Water** has a visible stylized surface but no collision.

The art direction is colorful fantasy: turquoise crystals and mossy greens against blue-violet rock shadows and amber village lights. Block surfaces have painted mineral planes, chipped stone edges, embedded ore flecks, wood grain, recessed plank seams, brick mortar, and elongated crystal facets generated directly in the shader. Solid blocks occlude local lights, nearby light sources cast softened shadows, contact shading darkens corners, and distance mist separates the cavern's layers. A restrained headlamp keeps nearby rock readable away from the village.

Underground rendering uses HDR color, multisample antialiasing, and a small bloom pass for visible luminous surfaces. The final exposure and color pass keeps bright lanterns and crystals colorful while smoothing their glow. The HUD is drawn afterwards, keeping it sharp. The surface editor retains its direct rendering path and block selection outline.

Paired crystal spires, occasional short wall growths, ceiling formations, and leafy grass tufts with sparse flowers are decorative meshes attached to exposed blocks. These details have no collision; movement follows the underlying voxel shapes.

Stone, limestone, and slate have sparse chipped fracture edges, with fewer chips on floors. Coal, copper, and iron ore form elongated mineral fragments with a shared vein direction. Most of each fragment sits nearly flush with the rock; only a small broken edge protrudes. The geometry follows the painted shapes, extending at most 0.03 blocks for stone and 0.05 for ore. Softer, interrupted stone seams and broad shading keep the cave walls from looking like laid tiles. Their distribution is fixed by the world seed. These details remain cosmetic, so walking uses the original block shapes.

**WASD** walks, **mouse** looks, **Space** jumps, and **Shift** sprints. **R** returns you to a safe space in the village. **Escape** pauses and releases the mouse; **Tab** resumes. **H** shows the walking controls, and **Q** quits.

Normal play currently supports exploration only. Ore extraction is planned as a future pickaxe mechanic and is not implemented. Block editing and material selection are available exclusively in the separate `--editor` mode.

The hub follows a fixed architectural plan, with seeded variations in rock, ore, and foliage. It resets on launch and does not load or overwrite editor saves. Use `--seed N` to change the variation. The original surface editor remains available through `--editor`.

## Understand the code

Start with [src/main.cpp](src/main.cpp). It opens the window and repeats three steps: **read input → update the game → draw the frame**.

Most modules have a `.h` file that declares their types and functions, and a matching `.cpp` file that implements them. `materials.h` contains the shared material shader text. Related code stays together:

```text
src/
  main.cpp          Startup and the main loop
  game.h/.cpp       Play/editor modes, movement actions, and HUD text
  player.h/.cpp     Player body, terrain collision, walking, jumping, and spawning
  scene.h/.cpp      Hub layout, landscape, tree, water, bridges, and departure hall
  hub_buildings.h/.cpp Rotatable cottages, halls, workshops, and storehouses
  input.h/.cpp      Keyboard and mouse controls
  camera.h/.cpp     Camera movement, view math, and visibility checks
  world.h/.cpp      Blocks, chunks, world storage, and terrain generation
  mesh.h/.cpp       Turn blocks into triangles
  editing.h/.cpp    Editor targeting, block changes, and undo/redo
  save.h/.cpp       Read and write world files
  graphics/
    window.h/.cpp   Open and close the GLFW window
    opengl.h/.cpp   Compile shaders and upload mesh data to the GPU
    renderer.h/.cpp Draw the world, cave lighting, shadows, and selection outlines
    materials.h    Procedural block paint, grain, facets, and moss
    post_process.h/.cpp Smooth edges, luminous bloom, exposure, and color
    cave_details.h/.cpp Build cosmetic crystal shards, ceiling formations, and tufts
    block_relief.h/.cpp Build sparse rock chips and shallow ore veins
    hud.h/.cpp      Draw text on the screen
tests/              Behavior tests and graphics smoke checks
CMakeLists.txt      List the source files and build targets
```

Read these first:

1. [main.cpp](src/main.cpp) shows how everything connects.
2. [game.h](src/game.h) names the current world and camera, plus the editor's selected material and edit history. [Game::update in game.cpp](src/game.cpp) handles the player's actions in order.
3. [world.h](src/world.h) explains the data: a `Block` is a material, a `Chunk` stores a 16×16×16 cube of blocks, and a `World` holds a 9×4×9 grid of chunks on the heap.
4. [scene.cpp](src/scene.cpp) lays out the hub and landscape; [hub_buildings.cpp](src/hub_buildings.cpp) builds its four architectural styles. [player.cpp](src/player.cpp) handles the body and movement.
5. [editing.cpp](src/editing.cpp) shows how aiming and editing work. `raycast` follows the camera's viewing direction through blocks to find a hit.

For one frame, `Input::update` handles looking and fills an `InputActions` value. In play mode, `Game::update` moves the `Player` and follows its eyes with the camera; `Player` resolves movement against solid voxels. `scene.cpp` builds the hub once at startup. `Game::update` handles walking and returning to the village. Only editor mode processes targeting, block changes, saving, loading, and undo. `WorldRenderer::render` rebuilds meshes for changed chunks and draws visible chunks. The outline and HUD are drawn last. World storage does not depend on the renderer.

Each renderer remembers the chunk contents it last drew. It rebuilds changed chunks and their neighbors, so switching worlds or opening another view cannot lose mesh updates. This uses a small world snapshot and straightforward comparisons; no global cache or event system is needed. The renderer also uploads the solid cells to a small 3D texture when the world changes. Cave lighting samples it for contact shading and short shadow rays, so world changes update lighting alongside geometry. Decorative meshes reuse the block materials but do not enter that solid-cell texture or cast their own shadows.

Light candidates are cached per chunk, including every source whose radius can reach its blocks or decorations. Nearby crystal and glowing-leaf blocks share fixed sources; each lantern keeps its own source. Every source that reaches a surface contributes, so stepping away cannot switch off its illumination. Candidates include neighboring chunks and rebuild after world edits. The shader skips sources outside their radius or behind the surface before checking shadows. Very dense custom lighting can increase rendering cost.

`PostProcess::begin` provides the underground world with an HDR color/depth target. `finish` resolves the smooth edges, blurs only emissive color at quarter resolution, and combines it with the scene. It returns the image and depth to the original framebuffer, so block outlines still hide correctly behind walls. Its buffers resize with the window and are reused between frames.

Use this map when changing something:

| I want to change… | Start here |
| --- | --- |
| Keys, mouse sensitivity, or editor flight speed | [input.cpp](src/input.cpp) |
| Walking speed, jumping, or collision | [player.cpp](src/player.cpp) |
| Cavern, tree, water, bridges, building positions, or ore deposits | [scene.cpp](src/scene.cpp) |
| Building walls, doors, windows, or roof shapes | [hub_buildings.cpp](src/hub_buildings.cpp) |
| Editor starting camera position and view direction | [camera.h](src/camera.h) |
| Village spawn and return behavior | [scene.h](src/scene.h) and [game.cpp](src/game.cpp) |
| Zoom limits | [camera.cpp](src/camera.cpp) |
| Block colors and names | [world.h](src/world.h) |
| Terrain shape | `terrainHeight` and `generateTerrain` in [world.cpp](src/world.cpp) |
| Default editing reach | `Options` in [game.h](src/game.h), or the `--reach` flag |
| The order of player actions | [game.cpp](src/game.cpp) |
| Editor placement rules or undo behavior | [editing.cpp](src/editing.cpp) |
| HUD words and controls text | `Game::statusText` and `playStatusText` in [game.cpp](src/game.cpp) |
| Material textures, wood grain, or crystal colors | [graphics/materials.h](src/graphics/materials.h) |
| Lantern lighting, shadows, or the selection outline | [graphics/renderer.cpp](src/graphics/renderer.cpp) |
| Glow, antialiasing, exposure, or final color | [graphics/post_process.cpp](src/graphics/post_process.cpp) |
| Decorative crystal shards, ceiling formations, or moss tufts | [graphics/cave_details.cpp](src/graphics/cave_details.cpp) |
| Chipped stone edges and embedded ore veins | [graphics/block_relief.cpp](src/graphics/block_relief.cpp) |

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

The game needs a display and a driver supporting OpenGL 4.1 core.

Run `./build/VoxelEngine --help` (or `-h`) for options, startup rules, and examples. Help works without a display and does not read or write world files.

## Editor controls

Run `./build/VoxelEngine --editor` for block removal and placement, unlimited materials, free flight, undo/redo, and world files. Water can be selected and edited even though it has no collision. These tools do not run in normal play. Press **H** in either mode for its controls.

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
| 1–9 | Select Grass, Dirt, Stone, Sand, Limestone, Slate, Coal, Copper, Iron |
| [ / ] | Cycle to the previous / next material |
| F | Toggle wireframe |
| Ctrl/Cmd + Z | Undo the last edit |
| Ctrl/Cmd + Y or Ctrl/Cmd + Shift + Z | Redo an edit |
| F5 / F9 | Save / reload the current world file |
| H | Show or hide the expanded controls overlay |
| Escape | Release the cursor |
| Tab | Toggle cursor capture |
| Q | Quit |

Movement uses a flying camera with no collision or gravity. Editing works while the cursor is captured, within eight blocks of the camera by default. Use `--reach N` to choose a whole-number distance from 1 to 64 blocks, such as `./build/VoxelEngine --editor --reach 16`. Reach is a session setting; it does not change the save format. The targeted block has a yellow outline; solid placement cannot fill the camera's current cell; non-solid water can. Undo history keeps the latest 256 edits for this session. A new edit after undo clears the redo branch.

The HUD shows the selected material, seed, camera position, zoom, undo/redo counts, visible chunks, submitted/total triangles, and average frame rate. `UNSAVED` and a title-bar `*` indicate changes from the loaded or last-saved contents; undoing back to that state clears them. FPS appears after the first sampling interval.

The target row identifies the block under the crosshair, its world coordinates, and its distance compared with your editing reach. It shows `TARGET NONE` when nothing is in range and `TARGET PAUSED` while the mouse is released.

Editing messages confirm the material placed, removed, or picked. If placement fails, the HUD explains why—for example, the destination is outside the world or contains the camera. Rejected edits do not create undo entries.

The crosshair and HUD scale with the display. The crosshair has a dark border so it remains visible over bright terrain and sky.

## Save and reload

In **editor mode**, startup loads `world.vxl` from the current working directory if it exists, or creates new terrain. Press **F5 before quitting** to keep edits; there is no automatic save on exit. Press **F9** to reload the file, discarding unsaved edits and clearing undo history only if loading succeeds. File errors appear in the HUD and terminal, and leave the current session available.

Choose another save file:

```sh
./build/VoxelEngine --editor --world my-world.vxl
```

A missing file starts a new world and is created on the first F5 save. Its parent directory must already exist. Saves store the seed and every block; camera state and edit history are not saved.

Version 2 stores the expanded 9×4×9 chunk volume. Original version 1 saves import their 5×1×5 region at the same coordinates, preserving all edits; the added space uses terrain generated from the saved seed. Saving an imported world writes version 2.

Loading validates dimensions, materials, and a checksum before replacing the active world. Saving writes and flushes a temporary sibling file, then atomically replaces the destination on macOS/Linux. Failed writes do not delete the old save. This is not a backup system: keep copies of worlds you want to preserve.

World files (`*.vxl`) are ignored by Git. To start fresh from a seed without automatically loading the default save:

```sh
./build/VoxelEngine --editor --seed 42
```

With `--editor`, if both `--world` and `--seed` are provided, an existing world's saved seed takes precedence; a missing world uses the requested seed. F5 writes to the chosen path, including `world.vxl` when no path was provided.

## Terrain and command-line options

The world contains 9×4×9 chunks, each holding 16×16×16 blocks: a **144×64×144** volume. World coordinates span `[-64, 79]` on X and Z, and `[0, 63]` on Y. Coordinates outside this volume are empty and cannot be edited. Chunk indices run along X, then Z, then vertical Y; edits also invalidate neighboring chunks across vertical seams.

The surface editor terrain is deterministic for a given seed. The default seed is `1337`. Greedy meshing merges adjacent exposed faces with the same material and normal. Chunks outside the camera's view are skipped during drawing, and edits rebuild affected chunks and seam neighbors. Cave decorations are generated separately and shown only in underground mode.

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

Smoke tests never implicitly load or save `world.vxl`. They render the game by default; add `--editor` for the overview camera, or `--editor --world path.vxl` to render an existing save. A fully empty saved world fails the terrain-presence check. Screenshots include the HUD and use portable binary PPM format. Run `--help` to list options.

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

Player tests cover collision, jumping, safe spawning, and falling through openings in the ground. Scene tests cover the full sealed shell, all 28 building entrances and interiors, bridges, the departure hall, tree and water features, shoreline jump egress, and deterministic generation. Decorative mesh tests check finite geometry, face winding, bounded triangle counts, chunk seams, and removal when anchors change or become covered. Relief tests also check all six face orientations, shallow depth bounds, and alignment with the original material coordinates. Play tests verify walking, jumping, pause, safe return inside the cavern, and that all editor actions leave play worlds unchanged. Editor tests cover block selection, placement, removal, history, and file actions separately. Save tests round-trip all 20 material IDs and verify version 1 imports retain their original coordinates and edits. Graphics checks exercise cave materials, colored lights, wall occlusion, decorative mesh updates, and lighting changes after edits alongside the existing editor checks. Relief graphics checks measure real depth and cover, reveal, and remove an ore face across a chunk boundary. Lighting regressions check tiny camera movements, distant views, and sources across chunk boundaries. They also verify multisample rendering, emission-only bloom, depth transfer, framebuffer resizing, graphics-state restoration, and shader cleanup.

GitHub Actions builds Debug and Release on Linux, runs core and sanitizer tests, and exercises graphics transitions and screenshot capture with Mesa through Xvfb.

This is a finite hub prototype. Building services, expedition transitions, village upgrades, NPCs, enemies, loot containers, crafting, chunk streaming, and persistent game progress are not implemented yet.
