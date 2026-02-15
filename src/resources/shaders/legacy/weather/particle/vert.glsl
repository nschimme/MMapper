uniform mat4 uViewProj;
uniform float uWeatherIntensity;

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

    // Project velocity to screen space for rain orientation
    // We use a small delta to see where it goes
    vec4 clipPosNext = uViewProj * vec4(inPos + inVel * 0.02, 1.0);
    vVelScreen = (clipPosNext.xy / clipPosNext.w) - (clipPos.xy / clipPos.w);

    if (inType < 0.5) {
        gl_PointSize = 20.0; // Long rain streak area
    } else {
        gl_PointSize = 8.0;  // Snow flake area
    }
}
