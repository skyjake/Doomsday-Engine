uniform sampler2D uTex;
varying highp vec2 vUV;

void main(void) {
    gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    /*
    highp vec4 original = texture2D(uTex, vUV);
    highp float intens = 
        (0.2125 * original.r) + 
        (0.7154 * original.g) + 
        (0.0721 * original.b);
    gl_FragColor = vec4(vec3(intens), 1.0);
    if(uFadeInOut < 1.0) {
        gl_FragColor = mix(original, gl_FragColor, uFadeInOut);
    }
    */
}
