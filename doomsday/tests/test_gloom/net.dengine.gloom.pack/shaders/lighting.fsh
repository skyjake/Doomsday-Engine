#include "common/gbuffer_in.glsl"

flat DENG_VAR vec3 vOrigin;     // view space
flat DENG_VAR vec3 vDirection;  // view space
flat DENG_VAR vec3 vIntensity;
flat DENG_VAR float vRadius;

void main(void) {
    vec3 pos    = GBuffer_FragViewSpacePos().xyz;
    vec3 normal = GBuffer_FragViewSpaceNormal();
    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);
    float sourceRadius = 1.0;

    vec3 emit = vec3(0.0);

    float linear = max(sourceRadius, distance(pos, vOrigin));
    float edgeFalloff = clamp(2.0 - 2.0 * linear / vRadius, 0.0, 1.0);
    float falloff = 1.0 / pow(linear, 2.0) * edgeFalloff;
    falloff = clamp(falloff, 0.0, 1.0);

    vec3 lightDir   = normalize(vOrigin - pos);
    vec3 viewDir    = normalize(-pos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // Blinn-Phong
    float shininess = 1024.0;
    vec4 specGloss = vec4(0.2);
    float spec = abs(pow(max(dot(normal, halfwayDir), 0.0), shininess));
    vec3 specular = specGloss.rgb * vIntensity * spec * edgeFalloff;

    float diffuse = max(0.0, dot(normal, lightDir));

    emit = (vIntensity * falloff * diffuse * albedo.rgb) + specular;

    out_FragColor = vec4(emit, 0.0);
}
