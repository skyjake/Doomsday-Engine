#version 330 core

in vec4 vWorldPos; // from GS

uniform vec3  uLightOrigin; // world space
uniform float uFarPlane;

void main(void) {
    // Normalized distance.
    gl_FragDepth = distance(vWorldPos.xyz, uLightOrigin) / uFarPlane;
}
