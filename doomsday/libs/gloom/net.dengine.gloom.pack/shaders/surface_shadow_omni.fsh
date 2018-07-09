#version 330 core

#include "common/material.glsl"

// Incoming from geometry shader:
in vec4 vWorldPos;
in vec2 vUV;
in vec3 vTSLightDir;
flat in uint vMaterial;

uniform vec3  uLightOrigin; // world space
uniform float uFarPlane;

void main(void) {
    if (vMaterial == InvalidIndex) discard;
    vec3 worldPos = vWorldPos.xyz;

    /* Material displacement. */ {
        float displacementDepth;
        vec3 lightDir = normalize(vTSLightDir);
        Gloom_Parallax(vMaterial, vUV, lightDir, displacementDepth);
        worldPos += normalize(worldPos - uLightOrigin) * displacementDepth
            / abs(dot(Axis_Z, lightDir));
    }

    // Normalized distance.
    gl_FragDepth = distance(worldPos, uLightOrigin) / uFarPlane;
}
