# VoxelEngine

A small C++20 voxel game for macOS and Linux using OpenGL 4.1, GLFW, GLEW, and GLM. The direction is an **entirely underground expedition game**: leave a starting village, find resources, return, and upgrade the settlement.

This version is an explorable **240×96×240 underground hub** with 38 buildings, a giant central tree, lakes, bridges, cascading waterfalls, and an expedition departure hall. The southwest arrival workshop is a working blacksmith for pickaxe upgrades. A **creative section builder** lets you author, test, and accept rooms into world libraries. A **level creator** joins accepted sections into playable, numbered expeditions for each world. The ten Root Crossing prototypes remain available as movement tests. Expedition rewards, other building services, village upgrades, and villagers are not implemented yet.

## Explore the underground hub

Launch normally on the arrival avenue inside a large enclosed cavern. Four long bridges connect the tree island to waterfront paths; winding streets and stair flights climb through terraced neighborhoods whose floors range from Y12 to Y32. Seven building styles—cottages, halls, workshops, storehouses, inns, towers, and greenhouses—vary in size, height, roofline, and facade. Every building has an accessible interior. The north bridge leads directly up a seven-wide stair, with level landings, through the expedition gateway to an open timber-framed mine entrance. Shallow shoreline steps provide a way out of the basins.

The central tree spreads uneven layers of rose and violet foliage around branching limbs, with smaller trees following the surrounding terrain. Irregular cavern walls, ledges, and roof heights frame the districts. Waterfalls descend from source reservoirs set into continuous cliffs, through catch pools, into the lakes. Rock outcrops and crystal beds extend down to solid foundations; roads and building clearances are created before these features so they cannot cut away their supports.

The cave palette includes **stone, pale limestone, dark slate, coal ore, copper ore, iron ore, and glowing crystal**. Buildings use **timber, planks, brick, roof tiles, and lanterns**. **Leaves and glowing leaves** form the tree canopy and planted borders. **Water** has clear shallows, deeper turquoise pools, and no collision. Sand and pale rock remain visible through the surface; the lakebed descends gradually beyond the shallow walking exits.

Water is drawn after the village, using a snapshot of its color and depth for refraction and depth-based tint. Crossing ripples catch lantern highlights; reflections use visible scenery and fade to a cave tint where the reflected scenery is off screen. Broken foam follows actual banks, and waterfall curtains flow downward into spreading foam rings. The material and lighting effects share world coordinates, so they continue across chunk boundaries. Entering water slows movement and cushions falls; swimming works in the hub, authored expeditions, and section playtests. Submerged views gain turquoise distance haze. Water blocks remain stationary; fluid simulation is not implemented.

The art direction is colorful fantasy: turquoise crystals and mossy greens against blue-violet rock shadows and amber village lights. Block surfaces have painted mineral planes, chipped stone edges, embedded ore flecks, wood grain, recessed plank seams, brick mortar, and elongated crystal facets generated directly in the shader. Solid blocks occlude local lights, nearby light sources cast softened shadows, contact shading darkens corners, and distance mist separates the cavern's layers. A restrained headlamp keeps nearby rock readable away from the village.

Underground rendering uses HDR color, multisample antialiasing, and a small bloom pass for visible luminous surfaces. The final exposure and color pass keeps bright lanterns and crystals colorful while smoothing their glow. The HUD is drawn afterwards, keeping it sharp. The surface editor retains its direct rendering path and block selection outline.

Paired crystal spires, occasional short wall growths, ceiling formations, and leafy grass tufts with sparse flowers are decorative meshes attached to exposed blocks. These details have no collision; movement follows the underlying voxel shapes.

A 54-type model library dresses the hub with lantern fittings, building details, plants, and fitted paving. Selected cottages have projecting bay windows, roof dormers, and bracketed porch canopies; inns gain timber balconies, and masonry chimney fittings break up roof silhouettes. Details vary by building style and available support. Ferns, flowers, mushrooms, and shrubs share garden pockets with broad folded leaves, layered fern beds, and low moss cushions. Reeds and lily pads follow the water, while leafy vines trail from eaves and canopies.

