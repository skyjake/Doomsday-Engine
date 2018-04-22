#ifndef GLOOM_DATABUFFER_H
#define GLOOM_DATABUFFER_H

#define samplerData sampler2D

vec4 dataFetch2D(samplerData buf, ivec2 loc2D) {
    return texelFetch(buf, loc2D, 0);
}

vec4 dataFetch(samplerData buf, int loc) {
    int rowLen = textureSize(buf, 0).x;
    return dataFetch2D(buf, ivec2(loc % rowLen, loc / rowLen));
}

vec4 dataElementFetch(samplerData buf, int element, int member) {
    return dataFetch2D(buf, ivec2(member, element));
}

#endif // GLOOM_DATABUFFER_H
