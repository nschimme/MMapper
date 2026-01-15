uniform mat4 uMVP;
uniform vec4 uViewport;
uniform float uWidth;

in vec3 aFrom;
in vec3 aTo;

noperspective out float isoLinePos;

vec2 toScreenSpace(vec4 v)
{
    vec3 ndc = v.xyz / v.w;
    return (ndc.xy * 0.5 + 0.5) * uViewport.zw;
}

void main()
{
    vec2 p1 = toScreenSpace(uMVP * vec4(aFrom, 1.0));
    vec2 p2 = toScreenSpace(uMVP * vec4(aTo, 1.0));

    vec2 dir = normalize(p2 - p1);
    vec2 normal = vec2(-dir.y, dir.x);

    float halfWidth = uWidth * 0.5;

    vec2 offset = normal * halfWidth;

    vec2 positions[4] = vec2[](
        p1 - offset,
        p1 + offset,
        p2 - offset,
        p2 + offset
    );

    vec2 pos = positions[gl_VertexID];

    // to NDC
    pos = (pos / uViewport.zw) * 2.0 - 1.0;

    gl_Position = vec4(pos, 0.0, 1.0);

    float weights[4] = float[](-1.0, 1.0, -1.0, 1.0);
    isoLinePos = weights[gl_VertexID];
}
