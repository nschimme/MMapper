uniform float uWeatherIntensity;
uniform vec4 uTimeOfDayColor;
in float vLife;
in float vType;
out vec4 vFragmentColor;

void main()
{
    if (vLife <= 0.0)
        discard;

    // Boost visibility during dark hours
    float darkBoost = 1.0 + uTimeOfDayColor.a * 2.0;

    vec4 color;
    if (vType < 0.5) {
        // Rain: bluish/white thin streaks (points here)
        color = vec4(0.8, 0.85, 1.0, 0.5 * uWeatherIntensity * darkBoost);
    } else {
        // Snow: white fluffy points
        color = vec4(1.0, 1.0, 1.0, 0.8 * uWeatherIntensity * darkBoost);
    }

    // Fade out based on life
    float alpha = color.a * clamp(vLife * 2.0, 0.0, 1.0);
    vFragmentColor = vec4(color.rgb, alpha);
}
