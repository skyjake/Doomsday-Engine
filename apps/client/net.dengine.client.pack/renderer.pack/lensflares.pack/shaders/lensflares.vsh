uniform mat4 uMvpMatrix;
uniform vec4 uActiveRect; // (x-scale, y-scale, x-offset, y-offset)
uniform vec2 uViewUnit;
uniform vec2 uPixelAsUv;
uniform sampler2D uDepthBuf;

in vec4 aVertex;
in vec4 aColor;
in vec2 aUV0;
in vec2 aUV1;
in vec2 aUV2;

out vec4 vColor;
out vec2 vUV;

bool isOccluded(vec2 uv) {
    // Apply a possible viewport transformation.
    uv = uv * uActiveRect.xy + uActiveRect.zw;

    float depth = texture(uDepthBuf, uv).r;
    return (depth < (gl_Position.z/gl_Position.w + 1.0) / 2.0);
}

float occlusionLevel() {
    vec2 depthUv = gl_Position.xy / gl_Position.w / 2.0 + vec2(0.5, 0.5);

    float occ = 0.0;
    if (!isOccluded(depthUv)) occ += 0.2;
    if (!isOccluded(depthUv + vec2(uPixelAsUv.x * 4.0, uPixelAsUv.y))) occ += 0.2;
    if (!isOccluded(depthUv - vec2(uPixelAsUv.x * 4.0, uPixelAsUv.y))) occ += 0.2;
    if (!isOccluded(depthUv + vec2(uPixelAsUv.x, uPixelAsUv.y * 4.0))) occ += 0.2;
    if (!isOccluded(depthUv - vec2(uPixelAsUv.x, uPixelAsUv.y * 4.0))) occ += 0.2;
    return occ;
}

void main(void) {
    vUV = aUV0;
    vColor = aColor;

    gl_Position = uMvpMatrix * aVertex;

    // Is the origin occluded in the depth buffer?
    float ocl = occlusionLevel();
    if (ocl <= 0.0) {
        // Occluded! Make it invisibile and leave the quad zero-sized.
        vUV = aUV0;
        vColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    else {
        vColor.a *= ocl;
    }

    // Position on the axis that passes through the center of the view.
    float axisPos = aUV2.s;
    gl_Position.xy *= axisPos;

    // Position the quad corners.
    gl_Position.xy += aUV1 * uViewUnit * vec2(gl_Position.w);
}
