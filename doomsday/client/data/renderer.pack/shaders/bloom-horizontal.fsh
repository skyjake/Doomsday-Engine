uniform sampler2D uTex;
uniform highp float uIntensity;
uniform highp float uThreshold;
uniform highp vec2 uBlurStep;

varying highp vec2 vUV;

void main(void) {
    highp vec4 sum = vec4(0.0);
    sum += texture2D(uTex, vec2(vUV.s - 4.0 * uBlurStep.s, vUV.t)) * 0.05;
    sum += texture2D(uTex, vec2(vUV.s - 3.0 * uBlurStep.s, vUV.t)) * 0.09;
    sum += texture2D(uTex, vec2(vUV.s - 2.0 * uBlurStep.s, vUV.t)) * 0.123;
    sum += texture2D(uTex, vec2(vUV.s - uBlurStep.s,       vUV.t)) * 0.154;
    sum += texture2D(uTex, vUV)                                    * 0.165;
    sum += texture2D(uTex, vec2(vUV.s + uBlurStep.s,       vUV.t)) * 0.154;
    sum += texture2D(uTex, vec2(vUV.s + 2.0 * uBlurStep.s, vUV.t)) * 0.123;
    sum += texture2D(uTex, vec2(vUV.s + 3.0 * uBlurStep.s, vUV.t)) * 0.09;
    sum += texture2D(uTex, vec2(vUV.s + 4.0 * uBlurStep.s, vUV.t)) * 0.05;

    // Apply a threshold that gets rid of dark, non-luminous pixels.
    highp float intens = max(sum.r, max(sum.g, sum.b));
                        
    intens = (intens - uThreshold) * uIntensity;

    gl_FragColor = vec4(vec3(intens) * normalize(sum.rgb + 0.2), 1.0);
}