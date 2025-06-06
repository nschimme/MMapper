// GS_Shader_Version_Placeholder (will be replaced by C++ code)
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

// Input from vertex shader for the 'acolor' (attribute color) case
in vec4 vs_color[]; // Received from vertex shader's 'out vec4 vColor'

// Uniforms
uniform float uLineWidth;       // Thickness in pixels (already scaled by DPI)
// uniform vec2 uViewportSize; // Viewport dimensions (e.g., width, height in pixels)
                                // uViewportSize might not be needed if uLineWidth is pre-scaled to NDC,
                                // or if we pass projection parameters. Let's try without first, assuming uLineWidth is small.
                                // For robust screen-space pixel thickness, perspective divide is important.

// Output to fragment shader for the 'acolor' case
out vec4 gs_color;

void main() {
    // Get vertex positions in Normalized Device Coordinates (NDC)
    vec2 p0_ndc = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
    vec2 p1_ndc = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

    // Calculate screen-space direction vector and its normal
    // For simplicity, using NDC space directly. This assumes orthographic or that perspective distortion on thickness is acceptable.
    vec2 line_dir_ndc = normalize(p1_ndc - p0_ndc);
    vec2 line_normal_ndc = vec2(-line_dir_ndc.y, line_dir_ndc.x);

    // Offset vector for thickness in NDC space.
    // uLineWidth is assumed to be a small factor here.
    // A more robust way would be uLineWidth / uViewportSize.y if uLineWidth is in pixels.
    // Let's assume uLineWidth is already scaled appropriately for NDC for now.
    vec2 offset_ndc = line_normal_ndc * uLineWidth * 0.01; // Multiplying by a small factor, adjust as needed.

    // Emit vertices for the quad (triangle strip)
    // Vertex 1
    gl_Position = vec4((p0_ndc - offset_ndc) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw);
    if (vs_color.length() > 0) gs_color = vs_color[0];
    EmitVertex();

    // Vertex 2
    gl_Position = vec4((p0_ndc + offset_ndc) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw);
    if (vs_color.length() > 0) gs_color = vs_color[0]; // Typically same color for both verts of the start of the line
    EmitVertex();

    // Vertex 3
    gl_Position = vec4((p1_ndc - offset_ndc) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
    if (vs_color.length() > 0) gs_color = vs_color[1];
    EmitVertex();

    // Vertex 4
    gl_Position = vec4((p1_ndc + offset_ndc) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
    if (vs_color.length() > 0) gs_color = vs_color[1]; // Typically same color for both verts of the end of the line
    EmitVertex();

    EndPrimitive();
}
