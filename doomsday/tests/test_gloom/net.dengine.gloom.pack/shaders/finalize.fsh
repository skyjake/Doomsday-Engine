#include "common/gbuffer_in.glsl"

uniform sampler2D uSSAOBuf;
uniform sampler2D uShadowMap;
uniform int uDebugMode;

DENG_VAR vec2 vUV;

void main(void) {
    if (uDebugMode == 0) {
        out_FragColor =
            texture(uGBufferAlbedo, vUV) *
            texture(uSSAOBuf, vUV).r;
    }
    else if (uDebugMode == 1) {
        out_FragColor = texture(uGBufferAlbedo, vUV);
    }
    else if (uDebugMode == 2) {
        out_FragColor = texture(uGBufferNormal, vUV);
    }
    else if (uDebugMode == 3) {
        out_FragColor = GBuffer_FragViewSpacePos();
    }
    else if (uDebugMode == 4) {
        out_FragColor = vec4(vec3(texture(uSSAOBuf, vUV).r), 1.0);
    }
    else if (uDebugMode == 5) {
        out_FragColor = vec4(vec3(texture(uShadowMap, vUV).s), 1.0);
    }
}
