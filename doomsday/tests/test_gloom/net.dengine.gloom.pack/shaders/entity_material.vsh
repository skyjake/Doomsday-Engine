#version 330 core

#include "common/defs.glsl"
#include "common/bones.glsl"

uniform mat4 uCameraMvpMatrix;

DENG_ATTRIB mat4 aInstanceMatrix; // model to world
DENG_ATTRIB vec4 aInstanceColor;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec3 aNormal;
DENG_ATTRIB vec3 aTangent;
DENG_ATTRIB vec3 aBitangent;
DENG_ATTRIB vec2 aUV;
DENG_ATTRIB vec4 aBounds0; // diffuse
DENG_ATTRIB vec4 aBounds1; // normals
DENG_ATTRIB vec4 aBounds2; // specular
DENG_ATTRIB vec4 aBounds3; // emissive

DENG_VAR vec3 vWSNormal;
DENG_VAR vec3 vWSTangent;
DENG_VAR vec3 vWSBitangent;
DENG_VAR vec2 vUV;
flat DENG_VAR ivec4 vGotTexture;
flat DENG_VAR vec4 vInstanceColor;
flat DENG_VAR vec4 vTexBounds[4];

void main(void) {
    TangentSpace ts = TangentSpace(aTangent, aBitangent, aNormal);
    vec4 modelPos = Gloom_BoneTransform(aVertex, ts);

    gl_Position = uCameraMvpMatrix * (aInstanceMatrix * modelPos);

    mat3 modelToWorld = mat3(aInstanceMatrix);
    vWSNormal    = modelToWorld * ts.normal;
    vWSTangent   = modelToWorld * ts.tangent;
    vWSBitangent = modelToWorld * ts.bitangent;

    vUV = aUV;
    vTexBounds[Texture_Diffuse] = aBounds0;
    vTexBounds[Texture_NormalDisplacement] = aBounds1;
    vTexBounds[Texture_SpecularGloss] = aBounds2;
    vTexBounds[Texture_Emissive] = aBounds3;
    vGotTexture.x = (aBounds0.xy != aBounds0.zw? 1 : 0);
    vGotTexture.y = (aBounds1.xy != aBounds1.zw? 1 : 0);
    vGotTexture.z = (aBounds2.xy != aBounds2.zw? 1 : 0);
    vGotTexture.w = (aBounds3.xy != aBounds3.zw? 1 : 0);
    vInstanceColor = aInstanceColor;
}
