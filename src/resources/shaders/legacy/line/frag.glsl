#if IS_GLES
#version 300 es
#else
#version 330 core
#endif

#if IS_GLES
precision mediump float;
#endif

in vec4 v_color;
in float v_dist_ratio; // Comes in as -1 or 1

out vec4 frag_color;

void main() {
    // The anti-aliasing factor is calculated based on the screen-space gradient
    // of the distance from the line's center. fwidth gives us this.
    float fw = fwidth(v_dist_ratio);

    // Calculate alpha based on the distance from the line's centerline.
    // The smoothstep function creates a nice fade at the edges.
    float alpha_edge = 1.0 - smoothstep(1.0 - fw, 1.0, abs(v_dist_ratio));

    frag_color = vec4(v_color.rgb, v_color.a * alpha_edge);
}
