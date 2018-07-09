#version 330 core

#include "common/material.glsl"

uniform vec3  uLightOrigin; // world space
uniform float uFarPlane;

in vec4 vWorldPos;
in vec2 vFaceUV;

void main(void) {
    float alpha = texture(uTextureAtlas[Texture_Diffuse], vFaceUV).a;
    if (alpha < 0.75) discard;

    // Normalized distance.
    gl_FragDepth = distance(vWorldPos.xyz, uLightOrigin) / uFarPlane;
}
