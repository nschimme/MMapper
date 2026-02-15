uniform float uWeatherIntensity;
uniform vec4 uWeatherIntensities;
uniform vec4 uTimeOfDayColor;
in float vLife;
in float vType;
in vec2 vVelScreen;
out vec4 vFragmentColor;

void main()
{
    if (vLife <= 0.0) discard;

    float darkBoost = 1.0 + uTimeOfDayColor.a * 1.5;
    vec2 p = gl_PointCoord * 2.0 - 1.0;

    if (vType < 0.5) {
        // Rain: streak aligned with velocity
        float len = length(vVelScreen);
        vec2 dir = len > 0.0001 ? vVelScreen / len : vec2(0.0, 1.0);
        float distToLine = length(p - dot(p, dir) * dir);

        // Elongated streak: very thin horizontally relative to the 'dir'
        float streak = 1.0 - smoothstep(0.0, 0.1, distToLine);

        // Fade ends of the streak area (gl_PointCoord is square, we want a line)
        float alongLine = dot(p, dir);
        streak *= smoothstep(1.0, 0.7, abs(alongLine));

        // Original-like rain color: vec4(0.6, 0.6, 1.0, 0.6)
        vFragmentColor = vec4(0.6, 0.6, 1.0,
                              streak * 0.8 * uWeatherIntensity * uWeatherIntensities.x * darkBoost);
    } else {
        // Snow: soft circle
        float dist = length(p);
        // Original-like flake: 1.0 - smoothstep(0.1, 0.2, dist) but adjusted for PointSize
        float flake = 1.0 - smoothstep(0.3, 0.8, dist);

        // Original-like snow color: vec4(1.0, 1.0, 1.1, 0.8)
        vFragmentColor = vec4(1.0, 1.0, 1.1,
                              flake * 0.9 * uWeatherIntensity * uWeatherIntensities.y * darkBoost);
    }
}
