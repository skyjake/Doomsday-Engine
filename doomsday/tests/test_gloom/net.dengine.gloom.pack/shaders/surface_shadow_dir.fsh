#version 330 core

#include "common/material.glsl"

uniform mat4 uLightMatrix;
uniform mat4 uInverseLightMatrix;
uniform vec3 uLightDir;
uniform vec2 uShadowSize;

in vec2 vUV;
in vec3 vTSLightDir; // note: ortho projection, so this is constant
flat in uint vMaterial;

void main(void) {
    if (vMaterial == InvalidIndex) discard;

    float displacementDepth;
    vec3 lightDir = normalize(vTSLightDir);
    vec2 texCoord = Gloom_Parallax(vMaterial, vUV, lightDir, displacementDepth);

    // Write a displaced depth.
    if (displacementDepth > 0.0) {
        float z = gl_FragCoord.z * 2.0 - 1.0;
        vec4 clipSpacePos = vec4(gl_FragCoord.xy / uShadowSize * 2.0 - 1.0, z, 1.0);
        vec4 worldSpacePos = uInverseLightMatrix * clipSpacePos;
        vec3 pos = worldSpacePos.xyz / worldSpacePos.w;
        pos += uLightDir * displacementDepth / abs(dot(Axis_Z, lightDir));
        vec4 adjPos = uLightMatrix * vec4(pos, 1.0);
        gl_FragDepth = 0.5 * adjPos.z / adjPos.w + 0.5;
    }
    else {
        gl_FragDepth = gl_FragCoord.z;
    }
}