The **54 models in the creative inventory have collision** in the hub, section playtests, and expeditions. Their surfaces support walking and jumping: use mushroom caps, rock ledges, logs, roots, crates, and balconies as parkour platforms. Collision follows each model's shape, rotation, and variant, so arch openings and spaces underneath platforms stay open. Creative flight still passes through models while building.

The lush-cave kit adds **16 models** with layered, faceted silhouettes: tree ferns, saplings, hanging roots, blossom vines, glowing mushroom colonies, giant mushrooms, shelf fungi, cave lotus, water grass, mossy logs, root fans, mossy stones, crystal flowers, spore stalks, cave orchids, and hanging planters. The village uses these around its tree, gardens, banks, cave walls, and overhead ledges. Ground models need full support, water plants sit on real water, and hanging growths attach to actual ceilings or branches. Placement reserves path shoulders, bridge headroom, doors, and the existing decorations. Glowing fungi and flowers add small, fixed lights.

**Leaves, glow leaves, and blossom leaves have cutout gaps** between painted leaf shapes and veins. Scenery remains visible through those gaps, and foliage allows light through instead of casting a solid cube shadow. The blocks remain solid for walking. The sculpted leaves on decorative models keep their shaped geometry.

The **12 new blocks** are mossy stone, mossy bricks, rooted dirt, clay, silt, root wood, aged planks, calcite, velvet moss, blossom leaves, amber crystal, and glow moss. They supply damp rock, soft garden floors, old ruins, twisted roots, pink canopy accents, and warm/cool glowing seams for authored lush rooms. Hub rock and soil gain matching surface accents without changing paths or water levels.

Faceted mossy boulders, layered rock ledges, and ceiling stalactite clusters soften the cavern's block outlines. Thick root arches sit inside the tree's planted bed, facing the four bridge approaches while leaving the full bridges and paved loop clear. Twisting root curtains attach to tall retaining faces. Placement checks the supporting terrain and clearance; larger nature models reserve space before small plants are scattered. These are cosmetic models with seeded variations: movement still uses the underlying voxels. Lamps and beacons add light; building fittings and plants have no interaction.

Streets use a dedicated **fitted paving** material with three matching models: shallow beveled flagstones over dark joints, narrow border stones, and capped masonry stair risers. Their shapes follow exposed paving blocks and neighboring tread heights, including across chunk boundaries. Covering or removing the supporting paving removes its models. The shallow surface details are cosmetic; walking still uses the underlying voxel stairs.

Stone, limestone, and slate have sparse chipped fracture edges, with fewer chips on floors. Coal, copper, and iron ore form elongated mineral fragments with a shared vein direction. Most of each fragment sits nearly flush with the rock; only a small broken edge protrudes. The geometry follows the painted shapes, extending at most 0.03 blocks for stone and 0.05 for ore. Softer, interrupted stone seams and broad shading keep the cave walls from looking like laid tiles. Their distribution is fixed by the world seed. These details remain cosmetic, so walking uses the original block shapes.

**WASD** walks, **mouse** looks, **Space** jumps, and **Shift** sprints. Walking automatically climbs one-voxel stairs when there is full body clearance and support. **R** returns you to a safe space in the village. **Escape** pauses and releases the mouse; **Tab** resumes. **H** shows the walking controls, and **Q** quits.

In water, **WASD** swims horizontally, **hold Space** to rise and stay at the surface, **hold Ctrl** to dive, and **Shift** swims faster. Releasing Space and Ctrl lets you sink gently. Hold **Space + forward** against a bank to climb onto a clear landing at water level or one block above it. Underwater movement still collides with the lakebed, walls, and ceilings. There is no breath timer or drowning damage yet. Creative building retains free flight; press **F8** to test swimming in a room you build.

The bottom hotbar holds a **pickaxe, lantern, and compass**. Select with **1–3**, the **mouse wheel**, or **[ / ]**. Equipping the lantern adds warm light; equipping the compass points toward the central heart tree. The pickaxe can be upgraded, but ore extraction is not implemented yet. Block editing is available in the section builder (`--dev`) and the original terrain editor (`--editor`).

