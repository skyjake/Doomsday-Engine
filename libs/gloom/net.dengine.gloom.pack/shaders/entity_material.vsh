#version 330 core

#include "common/defs.glsl"
#include "common/bones.glsl"

uniform mat4 uCameraMvpMatrix;

in mat4 aInstanceMatrix; // model to world
in vec4 aInstanceColor;

in vec4 aVertex;
in vec3 aNormal;
in vec3 aTangent;
in vec3 aBitangent;
in vec2 aUV;
in vec4 aBounds0; // diffuse
in vec4 aBounds1; // normals
in vec4 aBounds2; // specular
in vec4 aBounds3; // emissive

out vec3 vWSNormal;
out vec3 vWSTangent;
out vec3 vWSBitangent;
out vec2 vUV;
flat out ivec4 vGotTexture;
flat out vec4 vInstanceColor;
flat out vec4 vTexBounds[4];

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
