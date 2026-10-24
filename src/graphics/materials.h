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
// Mineral proportions also drive the embedded geometry in block_relief.cpp.
// The old coal/copper/iron seams keep their original outlines and paint.
bool embeddedOre(int id) { return (id >= 7 && id <= 9) || (id >= 65 && id <= 77); }
bool gemstoneOre(int id) { return id >= 68 && id <= 73; }
vec2 mineralAspect(int id) {
    if (id == 65) return vec2(0.90, 0.75);  // Tin flakes.
    if (id == 66) return vec2(1.10, 0.85);  // Silver ribbons.
    if (id == 67) return vec2(0.85, 1.20);  // Gold nuggets.
    if (id == 68) return vec2(1.12, 1.50);  // Quartz points.
    if (id == 69) return vec2(1.12, 1.80);  // Broad jade lenses.
    if (id == 70) return vec2(0.88, 1.60);
    if (id == 71) return vec2(0.80, 1.80);
    if (id == 72) return vec2(0.92, 1.60);
    if (id == 73) return vec2(1.20, 1.40);  // Long emerald crystals.
    if (id == 74) return vec2(1.22, 0.72);
    if (id == 75) return vec2(1.32, 0.70);
    if (id == 76) return vec2(1.30, 0.75);
    if (id == 77) return vec2(1.35, 0.90);  // Glassy volcanic blades.
    return vec2(1.0);
}
vec3 mineralColor(int id) {
    if (id == 7) return vec3(0.075, 0.090, 0.105);
    if (id == 8) return vec3(0.72, 0.36, 0.15);
    if (id == 9) return vec3(0.53, 0.55, 0.57);
    if (id == 65) return vec3(0.64, 0.72, 0.68);
    if (id == 66) return vec3(0.78, 0.88, 0.98);
    if (id == 67) return vec3(0.95, 0.64, 0.13);
    if (id == 68) return vec3(0.91, 0.86, 0.96);
    if (id == 69) return vec3(0.20, 0.65, 0.37);
    if (id == 70) return vec3(0.63, 0.25, 0.91);
    if (id == 71) return vec3(0.87, 0.075, 0.19);
    if (id == 72) return vec3(0.13, 0.34, 0.93);
    if (id == 73) return vec3(0.075, 0.83, 0.34);
    if (id == 74) return vec3(0.13, 0.39, 0.75);
    if (id == 75) return vec3(0.60, 0.76, 0.86);
    if (id == 76) return vec3(0.16, 0.87, 0.72);
    return vec3(0.18, 0.095, 0.27);
}
vec3 mineralHost(int id, float broad) {
    if (id == 67) return mix(vec3(0.30, 0.26, 0.23), vec3(0.45, 0.38, 0.29), broad);
    if (id == 68) return mix(vec3(0.39, 0.34, 0.39), vec3(0.53, 0.49, 0.51), broad);
    if (id == 69) return mix(vec3(0.26, 0.34, 0.28), vec3(0.39, 0.43, 0.30), broad);
    if (id >= 70 && id <= 73) return mix(vec3(0.25, 0.27, 0.34), vec3(0.38, 0.34, 0.43), broad);
    if (id >= 74 && id <= 76) return mix(vec3(0.22, 0.30, 0.35), vec3(0.37, 0.41, 0.43), broad);
    if (id == 77) return mix(vec3(0.075, 0.065, 0.11), vec3(0.19, 0.14, 0.25), broad);
    return mix(vec3(0.36, 0.40, 0.42), vec3(0.49, 0.49, 0.45), broad);
}
// One evaluation supplies color, shallow relief, roughness, and luminous glass.
// Correlated shapes are sampled on a 32-pixel grid, not random bright checkers.
MaterialSurface evaluateMaterial() {
    vec3 n = normalize(surfaceNormal);
    if (decorative == 1 || decorative == 3) {
        vec3 paint = decorative == 3 ? floor(modelPosition * 32.0) / 32.0 : worldPosition;
        float brush = floor(materialNoise(paint.xz * 2.0 + paint.y) * 4.0) / 4.0;
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
        if (materialId == 17 || materialId == 30) emission = 0.025;
        if (materialId == 18 || materialId == 32) emission = 0.15;
        if (materialId == 31) { roughness = 0.25; emission = 0.30; }
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
        || materialId == 21 || materialId == 28
        || embeddedOre(materialId)) {
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
        if (materialId >= 65 && materialId <= 77) color = mineralHost(materialId, broad);
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
        if (materialId == 21) {
            // Irregular velvet patches settle in fissures and creep over ledges.
            float cover = smoothstep(0.43, 0.64, broad * 0.60 + chips * 0.32
                                      + crack * 0.13 + (top ? 0.12 : 0.0));
            vec3 moss = mix(vec3(0.095, 0.22, 0.105), vec3(0.34, 0.48, 0.16), chips);
            color = mix(color * 0.88, moss * (0.91 + fine * 0.09), cover);
            slope *= 1.0 - cover * 0.75;
        }
        if (materialId == 28) {
            // Pale calcite has broad cream bands and cool mineral seams.
            float band = materialNoise(vec2(p.x * 0.8 + chips * 0.4, p.y * 7.5) + seed);
            color = mix(vec3(0.59, 0.66, 0.62), vec3(0.87, 0.84, 0.66), broad);
            color *= 0.86 + 0.17 * floor(band * 5.0) / 4.0 - crack * 0.13;
            color += vec3(0.065, 0.06, 0.045) * rim * max(edgeLight, 0.0);
            roughness = 0.65;
        }
        if (materialId == 77) {
            // Conchoidal bands and polished purple edges distinguish volcanic
            // glass from the rough host rock around every other ore.
            float band = floor(materialNoise(p * vec2(3.0, 8.0) + seed) * 5.0) / 4.0;
            color *= 0.72 + band * 0.40;
            color += vec3(0.10, 0.045, 0.16) * rim * max(edgeLight, 0.0);
            roughness = 0.34;
        }
        if (embeddedOre(materialId)) {
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
            vec2 aspect = mineralAspect(materialId);
            float halfLength = (0.32 + 0.13 * h2) * aspect.x;
            float halfWidth = (0.10 + 0.05 * h3) * aspect.y;
            vec2 corners[6];
            int cornerCount = gemstoneOre(materialId) ? 6 : 4;
            if (cornerCount == 6) {
                corners[0] = vec2(-halfLength, -halfWidth * 0.15);
                corners[1] = vec2(-halfLength * 0.45, -halfWidth);
                corners[2] = vec2(halfLength * 0.45, -halfWidth * 0.85);
                corners[3] = vec2(halfLength, halfWidth * 0.2);
                corners[4] = vec2(halfLength * 0.45, halfWidth);
                corners[5] = vec2(-halfLength * 0.4, halfWidth * 0.85);
            } else {
                corners[0] = vec2(-halfLength, -halfWidth * 0.15);
                corners[1] = vec2(-halfLength * 0.25, -halfWidth);
                corners[2] = vec2(halfLength, halfWidth * 0.2);
                corners[3] = vec2(halfLength * 0.20, halfWidth);
            }
            float edgeDistance = -10.0;
            for (int i = 0; i < cornerCount; ++i) {
                vec2 side = corners[(i + 1) % cornerCount] - corners[i];
                edgeDistance = max(edgeDistance, dot(q - corners[i], normalize(vec2(side.y, -side.x))));
            }
            if (h > 0.32 && edgeDistance < 0.035) {
                vec3 ore = mineralColor(materialId);
                if (materialId == 8 && h2 > 0.90) ore = mix(ore, vec3(0.20, 0.40, 0.31), 0.48);
                if (edgeDistance <= 0.0) {
                    float upper = clamp(q.y / halfWidth * 0.5 + 0.5, 0.0, 1.0);
                    vec3 mineral = ore * (0.78 + 0.29 * upper + 0.06 * stroke);
                    if (gemstoneOre(materialId)) {
                        // Broad light/dark facets reveal embedded crystals, with
                        // a pale ridge instead of noisy dots or emissive paint.
                        float facet = step(0.0, q.y + q.x * 0.24) * 0.23
                            + step(0.0, q.x - q.y * 0.35) * 0.16;
                        mineral = ore * (0.60 + facet + 0.14 * upper);
                        float ridge = 1.0 - smoothstep(0.01, 0.055, abs(q.y + q.x * 0.24));
                        mineral += mix(ore, vec3(0.87, 0.94, 1.0), 0.65) * ridge * 0.17;
                        if (materialId == 69) mineral = mix(mineral, ore * 0.75, 0.25);
                    } else if (materialId >= 65) {
                        float grain = floor(materialNoise(q * vec2(6.0, 24.0) + seed) * 4.0) / 3.0;
                        mineral *= 0.84 + grain * 0.23;
                    }
                    // Painted highlights remain broad, with a small broken glint.
                    float glint = smoothstep(-0.045, -0.012, edgeDistance) * upper;
                    mineral += (materialId == 7 ? vec3(0.025, 0.035, 0.045) : vec3(0.09, 0.075, 0.045)) * glint;
                    float exposed = smoothstep(-0.95, -0.15, q.x / halfLength)
                        * smoothstep(-1.0, -0.2, q.y / halfWidth);
                    color = mix(color, mineral, exposed);
                    float mineralRoughness = materialId == 7 ? 0.77 : 0.67;
                    if (materialId >= 65) mineralRoughness = gemstoneOre(materialId) ? 0.31 : 0.46;
                    if (materialId == 69) mineralRoughness = 0.43;
                    if (materialId == 77) mineralRoughness = 0.22;
                    roughness = mix(roughness, mineralRoughness, exposed);
                    slope += across * 0.055 * exposed;
                } else {
                    color *= 0.72 + 0.28 * smoothstep(0.0, 0.035, edgeDistance);
                }
            }
        }
    } else if (materialId == 16) {
        // Continuous world-space waves cross voxel and chunk edges without seams.
        vec2 flow = top ? worldPosition.xz : vec2(dot(worldPosition, u), worldPosition.y);
        float t = timeSeconds;
        if (top) {
            float broadWave = dot(flow, vec2(0.72, 0.46)) + t * 0.65;
            float crossWave = dot(flow, vec2(-1.65, 2.15)) - t * 1.1;
            float smallWave = dot(flow, vec2(4.2, 3.4)) + t * 1.45;
            float detail = 1.0 - smoothstep(0.08, 0.45, length(fwidth(flow)));
            slope = vec2(0.72, 0.46) * cos(broadWave) * 0.055
                  + vec2(-1.65, 2.15) * cos(crossWave) * 0.028
                  + vec2(4.2, 3.4) * cos(smallWave) * 0.009 * detail;
            float body = materialNoise(flow * 0.28 + vec2(t * 0.015, -t * 0.012));
            color = mix(vec3(0.025, 0.28, 0.32), vec3(0.045, 0.43, 0.40), body);
        } else {
            // Long, narrow streaks travel downward along every vertical curtain.
            vec2 falling = flow + vec2(0.0, t * 2.4);
            float ribbon = materialNoise(falling * vec2(5.5, 0.42));
            float threads = materialNoise(falling * vec2(16.0, 1.2));
            float foam = smoothstep(0.49, 0.84, ribbon) * (0.5 + 0.5 * threads);
            color = mix(vec3(0.025, 0.31, 0.35), vec3(0.48, 0.78, 0.74), foam);
            slope = vec2((ribbon - 0.5) * 0.18, sin(falling.y * 4.0 + ribbon) * 0.025);
        }
        roughness = top ? 0.13 : 0.24;
    } else if (materialId == 17 || materialId == 18 || materialId == 30) {
        // Pointed overlapping leaves leave real holes in the voxel face. This
        // fixed world-space mask never animates or switches off with distance.
        // Model foliage returned above already has shaped leaves, so only the
        // block shell gets cut out. No sorted transparency pass is necessary.
        vec2 grid = p * 3.0;
        vec2 tile = floor(grid), within = fract(grid);
        float best = -10.0, leafSeed = 0.0;
        vec2 leafPoint = vec2(0.0);
        for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(x, y);
            float h = materialHash(tile + offset + seed);
            float h2 = materialHash(tile + offset + seed + 13.0);
            vec2 center = offset + vec2(0.18 + 0.64 * h, 0.18 + 0.64 * h2);
            vec2 q = within - center;
            float angle = floor(h2 * 8.0) * 0.785398;
            vec2 along = vec2(cos(angle), sin(angle));
            q = vec2(dot(q, along), dot(q, vec2(-along.y, along.x)));
            q /= vec2(0.70 + h * 0.14, 0.36 + h2 * 0.08);
            // The curved sides meet in pointed tips, unlike the soil/grass paint.
            float inside = 1.0 - q.x * q.x - abs(q.y);
            if (inside > best) { best = inside; leafPoint = q; leafSeed = h; }
        }
        if (best <= 0.035) discard;
        float mass = materialNoise(p * 0.43 + seed);
        vec3 pigment = mix(vec3(0.075, 0.25, 0.11), vec3(0.30, 0.57, 0.18),
                           mass * 0.55 + leafSeed * 0.45);
        if (materialId == 18)
            pigment = mix(vec3(0.055, 0.32, 0.22), vec3(0.18, 0.65, 0.42), leafSeed);
        // Preserve the heart tree's broad rose crown and add a pink leaf block
        // that builders can use at any height in their own gardens.
        float crown = materialId == 30 ? 1.0 : smoothstep(29.0, 39.0, worldPosition.y);
        float rose = materialId == 30 ? 1.0
            : smoothstep(0.22, 0.72, 0.5 + 0.30 * sin(worldPosition.x * 0.065)
                                       + 0.26 * cos(worldPosition.z * 0.055));
        vec3 blossom = mix(vec3(0.27, 0.065, 0.26), vec3(0.77, 0.31, 0.48),
                           clamp(mass * 0.35 + leafSeed * 0.40 + n.y * 0.15 + 0.12, 0.0, 1.0));
        pigment = mix(pigment, mix(vec3(0.12, 0.34, 0.26), blossom, rose), crown);
        float edge = smoothstep(0.035, 0.21, best);
        color = pigment * (0.70 + edge * 0.28 + leafPoint.y * 0.12);
        // A pale midrib and a few oblique veins make each leaf recognizable.
        float vein = 1.0 - smoothstep(0.025, 0.10, abs(leafPoint.y));
        float ribs = 1.0 - smoothstep(0.03, 0.08,
            abs(fract(leafPoint.x * 2.2 - abs(leafPoint.y) * 1.2) - 0.5));
        color += pigment * (vein * 0.27 + ribs * 0.075) * edge;
        slope = vec2(leafPoint.x, -leafPoint.y) * 0.09;
        roughness = 0.84;
        emission = materialId == 18 ? mix(0.14, 0.09, crown) : 0.025;
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
    } else if (materialId == 11 || materialId == 12 || materialId == 26 || materialId == 27) {
        bool timber = materialId == 11 || materialId == 26;
        if (materialId == 26) {
            // Twisting fibrous roots, kept separate from regular straight timber.
            p.x += sin(p.y * 2.1 + seed) * 0.09 + sin(p.y * 0.73 + seed) * 0.12;
            color = mix(vec3(0.23, 0.17, 0.105), vec3(0.50, 0.37, 0.20), broad);
        }
        if (materialId == 27)
            color = mix(vec3(0.30, 0.34, 0.30), vec3(0.58, 0.53, 0.37), broad);
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
        if (materialId == 26 && !top) {
            float deepGrain = materialNoise(vec2(p.x * 13.0, p.y * 0.65) + seed);
            color *= 0.82 + deepGrain * 0.34;
            color = mix(color, vec3(0.24, 0.37, 0.14), smoothstep(0.72, 0.85, chips) * 0.48);
        }
        if (materialId == 27) {
            float wear = smoothstep(0.53, 0.72, chips);
            color = mix(color, vec3(0.48, 0.50, 0.41), wear * 0.40);
        }
        roughness = materialId >= 26 ? 0.93 : 0.78;
    } else if (materialId == 13 || materialId == 22) {
        float row = floor(p.y * 2.0);
        vec2 brick = vec2(p.x + mod(row, 2.0) * 0.5, p.y * 2.0);
        vec2 f = fract(brick), edge = min(f, 1.0 - f) * vec2(1.0, 0.5);
        color *= 0.88 + 0.22 * materialHash(floor(brick) + seed);
        if (min(edge.x, edge.y) < 0.024) color = vec3(0.24, 0.225, 0.20) * (0.94 + chips * 0.09);
        else {
            slope = sign(f - 0.5) * (1.0 - smoothstep(vec2(0.025), vec2(0.065), edge)) * 0.28;
            color *= 0.91 + 0.09 * smoothstep(0.025, 0.06, min(edge.x, edge.y));
        }
        if (materialId == 22) {
            float crevice = 1.0 - smoothstep(0.03, 0.14, min(edge.x, edge.y));
            float growth = smoothstep(0.39, 0.72, chips * 0.50 + broad * 0.28 + crevice * 0.35);
            vec3 moss = mix(vec3(0.10, 0.23, 0.11), vec3(0.35, 0.46, 0.17), broad);
            color = mix(color, moss, growth);
        }
    } else if (materialId == 10 || materialId == 31) {
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
        if (materialId == 31) {
            color = mix(vec3(0.46, 0.12, 0.025), vec3(0.95, 0.57, 0.12), facet);
            color *= 0.83 + 0.17 * broad;
            color *= 0.76 + 0.24 * smoothstep(0.015, 0.10, edge);
            color += vec3(0.18, 0.15, 0.065) * gleam * smoothstep(-0.10, 0.25, plane.x);
        }
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
    } else if (materialId == 24 || materialId == 25) {
        // Broad sediment bands for riverbanks: warm clay and damp grey-green silt.
        float band = materialNoise(vec2(p.x * 0.75 + chips * 0.3, p.y * 6.5) + seed);
        float steps = floor(band * 6.0) / 5.0;
        vec3 low = materialId == 24 ? vec3(0.44, 0.22, 0.15) : vec3(0.23, 0.29, 0.26);
        vec3 high = materialId == 24 ? vec3(0.75, 0.47, 0.31) : vec3(0.46, 0.48, 0.37);
        color = mix(low, high, broad * 0.55 + steps * 0.45);
        float pebbles = smoothstep(0.78, 0.90, materialNoise(p * 11.0 + seed));
        color += (materialId == 24 ? vec3(0.10, 0.07, 0.035) : vec3(0.08, 0.085, 0.065)) * pebbles;
        slope = vec2(chips - 0.5, band - 0.5) * 0.07;
        roughness = materialId == 24 ? 0.92 : 0.68;
    } else if (materialId == 29 || materialId == 32) {
        float cushions = materialNoise(p * 3.7 + seed);
        float tips = floor(chips * 5.0) / 4.0;
        vec3 low = materialId == 32 ? vec3(0.025, 0.26, 0.17) : vec3(0.11, 0.23, 0.075);
        vec3 high = materialId == 32 ? vec3(0.16, 0.68, 0.41) : vec3(0.40, 0.56, 0.16);
        color = mix(low, high, cushions * 0.65 + tips * 0.35);
        color *= 0.92 + fine * 0.10;
        slope = vec2(cushions - 0.5, chips - 0.5) * 0.12;
        roughness = 0.98;
        if (materialId == 32) {
            // Clusters of soft luminous spores, no temporal twinkling/pop-in.
            float spores = smoothstep(0.66, 0.86, chips);
            color += vec3(0.055, 0.13, 0.065) * spores;
            emission = 0.10 + spores * 0.25;
        }
    } else if (materialId == 1 || materialId == 2 || materialId == 23) {
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
        if (materialId == 23) {
            vec2 rootUV = p;
            rootUV.x += sin(p.y * 3.8 + seed) * 0.075;
            float root = abs(fract(rootUV.x * 2.7 + materialNoise(p * 0.85 + seed) * 0.65) - 0.5);
            float skin = 1.0 - smoothstep(0.035, 0.10, root);
            vec3 wood = mix(vec3(0.34, 0.22, 0.12), vec3(0.62, 0.43, 0.23), 1.0 - root * 7.0);
            color = mix(earth * 0.91, wood, skin);
        }
        slope = vec2(clumps - 0.5, strata - 0.5) * 0.13;
    }

    vec3 shadingNormal = decorative == 2 ? normalize(surfaceNormal) : normalize(n + u * slope.x + v * slope.y);
    return MaterialSurface(clamp(color, 0.0, 1.0), shadingNormal, roughness, emission);
}

)glsl";

} // namespace voxel
