uniform mat4 uViewProj;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inVel;
layout(location = 2) in float inLife;
layout(location = 3) in float inType;

out float vLife;
out float vType;

void main()
{
    vLife = inLife;
    vType = inType;
    gl_Position = uViewProj * vec4(inPos, 1.0);
    gl_PointSize = mix(2.0, 4.0, inType); // Snow is bigger
}
