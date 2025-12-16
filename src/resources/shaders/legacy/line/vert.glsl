uniform mat4 uMVP;
uniform float uWidth;

in vec3 aFrom;
in vec3 aTo;
in vec4 aColor;

out vec3 vFrom;
out vec3 vTo;
out vec4 vColor;
out float vT;

void main()
{
    vFrom = aFrom;
    vTo = aTo;
    vColor = aColor;

    // vT is used by the fragment shader to know how far along the line
    // a fragment is, to fade the middle part of long lines.
    // The quad is rendered as a triangle strip with 4 vertices.
    // The first two vertices are at the start of the line (vT = 0.0),
    // the next two are at the end (vT = 1.0).
    if (gl_VertexID == 0 || gl_VertexID == 1) {
        vT = 0.0;
    } else {
        vT = 1.0;
    }

    // Calculate the line quad's geometry in world space.
    vec3 dir = aTo - aFrom;
    vec2 dir_xy = dir.xy;

    // Handle pure Z-axis lines to avoid division by zero.
    if (length(dir_xy) < 0.0001) {
        dir_xy = vec2(1.0, 0.0); // Arbitrary direction on XY plane
    } else {
        dir_xy = normalize(dir_xy);
    }

    // The normal is on the XY plane, creating a flat "ribbon".
    vec2 normal_xy = vec2(-dir_xy.y, dir_xy.x);

    float halfWidth = uWidth * 0.5;
    vec3 offset = vec3(normal_xy * halfWidth, 0.0);

    vec3 worldPos;
    if (gl_VertexID == 0) {        // Bottom-left vertex
        worldPos = aFrom - offset;
    } else if (gl_VertexID == 1) { // Top-left vertex
        worldPos = aFrom + offset;
    } else if (gl_VertexID == 2) { // Bottom-right vertex
        worldPos = aTo - offset;
    } else {                       // Top-right vertex
        worldPos = aTo + offset;
    }

    gl_Position = uMVP * vec4(worldPos, 1.0);
}
