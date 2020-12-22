#version 330 core

#include "common/gbuffer_in.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"
#include "common/ambient.glsl"
#include "common/dir_lights.glsl"
#include "common/omni_lights.glsl"
#include "common/time.glsl"

uniform mat4      uProjMatrix;
uniform sampler2D uRefractedFrame;
uniform mat3      uWorldToViewRotate;

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

    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // Depth at fragment.
    vec3 backPos;

    MaterialSampler matSamp = Gloom_Sampler(matIndex, Texture_NormalDisplacement);

    // Parallax mapping.
    float displacementDepth;
    vec3 viewDir  = normalize(vTSViewDir);
    vec2 texCoord = Gloom_SamplerParallax(matSamp, vUV, viewDir, displacementDepth);
    vec3 normal   = normalize(GBuffer_UnpackNormal(Gloom_SampleMaterial(matSamp, texCoord)));

    mat3 tsToViewRotate = uWorldToViewRotate * Gloom_TangentMatrix(ts);
    vec3 vsNormal = tsToViewRotate * normal;

    vec4 diffuse   = Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord);
    float density  = diffuse.a;
    vec3 emissive  = Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb;
    vec4 specGloss = Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord);
    vec3 vsViewDir = -normalize(vVSPos.xyz);

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

    float viewDistance = length(vsPos);
    float directVolDist = distance(vsPos, GBuffer_ViewSpacePos(vUV).xyz);

    // Refraction. This is calculated in view space so the offset will be view-dependent.
    vec4  refractedColor;
    float reflectRatio;
    vec3  refracted = refract(vsViewDir, vsNormal, 1.05);
    if (refracted != vec3(0.0)) {
        reflectRatio = max(0.0, dot(refracted, vsViewDir));
        float refrFactor = 0.05 / max(1.0, viewDistance * 0.5) *
            min(directVolDist * 4.0, 1.0);
        vec2 fragPos = (gl_FragCoord.xy + refracted.xy * uViewportSize.y * refrFactor) / uViewportSize;
        refractedColor = texture(uRefractedFrame, fragPos) * (1.0 - reflectRatio);
        backPos = GBuffer_ViewSpacePos(fragPos).xyz;

        // How far does light travel through the volume?
        // if (backPos.z < vsPos.z) {
        float volDist = distance(backPos, vsPos);
        float volumetric = pow(density, 2.0) * 5.0 * log(volDist + 1.0);
        if (density > 0.0) {
            refractedColor = mix(refractedColor, diffuse,
                min(volumetric /* * density + pow(density, 2.0)*/, 1.0));
        }
        // }
        // else {
        //     refractedColor = vec4(0.0);
        // }
    }
    else {
        refractedColor = vec4(0.0);
        reflectRatio = 1.0;
    }

    // Now we can apply lighting to the surface.
    SurfacePoint sp = SurfacePoint(vsPos, vsNormal, diffuse.rgb * diffuse.a, specGloss);

    out_FragColor = vec4(emissive, 0.0) + refractedColor;
    out_FragColor.rgb +=
        Gloom_DirectionalLighting(sp) +
        Gloom_OmniLighting(sp) +
        reflectRatio * Gloom_CubeReflection(uEnvMap, sp);
}
