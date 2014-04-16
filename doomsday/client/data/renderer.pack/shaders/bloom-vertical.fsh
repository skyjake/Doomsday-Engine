uniform sampler2D uTex;
uniform highp vec2 uBlurStep;

varying highp vec2 vUV;

void main(void) 
{
    highp vec4 sum = vec4(0.0);
    
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 7.0 * uBlurStep.t)) * 0.0249;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 6.0 * uBlurStep.t)) * 0.0367;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 5.0 * uBlurStep.t)) * 0.0498;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 4.0 * uBlurStep.t)) * 0.0660;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 3.0 * uBlurStep.t)) * 0.0803;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - 2.0 * uBlurStep.t)) * 0.0915;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t - uBlurStep.t      )) * 0.0996;
    sum += texture2D(uTex, vUV)                                    * 0.1027;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + uBlurStep.t      )) * 0.0996;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 2.0 * uBlurStep.t)) * 0.0915;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 3.0 * uBlurStep.t)) * 0.0803;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 4.0 * uBlurStep.t)) * 0.0660;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 5.0 * uBlurStep.t)) * 0.0498;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 6.0 * uBlurStep.t)) * 0.0367;
    sum += texture2D(uTex, vec2(vUV.s, vUV.t + 7.0 * uBlurStep.t)) * 0.0249;

    gl_FragColor = sum;
}