#version 330 core

#include "common/bones.glsl"

uniform mat4 uCameraMvpMatrix;

DENG_ATTRIB mat4 aInstanceMatrix;
DENG_ATTRIB vec4 aInstanceColor;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec3 aNormal;
DENG_ATTRIB vec2 aUV;
DENG_ATTRIB vec4 aBounds0;
DENG_ATTRIB vec4 aBounds1;
DENG_ATTRIB vec4 aBounds2;
DENG_ATTRIB vec4 aBounds3;

DENG_VAR vec2 vUV[4];
DENG_VAR vec4 vInstanceColor;
DENG_VAR vec3 vNormal;

void main(void) {
    vNormal = aNormal;
    vec4 modelPos = Gloom_BoneTransform(aVertex, vNormal);
    gl_Position = uCameraMvpMatrix * (aInstanceMatrix * modelPos);
    vNormal = (aInstanceMatrix * vec4(vNormal, 0.0)).xyz;
    vUV[0] = aBounds0.xy + aUV * aBounds0.zw;
    vUV[1] = aBounds1.xy + aUV * aBounds1.zw;
    vUV[2] = aBounds2.xy + aUV * aBounds2.zw;
    vUV[3] = aBounds3.xy + aUV * aBounds3.zw;
    vInstanceColor = aInstanceColor;
}
