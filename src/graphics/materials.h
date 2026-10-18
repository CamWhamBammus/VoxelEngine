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
        if (materialId == 10) color *= vec3(0.55, 0.95, 1.0);
        if (materialId == 1) {
            float tip = smoothstep(0.05, 0.50, fract(worldPosition.y));
            color *= mix(vec3(0.62, 0.76, 0.83), vec3(1.18, 1.03, 0.72), tip);
        }
        return MaterialSurface(clamp(color, 0.0, 1.0), n,
            materialId == 10 ? 0.24 : 0.88, materialId == 10 ? 0.30 + brush * 0.08 : 0.0);
    }
    // Sample the same paint on a raised plate and on its supporting block.
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
        // Irregular mineral plates give stone broad painted planes and chipped
        // edges rather than the repeating wavy lines of a noise-only surface.
        // block_relief.cpp uses these same centers for the raised plate outlines.
        vec2 plate = p * vec2(0.95, shale ? 2.4 : 1.15);
        vec2 tile = floor(plate), within = fract(plate);
        float nearest = 10.0, second = 10.0, plateColor = 0.0;
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
                    nearest = distance; plane = delta; plateColor = h;
                } else if (distance < second) { second = distance; secondPlane = delta; }
            }
        }
        // A broad chipped bevel and one-sided painted edge lift the planes.
        // Broken dark seams avoid a uniform outline around every stone tile.
        float edge = sqrt(second) - sqrt(nearest) - smoothstep(0.58, 0.84, chips) * 0.055;
        float crack = 1.0 - smoothstep(0.012, 0.045, edge);
        float bevel = 1.0 - smoothstep(0.035, 0.20, edge);
        vec2 outward = normalize(plane - secondPlane);
        float edgeLight = dot(outward, normalize(vec2(-0.55, 0.83)));
        float stroke = materialNoise(vec2(p.x * 0.8 - p.y * 0.35, p.y * 7.0 + p.x * 1.6) + seed);
        stroke = floor(stroke * 5.0) / 5.0;
        vec3 cool = shale ? vec3(0.23, 0.34, 0.46) : vec3(0.37, 0.50, 0.59);
        vec3 warm = shale ? vec3(0.36, 0.36, 0.43) : vec3(0.67, 0.61, 0.47);
        vec3 pigment = mix(cool, warm, smoothstep(0.20, 0.85, plateColor));
        color = mix(surfaceColor, pigment, materialId == 5 ? 0.70 : 0.58);
        if (materialId == 15) color *= 0.65;
        if (materialId >= 7 && materialId <= 9) color = mix(vec3(0.36, 0.42, 0.49), pigment, 0.24);
        color *= 0.79 + 0.21 * stroke + 0.13 * clamp(dot(plane, vec2(-0.6, 0.8)) + 0.5, 0.0, 1.0);
        color *= 1.0 + bevel * (0.19 * edgeLight - 0.09);
        color *= 1.0 - crack * (0.22 + 0.25 * smoothstep(0.28, 0.72, chips));
        float rim = smoothstep(0.025, 0.055, edge) * (1.0 - smoothstep(0.055, 0.12, edge));
        color += vec3(0.11, 0.15, 0.16) * rim * max(edgeLight, 0.0);
        slope = outward * bevel * (shale ? 0.28 : 0.38) + plane * 0.04;
        if (materialId >= 7 && materialId <= 9) {
            // Keep the fleck centers and outline in sync with block_relief.cpp.
            // On real facets use the continuous footprint: snapping a bevel to
            // the paint grid would stripe its side with neighboring rock pixels.
            vec2 mineralUV = decorative == 2 ? uv : p;
            vec2 mineralCell = floor(mineralUV * 3.0);
            float h = mineralHash(mineralCell + seed), h2 = mineralHash(mineralCell + seed + 11.0);
            vec2 q = fract(mineralUV * 3.0) - vec2(0.35 + 0.30 * h, 0.35 + 0.30 * h2);
            q = mat2(0.8, -0.6, 0.6, 0.8) * q;
            float shape = max(abs(q.x) * 0.83 + abs(q.y) * 0.66, max(abs(q.x), abs(q.y)) * 0.94);
            float radius = 0.21 + 0.16 * h2;
            if (h > 0.48 && shape < radius + 0.035) {
                vec3 ore = materialId == 7 ? vec3(0.065, 0.080, 0.095)
                    : (materialId == 8 ? vec3(0.68, 0.34, 0.16) : vec3(0.58, 0.57, 0.50));
                if (materialId == 8 && h2 > 0.77) ore = mix(ore, vec3(0.20, 0.42, 0.34), 0.58);
                float facet = q.x + q.y * 0.7 > 0.0 ? 1.17 : 0.78;
                color = shape > radius ? vec3(0.16, 0.19, 0.20) : ore * facet;
                if (shape < radius && q.y > radius * 0.40) color += vec3(0.07, 0.065, 0.05);
                roughness = materialId == 7 ? 0.64 : 0.48;
                slope += vec2(sign(q.x), sign(q.y)) * 0.12;
            }
        }
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
    } else if (materialId == 1) {
        float moss = smoothstep(0.25, 0.75, broad * 0.7 + chips * 0.3);
        color = mix(vec3(0.14, 0.35, 0.16), vec3(0.40, 0.64, 0.18), moss);
        color *= 0.92 + 0.08 * floor(chips * 4.0) / 4.0;
        if (!top) {
            float grass = step(0.78 + chips * 0.12, local.y);
            color = mix(vec3(0.39, 0.27, 0.15) * (0.90 + 0.14 * broad), color, grass);
        }
    }
    vec3 shadingNormal = decorative == 2 ? normalize(surfaceNormal) : normalize(n + u * slope.x + v * slope.y);
    return MaterialSurface(clamp(color, 0.0, 1.0), shadingNormal, roughness, emission);
}

)glsl";

} // namespace voxel