Find the **blacksmith in the southwest district, west of the arrival avenue**. Its hanging anvil sign marks the entrance; inside are a glowing forge, anvil, tool rack, and workbench. Approach the anvil and press **E**, then **Enter** to upgrade. **E**, **Escape**, or **Tab** closes the forge menu. The menu pauses walking and looking. New equipment starts with **12 copper**; tier 1 → 2 costs **8 copper**, and tier 2 → 3 costs **8 iron**. Iron gathering will come with expeditions. Lantern and compass upgrades are not offered yet.

Successful upgrades save the pickaxe tier and ore balances to **`progress.vxp` in the working directory**. Restarting restores that profile; starter copper is not added to an existing profile. The profile is separate from editor world files. Invalid profiles report an error and are preserved; failed saves leave the upgrade and ore balances unchanged. Smoke tests use fresh in-memory equipment.

The hub follows a fixed architectural plan, with seeded variations in rock, ore, and foliage. The terrain resets on launch and does not load or overwrite editor saves. Use `--seed N` to change the variation. The original surface editor remains available through `--editor`.

## Build your own expedition sections

Run **`./build/VoxelEngine --dev`**, or press **F7** in the game. On a Mac keyboard you may need **Fn** with the function keys.

1. Choose a world, then **New empty section**. The room has a protected stone shell around a **64 × 48 × 64** editable interior. Lush Caves is the current world; Crystal Hollows and Ember Depths are library destinations for future themes.
2. Press **E** for the creative inventory. Choose a hotbar slot, then click a block, asset, or marker to equip it. The tabs contain all **31 building materials**, **54 decorative models**, and **Start**, **End**, and optional **Checkpoint** markers. Asset icons show the actual models. The inventory has two block pages and three asset pages; the new lush models are on asset pages two and three. The selected item shows its suggested ground, wall, ceiling, or water placement. Use the page arrows to see the rest of the assets; **Escape** or **E** closes the inventory.
3. Fly and build. **Left click** removes, **right click** places, and **middle click** picks the targeted item. Holding a button repeats block edits. **1–9** or the wheel selects a hotbar slot; **T** rotates models and **Y** changes their variant. The green model preview shows where an asset will go. Aim at the underside of a ceiling for hanging roots or planters, a wall for shelf fungi or root fans, and the top of water for lotus or water grass; aquatic roots extend below the surface.
4. Equip markers from the inventory and right click a clear, solid top surface on a block or model. **Green Start** sets the spawn, **gold End** advances to the next section, and **cyan** is an optional checkpoint. Every section has exactly one Start and one End: placing either moves the existing marker, and undo restores its previous position. Their labeled frames have no collision. Keep enough footing and headroom for the player's body; Start, End, and Checkpoint can sit at fractional heights on model platforms.
5. Press **F8** to test with normal walking, jumping, swimming, gravity, and collision. Reach the gold End to pass. **R** returns to the start or checkpoint; **Shift+R** restarts the attempt. **F8** returns to your previous build view. Any geometry or marker edit, including undo/redo, requires another test before acceptance.
6. Press **F10** after passing, type a name, and choose **Parkour, Puzzle, Exploration, Mining, or Treasure**. Press **Enter** or click **Accept section**. It appears in that world's library immediately. **F9** opens the library to continue a draft or edit a copy of an accepted section.

Press **G** while building for **Build tools**:

