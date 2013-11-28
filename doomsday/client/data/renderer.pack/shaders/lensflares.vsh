uniform highp mat4 uMvpMatrix;
uniform highp vec2 uViewUnit;
uniform sampler2D uDepthBuf;

attribute highp vec4 aVertex;
attribute highp vec4 aColor;
attribute highp vec2 aUV;
attribute highp vec2 aUV2;
attribute highp vec2 aUV3;

varying highp vec4 vColor;
varying highp vec2 vUV;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    
    // Position on the axis that passes through the center of the view.
    highp float axisPos = aUV3.s;
    gl_Position.xy *= axisPos;
    
    // Is this occluded in the depth buffer?
    highp vec2 depthUv = gl_Position.xy / gl_Position.w / 2.0 + vec2(0.5, 0.5);
    float depth = texture2D(uDepthBuf, depthUv).r;
/*    if(depth < (gl_Position.z/gl_Position.w + 1.0)/2.0) {
        // Occluded!
        vUV = aUV;
        vColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;        
    }    */

    gl_Position.xy += aUV2 * uViewUnit * vec2(gl_Position.w);

    vUV = aUV;
    vColor = aColor;
    
    if(depth < (gl_Position.z/gl_Position.w + 1)/2) {
        // Occluded!
        vColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;        
    }    
    
}
