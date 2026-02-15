// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform float uTime;
uniform vec4 uWeatherIntensities; // x: rain, y: snow, z: clouds, w: fog
uniform vec4 uTimeOfDayColor;
uniform vec3 uPlayerPos;
uniform float uZScale;
uniform sampler2D uNoiseTexture;
uniform sampler2D uDepthTexture;

in vec3 vNearPos;
in vec3 vFarPos;
out vec4 vFragmentColor;

vec3 intersect(float targetZ)
{
    float diff = vFarPos.z - vNearPos.z;
    if (abs(diff) < 0.0001) return vNearPos;
    float t = (targetZ - vNearPos.z) / diff;
    return mix(vNearPos, vFarPos, t);
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

void main()
{
    // Reconstruct world position from depth
    float depth = texture(uDepthTexture, gl_FragCoord.xy / vec2(textureSize(uDepthTexture, 0))).r;
    vec3 worldPos = mix(vNearPos, vFarPos, depth);

    // Fallback if no depth (clear color area)
    if (depth >= 1.0) worldPos = vFarPos;

    float distToPlayer = distance(worldPos.xy, uPlayerPos.xy);
    float weatherMask = smoothstep(15.0, 10.0, distToPlayer);

    vec4 weatherAccum = vec4(0.0);

    // 1. Fog (Ground Level)
    if (uWeatherIntensities.w > 0.01) {
        float groundZ = uPlayerPos.z * uZScale;
        float fogFactor = smoothstep(groundZ + 5.0, groundZ - 2.0, worldPos.z);
        vec2 uv = worldPos.xy * 0.1 + uTime * 0.05;
        float n = texture(uNoiseTexture, uv * 0.1).r;
        float fogAlpha = uWeatherIntensities.w * n * fogFactor * 0.6 * weatherMask;
        weatherAccum = mix(weatherAccum, vec4(0.8, 0.8, 0.9, 1.0), fogAlpha);
    }

    // 2. Volumetric Rain Sheets
    if (uWeatherIntensities.x > 0.01) {
        // We simulate 3 layers of rain sheets at different scales
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            float scale = 0.5 + fi * 0.5;
            // Project world XZ/YZ to create vertical sheets
            vec2 uv = worldPos.xz * 0.2 * scale + vec2(uTime * 0.1, uTime * 3.0 + worldPos.y * 0.01);
            float rainN = texture(uNoiseTexture, uv * 0.05).g;
            float streak = smoothstep(0.7, 0.9, rainN);

            float alpha = streak * uWeatherIntensities.x * (0.3 / (fi + 1.0)) * weatherMask;
            weatherAccum += vec4(0.6, 0.6, 1.0, alpha);
        }

        // Ground Splashes
        float splashProb = hash(worldPos.xy * 10.0 + floor(uTime * 20.0));
        if (splashProb > 0.98) {
            float splash = smoothstep(0.1, 0.0, abs(fract(uTime * 10.0 + hash(worldPos.xy)) - 0.5));
            weatherAccum += vec4(0.8, 0.8, 1.0, splash * uWeatherIntensities.x * 0.4 * weatherMask);
        }
    }

    // 3. Volumetric Snow
    if (uWeatherIntensities.y > 0.01) {
        vec2 uv = worldPos.xy * 0.2 + vec2(sin(uTime + worldPos.z), uTime * 0.5 + worldPos.z * 0.1);
        float snowN = texture(uNoiseTexture, uv * 0.1).r;
        float flake = smoothstep(0.6, 0.8, snowN);
        weatherAccum += vec4(0.9, 0.9, 1.0, flake * uWeatherIntensities.y * 0.5 * weatherMask);
    }

    // 4. Clouds (High altitude)
    if (uWeatherIntensities.z > 0.01) {
        float skyZ = (uPlayerPos.z + 30.0) * uZScale;
        if (worldPos.z < skyZ) {
             vec3 skyPos = intersect(skyZ);
             vec2 uv = skyPos.xy * 0.05 - uTime * 0.02;
             float cloudN = texture(uNoiseTexture, uv).g;
             float puffy = smoothstep(0.5, 0.8, cloudN);
             float skyMask = smoothstep(20.0, 15.0, distance(skyPos.xy, uPlayerPos.xy));
             weatherAccum = mix(weatherAccum, vec4(1.0, 1.0, 1.0, 1.0), puffy * uWeatherIntensities.z * 0.3 * skyMask);
        }
    }

    // Final composition
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    // Base ToD
    vFragmentColor = vec4(todTint, todAlpha);

    // Add weather with night-visibility boost
    float emissive = 0.2 * todAlpha; // More emissive at night
    vFragmentColor.rgb += weatherAccum.rgb * weatherAccum.a * (1.0 + emissive * 3.0);
    vFragmentColor.a = max(vFragmentColor.a, weatherAccum.a * 0.5);
}
