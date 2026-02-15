uniform float uDeltaTime;
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
    // Simulate in screen space [-1.1, 1.1]
    vec3 pos = inPos + inVel * uDeltaTime;
    vec3 vel = inVel;
    float life = inLife - uDeltaTime;
    float type = inType;

    // Bounds check in screen space
    if (life <= 0.0 || pos.y < -1.1 || pos.x < -1.1 || pos.x > 1.1) {
        float h = hash(float(gl_VertexID) * 1.234 + life + uDeltaTime);
        pos.x = hash(h) * 2.2 - 1.1;
        pos.y = 1.1; // Spawn at top
        pos.z = hash(h + 1.0); // Depth purely for variation

        float totalIntensity = uWeatherIntensities.x + uWeatherIntensities.y;
        if (totalIntensity > 0.01) {
             float pRain = uWeatherIntensities.x / totalIntensity;
             type = step(pRain, hash(h + 2.0)); // 0 for rain, 1 for snow
        } else {
             life = -1.0;
        }

        // Screen space velocities
        if (type < 0.5) {
            vel = vec3(0.0, -2.0 - hash(h + 3.0) * 1.0, 0.0); // Falling fast
            life = 2.0;
        } else {
            vel = vec3((hash(h + 3.0) - 0.5) * 0.2, -0.4 - hash(h + 4.0) * 0.2, 0.0); // Falling slow
            life = 10.0;
        }
    }

    // Snow swaying
    if (type > 0.5) {
        pos.x += sin(life * 2.0 + float(gl_VertexID)) * 0.002;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
