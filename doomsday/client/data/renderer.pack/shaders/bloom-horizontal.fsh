uniform sampler2D uTex;
uniform highp float uIntensity;
uniform highp float uThreshold;
uniform highp vec2 uBlurStep;

varying highp vec2 vUV;

void main(void) 
{
    highp vec4 sum = vec4(0.0);
    
    sum += texture2D(uTex, vec2(vUV.s - 7.0 * uBlurStep.s, vUV.t)) * 0.0249;
    sum += texture2D(uTex, vec2(vUV.s - 6.0 * uBlurStep.s, vUV.t)) * 0.0367;
    sum += texture2D(uTex, vec2(vUV.s - 5.0 * uBlurStep.s, vUV.t)) * 0.0498;
    sum += texture2D(uTex, vec2(vUV.s - 4.0 * uBlurStep.s, vUV.t)) * 0.0660;
    sum += texture2D(uTex, vec2(vUV.s - 3.0 * uBlurStep.s, vUV.t)) * 0.0803;
    sum += texture2D(uTex, vec2(vUV.s - 2.0 * uBlurStep.s, vUV.t)) * 0.0915;
    sum += texture2D(uTex, vec2(vUV.s - uBlurStep.s,       vUV.t)) * 0.0996;
    sum += texture2D(uTex, vUV)                                    * 0.1027;
    sum += texture2D(uTex, vec2(vUV.s + uBlurStep.s,       vUV.t)) * 0.0996;
    sum += texture2D(uTex, vec2(vUV.s + 2.0 * uBlurStep.s, vUV.t)) * 0.0915;
    sum += texture2D(uTex, vec2(vUV.s + 3.0 * uBlurStep.s, vUV.t)) * 0.0803;
    sum += texture2D(uTex, vec2(vUV.s + 4.0 * uBlurStep.s, vUV.t)) * 0.0660;
    sum += texture2D(uTex, vec2(vUV.s + 5.0 * uBlurStep.s, vUV.t)) * 0.0498;
    sum += texture2D(uTex, vec2(vUV.s + 6.0 * uBlurStep.s, vUV.t)) * 0.0367;
    sum += texture2D(uTex, vec2(vUV.s + 7.0 * uBlurStep.s, vUV.t)) * 0.0249;

    // Apply a threshold that gets rid of dark, non-luminous pixels.
    highp float intens = max(sum.r, max(sum.g, sum.b));
                        
    intens = (intens - uThreshold) * uIntensity;

    gl_FragColor = vec4(vec3(intens) * normalize(sum.rgb + 0.2), 1.0);
}