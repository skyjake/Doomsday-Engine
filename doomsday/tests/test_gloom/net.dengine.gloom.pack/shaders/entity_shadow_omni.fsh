#version 330 core

uniform sampler2D uDiffuseAtlas;

uniform vec3  uLightOrigin; // world space
uniform float uFarPlane;

in vec4 vWorldPos;
in vec2 vFaceUV;

void main(void) {
    float alpha = texture(uDiffuseAtlas, vFaceUV).a;
    if (alpha < 0.75) discard;

    // Normalized distance.
    gl_FragDepth = distance(vWorldPos.xyz, uLightOrigin) / uFarPlane;
}
