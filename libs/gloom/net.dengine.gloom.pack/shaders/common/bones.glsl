#ifndef GLOOM_BONES_H
#define GLOOM_BONES_H

#include "tangentspace.glsl"

uniform mat4 uBoneMatrices[64];

in vec4 aBoneIDs;
in vec4 aBoneWeights;

mat4 Gloom_BoneMatrix(void) {
    return uBoneMatrices[int(aBoneIDs.x + 0.5)] * aBoneWeights.x +
           uBoneMatrices[int(aBoneIDs.y + 0.5)] * aBoneWeights.y +
           uBoneMatrices[int(aBoneIDs.z + 0.5)] * aBoneWeights.z +
           uBoneMatrices[int(aBoneIDs.w + 0.5)] * aBoneWeights.w;
}

vec4 Gloom_BoneTransform(vec4 vertex, inout TangentSpace ts) {
    mat4 bone = Gloom_BoneMatrix();
    mat3 bone3 = mat3(bone);
    ts.normal    = bone3 * ts.normal;
    ts.tangent   = bone3 * ts.tangent;
    ts.bitangent = bone3 * ts.bitangent;
    return bone * vertex;
}

#endif // GLOOM_BONES_H
