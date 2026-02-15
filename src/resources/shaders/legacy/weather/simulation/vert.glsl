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

    if (life <= 0.0 || distance(pos.xy, uPlayerPos.xy) > 40.0 || pos.z < uPlayerPos.z - 5.0) {
        float h = hash(float(gl_VertexID) * 1.234 + life + uDeltaTime);
        pos.x = uPlayerPos.x + (hash(h) * 80.0 - 40.0);
        pos.y = uPlayerPos.y + (hash(h + 1.0) * 80.0 - 40.0);
        pos.z = uPlayerPos.z + 20.0 + hash(h + 2.0) * 10.0;

        // Randomly assign type based on intensity
        float totalIntensity = uWeatherIntensities.x + uWeatherIntensities.y;
        if (totalIntensity > 0.01) {
             float pRain = uWeatherIntensities.x / totalIntensity;
             type = step(pRain, hash(h + 3.0)); // 0 for rain, 1 for snow
        } else {
             life = -1.0; // Stay dead
        }

        float speed = mix(25.0, 8.0, type);
        vel = vec3((hash(h + 4.0) - 0.5) * 2.0, (hash(h + 5.0) - 0.5) * 2.0, -speed - hash(h + 6.0) * 5.0);
        life = 2.0 + hash(h + 7.0) * 2.0;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
