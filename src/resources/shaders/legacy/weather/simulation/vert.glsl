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
    // Simulate in world space
    vec3 pos = inPos + inVel * uDeltaTime;
    vec3 vel = inVel;
    float life = inLife - uDeltaTime;
    float type = inType;

    // Bounds check in world space centered on player
    // Spawning closer to the player's plane to match the "radius" feel
    // Increased vertical span slightly for better depth in 3D
    if (life <= 0.0 || distance(pos.xy, uPlayerPos.xy) > 12.0 || pos.z < uPlayerPos.z - 15.0
        || pos.z > uPlayerPos.z + 25.0) {
        float h = hash(float(gl_VertexID) * 1.234 + life + uDeltaTime);
        pos.x = uPlayerPos.x + (hash(h) * 24.0 - 12.0);
        pos.y = uPlayerPos.y + (hash(h + 1.0) * 24.0 - 12.0);
        pos.z = uPlayerPos.z + 10.0 + hash(h + 2.0) * 15.0;

        float totalIntensity = uWeatherIntensities.x + uWeatherIntensities.y;
        if (totalIntensity > 0.01) {
             float pRain = uWeatherIntensities.x / totalIntensity;
             type = step(pRain, hash(h + 3.0)); // 0 for rain, 1 for snow
        } else {
             life = -1.0;
        }

        // World space velocities (rooms/sec)
        if (type < 0.5) {
            // Rain: Increased horizontal wind component to ensure slant in 2D top-down mode
            vel = vec3(25.0 + (hash(h + 4.0) - 0.5) * 10.0,
                       18.0 + (hash(h + 5.0) - 0.5) * 8.0,
                       -45.0 - hash(h + 6.0) * 15.0);
            life = 1.5; // Fast fall
        } else {
            // Snow: drifting more horizontally and slower fall
            vel = vec3((hash(h + 4.0) - 0.5) * 12.0,
                       (hash(h + 5.0) - 0.5) * 12.0,
                       -5.0 - hash(h + 6.0) * 5.0);
            life = 10.0;
        }
    }

    // Snow swaying
    if (type > 0.5) {
        pos.x += sin(pos.z * 0.2 + life) * 0.05;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
