uniform float uDeltaTime;
uniform vec3 uPlayerPos;
uniform vec4 uWeatherIntensities; // x: rain, y: snow

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inVel;
layout(location = 2) in float inLife;
layout(location = 3) in float inType;

out vec3 outPos;
out vec3 outVel;
out float outLife;
out float outType;

float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec3 pos = inPos + inVel * uDeltaTime;
    vec3 vel = inVel;
    float life = inLife - uDeltaTime;
    float type = inType;

    // Bounds for respawning
    if (life <= 0.0 || distance(pos.xy, uPlayerPos.xy) > 50.0 || pos.z < -10.0 || pos.z > 120.0) {
        float h = hash(float(gl_VertexID) * 1.234 + life + uDeltaTime);
        pos.x = uPlayerPos.x + (hash(h) * 100.0 - 50.0);
        pos.y = uPlayerPos.y + (hash(h + 1.0) * 100.0 - 50.0);
        pos.z = 90.0 + hash(h + 2.0) * 20.0;

        float totalIntensity = uWeatherIntensities.x + uWeatherIntensities.y;
        if (totalIntensity > 0.01) {
             float pRain = uWeatherIntensities.x / totalIntensity;
             type = step(pRain, hash(h + 3.0)); // 0 for rain, 1 for snow
        } else {
             life = -1.0;
        }

        // Rain is fast and vertical, snow is slow and wafting
        if (type < 0.5) {
            vel = vec3(0.0, 0.0, -40.0 - hash(h + 4.0) * 10.0);
            life = 3.0 + hash(h + 5.0) * 1.0;
        } else {
            vel = vec3((hash(h + 4.0) - 0.5) * 4.0, (hash(h + 5.0) - 0.5) * 4.0, -4.0 - hash(h + 6.0) * 2.0);
            life = 15.0 + hash(h + 7.0) * 5.0;
        }
    }

    // Snow swaying
    if (type > 0.5) {
        pos.x += sin(pos.z * 0.15 + life * 0.5) * 0.08;
        pos.y += cos(pos.z * 0.12 + life * 0.4) * 0.08;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
