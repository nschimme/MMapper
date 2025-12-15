in vec3 vFrom;
in vec3 vTo;
in vec4 vColor;
in float vT; // 0.0 at the start of the line, 1.0 at the end

out vec4 vFragmentColor;

uniform bool u_connectionFading;

const float GEOMETRIC_EPSILON = 1e-4;
const float FAINT_CONNECTION_ALPHA = 0.1;

bool isCrossingZAxis(vec3 p1, vec3 p2)
{
    return abs(p1.z - p2.z) > GEOMETRIC_EPSILON;
}

const float LONG_LINE_HALFLEN = 1.5;
const float LONG_LINE_LEN = 2.0 * LONG_LINE_HALFLEN;

bool isLongLine(vec3 a, vec3 b)
{
    return length(a - b) >= LONG_LINE_LEN;
}

void main()
{
    vec4 color = vColor;

    if (u_connectionFading) {
        if (isCrossingZAxis(vFrom, vTo)) {
            color.a *= FAINT_CONNECTION_ALPHA;
        } else if (isLongLine(vFrom, vTo)) {
            float len = length(vTo - vFrom);
            float faintCutoff = (len > 1e-6) ? (LONG_LINE_HALFLEN / len) : 0.5;
            if (vT > faintCutoff && vT < (1.0 - faintCutoff)) {
                color.a *= FAINT_CONNECTION_ALPHA;
            }
        }
    }

    vFragmentColor = color;
}
