#version 330 core

in vec4 vModelPos; // from GS

uniform vec3  uLightOrigin;
uniform float uFarPlane;

void main(void) {
    // Normalized distance.
    gl_FragDepth = distance(vModelPos.xyz, uLightOrigin) / uFarPlane;
}
