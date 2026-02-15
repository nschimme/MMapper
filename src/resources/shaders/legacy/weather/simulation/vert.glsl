uniform float uDeltaTime;
uniform float uTime;
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

float hash11(float p)
{
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main()
{
    // Simulate in world space
    vec3 pos = inPos + inVel * uDeltaTime;
    vec3 vel = inVel;
    float life = inLife - uDeltaTime;
    float type = inType;

    // Bounds check in world space centered on player
    if (life <= 0.0 || distance(pos.xy, uPlayerPos.xy) > 15.0 || pos.z < uPlayerPos.z - 15.0
        || pos.z > uPlayerPos.z + 30.0) {
        float h = hash11(float(gl_VertexID) * 0.123 + uTime * 0.01);

        float totalIntensity = uWeatherIntensities.x + uWeatherIntensities.y;
        float spawnProb = hash11(h + 0.543);

        if (spawnProb < totalIntensity) {
            pos.x = uPlayerPos.x + (hash11(h + 0.1) * 30.0 - 15.0);
            pos.y = uPlayerPos.y + (hash11(h + 0.2) * 30.0 - 15.0);
            pos.z = uPlayerPos.z + 15.0 + hash11(h + 0.3) * 15.0;

            float pRain = uWeatherIntensities.x / max(0.001, totalIntensity);
            type = step(pRain, hash11(h + 0.4)); // 0 for rain, 1 for snow

            // World space velocities (rooms/sec)
            if (type < 0.5) {
                // Rain: Significant horizontal wind component for 2D slant
                vel = vec3(22.0 + (hash11(h + 0.5) - 0.5) * 8.0,
                           15.0 + (hash11(h + 0.6) - 0.5) * 6.0,
                           -40.0 - hash11(h + 0.7) * 10.0);
                life = 2.0;
            } else {
                // Snow: Drifting
                vel = vec3((hash11(h + 0.5) - 0.5) * 10.0,
                           (hash11(h + 0.6) - 0.5) * 10.0,
                           -4.0 - hash11(h + 0.7) * 4.0);
                life = 12.0;
            }
        } else {
            life = -1.0; // Inactive
            pos = uPlayerPos; // Keep near player for floating point stability
            vel = vec3(0.0);
        }
    }

    // Snow swaying
    if (type > 0.5 && life > 0.0) {
        pos.x += sin(uTime * 1.5 + pos.z * 0.5) * 0.02;
        pos.y += cos(uTime * 1.2 + pos.x * 0.5) * 0.02;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
