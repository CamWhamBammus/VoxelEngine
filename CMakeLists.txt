cmake_minimum_required(VERSION 3.20)

project(VoxelEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glm CONFIG REQUIRED)

function(voxel_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endfunction()

# Game logic and CPU mesh generation can be tested without an OpenGL window.
add_library(VoxelCore
    src/game.cpp
    src/game_health.cpp
    src/game_view.cpp
    src/game_combat.cpp
    src/game_mining.cpp
    src/health.cpp
    src/monsters.cpp
    src/encounters.cpp
    src/monster_preview.cpp
    src/graphics/monster_skeleton.cpp
    src/graphics/monster_wizard.cpp
    src/graphics/monster_slime.cpp
    src/graphics/held_sword.cpp
    src/graphics/held_pickaxe.cpp
    src/graphics/held_lantern.cpp
    src/graphics/held_compass.cpp
    src/graphics/pickaxe_pose.cpp
    src/game_inventory.cpp
    src/equipment.cpp
    src/items.cpp
    src/ores.cpp
    src/mining.cpp
    src/inventory.cpp
    src/loot.cpp
    src/mining_loot.cpp
    src/spawner_loot.cpp
    src/scene.cpp
    src/root_crossings.cpp
    src/sections.cpp
    src/section_presets.cpp
    src/section_editor.cpp
    src/section_tools.cpp
    src/levels.cpp
    src/level_editor.cpp
    src/hub_buildings.cpp
    src/hub_assets.cpp
    src/asset_collision.cpp
    src/hub_landscape.cpp
    src/hub_lush.cpp
    src/player.cpp
    src/camera.cpp
    src/world.cpp
    src/mesh.cpp
    src/graphics/cave_details.cpp
    src/graphics/block_relief.cpp
    src/graphics/built_surface_relief.cpp
    src/graphics/hub_props.cpp
    src/graphics/hub_foliage.cpp
    src/graphics/hub_paths.cpp
    src/graphics/hub_nature.cpp
    src/graphics/hub_gardens.cpp
    src/graphics/hub_architecture.cpp
    src/graphics/hub_fences.cpp
    src/graphics/hub_building_kit.cpp
    src/graphics/hub_blacksmith.cpp
    src/graphics/hub_lush.cpp
    src/editing.cpp
    src/save.cpp
)
target_include_directories(VoxelCore PUBLIC src)
target_link_libraries(VoxelCore PUBLIC glm::glm)
voxel_warnings(VoxelCore)

option(VOXEL_BUILD_APP "Build the voxel game and editor" ON)
if(VOXEL_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    add_executable(VoxelEngine
        src/main.cpp
        src/input.cpp
        src/graphics/window.cpp
        src/graphics/opengl.cpp
        src/graphics/post_process.cpp
        src/graphics/renderer.cpp
        src/graphics/hud.cpp
        src/graphics/palette_previews.cpp
        tests/graphics_smoke.cpp
        tests/water_graphics_smoke.cpp
        tests/hud_play_smoke.cpp
        tests/held_sword_smoke.cpp
        tests/held_pickaxe_smoke.cpp
        tests/held_lantern_smoke.cpp
        tests/held_compass_smoke.cpp
        tests/cave_lighting_smoke.cpp
        tests/ore_material_smoke.cpp
        tests/surface_relief_smoke.cpp
        tests/material_continuity_smoke.cpp
        tests/mining_hud_smoke.cpp
        tests/damage_feedback_smoke.cpp
        tests/inventory_hud_smoke.cpp
        tests/root_crossing_hud_smoke.cpp
        tests/section_hud_smoke.cpp
        tests/palette_preview_smoke.cpp
        tests/level_hud_smoke.cpp
        tests/lush_material_smoke.cpp
    )
    target_include_directories(VoxelEngine PRIVATE tests)
    target_link_libraries(VoxelEngine PRIVATE VoxelCore OpenGL::GL glfw GLEW::GLEW)
    voxel_warnings(VoxelEngine)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(test_name IN ITEMS
        chunk mesh terrain camera world raycast editor options equipment world_codec ore_blocks
        edit_history world_io greedy_mesh frustum world_session edit_shortcuts game scene player play cave_details block_relief hub_assets
        root_crossings root_crossing_game asset_collision asset_game
        sections section_editor section_tools section_build_tools section_gap_tools
        levels levels_game inventory loot_game chest_editor key_loot item_pickup_editor item_pickup_game
        lush_hub monsters health spawner_editor spawner_loot encounters encounter_game held_sword held_pickaxe held_lantern
        mining mining_loot mining_game built_surface_relief cave_game section_naming section_presets section_creation
        held_compass compass_target
    )
        add_executable(${test_name}_tests tests/${test_name}_tests.cpp)
        target_link_libraries(${test_name}_tests PRIVATE VoxelCore)
        voxel_warnings(${test_name}_tests)
        add_test(NAME ${test_name} COMMAND ${test_name}_tests)
    endforeach()
endif()
