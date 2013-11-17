uniform highp mat4 uMvpMatrix;
uniform highp vec2 uViewUnit;
attribute highp vec4 aVertex;
attribute highp vec2 aUV;
attribute highp vec2 aUV2;
varying highp vec2 vUV;

void main(void) {
    gl_Position = (uMvpMatrix * aVertex) + vec4(aUV2 * uViewUnit, 0, 0);
    vUV = aUV;
}
