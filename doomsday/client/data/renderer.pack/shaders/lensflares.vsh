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
    vUV = aUV;
    vColor = aColor;
    
    gl_Position = uMvpMatrix * aVertex;
    
    // Is this occluded in the depth buffer?
    highp vec2 depthUv = gl_Position.xy / gl_Position.w / 2.0 + vec2(0.5, 0.5);
    float depth = texture2D(uDepthBuf, depthUv).r;
/*    if(depth < (gl_Position.z/gl_Position.w + 1.0)/2.0) {
        // Occluded!
        vUV = aUV;
        vColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;        
    }    */

    // Position on the axis that passes through the center of the view.
    highp float axisPos = aUV3.s;
    gl_Position.xy *= axisPos;

    gl_Position.xy += aUV2 * uViewUnit * vec2(gl_Position.w);
        
    /*
    // Testing
    vColor = vec4(1.0, 1.0, 1.0, 1.0);
    vUV = (aUV2 + 1.0) / 2.0;
    gl_Position.xy = aUV2;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
    */
        
    if(depth < (gl_Position.z/gl_Position.w + 1.0)/2.0) {
        // Occluded!
        vColor = vec4(1.0, 0.0, 0.0, 1.0);
    }    
    
}