- **Select box:** left click sets corner A, right click sets corner B. Both selected blocks are included. Clicking the protected room shell selects the empty cell just inside it. To choose a corner in empty space, fly there and press **J** for A or **K** for B. The outline shows your selected volume; the Tools panel can raise or lower its top by one block.
- **Fill / Shell / Clear:** use your selected hotbar block to fill the whole box or build just its boundary. Shell preserves anything already inside. Clear removes the selected blocks and decorations whose origins are in the box. Move outside the area before filling it.
- **Cube brush:** choose **1, 3, 5, or 9** blocks wide. Hold right click to add cubes of your selected block, or left click to carve them away. Painting fills empty cells and leaves existing blocks intact; brush edges stop at the protected room shell.
- **Copy / Paste:** select a box and press **Cmd/Ctrl+C**, then **Cmd/Ctrl+V**. Blocks and decorations are copied together; Start, End, and Checkpoint stay unique. Aim at a surface to position the preview, press **T** to rotate by 90 degrees, and right click once per stamp. Empty copied cells preserve the destination by default. Open **G → Paste Settings** to enable **With air** for replacing the entire destination box, including its existing decorations.
- **Fill Gaps:** copy a floor or other block shape, then choose **G → Paste Settings → Fill Gaps**. Aim at the top of an existing floor and **left click** (or press **L**) to lock its height. Move the preview over the hole and **right click**. Only empty cells receive blocks; existing blocks and decorations stay in place, and copied decorations are skipped. The bright preview shows only the blocks that will be added. A large stamp can overlap a smaller hole without sitting above the surrounding floor. Left click or **L** picks a new floor height.
- **Precise placement:** in either paste mode, **arrow keys** nudge one block horizontally relative to your viewing direction, and **Page Up / Page Down** move it up or down. On a Mac keyboard, use **Fn+Up / Fn+Down** for Page Up / Page Down. Nudging pins the preview so looking around cannot move it. **P** toggles between pinning and following your aim; **T** rotates around the pinned origin. Fill Gaps keeps its chosen height when following your aim, including a height adjusted with Page Up / Down.
- **X** returns to single-item building while keeping your selection and clipboard. The clipboard can be reused in another section during the same session. Each fill, clear, brush stamp, or paste is one **Cmd/Ctrl+Z** undo step; **Cmd/Ctrl+Shift+Z** or **Cmd/Ctrl+Y** redoes it. Changes require another playtest before acceptance.

The hotbar wheel direction is reversed from the earlier build, and fractional trackpad movement accumulates into whole slot steps instead of jumping on every tiny event. Camera zoom stays fixed in the section builder.

**WASD** flies, **Space** rises, **Left Ctrl** descends, **Shift** speeds up, and **Alt/Option** slows down for precision. **Ctrl/Cmd+Z** undoes; **Ctrl/Cmd+Y** or **Ctrl/Cmd+Shift+Z** redoes blocks, models, and markers. **Escape** pauses; **Tab** resumes. The inventory also supports arrows, **Tab** to switch tabs, and **Enter** to equip. In the acceptance form, typing replaces the initially selected name; **Ctrl/Cmd+A** selects it again.

**F5** saves a draft. Changed work also saves after 20 seconds of active editing, before tests or library changes, when leaving with **B/F7**, and when closing the window. A failed save keeps your work open. New rooms and accepted revisions receive unique files so earlier creations are preserved. Files live under **`sections/` in the working directory**, separately from equipment and terrain-editor saves:

```text
sections/
  drafts/lush-caves/untitled-section-001.vxs
  lush-caves/parkour/your-section-name-001.vxs
  lush-caves/puzzle/another-section-001.vxs
```

Each `.vxs` contains the blocks, placed models, markers, name, world, category, and successful-test flag. Keep or copy this directory to preserve your creations; these local files are ignored by Git. Accepted rooms are available to reopen and test from the library. Use the level creator below to arrange accepted sections into expeditions. Procedural assembly and puzzle/treasure/mining mechanics are future work; categories organize the authored sections now. Dev mode preserves your equipment and does not award or spend resources.

## Assemble and play levels

Press **F11**, click **Level creator** in the section builder, or launch **`./build/VoxelEngine --level-creator`**. Use **Fn** with the function keys if macOS reserves them.

1. Choose the world, then **New level**. Only accepted, successfully tested sections from that world appear in the catalog.
2. Click sections on the left to append them to the route on the right. You can repeat a section. Select a route entry and use **Up**, **Down**, or **Remove** to change the order; arrows below either list change pages. A level holds **1–64 sections**.
3. Click the name and level number fields to edit them. Choose **1–999**: for example, number **5** places it at **Lush Caves — Level 5**. Typing replaces the selected text; **Ctrl/Cmd+A** selects it again. **Tab** cycles the fields and **Enter** finishes typing.
4. Click **Publish** to add it to the game's world list. If that number is occupied, the creator asks before replacing the existing level. **Play** tests the current route, including unpublished changes. **Back** returns to the library, where published levels can be reopened and edited and the latest draft can be resumed.

