#version 330 core

#include "common/gbuffer.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"
#include "common/lightmodel.glsl"

uniform mat4  uProjMatrix;
uniform float uCurrentTime;

     in vec2  vUV;
     in vec4  vVSPos;
     in vec3  vTSViewDir;
     in vec3  vWSTangent;
     in vec3  vWSBitangent;
     in vec3  vWSNormal;
flat in uint  vMaterial;
flat in uint  vFlags;

void main(void) {
    uint matIndex = vMaterial;

    // One-sided surfaces may lack one of the materials.
    if (matIndex == InvalidIndex) discard;

    // Normal mapping.
    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // Water offsets.
    vec2 waterOff[2] = vec2[2](
        vec2(0.182, -0.3195)  * uCurrentTime,
        vec2(-0.203, 0.01423) * uCurrentTime
    );

    // Parallax mapping.
    // TODO: Better to sample from two textures at the lower level, not here...
    float displacementDepths[2];
    vec3 viewDir  = normalize(vTSViewDir);
    vec2 texCoord = Gloom_Parallax(matIndex, vUV + waterOff[0], viewDir, displacementDepths[0]);
    vec2 texCoord2 = Gloom_Parallax(matIndex, vUV + waterOff[1], viewDir, displacementDepths[1]);

    vec3 normal = GBuffer_UnpackNormal(
        Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoord));
    vec3 normal2 = GBuffer_UnpackNormal(
        Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoord2));

    float displacementDepth = (displacementDepths[0] + displacementDepths[1]) / 2.0;

    normal = normalize(normal + normal2);

    vec4 diffuse   = Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord);
    vec3 emissive  = Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb;
    vec4 specGloss = Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord);

    out_FragColor = vec4(normal, 0.5);

    vec3 vsPos = vVSPos.xyz / vVSPos.w;

    // Write a displaced depth.  // TODO: Add a routine for doing this.
    if (displacementDepth > 0.0) {
        vsPos += normalize(vsPos) * displacementDepth / abs(dot(Axis_Z, viewDir));
        vec4 dispPos = uProjMatrix * vec4(vsPos, 1.0);
        float ndc = dispPos.z / dispPos.w;
        gl_FragDepth = 0.5 * ndc + 0.5;
    }
    else {
        gl_FragDepth = gl_FragCoord.z;
    }

    // Now we can apply lighting to the surface.
    SurfacePoint sp = SurfacePoint(vsPos, normal, diffuse.rgb, specGloss);


}
