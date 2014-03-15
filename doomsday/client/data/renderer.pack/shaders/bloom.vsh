uniform highp mat4 uMvpMatrix;
uniform highp vec4 uColor;
uniform highp vec4 uWindow;

attribute highp vec4 aVertex;
attribute highp vec2 aUV;

varying highp vec2 vUV;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = uWindow.xy + aUV * uWindow.zw;
}
