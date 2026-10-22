#pragma once

namespace voxel {

// Shared by the world material shader and its focused GPU geometry checks.
// Occupancy values: opaque cube 255, stair 200..203, water 127, air/cutout 0.
inline constexpr const char* voxelLightingShader = R"glsl(
int occupancyCode(ivec3 cell) {
    ivec3 local = cell - ivec3(worldMinimum);
    if (any(lessThan(local, ivec3(0))) || any(greaterThanEqual(local, textureSize(occupancy, 0)))) return 0;
    return int(texelFetch(occupancy, local, 0).r * 255.0 + 0.5);
}

// Water-bank queries need to know whether a voxel contains any solid shape.
bool solid(ivec3 cell) { return occupancyCode(cell) >= 200; }

// Query the occupied half of a stair, not merely the containing voxel.
bool occupiedAt(vec3 point, out bool stair) {
    int code = occupancyCode(ivec3(floor(point)));
    stair = code >= 200 && code <= 203;
    if (code == 255) return true;
    if (code < 200 || code > 203) return false;
    vec3 local = fract(point);
    if (local.y < 0.5) return true;
    if (code == 200) return local.z < 0.5;
    if (code == 201) return local.x < 0.5;
    if (code == 202) return local.z >= 0.5;
    return local.x >= 0.5;
}

bool occupiedAt(vec3 point) {
    bool stair;
    return occupiedAt(point, stair);
}

bool contactOccupiedAt(vec3 point, inout bool touchesStair) {
    bool stair;
    bool occupied = occupiedAt(point, stair);
    touchesStair = touchesStair || stair;
    return occupied;
}

bool crossesBox(vec3 start, vec3 ray, vec3 low, vec3 high) {
    float near = 0.0, far = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
        if (abs(ray[axis]) < 0.000001) {
            if (start[axis] < low[axis] || start[axis] >= high[axis]) return false;
        } else {
            float a = (low[axis] - start[axis]) / ray[axis];
            float b = (high[axis] - start[axis]) / ray[axis];
            near = max(near, min(a, b)); far = min(far, max(a, b));
        }
    }
    return far > near + 0.000001;
}

bool blocksLight(ivec3 cell, vec3 start, vec3 ray) {
    int code = occupancyCode(cell);
    if (code == 255) return true;
    if (code < 200 || code > 203) return false;
    vec3 local = start - vec3(cell);
    if (crossesBox(local, ray, vec3(0), vec3(1, 0.5, 1))) return true;
    vec3 low = vec3(0, 0.5, 0), high = vec3(1);
    if (code == 200) high.z = 0.5;
    else if (code == 201) high.x = 0.5;
    else if (code == 202) low.z = 0.5;
    else low.x = 0.5;
    return crossesBox(local, ray, low, high);
}

// Test just beyond each face edge. Whole-block samples preserve the broad
// shading of cube corners; half-block samples also see a stair's inner riser.
// Taking the darker result avoids counting the same wall twice.
float contactAtScale(vec3 anchor, vec3 normal, vec3 u, vec3 v, float scale, inout bool touchesStair) {
    vec2 position = fract(vec2(dot(anchor, u), dot(anchor, v)) * scale) / scale;
    vec2 opposite = vec2(1.0 / scale) - position;
    vec2 low = 1.0 - smoothstep(vec2(0.0), vec2(0.65), position);
    vec2 high = 1.0 - smoothstep(vec2(0.0), vec2(0.65), opposite);
    vec3 outside = anchor + normal * 0.02;
    vec3 lowU = -u * (position.x + 0.02), highU = u * (opposite.x + 0.02);
    vec3 lowV = -v * (position.y + 0.02), highV = v * (opposite.y + 0.02);
    float sides = float(contactOccupiedAt(outside + lowU, touchesStair)) * low.x
        + float(contactOccupiedAt(outside + highU, touchesStair)) * high.x
        + float(contactOccupiedAt(outside + lowV, touchesStair)) * low.y
        + float(contactOccupiedAt(outside + highV, touchesStair)) * high.y;
    // Half-grid intersections inside a flat wall are not extra corners. Only
    // their side samples discover new risers; full-cell corners keep their AO.
    if (scale > 1.0) return sides * 0.24;
    float corners = float(contactOccupiedAt(outside + lowU + lowV, touchesStair)) * low.x * low.y
        + float(contactOccupiedAt(outside + lowU + highV, touchesStair)) * low.x * high.y
        + float(contactOccupiedAt(outside + highU + lowV, touchesStair)) * high.x * low.y
        + float(contactOccupiedAt(outside + highU + highV, touchesStair)) * high.x * high.y;
    return sides * 0.24 + corners * 0.16;
}

float contactShade() {
    vec3 normal = decorative == 2 ? reliefSurface.xyz : normalize(surfaceNormal);
    vec3 anchor = decorative == 2 ? worldPosition - normal * reliefSurface.w : worldPosition;
    vec3 u = abs(normal.x) > 0.5 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 v = abs(normal.y) > 0.5 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    bool touchesStair;
    occupiedAt(anchor - normal * 0.02, touchesStair);
    float shade = contactAtScale(anchor, normal, u, v, 1.0, touchesStair);
    // Ordinary cube neighborhoods retain a single pass. The first pass already
    // visits the relevant neighbors, so detecting a nearby stair needs no scan.
    if (touchesStair)
        shade = max(shade, contactAtScale(anchor, normal, u, v, 2.0, touchesStair));
    return max(0.42, 1.0 - shade);
}

// Walk crossed voxels, not fixed-distance samples: even a one-block wall casts
// a reliable shadow. The final cell is the emitting block and is not a blocker.
float visibleLight(vec3 start, vec3 end) {
    vec3 ray = end - start;
    ivec3 cell = ivec3(floor(start));
    ivec3 target = ivec3(floor(end));
    ivec3 advance = ivec3(sign(ray));
    vec3 delta = 1.0 / max(abs(ray), vec3(0.000001));
    vec3 next = mix(start - vec3(cell), vec3(cell + 1) - start, greaterThan(ray, vec3(0.0))) * delta;
    next = mix(vec3(1e20), next, greaterThan(abs(ray), vec3(0.000001)));
    // Sources reach at most eleven blocks: 32 crossings cover every direction.
    for (int step = 0; step < 32; ++step) {
        if (all(equal(cell, target))) return 1.0;
        if (blocksLight(cell, start, ray)) return 0.0;
        if (next.x < next.y && next.x < next.z) { cell.x += advance.x; next.x += delta.x; }
        else if (next.y < next.z) { cell.y += advance.y; next.y += delta.y; }
        else { cell.z += advance.z; next.z += delta.z; }
    }
    return 0.0;
}
)glsl";

} // namespace voxel
