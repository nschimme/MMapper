uniform mat4 uViewProj;
uniform vec3 uPlayerPos;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inVel;
layout(location = 2) in float inLife;
layout(location = 3) in float inType;

out float vLife;
out float vType;
out vec2 vVelScreen;

void main()
{
    vLife = inLife;
    vType = inType;

    vec4 clipPos = uViewProj * vec4(inPos, 1.0);
    gl_Position = clipPos;

    // Project velocity to screen space using player position as reference point
    // This ensures all particles share the same orientation (parallel streaks)
    // rather than converging to a vanishing point (faucet effect).
    // Using a large world-space multiplier (100.0) ensures stability when zoomed out.
    vec4 clipRef = uViewProj * vec4(uPlayerPos, 1.0);
    vec4 clipNext = uViewProj * vec4(uPlayerPos + inVel * 100.0, 1.0);
    vVelScreen = (clipNext.xy / clipNext.w) - (clipRef.xy / clipRef.w);

    if (inType < 0.5) {
        gl_PointSize = 48.0; // Rain streak area - larger for longer streaks
    } else {
        gl_PointSize = 14.0; // Snow flake area
    }
}
