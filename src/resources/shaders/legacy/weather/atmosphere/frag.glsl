// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uInvViewProj;
uniform vec3 uPlayerPos;
uniform float uZScale;
uniform ivec4 uPhysViewport;
uniform float uTime;
uniform float uCloudsIntensity;
uniform float uFogIntensity;
uniform vec4 uTimeOfDayColor;
uniform sampler2D uNoiseTexture;
uniform sampler2D uDepthTexture;

in vec3 vNearPos;
in vec3 vFarPos;

out vec4 vFragmentColor;

void main()
{
    vec2 screenUV = gl_FragCoord.xy / vec2(uPhysViewport.zw);
    float depth = texture(uDepthTexture, screenUV).r;

    // Reconstruct world position of the room pixel
    vec2 screenPos = screenUV * 2.0 - 1.0;
    vec4 clipPos = vec4(screenPos, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos4 = uInvViewProj * clipPos;
    vec3 worldPos = worldPos4.xyz / worldPos4.w;
    worldPos.z /= uZScale;

    // Intersection with player plane for clouds/fog drift context
    float t_player = (uPlayerPos.z * uZScale - vNearPos.z) / (vFarPos.z - vNearPos.z);
    vec3 playerPlanePos = mix(vNearPos, vFarPos, t_player);
    playerPlanePos.z /= uZScale;

    float distToPlayer = distance(worldPos.xy, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 8.0, distToPlayer);

    vec4 weatherAccum = vec4(0.0);

    // Fog: based on actual world position depth
    if (uFogIntensity > 0.01) {
        vec2 uv = worldPos.xy * 0.15 + uTime * 0.1;
        float n = texture(uNoiseTexture, uv * 0.05).r;
        vec4 fogColor = vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * 0.6);
        weatherAccum = fogColor;
    }

    // Clouds: puffy high-contrast noise (G channel) on player plane
    if (uCloudsIntensity > 0.01) {
        vec2 uv = playerPlanePos.xy * 0.06 - uTime * 0.03;
        float n = texture(uNoiseTexture, uv * 0.05).g;
        float puffy = smoothstep(0.45, 0.65, n);
        vec4 clouds = vec4(0.95, 0.95, 1.0, uCloudsIntensity * puffy * localMask * 0.8);
        weatherAccum.rgb = mix(weatherAccum.rgb, clouds.rgb, clouds.a);
        weatherAccum.a = max(weatherAccum.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    vec4 result = vec4(todTint, todAlpha);

    // Blend weather on top of ToD
    if (weatherAccum.a > 0.0) {
        result.rgb = mix(result.rgb, weatherAccum.rgb, weatherAccum.a);
        result.a = max(result.a, weatherAccum.a);
    }

    vFragmentColor = result;
}
