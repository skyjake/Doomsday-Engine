uniform highp mat4 uMvpMatrix;
uniform highp vec2 uViewUnit;
attribute highp vec4 aVertex;
attribute highp vec2 aUV;
attribute highp vec2 aUV2;
attribute highp vec2 aUV3;
varying highp vec2 vUV;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    
    // Position on the axis that passes through the center of the view.
    highp float axisPos = aUV3.s;
    gl_Position.xy *= axisPos;

    gl_Position.xy += aUV2 * uViewUnit * vec2(gl_Position.w);
    vUV = aUV;
}
