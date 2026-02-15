uniform vec4 uWeatherIntensities;
uniform vec4 uTimeOfDayColor;
in float vLife;
in float vType;
in float vDist;
in vec2 vVelScreen;
out vec4 vFragmentColor;

void main()
{
    if (vLife <= 0.0) discard;

    // Near-field fade to prevent "pop" or clipping into camera
    float nearFade = smoothstep(1.0, 3.0, vDist);
    if (nearFade <= 0.0) discard;

    float emissive = 1.0 + uTimeOfDayColor.a * 2.0;
    vec2 p = gl_PointCoord * 2.0 - 1.0;

    if (vType < 0.5) {
        // Rain: streak aligned with velocity
        float len = length(vVelScreen);
        vec2 dir = len > 0.0001 ? vVelScreen / len : vec2(0.0, 1.0);
        float distToLine = length(p - dot(p, dir) * dir);

        float streak = 1.0 - smoothstep(0.0, 0.1, distToLine);
        float alongLine = dot(p, dir);
        streak *= smoothstep(1.0, 0.7, abs(alongLine));

        vFragmentColor = vec4(0.7, 0.7, 1.0,
                              streak * 0.4 * uWeatherIntensities.x * nearFade);
    } else {
        // Snow: soft circle
        float dist = length(p);
        float flake = 1.0 - smoothstep(0.4, 1.0, dist);

        vFragmentColor = vec4(1.0, 1.0, 1.1,
                              flake * 0.6 * uWeatherIntensities.y * nearFade);
    }

    // Night visibility boost
    vFragmentColor.rgb *= emissive;
}