Play published levels with **F4**, or approach the north expedition gate and press **E**. Choose a world and a level. You spawn at the first section's green Start. Walking into the gold End shows a brief transition, loads the next section, and places you at its Start. The final End shows a completion screen with **Replay** and **Return to hub**. The compass points toward the current End. **R** resets to the current checkpoint or Start; **Shift+R** restarts the entire level; **B** returns to the village. Pausing or opening a menu pauses the run timer.

Composition changes save a draft automatically. Invalid fields or a failed save keep the composition open so you can correct them. Publishing preserves the original accepted section files. Each level stores their paths in order, so keep **both `sections/` and `levels/`** when moving or backing up your creations. Existing section files remain compatible with the Start/End markers.

```text
levels/
  drafts/lush-caves.vlevel
  lush-caves/level-001.vlevel
  lush-caves/level-005.vlevel
```

These local files are ignored by Git. Each world has its own numbered slots and builder draft. A missing section reports an error; if a file disappears during a run, the current room is retained and you can retry or return to the hub. Levels currently provide traversal and section transitions; they do not award loot or change equipment.

## Test the Root Crossings

Press **F6** during play to open the ten-course prototype picker. Use **Up / Down** or **1–9 / 0** to select a course, then **Enter** to start. **Escape** closes the picker. You can also launch a particular course directly:

```sh
./build/VoxelEngine --root-crossing 1
```

The number accepts **1 through 10**. Each course is an enclosed lush grotto with rooted timber crossings, planted landings, trees, luminous foliage, and water below. The main routes use ordinary walking jumps; **Shift** still sprints. Walk through the **cyan checkpoint** to activate it and reach the **amber exit markers** to finish. Falling below the route returns you to your last checkpoint automatically.

| Course | Route to try |
| --- | --- |
| 1. Fallen Elder | A wide introductory root with two broken spans |
| 2. Braided Roots | Weaving turns across the grotto |
| 3. Forked Grove | A broad western route and a narrow eastern shortcut |
| 4. Root Stair | A rising ridge followed by a descending route |
| 5. Elder Spiral | A climbing loop around an ancient tree |
| 6. Hanging Garden | High roots attached to the cave canopy |
| 7. Waterfall Ledge | A wall-side crossing beside a cascade |
| 8. Garden Knots | Jumps between planted root knots |
| 9. Broken Crown | Narrow branches with changes in height |
| 10. Tangled Heart | A longer mixture of turns, climbs, and gaps |

**N / P** switches to the next or previous course. **R** returns to the active checkpoint; **Shift+R** starts a fresh attempt at the entrance. **F6** reopens the picker, and **B** returns to the village. The HUD tracks time, falls, checkpoint activation, and the best time for each course during the current session. The compass points toward the course exit. Pausing or opening a menu pauses the timer.

These are individual movement tests: finishing grants no resources and does not change your saved equipment. Session best times reset when you quit. Ore mining and treasure rewards come later. Use the section builder and level creator to make your own connected routes. Terrain editing is disabled during these tests; use F7 for the section builder. `--root-crossing` cannot be combined with `--dev`, `--editor`, or `--world`.

## Planned expedition loop

Expeditions will assemble compatible sections into a route: entrance, challenges with optional ore branches, then a main treasure chest that rewards the run and returns the player to the hub. Pickaxe tier gates ore extraction. Depth selects the world's theme, from lush caves near the surface to lava caverns deeper down. The central heart tree is the planned access point for a literal skill tree. The Root Crossing collection tests the first section type; the complete expedition loop and progression systems are not implemented yet.

The lush-world section ideas to revisit after Root Crossing are Flooded Grotto, Overgrown Mine, Rootwell Ascent, Ancient Water Garden, Lantern Grove, Hidden Sanctuary, and the final Rootbound Vault. They combine exploration, ore branches, observation, water puzzles, and parkour. They can provide risk and variety with simple object movement and effects before creature animation is needed.

