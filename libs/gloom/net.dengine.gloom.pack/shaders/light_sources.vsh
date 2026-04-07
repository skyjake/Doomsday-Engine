#version 330 core

uniform mat4 uCameraMvpMatrix;
uniform mat4 uModelViewMatrix;
uniform mat3 uWorldToViewRotate;

in vec4  aVertex;
in float aUV;
in vec3  aOrigin;
in vec3  aIntensity;
in vec3  aDirection;
in float aIndex;

flat out vec3  vOrigin;     // view space
flat out vec3  vDirection;  // view space
flat out vec3  vIntensity;
flat out float vRadius;
flat out int   vShadowIndex;

void main(void) {
    vRadius = aUV;
    vShadowIndex = floatBitsToInt(aIndex);

    // Position each instance at its origin.
    gl_Position = uCameraMvpMatrix * vec4(aOrigin + vRadius * aVertex.xyz, 1.0);

    vec4 origin = uModelViewMatrix * vec4(aOrigin, 1.0);
    vOrigin    = origin.xyz / origin.w;
    vDirection = uWorldToViewRotate * aDirection;
    vIntensity = aIntensity;
}
