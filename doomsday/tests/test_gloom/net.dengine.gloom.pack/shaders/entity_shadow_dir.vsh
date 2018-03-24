#version 330 core

#include "common/bones.glsl"

uniform mat4 uLightMatrix;

in mat4 aInstanceMatrix;
in vec4 aInstanceColor;
in vec4 aVertex;
in vec2 aUV;
in vec4 aBounds0;

out vec2 vUV;

void main(void) {
    mat4 bone = Gloom_BoneMatrix();
    gl_Position = uLightMatrix * (aInstanceMatrix * (bone * aVertex));
    vUV = aBounds0.xy + aUV * aBounds0.zw;
}