## Understand the code

Start with [src/main.cpp](src/main.cpp). It opens the window and repeats three steps: **read input → update the game → draw the frame**.

Most modules have a `.h` file that declares their types and functions, and a matching `.cpp` file that implements them. `materials.h` contains the shared material shader text. Related code stays together:

```text
src/
  main.cpp          Startup and the main loop
  game.h/.cpp       Play/editor modes, movement, blacksmith interaction, and HUD text
  equipment.h/.cpp  Tool selection, pickaxe upgrade costs, and progress profiles
  player.h/.cpp     Player body, collision, walking, jumping, swimming, and spawning
  asset_collision.h/.cpp Model surfaces, collision queries, and precise surface targeting
  scene.h/.cpp      Hub layout, landscape, tree, water, bridges, and departure hall
  root_crossings.h/.cpp Ten lush crossing layouts, shared landings, and cave construction
  sections.h/.cpp   Room template, world/category catalogs, and .vxs file library
  section_editor.h/.cpp Creative building, palette, markers, tests, and acceptance
  section_tools.h/.cpp Box fills, brushes, clipboard rotation, and batch edit validation
  levels.h/.cpp     Ordered section references, numbered world slots, and .vlevel files
  level_editor.h/.cpp Level composition, publishing, playback, and section transitions
  hub_buildings.h/.cpp Seven building styles, facade variants, and height profiles
  hub_assets.h/.cpp Plan hub decorations and batch their meshes by chunk
  hub_landscape.cpp Attach large rocks and roots to supporting terrain
  hub_lush.h/.cpp   Layer lush gardens, wall growths, water plants, and natural surface accents
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
    water.h        Water refraction, reflections, shoreline foam, and fall impacts
    post_process.h/.cpp Smooth edges, luminous bloom, exposure, and color
    cave_details.h/.cpp Build cosmetic crystal shards, ceiling formations, and tufts
    block_relief.h/.cpp Build sparse rock chips and shallow ore veins
    asset_builder.h Shared triangle, box, and beam helpers for hub models
    hub_props.cpp  Ten architectural, lighting, and supply models
    hub_foliage.cpp Seven plant models, from ferns to hanging vines
    hub_paths.cpp  Fitted paving, border stones, and stair caps
    hub_nature.cpp Sculpted rocks, stalactites, and large roots
    hub_gardens.cpp Broad leaves, fern beds, and moss cushions
    hub_lush.cpp    Sixteen lush models: trees, fungi, roots, orchids, and aquatic plants
    hub_architecture.cpp Dormers, bay windows, chimneys, balconies, and canopies
    hub_blacksmith.cpp Forge, anvil, tools, workbench, and workshop sign
    hud.h/.cpp      Draw text, tool icons, the compass, and the blacksmith panel
tests/              Behavior tests and graphics smoke checks
CMakeLists.txt      List the source files and build targets
```

Read these first:

1. [main.cpp](src/main.cpp) shows how everything connects.
2. [game.h](src/game.h) names the current world and camera, plus the editor's selected material and edit history. [Game::update in game.cpp](src/game.cpp) handles the player's actions in order.
3. [world.h](src/world.h) explains the data: a `Block` is a material, a `Chunk` stores a 16×16×16 cube of blocks, and a `World` holds a 15×6×15 grid of chunks on the heap.
4. [scene.cpp](src/scene.cpp) lays out the hub and landscape; [hub_buildings.cpp](src/hub_buildings.cpp) builds its seven architectural styles. [player.cpp](src/player.cpp) handles the body and movement.
5. [editing.cpp](src/editing.cpp) shows how aiming and editing work. `raycast` follows the camera's viewing direction through blocks to find a hit.

For one frame, `Input::update` handles looking and fills an `InputActions` value. In play mode, `Game::update` moves the `Player` and follows its eyes with the camera; `Player` resolves movement against solid voxels. `scene.cpp` builds the hub once at startup. `Game::update` handles walking, tool selection, returning to the village, and blacksmith interaction. `equipment.cpp` owns upgrade costs and the small progress profile; it has no graphics dependency. Only editor mode processes targeting, block changes, saving, loading, and undo. `WorldRenderer::render` rebuilds meshes for changed chunks and draws visible chunks. The outline and HUD are drawn last. World storage does not depend on the renderer.

