#include "common/gbuffer_in.glsl"

flat DENG_VAR vec3 vOrigin;     // view space
flat DENG_VAR vec3 vDirection;  // view space
flat DENG_VAR vec3 vIntensity;
flat DENG_VAR float vRadius;

void main(void) {
    vec3 pos    = GBuffer_FragViewSpacePos().xyz;
    vec3 normal = GBuffer_FragViewSpaceNormal();
    vec4 albedo = texelFetch(uGBufferAlbedo, ivec2(gl_FragCoord.xy), 0);

    vec3 emit = vec3(0.0);

    float sourceRadius = 0.25;

    float linear = max(sourceRadius, distance(pos, vOrigin));
    float falloff = /*1.0 / pow(linear, 2.0) * */min(1.0, 2.0 - 2.0 * linear / vRadius);
    falloff = clamp(falloff, 0.0, 1.0);

    vec3 lightDir = normalize(pos - vOrigin);
    float diffuse = max(0.0, -dot(normal, lightDir));

    emit = vIntensity * falloff * diffuse;

    emit *= albedo.rgb;

    out_FragColor = vec4(emit, 0.0);
}
