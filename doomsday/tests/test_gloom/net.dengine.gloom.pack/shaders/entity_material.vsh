#version 330 core

#include "common/bones.glsl"

uniform mat4 uMvpMatrix;

DENG_ATTRIB mat4 aInstanceMatrix;
DENG_ATTRIB vec4 aInstanceColor;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec3 aNormal;
DENG_ATTRIB vec2 aUV;
DENG_ATTRIB vec4 aBounds0;

DENG_VAR vec2 vUV;
DENG_VAR vec4 vInstanceColor;
DENG_VAR vec3 vNormal;

void main(void) {
    vNormal = aNormal;
    vec4 modelPos = Gloom_BoneTransform(aVertex, vNormal);
    gl_Position = uMvpMatrix * (aInstanceMatrix * modelPos);
    vNormal = (aInstanceMatrix * vec4(vNormal, 0.0)).xyz;
    vUV = aBounds0.xy + aUV * aBounds0.zw;
    vInstanceColor = aInstanceColor;
}