Each renderer remembers the chunk contents it last drew. It rebuilds changed chunks and their neighbors, so switching worlds or opening another view cannot lose mesh updates. This uses a small world snapshot and straightforward comparisons; no global cache or event system is needed. The renderer also uploads the solid cells to a small 3D texture when the world changes. Cave lighting samples it for contact shading and short shadow rays, so world changes update lighting alongside geometry. Decorative meshes reuse the block materials but do not enter that solid-cell texture or cast their own shadows.

`hub_assets.cpp` places models against the building plan and actual terrain, then combines them into chunk meshes for drawing. `hub_landscape.cpp` handles support and spacing for the larger rock and root models. The `graphics/hub_*.cpp` files describe each model's shapes using `AssetBuilder`; rotation and translation are handled by that shared helper. `asset_collision.cpp` caches those same surfaces for player collision and marker targeting, independently of voxel storage. The cache is replaced when a scene or its models change.

Light candidates are cached per chunk, including every source whose radius can reach its blocks or decorations. Nearby crystal and glowing-leaf blocks share fixed sources; each lantern keeps its own source. Every source that reaches a surface contributes, so stepping away cannot switch off its illumination. Candidates include neighboring chunks and rebuild after world edits. The shader skips sources outside their radius or behind the surface before checking shadows. Very dense custom lighting can increase rendering cost.

`PostProcess::begin` provides the underground world with an HDR color/depth target. `finish` resolves the smooth edges, blurs only emissive color at quarter resolution, and combines it with the scene. It returns the image and depth to the original framebuffer, so block outlines still hide correctly behind walls. Its buffers resize with the window and are reused between frames.

Use this map when changing something:

| I want to change… | Start here |
| --- | --- |
| Tool names, upgrade costs, or equipment progress | [equipment.cpp](src/equipment.cpp) |
| Hotbar icons and the blacksmith panel | [graphics/hud.cpp](src/graphics/hud.cpp) |
| Keys, mouse sensitivity, or editor flight speed | [input.cpp](src/input.cpp) |
| Walking speed, jumping, or collision | [player.cpp](src/player.cpp) |
| Cavern, tree, water, bridges, building positions, or ore deposits | [scene.cpp](src/scene.cpp) |
| Water color, reflections, foam, or ripples | [graphics/water.h](src/graphics/water.h) and [graphics/materials.h](src/graphics/materials.h) |
| Building walls, doors, windows, or roof shapes | [hub_buildings.cpp](src/hub_buildings.cpp) |
| Hub prop and plant placement | [hub_assets.cpp](src/hub_assets.cpp) |
| Lantern fittings, window boxes, awnings, banners, or crates | [graphics/hub_props.cpp](src/graphics/hub_props.cpp) |
| Ferns, flowers, mushrooms, reeds, lily pads, vines, or shrubs | [graphics/hub_foliage.cpp](src/graphics/hub_foliage.cpp) |
| Editor starting camera position and view direction | [camera.h](src/camera.h) |
| Village spawn and return behavior | [scene.h](src/scene.h) and [game.cpp](src/game.cpp) |
| Section library, room bounds, and Start/End validation | [sections.cpp](src/sections.cpp) |
| Level ordering, world slots, transitions, and checkpoints | [level_editor.cpp](src/level_editor.cpp) and [levels.cpp](src/levels.cpp) |
| Root Crossing paths, jumps, checkpoints, and environment | [root_crossings.cpp](src/root_crossings.cpp) |
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

Version 3 stores the expanded 15×6×15 chunk volume. Version 1 and version 2 saves remain readable: their original regions import at the same coordinates, preserving all edits, while added space uses terrain generated from the saved seed. Saving an imported world writes version 3.

Loading validates dimensions, materials, and a checksum before replacing the active world. Saving writes and flushes a temporary sibling file, then atomically replaces the destination on macOS/Linux. Failed writes do not delete the old save. This is not a backup system: keep copies of worlds you want to preserve.

