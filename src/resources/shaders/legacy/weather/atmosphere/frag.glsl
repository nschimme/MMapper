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

    // Do not show weather on geometry below the player's layer
    float zDiff = worldPos.z - uPlayerPos.z;
    if (zDiff < -0.1) {
        localMask *= smoothstep(-1.0, -0.1, zDiff);
    }

    vec4 weatherAccum = vec4(0.0);
    float darkBoost = uTimeOfDayColor.a * 1.5;

    // Fog: based on actual world position depth
    if (uFogIntensity > 0.01) {
        vec2 uv = worldPos.xy * 0.15 + uTime * 0.1;
        float n = texture(uNoiseTexture, uv * 0.05).r;
        vec4 fogColor = vec4(0.8 + darkBoost, 0.8 + darkBoost, 0.85 + darkBoost, uFogIntensity * n * localMask * 0.7);
        weatherAccum = fogColor;
    }

    // Clouds: puffy high-contrast noise (G channel) on player plane
    if (uCloudsIntensity > 0.01) {
        vec2 uv = playerPlanePos.xy * 0.06 - uTime * 0.03;
        float n = texture(uNoiseTexture, uv * 0.05).g;
        float puffy = smoothstep(0.45, 0.65, n);
        vec4 clouds = vec4(0.95 + darkBoost, 0.95 + darkBoost, 1.0 + darkBoost, uCloudsIntensity * puffy * localMask * 0.8);

        // Blend clouds into fog
        weatherAccum.rgb = mix(weatherAccum.rgb, clouds.rgb, clouds.a);
        weatherAccum.a = max(weatherAccum.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    // Sequential blending math (Overlaid):
    // Standard alpha blending result of (background blended with TOD) then (result blended with Weather)
    float outAlpha = todAlpha + weatherAccum.a - todAlpha * weatherAccum.a;
    vec3 outColor;
    if (outAlpha > 0.001) {
        outColor = (todTint * todAlpha * (1.0 - weatherAccum.a) + weatherAccum.rgb * weatherAccum.a) / outAlpha;
    } else {
        outColor = todTint;
    }

    vFragmentColor = vec4(outColor, outAlpha);
}
