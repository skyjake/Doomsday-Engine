#version 330 core

#include "common/bones.glsl"

uniform mat4 uLightMatrix;

DENG_ATTRIB mat4 aInstanceMatrix;
DENG_ATTRIB vec4 aInstanceColor;
DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec2 aUV;
DENG_ATTRIB vec4 aBounds0;

DENG_VAR vec2 vUV;

void main(void) {
    mat4 bone = Gloom_BoneMatrix();
    gl_Position = uLightMatrix * (aInstanceMatrix * (bone * aVertex));
    vUV = aBounds0.xy + aUV * aBounds0.zw;
}
