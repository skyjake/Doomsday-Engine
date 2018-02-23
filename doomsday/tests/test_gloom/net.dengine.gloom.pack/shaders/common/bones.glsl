#ifndef GLOOM_BONES_H
#define GLOOM_BONES_H

uniform mat4 uBoneMatrices[64];

DENG_ATTRIB vec4 aBoneIDs;
DENG_ATTRIB vec4 aBoneWeights;

mat4 Gloom_BoneMatrix(void) {
    return uBoneMatrices[int(aBoneIDs.x + 0.5)] * aBoneWeights.x +
           uBoneMatrices[int(aBoneIDs.y + 0.5)] * aBoneWeights.y +
           uBoneMatrices[int(aBoneIDs.z + 0.5)] * aBoneWeights.z +
           uBoneMatrices[int(aBoneIDs.w + 0.5)] * aBoneWeights.w;
}

vec4 Gloom_BoneTransform(vec4 vertex, inout vec3 normal) {
    mat4 bone = Gloom_BoneMatrix();
    normal = (bone * vec4(normal, 0.0)).xyz;
    return bone * vertex;
}

#endif // GLOOM_BONES_H
