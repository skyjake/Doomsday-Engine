uniform highp mat4 uMvpMatrix;
uniform highp vec4 uColor;
uniform highp vec4 uWindow;

attribute highp vec4 aVertex;
attribute highp vec2 aUV;
attribute highp vec4 aColor;

varying highp vec2 vUV;
varying highp vec4 vColor;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = uWindow.xy + aUV * uWindow.zw;
    vColor = aColor * uColor;
}
