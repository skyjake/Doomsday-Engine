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

    // From the back side, don't displace to avoid leaks along edges.
    vec3 lightDir = normalize(vTSLightDir);
    if (lightDir.z < 0.0) {
        gl_FragDepth = gl_FragCoord.z;
        return;
    }

    float displacementDepth;
    Gloom_Parallax(vMaterial, vUV, lightDir, displacementDepth);

    // Write a displaced depth.
    if (displacementDepth > 0.0) {
        // TODO: This is an ortho projection, so the displacement could
        // be scaled to the projected depth range without having to
        // transform back and forth between the coordinate systems.
        float z = gl_FragCoord.z * 2.0 - 1.0;
        vec3 clipSpacePos = vec3(gl_FragCoord.xy / uShadowSize * 2.0 - 1.0, z);
        vec3 pos = mat3(uInverseLightMatrix) * clipSpacePos;
        pos += uLightDir * displacementDepth / abs(dot(Axis_Z, lightDir));
        vec3 adjPos = mat3(uLightMatrix) * pos;
        gl_FragDepth = 0.5 * adjPos.z + 0.5;
    }
    else {
        gl_FragDepth = gl_FragCoord.z;
    }
}
