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
    // Simulation Box (Toroidal Volume)
    vec3 boxHalfSize = vec3(10.0, 10.0, 15.0);
    vec3 boxSize = boxHalfSize * 2.0;

    // If life is negative, it's a new or reset particle
    vec3 pos = inPos;
    vec3 vel = inVel;
    float life = inLife;
    float type = inType;

    if (life <= 0.0) {
        float h = hash11(float(gl_VertexID) * 0.123 + uTime);
        pos = uPlayerPos + (vec3(hash11(h), hash11(h+0.1), hash11(h+0.2)) - 0.5) * boxSize;
        life = 10.0 + hash11(h+0.3) * 10.0;
        type = step(0.5, hash11(h+0.4));

        if (type < 0.5) { // Rain
            vel = vec3(15.0, 10.0, -40.0);
        } else { // Snow
            vel = vec3(2.0, 1.0, -5.0);
        }
    }

    // Movement
    pos += vel * uDeltaTime;

    // Toroidal Wrap relative to player
    vec3 relPos = pos - uPlayerPos;
    relPos = mod(relPos + boxHalfSize, boxSize) - boxHalfSize;
    pos = relPos + uPlayerPos;

    // Continuous swaying for snow
    if (type > 0.5) {
        pos.x += sin(uTime * 1.5 + pos.z * 0.5) * 0.02;
        pos.y += cos(uTime * 1.2 + pos.x * 0.5) * 0.02;
    }

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
