#version 330 core

#include "common/bones.glsl"

in mat4 aInstanceMatrix;
in vec4 aInstanceColor;
in vec4 aVertex;
in vec2 aUV;
in vec4 aBounds0;

out vec2 vUV;

void main(void) {
    mat4 bone = Gloom_BoneMatrix();
    gl_Position = aInstanceMatrix * (bone * aVertex);
    vUV = aBounds0.xy + aUV * aBounds0.zw;
}
