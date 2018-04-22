#version 330 core

#include "common/material.glsl"

in vec2 vUV;

void main(void) {
    float alpha = texture(uTextureAtlas[Texture_Diffuse], vUV).a;
    if (alpha < 0.75) discard;
}
