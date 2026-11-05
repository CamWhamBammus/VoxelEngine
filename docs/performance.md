# Village rendering

The enlarged tree made the hub too expensive to draw. Looking toward it submitted about 2.8 million triangles, and each surface considered the lights assigned to its whole 16-block chunk. Shadow checks ran again for every pixel on every frame.

The renderer now uses four-block light cells. Every source whose sphere reaches a cell remains in its list; there is no nearest-light limit or camera-dependent light cutoff. Points beyond the grid use the complete source list. Native tests compare this path against the complete list across cell boundaries, water, models, and edits.

When the world changes, the renderer also checks which lights have no solid voxel anywhere within their reach. Those sources can skip shadow rays. Sources beside walls still trace their shadows normally. This caches proven clear regions, not a baked approximation of the whole village. Moving lamps and world edits rebuild the affected scene data.

Shallow relief and automatic cave dressing fade in three distance bands in the hub. Each band shares the existing mesh buffers and stops drawing after its fade ends. The tree's blocks, canopy, buildings, authored models, stair shapes, and collision remain intact. Near surfaces keep their relief. Authored expedition sections retain their existing detail distances.

Chunk revision tokens replace the full-world byte comparison on unchanged frames. Shader uniform locations are cached too. Copies, undo, reloads, and edits still invalidate the right meshes.

## Graphics menu

Press Escape, then Graphics. Changes wait for Apply; Back or Tab discards adjustments you haven't applied. The world pauses while the menu is open. Apply writes `graphics.cfg` in the launch directory. A failed write leaves the previous file intact, shows a message, and keeps the new choices for the current session; Apply can retry the save.

| Preset | Render scale | Antialiasing | Lighting | Hub detail |
| --- | ---: | --- | --- | --- |
| Low | 65% | Off | Low | Near |
| Medium | 85% | 2× | Medium | Medium |
| High | 100% | 4× | High | Far |

Presets leave fullscreen, VSync, and the frame limit alone. Adjusting an individual quality control shows Custom until it matches a preset again. Defaults restores all original choices, including windowed mode and VSync, after Apply.

Lighting quality changes the number of shadow samples, from one to three. It keeps every light source and still checks walls on Low. Unlit caves remain black. Detail distance changes only the village's shallow surface relief and automatic cave dressing; buildings, parkour obstacles, authored section geometry, and collision remain intact.

The frame limit offers Unlimited, 30, 60, 90, 120, or 144 FPS. It accounts for rendering and display synchronization before waiting, so VSync and the cap don't each add a full frame of delay. A limit is a maximum, not a target the hardware is guaranteed to reach. VSync can impose a lower limit.

`--render-scale` and `--msaa` override the matching saved preferences for one launch. Applying from the menu saves those values too. `--reset-graphics` ignores saved preferences without overwriting them; Apply saves the recovered choices. Smoke tests and benchmarks bypass preferences entirely.

## Measurements

Measured on November 3, 2026 with an Apple M4 Pro, a Release build, seed 1337, and a 2560 × 1440 output. Each fixed view gets four warmup frames and sixteen measured frames. Times below are medians in milliseconds; lower is better. Frames wait for GPU completion. The benchmark excludes gameplay updates, HUD drawing, and display synchronization.

| View | Before | Optimized, full resolution / 4× MSAA | Optimized, 75% resolution / 2× MSAA |
| --- | ---: | ---: | ---: |
| Arrival facing the tree | 32.65 | 19.81 | 14.59 |
| Wide tree view | 49.42 | 21.30 | 16.14 |
| Inside the hall | 41.80 | 30.93 | 21.59 |
| Looking away from the tree | 16.61 | 14.18 | 9.42 |

The wide tree view submits 799,929 triangles, down from 2,804,107. All measured views reported zero scenery rebuilds during steady rendering. The tree view's render time corresponds to roughly 20 FPS before, 47 FPS at full resolution now, and 62 FPS with the lighter settings. Actual gameplay performance depends on the view, output resolution, and machine.

## Repeat the comparison

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/VoxelEngine --benchmark
./build/VoxelEngine --benchmark --render-scale 0.75 --msaa 2
```

The command reports GPU name, resolution, median and 95th-percentile frame time, submitted triangles, visible chunks, and scenery rebuild count. It constructs a fresh village without reading or writing player progress, sections, or levels. Compare matching views and output resolutions, and run GPU-heavy programs separately.

`--render-scale` accepts 0.5 through 1. The scene color and depth are scaled together, then resolved to the native window before drawing outlines and the HUD. `--msaa` accepts 0, 2, or 4. Benchmarks default to full quality regardless of saved preferences; neither option changes saved terrain or gameplay.
