#pragma once

namespace voxel {

// Procedural paint, grain, facets, and shallow relief for each block material.
inline constexpr const char* materialShader = R"glsl(
struct MaterialSurface { vec3 albedo; vec3 normal; float roughness; float emission; };
float materialHash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
// Integer coordinates and hashing match block_relief.cpp exactly on CPU/GPU.
// Float noise can differ between drivers; use it only for paint and grain.
float mineralHash(vec2 p) {
    uvec2 cell = uvec2(ivec2(p));
    uint h = cell.x * 0x9e3779b9u ^ cell.y * 0x85ebca6bu;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 16777216.0;
}
float materialNoise(vec2 p) {
    vec2 cell = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(materialHash(cell), materialHash(cell + vec2(1, 0)), f.x),
               mix(materialHash(cell + vec2(0, 1)), materialHash(cell + 1.0), f.x), f.y);
}
// One evaluation supplies color, shallow relief, roughness, and luminous glass.
// Correlated shapes are sampled on a 32-pixel grid, not random bright checkers.
MaterialSurface evaluateMaterial() {
    vec3 n = normalize(surfaceNormal);
    if (decorative == 1) {
        float brush = floor(materialNoise(worldPosition.xz * 2.0 + worldPosition.y) * 4.0) / 4.0;
        vec3 color = surfaceColor * (0.87 + 0.18 * brush);
        if (materialId == 20) {
            // Broad painted grain and sparse pale flecks keep the fitted stones
            // tactile at walking height without changing their beveled outline.
            vec3 paint = (floor(worldPosition * 32.0) + 0.5) / 32.0;
            vec2 uv = abs(n.y) > 0.5 ? paint.xz : vec2(paint.x + paint.z, paint.y);
            float grain = materialNoise(uv * vec2(7.0, 11.0));
            float stroke = materialNoise(uv * vec2(1.8, 18.0) + grain * 0.35);
            color *= 0.87 + floor(grain * 5.0) * 0.035 + stroke * 0.09;
            float fleck = smoothstep(0.89, 0.98, materialHash(floor(uv * 24.0)));
            color += surfaceColor * fleck * 0.065;
        }
        if (materialId == 10) color *= vec3(0.55, 0.95, 1.0);
        if (materialId == 1) {
            float tip = smoothstep(0.05, 0.50, fract(worldPosition.y));
            color *= mix(vec3(0.62, 0.76, 0.83), vec3(1.18, 1.03, 0.72), tip);
        }
        float roughness = 0.88, emission = 0.0;
        if (materialId == 10) { roughness = 0.24; emission = 0.30 + brush * 0.08; }
        if (materialId == 14) { roughness = 0.22; emission = 1.0; }
        if (materialId == 17) emission = 0.025;
        if (materialId == 18) emission = 0.15;
        return MaterialSurface(clamp(color, 0.0, 1.0), n, roughness, emission);
    }
    // Sample the same paint on a raised chip and on its supporting block.
    // Facet normals still light the geometry, but cannot rotate its UV axes.
    vec3 paintPosition = worldPosition;
    if (decorative == 2) {
        n = reliefSurface.xyz;
        paintPosition -= n * reliefSurface.w;
    }
    bool top = abs(n.y) > 0.5, sideX = abs(n.x) > 0.5;
    vec3 u = top || !sideX ? vec3(1, 0, 0) : vec3(0, 0, 1);
    vec3 v = top ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec2 uv = vec2(dot(paintPosition, u), dot(paintPosition, v));
    vec2 p = (floor(uv * 32.0) + 0.5) / 32.0, local = fract(p);
    vec3 cell = floor(paintPosition - n * 0.001);
    float seed = dot(cell * abs(n), vec3(17, 37, 57)) + float(materialId) * 19.0;
    float broad = materialNoise(p * 1.7 + seed);
    float chips = materialNoise(p * 5.5 + seed + 8.0);
    float fine = materialHash(floor(p * 32.0) + seed) - 0.5;
    vec3 color = surfaceColor * (0.86 + 0.18 * broad + 0.06 * chips + 0.035 * fine);
    vec2 slope = vec2(0.0);
    float roughness = 0.90, emission = 0.0;
    if (materialId == 3 || materialId == 5 || materialId == 6 || materialId == 15
        || (materialId >= 7 && materialId <= 9)) {
        bool shale = materialId == 6 || materialId == 15;
        // These irregular fracture lines also anchor the occasional rock chips
        // in block_relief.cpp. Color strokes continue across their boundaries.
        vec2 plate = p * vec2(1.35, shale ? 3.2 : 1.65);
        vec2 tile = floor(plate), within = fract(plate);
        float nearest = 10.0, second = 10.0;
        vec2 plane = vec2(0.0), secondPlane = vec2(0.0);
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                vec2 offset = vec2(x, y);
                float h = mineralHash(tile + offset + seed);
                vec2 center = offset + 0.18 + 0.64 * vec2(h, fract(h * 17.73));
                vec2 delta = within - center;
                float distance = dot(delta, delta);
                if (distance < nearest) {
                    second = nearest; secondPlane = plane;
                    nearest = distance; plane = delta;
                } else if (distance < second) { second = distance; secondPlane = delta; }
            }
        }
        // Broad painted planes carry the rock. Break the seams into occasional
        // fissures instead of outlining every region like laid cobblestones.
        float edge = sqrt(second) - sqrt(nearest);
        float fracture = smoothstep(0.49, 0.76, chips * 0.65 + broad * 0.35);
        float crack = (1.0 - smoothstep(0.008, 0.040, edge)) * fracture;
        float bevel = 1.0 - smoothstep(0.02, 0.19, edge);
        vec2 outward = normalize(plane - secondPlane);
        float edgeLight = dot(outward, normalize(vec2(-0.55, 0.83)));
        float stroke = materialNoise(vec2(p.x * 0.8 - p.y * 0.35, p.y * 5.0 + p.x * 1.6) + seed);
        vec3 cool = shale ? vec3(0.27, 0.34, 0.42) : vec3(0.43, 0.49, 0.54);
        vec3 warm = shale ? vec3(0.36, 0.35, 0.40) : vec3(0.58, 0.55, 0.48);
        vec3 pigment = mix(cool, warm, smoothstep(0.22, 0.80, broad));
        color = mix(surfaceColor, pigment, materialId == 5 ? 0.32 : 0.42);
        if (materialId == 15) color *= 0.65;
        if (materialId >= 7 && materialId <= 9) color = mix(vec3(0.39, 0.43, 0.47), pigment, 0.28);
        // Overlapping broad and narrow brush marks suggest rock layers. They
        // never reset at a fracture, avoiding a wall full of separate tiles.
        float strata = materialNoise(vec2(p.x * 1.8 + broad * 0.3, p.y * (shale ? 11.0 : 6.0)) + seed + 5.0);
        float brush = smoothstep(0.28, 0.48, strata) * 0.5 + smoothstep(0.56, 0.74, strata) * 0.5;
        color *= 0.81 + 0.12 * stroke + 0.11 * brush;
        color *= 1.0 + bevel * fracture * (0.045 * edgeLight - 0.015);
        color *= 1.0 - crack * 0.21;
        // A short upper-edge accent reads as a chipped surface in flat light.
        float rim = smoothstep(0.025, 0.05, edge) * (1.0 - smoothstep(0.05, 0.09, edge));
        color += vec3(0.040, 0.045, 0.045) * rim * max(edgeLight, 0.0) * fracture;
        slope = outward * bevel * fracture * (shale ? 0.08 : 0.10);
        if (materialId >= 7 && materialId <= 9) {
            // These elongated, asymmetric fragments also define the attached
            // ore geometry in block_relief.cpp. Nearby pieces share a direction.
            vec2 mineralUV = decorative == 2 ? uv : p;
            vec2 mineralCell = floor(mineralUV * 2.0);
            float h = mineralHash(mineralCell + seed);
            float h2 = mineralHash(mineralCell + seed + 11.0);
            float h3 = mineralHash(mineralCell + seed + 23.0);
            float direction = mineralHash(floor(mineralCell / 3.0) + seed + 47.0) > 0.5 ? 1.0 : -1.0;
            vec2 along = vec2(0.8, direction * 0.6), across = vec2(-direction * 0.6, 0.8);
            vec2 offset = mineralUV * 2.0 - mineralCell - vec2(0.2 + 0.6 * h, 0.2 + 0.6 * h2);
            vec2 q = vec2(dot(offset, along), dot(offset, across));
            float halfLength = 0.32 + 0.13 * h2, halfWidth = 0.10 + 0.05 * h3;
            vec2 corners[4] = vec2[4](vec2(-halfLength, -halfWidth * 0.15), vec2(-halfLength * 0.25, -halfWidth),
                                      vec2(halfLength, halfWidth * 0.2), vec2(halfLength * 0.20, halfWidth));
            float edgeDistance = -10.0;
            for (int i = 0; i < 4; ++i) {
                vec2 side = corners[(i + 1) % 4] - corners[i];
                edgeDistance = max(edgeDistance, dot(q - corners[i], normalize(vec2(side.y, -side.x))));
            }
            if (h > 0.32 && edgeDistance < 0.035) {
                vec3 ore = materialId == 7 ? vec3(0.075, 0.090, 0.105)
                    : (materialId == 8 ? vec3(0.72, 0.36, 0.15) : vec3(0.53, 0.55, 0.57));
                if (materialId == 8 && h2 > 0.90) ore = mix(ore, vec3(0.20, 0.40, 0.31), 0.48);
                if (edgeDistance <= 0.0) {
                    float upper = clamp(q.y / halfWidth * 0.5 + 0.5, 0.0, 1.0);
                    vec3 mineral = ore * (0.78 + 0.29 * upper + 0.06 * stroke);
                    // Painted highlights remain broad, with a small broken glint.
                    float glint = smoothstep(-0.045, -0.012, edgeDistance) * upper;
                    mineral += (materialId == 7 ? vec3(0.025, 0.035, 0.045) : vec3(0.09, 0.075, 0.045)) * glint;
                    float exposed = smoothstep(-0.95, -0.15, q.x / halfLength)
                        * smoothstep(-1.0, -0.2, q.y / halfWidth);
                    color = mix(color, mineral, exposed);
                    roughness = mix(roughness, materialId == 7 ? 0.77 : 0.67, exposed);
                    slope += across * 0.055 * exposed;
                } else {
                    color *= 0.72 + 0.28 * smoothstep(0.0, 0.035, edgeDistance);
                }
            }
        }
    } else if (materialId == 16) {
        // Water remains an opaque voxel surface. Broad flowing paint and
        // shifting highlights suggest depth without hiding paths or the banks.
        vec2 flow = top ? p + vec2(timeSeconds * 0.035, -timeSeconds * 0.025)
                        : p + vec2(0.0, timeSeconds * 0.65);
        float body = materialNoise(flow * (top ? vec2(0.45, 0.65) : vec2(3.0, 0.6)));
        float wave = sin(flow.x * 2.8 + flow.y * 1.7 + body * 1.5);
        float ripple = sin(flow.x * 3.0 - flow.y * 5.0 + body * 2.0);
        color = mix(vec3(0.015, 0.20, 0.25), vec3(0.035, 0.52, 0.51), body);
        if (top) {
            float glint = smoothstep(0.84, 0.99, wave) * smoothstep(-0.2, 0.8, ripple);
            color += vec3(0.12, 0.23, 0.21) * glint;
            slope = vec2(wave * 0.10, ripple * 0.075);
        } else {
            // Vertical surfaces read as falling ribbons, not a rotated pond.
            float ribbon = materialNoise(vec2(flow.x * 9.0, flow.y * 1.3));
            float foam = smoothstep(0.66, 0.88, ribbon) * (0.55 + 0.45 * body);
            color = mix(color * 1.25, vec3(0.38, 0.78, 0.76), foam);
            slope = vec2((ribbon - 0.5) * 0.18, ripple * 0.035);
        }
        roughness = 0.16;
    } else if (materialId == 17 || materialId == 18) {
        // Paint overlapping leaf shapes into the cubes: a dense tree keeps a
        // simple silhouette instead of multiplying thousands of small meshes.
        vec2 leafGrid = p * 1.5;
        leafGrid.x += mod(floor(leafGrid.y), 2.0) * 0.45;
        float leafSeed = materialHash(floor(leafGrid) + seed);
        vec2 q = fract(leafGrid) - 0.5;
        q.x += q.y * (leafSeed - 0.5) * 0.7;
        float shape = abs(q.x) * 1.55 + abs(q.y) * 0.85;
        float leaf = 1.0 - smoothstep(0.30, 0.62, shape);
        float mass = materialNoise(p * 0.45 + seed);
        vec3 pigment = mix(vec3(0.09, 0.30, 0.10), vec3(0.22, 0.48, 0.13), mass * 0.75 + leafSeed * 0.25);
        if (materialId == 18) pigment = mix(vec3(0.06, 0.40, 0.24), vec3(0.10, 0.54, 0.30), mass);
        // Broad crown regions make the heart tree a distinct landmark. Ordinary
        // low garden trees stay green; color varies across whole boughs, not blocks.
        float crown = smoothstep(29.0, 39.0, worldPosition.y);
        float rose = smoothstep(0.22, 0.72, 0.5 + 0.30 * sin(worldPosition.x * 0.065)
                                            + 0.26 * cos(worldPosition.z * 0.055));
        vec3 blossom = mix(vec3(0.23, 0.055, 0.27), vec3(0.56, 0.16, 0.33),
                           clamp(mass * 0.45 + leafSeed * 0.20 + n.y * 0.20 + 0.20, 0.0, 1.0));
        vec3 crownPigment = mix(vec3(0.11, 0.27, 0.23), blossom, rose);
        pigment = mix(pigment, crownPigment, crown);
        color = mix(pigment * 0.82, pigment * (0.95 + q.y * 0.12), leaf);
        float vein = (1.0 - smoothstep(0.015, 0.045, abs(q.x))) * leaf;
        color += (materialId == 18 ? vec3(0.015, 0.045, 0.025) : vec3(0.025, 0.04, 0.01)) * vein;
        color *= 0.91 + 0.16 * broad;
        slope = q * leaf * 0.08;
        roughness = 0.85;
        // A tiny emissive fill keeps unlit canopy readable. Glow leaves remain
        // accents within that canopy instead of bright cyan checker tiles.
        emission = materialId == 18 ? mix(0.08 + leaf * 0.04, 0.065, crown) : 0.030;
    } else if (materialId == 19) {
        vec2 tiles = p * vec2(2.0, 2.5);
        tiles.x += mod(floor(tiles.y), 2.0) * 0.5;
        vec2 f = fract(tiles);
        float tileSeed = materialHash(floor(tiles) + seed);
        color = mix(vec3(0.045, 0.12, 0.23), vec3(0.065, 0.27, 0.30), tileSeed);
        color *= 0.78 + 0.25 * f.y + 0.12 * broad;
        float edge = min(f.x, 1.0 - f.x);
        if (edge < 0.04 || f.y < 0.08) color *= 0.48;
        else color += vec3(0.045, 0.08, 0.10) * (1.0 - smoothstep(0.08, 0.18, f.y));
        slope = vec2((f.x - 0.5) * 0.22, -0.08);
        roughness = 0.62;
    } else if (materialId == 11 || materialId == 12) {
        bool timber = materialId == 11;
        if (timber && top) {
            vec2 q = local - vec2(0.46, 0.52);
            float radius = length(q * vec2(1.0, 1.12)) + chips * 0.025;
            float ring = smoothstep(0.68, 0.87, fract(radius * 13.0));
            color *= 1.12 - 0.29 * ring;
            if (abs(q.x + q.y * 0.25) < 0.018 && radius > 0.12) color *= 0.72;
        } else {
            vec2 along = timber ? p.yx : p.xy;
            float fibers = materialNoise(along * vec2(1.2, 25.0) + seed);
            float bend = materialNoise(along * vec2(0.7, 4.0) + seed + 2.0);
            float grain = fract(along.y * 11.0 + bend * 0.8);
            float stroke = 1.0 - smoothstep(0.035, 0.13, abs(grain - 0.5));
            color *= 0.83 + 0.22 * fibers + 0.15 * bend - stroke * 0.12;
            color += vec3(0.055, 0.027, 0.008) * smoothstep(0.76, 0.90, grain);
            vec2 knotCell = floor(along / vec2(2.0, 1.0));
            float knotSeed = materialHash(knotCell + seed);
            vec2 q = fract(along / vec2(2.0, 1.0)) - vec2(0.25 + knotSeed * 0.5, 0.5);
            float knot = length(q * vec2(2.8, 7.0));
            if (knotSeed > 0.45 && knot < 0.65) {
                color *= 0.65 + 0.29 * smoothstep(0.2, 0.5, knot);
                color *= 0.88 + 0.12 * sin(knot * 32.0);
            }
            slope = (timber ? vec2(fibers - 0.5, 0.0) : vec2(0.0, fibers - 0.5)) * 0.09;
        }
        if (!timber) {
            float row = floor(p.y * 2.0);
            vec2 board = vec2(p.x / 2.0 + mod(row, 3.0) / 3.0, p.y * 2.0);
            vec2 f = fract(board), edge = min(f, 1.0 - f) * vec2(2.0, 0.5);
            float timberTone = materialHash(floor(board) + seed);
            color *= mix(vec3(0.82, 0.87, 0.93), vec3(1.13, 1.04, 0.89), timberTone);
            if (min(edge.x, edge.y) < 0.023) color *= 0.43;
            else slope += sign(f - 0.5) * (1.0 - smoothstep(vec2(0.025), vec2(0.06), edge)) * 0.22;
        }
        roughness = 0.78;
    } else if (materialId == 13) {
        float row = floor(p.y * 2.0);
        vec2 brick = vec2(p.x + mod(row, 2.0) * 0.5, p.y * 2.0);
        vec2 f = fract(brick), edge = min(f, 1.0 - f) * vec2(1.0, 0.5);
        color *= 0.88 + 0.22 * materialHash(floor(brick) + seed);
        if (min(edge.x, edge.y) < 0.024) color = vec3(0.24, 0.225, 0.20) * (0.94 + chips * 0.09);
        else {
            slope = sign(f - 0.5) * (1.0 - smoothstep(vec2(0.025), vec2(0.065), edge)) * 0.28;
            color *= 0.91 + 0.09 * smoothstep(0.025, 0.06, min(edge.x, edge.y));
        }
    } else if (materialId == 10) {
        // Long angular mineral planes continue across voxels rather than stamping
        // the same X onto every cube. Sparse edge gleams preserve deep jewel hues.
        vec2 mineral = p * (top ? vec2(1.8) : vec2(2.8, 0.70));
        mineral.x += top ? 0.0 : p.y * 0.22;
        vec2 tile = floor(mineral), within = fract(mineral), plane = vec2(0.0);
        float nearest = 10.0, second = 10.0, facet = 0.0;
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                vec2 offset = vec2(x, y);
                float h = materialHash(tile + offset + seed);
                vec2 delta = within - offset - 0.2 - 0.6 * vec2(h, fract(h * 13.71));
                float distance = max(abs(delta.x), abs(delta.y) * 0.85) + 0.22 * abs(delta.x + delta.y);
                if (distance < nearest) {
                    second = nearest; nearest = distance; plane = delta; facet = h;
                } else second = min(second, distance);
            }
        }
        float edge = second - nearest;
        color = mix(vec3(0.025, 0.20, 0.52), vec3(0.025, 0.66, 0.79), smoothstep(0.10, 0.75, facet));
        color = mix(color, vec3(0.055, 0.76, 0.67), smoothstep(0.72, 0.96, facet) * 0.65);
        color *= (0.83 + 0.17 * broad) * (0.76 + 0.24 * smoothstep(0.015, 0.10, edge));
        float gleam = smoothstep(0.02, 0.045, edge) * (1.0 - smoothstep(0.045, 0.11, edge));
        color += vec3(0.09, 0.22, 0.25) * gleam * smoothstep(-0.10, 0.25, plane.x);
        slope = vec2((facet - 0.5) * 0.60, (fract(facet * 13.71) - 0.5) * 0.24);
        roughness = 0.20; emission = 0.26 + 0.14 * facet;
    } else if (materialId == 20) {
        // The fitted slab models sit just above this recessed mortar bed.
        // Exposed retaining faces use small, staggered masonry courses.
        color = mix(vec3(0.22, 0.24, 0.20), vec3(0.33, 0.34, 0.28), broad);
        if (!top) {
            vec2 course = vec2(p.x * 1.6 + mod(floor(p.y * 3.0), 2.0) * 0.5, p.y * 3.0);
            vec2 edge = min(fract(course), 1.0 - fract(course));
            float joint = 1.0 - smoothstep(0.025, 0.075, min(edge.x, edge.y));
            float shade = materialHash(floor(course) + seed);
            vec3 stone = mix(vec3(0.43, 0.46, 0.42), vec3(0.61, 0.58, 0.46), shade);
            color = mix(stone * (0.90 + 0.12 * broad), color * 0.78, joint);
        }
        roughness = 0.96;
    } else if (materialId == 14) {
        vec2 edge = min(local, 1.0 - local);
        bool metal = min(edge.x, edge.y) < 0.095 || abs(local.x - 0.5) < 0.027
            || abs(local.y - 0.72) < 0.025 || (top && abs(local.y - 0.5) < 0.04);
        if (metal) {
            color = vec3(0.16, 0.13, 0.09) * (0.85 + chips * 0.23);
            roughness = 0.48;
        } else {
            float glow = 1.0 - smoothstep(0.05, 0.55, length((local - vec2(0.5, 0.4)) * vec2(1.2, 1.0)));
            color = mix(vec3(0.81, 0.36, 0.075), vec3(1.0, 0.72, 0.26), glow);
            roughness = 0.22; emission = 1.0;
        }
        slope = sign(local - 0.5) * (1.0 - smoothstep(vec2(0.07), vec2(0.14), edge)) * 0.20;
    } else if (materialId == 1 || materialId == 2) {
        // Painted soil clumps, root strokes and a few embedded pebbles carry
        // the exposed stair banks; moss spills down unevenly from the surface.
        float clumps = materialNoise(p * vec2(4.0, 5.5) + seed);
        float strata = materialNoise(vec2(p.x * 1.7, p.y * 9.0 + broad) + seed);
        vec3 earth = mix(vec3(0.24, 0.17, 0.11), vec3(0.49, 0.35, 0.22),
                         smoothstep(0.18, 0.82, clumps * 0.65 + broad * 0.35));
        earth *= 0.80 + 0.28 * floor(strata * 5.0) / 4.0;
        float roots = materialNoise(vec2(p.x * 16.0 + sin(p.y * 2.5), p.y * 1.4) + seed);
        earth *= 1.0 - smoothstep(0.73, 0.88, roots) * 0.22;
        vec2 pebble = p * vec2(5.0, 4.0);
        vec2 q = fract(pebble) - 0.5;
        float stone = (1.0 - smoothstep(0.20, 0.28, abs(q.x) + abs(q.y) * 1.4))
                    * step(0.82, materialHash(floor(pebble) + seed));
        earth = mix(earth, vec3(0.47, 0.44, 0.35) * (0.8 + clumps * 0.3), stone * 0.75);
        color = earth;
        if (materialId == 1) {
            float moss = smoothstep(0.25, 0.75, broad * 0.7 + chips * 0.3);
            vec3 green = mix(vec3(0.12, 0.29, 0.14), vec3(0.35, 0.52, 0.17), moss);
            green *= 0.87 + 0.16 * floor(chips * 5.0) / 4.0;
            float depth = 0.43 + 0.35 * materialNoise(vec2(p.x * 2.2, seed));
            float fringe = smoothstep(depth - 0.10, depth + 0.04, local.y + chips * 0.12);
            color = top ? green : mix(earth, green, fringe);
        }
        slope = vec2(clumps - 0.5, strata - 0.5) * 0.13;
    }

    vec3 shadingNormal = decorative == 2 ? normalize(surfaceNormal) : normalize(n + u * slope.x + v * slope.y);
    return MaterialSurface(clamp(color, 0.0, 1.0), shadingNormal, roughness, emission);
}

)glsl";

} // namespace voxel
