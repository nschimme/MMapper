uniform mat4 uMVP;
uniform vec4 uViewport;
uniform float uWidth;

in vec3 aFrom;
in vec3 aTo;

void main()
{
    vec4 p1 = uMVP * vec4(aFrom, 1.0);
    vec4 p2 = uMVP * vec4(aTo, 1.0);

    vec2 p1_screenspace = (p1.xy / p1.w) * uViewport.zw;
    vec2 p2_screenspace = (p2.xy / p2.w) * uViewport.zw;

    vec2 dir = normalize(p2_screenspace - p1_screenspace);
    vec2 normal = vec2(-dir.y, dir.x);

    float halfWidth = uWidth * 0.5;
    vec2 offset = normal * halfWidth;

    if (gl_VertexID == 0) {
        gl_Position = p1 + vec4(offset * 2.0 / uViewport.zw * p1.w, 0.0, 0.0);
    } else if (gl_VertexID == 1) {
        gl_Position = p1 - vec4(offset * 2.0 / uViewport.zw * p1.w, 0.0, 0.0);
    } else if (gl_VertexID == 2) {
        gl_Position = p2 + vec4(offset * 2.0 / uViewport.zw * p2.w, 0.0, 0.0);
    } else {
        gl_Position = p2 - vec4(offset * 2.0 / uViewport.zw * p2.w, 0.0, 0.0);
    }
}