World files (`*.vxl`) are ignored by Git. To start fresh from a seed without automatically loading the default save:

```sh
./build/VoxelEngine --editor --seed 42
```

With `--editor`, if both `--world` and `--seed` are provided, an existing world's saved seed takes precedence; a missing world uses the requested seed. F5 writes to the chosen path, including `world.vxl` when no path was provided.

## Terrain and command-line options

The world contains 15×6×15 chunks, each holding 16×16×16 blocks: a **240×96×240** volume. World coordinates span `[-112, 127]` on X and Z, and `[0, 95]` on Y. Coordinates outside this volume are empty and cannot be edited. Chunk indices run along X, then Z, then vertical Y; edits also invalidate neighboring chunks across vertical seams.

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

Player tests cover collision, one-block stair climbing, headroom, jumping, safe spawning, and falling through openings in the ground. Scene tests walk three-dimensional routes to all 38 building entrances and interiors, bridges, and the departure hall, including direct travel in both directions on every lane of the mine approach. They also cover the sealed shell, connected solid foundations, tree and water features, shoreline stair egress, and deterministic generation. Decorative mesh tests check finite geometry, face winding, bounded triangle counts, chunk seams, and removal when anchors change or become covered. Hub asset tests cover all 54 model types, seeded variants, quarter-turn rotations, placement, chunk batching, and empty worlds. Relief tests also check all six face orientations, shallow depth bounds, and alignment with the original material coordinates. Play tests verify walking, jumping, pause, safe return inside the cavern, tool selection, blacksmith reach and menu behavior, and that all editor actions leave play worlds unchanged. Equipment tests cover upgrade costs and limits, profile validation and round-trips, failed writes, and progress restoration. Editor tests cover block selection, placement, removal, history, and file actions separately. Save tests round-trip all 21 material IDs and verify version 1 and version 2 imports retain their original coordinates and edits. Graphics checks exercise cave materials, colored lights, wall occlusion, decorative mesh updates, and lighting changes after edits alongside the existing editor checks. Relief graphics checks measure real depth and cover, reveal, and remove an ore face across a chunk boundary. Lighting regressions check tiny camera movements, distant views, and sources across chunk boundaries. They also verify multisample rendering, emission-only bloom, opaque scene snapshots, depth transfer, framebuffer resizing, graphics-state restoration, and shader cleanup. Water checks cover visible lakebeds, foreground occlusion, animation, and fresh snapshots after edits and view changes.

Model collision tests cover all 54 asset kinds and rotations, thin and sloped surfaces, arch openings, hollow logs, long sweeps, and cache replacement. Player and game tests exercise real jumps between model platforms at 30, 60, and 120 FPS, fitted hub paving, and section transitions. Editor tests place markers on fractional model tops and protect their support and headroom.

Root Crossing tests traverse all ten main routes with the actual player physics at 30 and 60 FPS, check the optional fork, and verify that solid roots connect each landing to the cave terrain. They also check safe reset landings, real jump gaps, deterministic layouts, selection, timing, checkpoints, completion, and unchanged equipment. Native checks render all ten rooms, switch back to the hub, and verify the picker, compass, and completion HUD at different display scales.

Section tests cover protected room bounds, all model catalogs, marker safety, block/model/marker undo, a real walking test, test invalidation, world/category acceptance, safe hub transitions, draft recovery, and rejected saves. The file tests check complete round-trips, malformed documents, checksum failures, atomic writes, unique revisions, and world-filtered library discovery. Native checks cover all inventory pages, mouse hit regions at different DPI scales, test/accept menus, explicit model rendering, and asset-only cache updates.

GitHub Actions builds Debug and Release on Linux, runs core and sanitizer tests, and exercises graphics transitions and screenshot capture with Mesa through Xvfb.

This is a finite hub prototype with a tool hotbar, saved blacksmith pickaxe upgrades, ten selectable Root Crossing movement tests, and a creative section builder with world libraries. Full expeditions, other building services, mining, village upgrades, NPCs, enemies, loot containers, general crafting, and chunk streaming are not implemented yet.
