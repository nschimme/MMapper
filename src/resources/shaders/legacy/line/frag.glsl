#version 330 core
#ifdef GL_ES
precision highp float;
#endif

// The interpolated vertex color from the vertex shader.
in vec4 color_out;
in float side_out;

// The final output color.
out vec4 frag_color;

void main()
{
    float distance = abs(side_out);
    float width = fwidth(distance);
    float alpha = smoothstep(1.0, 1.0 - width, distance);

    frag_color = vec4(color_out.rgb, color_out.a * alpha);
}
