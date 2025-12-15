uniform vec4 uColor;
uniform float uStipple;

noperspective in float isoLinePos;

out vec4 vFragmentColor;

void main()
{
    if (uStipple > 0.0) {
        if (mod(gl_FragCoord.x, uStipple) < uStipple * 0.5) {
            discard;
        }
    }

    float x = 1.0 - abs(isoLinePos);
    x = smoothstep(0.0, 1.0, x);
    vFragmentColor = vec4(uColor.rgb, uColor.a * x);
}
