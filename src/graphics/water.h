#pragma once

namespace voxel {

// Water is drawn after opaque terrain. The color/depth snapshot contains the
// actual banks and lakebed, so refraction never samples its own output.
inline constexpr const char* waterShader = R"glsl(
uniform sampler2D opaqueColor;
uniform sampler2D opaqueDepth;
uniform mat4 inverseViewProjection;
uniform mat4 viewProjection;
uniform int refractiveWater;

vec3 scenePosition(vec2 uv, float depth) {
    vec4 p = inverseViewProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

bool waterCell(ivec3 cell) {
    ivec3 local = cell - ivec3(worldMinimum);
    if (any(lessThan(local, ivec3(0))) || any(greaterThanEqual(local, textureSize(occupancy, 0)))) return false;
    float value = texelFetch(occupancy, local, 0).r;
    return value > 0.25 && value < 0.5;
}

// Distance to actual banks and falling water, independent of the camera.
vec2 waterEdges() {
    ivec3 cell = ivec3(floor(worldPosition - vec3(0, 0.02, 0)));
    vec2 distanceToEdge = vec2(4.0);
    for (int z = -2; z <= 2; ++z) for (int x = -2; x <= 2; ++x) {
        ivec3 neighbor = cell + ivec3(x, 0, z);
        vec2 delta = max(max(vec2(neighbor.xz) - worldPosition.xz,
                            worldPosition.xz - vec2(neighbor.xz + 1)), vec2(0.0));
        float distance = length(delta);
        if (abs(x) <= 1 && abs(z) <= 1 && solid(neighbor))
            distanceToEdge.x = min(distanceToEdge.x, distance);
        if (waterCell(neighbor + ivec3(0, 1, 0)) && waterCell(neighbor + ivec3(0, 2, 0)))
            distanceToEdge.y = min(distanceToEdge.y, distance);
    }
    return distanceToEdge;
}

vec4 waterReflection(vec3 normal, vec3 viewDirection) {
    vec3 direction = reflect(-viewDirection, normal);
    if (direction.y < 0.02) return vec4(0.0);
    float previousTravel = 0.12;
    // A short, bounded screen-space ray finds visible buildings and lanterns.
    // Off-screen scenery fades back to the cave tint instead of smearing edges.
    for (int i = 0; i < 24; ++i) {
        float travel = 0.20 + float(i) * 0.22 + float(i * i) * 0.045;
        vec3 probe = worldPosition + normal * 0.06 + direction * travel;
        vec4 projected = viewProjection * vec4(probe, 1.0);
        if (projected.w <= 0.0) break;
        vec3 screen = projected.xyz / projected.w * 0.5 + 0.5;
        if (any(lessThan(screen, vec3(0.001))) || any(greaterThan(screen, vec3(0.999)))) break;
        float depth = texture(opaqueDepth, screen.xy).r;
        if (depth >= 0.99999) { previousTravel = travel; continue; }
        vec3 hit = scenePosition(screen.xy, depth);
        float behind = length(probe - cameraPosition) - length(hit - cameraPosition);
        if (behind > 0.0 && behind < (travel - previousTravel) * 2.0 + 0.3
            && hit.y > worldPosition.y + 0.15) {
            float edge = min(min(screen.x, 1.0 - screen.x), min(screen.y, 1.0 - screen.y));
            float confidence = smoothstep(0.0, 0.09, edge) * (1.0 - smoothstep(18.0, 30.0, travel));
            return vec4(texture(opaqueColor, screen.xy).rgb, confidence);
        }
        previousTravel = travel;
    }
    return vec4(0.0);
}

vec3 shadeWater(vec3 lit, vec3 normal, vec3 viewDirection) {
    bool exiting = dot(surfaceNormal, viewDirection) < 0.0;
    bool top = surfaceNormal.y > 0.5 && !exiting;
    float fresnel = pow(1.0 - abs(dot(normal, viewDirection)), 4.0);
    vec3 reflection = underground != 0 ? vec3(0.025, 0.095, 0.12) : vec3(0.32, 0.52, 0.68);
    if (refractiveWater == 0) return mix(lit, reflection, 0.18 + 0.45 * fresnel);

    vec2 uv = gl_FragCoord.xy / vec2(textureSize(opaqueColor, 0));
    float depth = texture(opaqueDepth, uv).r;
    float thickness = depth < 0.99999 ? length(scenePosition(uv, depth) - worldPosition) : 18.0;
    // Looking out from below the water absorbs only the camera-to-surface
    // segment, not the open air between the surface and the cavern ceiling.
    if (exiting) thickness = length(cameraPosition - worldPosition);
    // Keep distortion small in the shallows. Reject samples in front of this
    // water face, so bridges and bank silhouettes cannot bleed into the lake.
    vec2 bend = top ? normal.xz : vec2(normal.x + normal.z, normal.y);
    vec2 refracted = clamp(uv + bend * 0.016 * smoothstep(0.0, 1.5, thickness), vec2(0.001), vec2(0.999));
    float bentDepth = texture(opaqueDepth, refracted).r;
    if (bentDepth <= gl_FragCoord.z + 0.00001) refracted = uv;
    vec3 bottom = texture(opaqueColor, refracted).rgb;
    // Red is absorbed first: sand stays visible in shallow jade water while
    // deeper pools become blue-green. Optical thickness follows the view ray.
    vec3 transmission = exp(-vec3(0.48, 0.18, 0.12) * min(thickness, 28.0));
    vec3 body = max(lit * 0.7, vec3(0.012, 0.095, 0.105));
    vec3 water = bottom * transmission + body * (1.0 - transmission);
    if (exiting) return water;
    if (top) {
        // Gentle moving caustic light is restricted to visible shallow beds.
        vec3 floorPosition = depth < 0.99999 ? scenePosition(uv, depth) : worldPosition;
        vec2 causticUV = floorPosition.xz;
        float a = sin(dot(causticUV, vec2(2.3, 1.6)) + timeSeconds * 0.9);
        float b = sin(dot(causticUV, vec2(-1.7, 2.6)) - timeSeconds * 0.7);
        float caustic = pow(max(0.0, 1.0 - abs(a + b)), 10.0);
        water += bottom * caustic * 0.32 * exp(-thickness * 0.22);
        vec4 reflected = waterReflection(normal, viewDirection);
        reflection = mix(reflection, reflected.rgb, reflected.a);
        water = mix(water, reflection, 0.08 + 0.58 * fresnel);
        water += lit * (0.16 + 0.25 * fresnel);

        vec2 edges = waterEdges();
        float noise = materialNoise(worldPosition.xz * 3.2 + timeSeconds * vec2(-0.12, 0.17));
        float patches = materialNoise(worldPosition.xz * 1.3 + timeSeconds * vec2(0.07, -0.04));
        float shore = (1.0 - smoothstep(0.02, 0.18 + noise * 0.18, edges.x))
                    * smoothstep(0.40, 0.70, noise * 0.55 + patches * 0.45);
        float ripple = sin(edges.y * 8.0 - timeSeconds * 3.8 + patches * 4.0);
        float splash = (1.0 - smoothstep(0.1, 2.7, edges.y))
                     * (0.08 + 0.92 * smoothstep(0.35, 0.9, ripple))
                     * smoothstep(0.20, 0.65, noise);
        float foam = clamp(shore * 0.65 + splash * 0.78, 0.0, 0.85);
        water = mix(water, vec3(0.24, 0.43, 0.40) + lit * 0.35, foam);
    } else {
        // Thin falling curtains reveal hints of the cliff through bright,
        // downward-moving strands instead of looking like opaque blue walls.
        vec2 falling = vec2(worldPosition.x + worldPosition.z, worldPosition.y + timeSeconds * 2.4);
        float strands = materialNoise(falling * vec2(5.5, 0.42));
        float spray = smoothstep(0.50, 0.85, strands);
        vec3 curtain = lit + mix(vec3(0.018, 0.055, 0.065), vec3(0.16, 0.25, 0.24), spray);
        water = mix(water, curtain, 0.48 + spray * 0.32);
    }
    return water;
}
)glsl";

} // namespace voxel
