#version 330 core

uniform sampler2D uDiffuseAtlas;

DENG_VAR vec2 vUV;

void main(void) {
    float alpha = texture(uDiffuseAtlas, vUV).a;
    if (alpha < 0.75) discard;
}
