#ifndef GLOOM_MIPLEVEL_H
#define GLOOM_MIPLEVEL_H

// Determines mipmap level.
float mipLevel(vec2 uv, vec2 texSize) {
    vec2 dx = dFdx(uv * texSize.x);
    vec2 dy = dFdy(uv * texSize.y);
    float d = max(dot(dx, dx), dot(dy, dy));
    return 0.5 * log2(d);
}

#endif // GLOOM_MIPLEVEL_H
