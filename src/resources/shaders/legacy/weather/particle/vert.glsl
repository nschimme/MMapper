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

    // Position is already in screen space [-1, 1]
    gl_Position = vec4(inPos.xy, 0.0, 1.0);

    // Velocity in screen space for orientation
    vVelScreen = inVel.xy;

    if (inType < 0.5) {
        gl_PointSize = 24.0; // Rain streak area
    } else {
        gl_PointSize = 8.0;  // Snow flake area
    }
}
