uniform highp mat4 uMvpMatrix;
uniform highp vec2 uViewUnit;
attribute highp vec4 aVertex;
attribute highp vec2 aUV;
attribute highp vec2 aUV2;
varying highp vec2 vUV;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    gl_Position.xy += aUV2 * uViewUnit * vec2(gl_Position.w);
    vUV = aUV;
}
