uniform vec4 uColor;

in vec4 vColor;
in vec2 vUv;
in float vLineLength;
in float vWidth;

out vec4 vFragmentColor;

void main()
{
    float halfWidth = vWidth * 0.5;

    // Calculate distance to the segment
    float dx = max(-vUv.x, 0.0) + max(vUv.x - vLineLength, 0.0);
    float dy = vUv.y;
    float dist = sqrt(dx*dx + dy*dy);

    // Anti-aliasing
    // Smoothstep creates a nice alpha ramp over 1 pixel
    float feather = 1.0;
    float alpha = 1.0 - smoothstep(halfWidth - feather, halfWidth + feather, dist);

    if (alpha <= 0.0) {
        discard;
    }

    vFragmentColor = vColor * uColor;
    vFragmentColor.a *= alpha;
}
