#version 330 core

uniform sampler2D uTex;

DENG_VAR vec2 vUV;

void main(void) {
    float alpha = texture(uTex, vUV).a;
    if (alpha < 0.75) discard;
}
