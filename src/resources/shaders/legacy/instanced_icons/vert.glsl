#version 150 core
// Per-vertex attributes for the base quad
in vec2 a_quad_pos;    // Corresponds to AttributesEnum::ATTR_BASE_QUAD_POSITION
in vec2 a_quad_uv;     // Corresponds to AttributesEnum::ATTR_BASE_QUAD_UV

// Per-instance attributes
in vec3 a_instance_world_pos_center; // Corresponds to AttributesEnum::ATTR_INSTANCE_WORLD_POS_CENTER
in float a_instance_tex_layer;    // Corresponds to AttributesEnum::ATTR_INSTANCE_TEX_LAYER_INDEX

uniform mat4 u_projection_view_matrix;
uniform float u_icon_base_size; // Size of the icon quad in world units

out vec2 v_tex_coord;
out float v_tex_layer;

void main() {
    // Center the quad, then scale it
    vec2 centered_scaled_pos = (a_quad_pos - 0.5) * u_icon_base_size;
    // Position the scaled quad relative to the instance's world center
    vec3 world_pos = a_instance_world_pos_center + vec3(centered_scaled_pos, 0.0);

    gl_Position = u_projection_view_matrix * vec4(world_pos, 1.0);
    v_tex_coord = a_quad_uv;
    v_tex_layer = a_instance_tex_layer;
}
