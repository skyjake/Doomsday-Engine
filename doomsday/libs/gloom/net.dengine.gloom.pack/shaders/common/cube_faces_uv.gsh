#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uCubeFaceMatrices[6];

in  vec2 vUV[];
out vec4 vWorldPos;
out vec2 vFaceUV;

void main()
{
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i) {
            vWorldPos = gl_in[i].gl_Position;
            vFaceUV = vUV[i];
            gl_Position = uCubeFaceMatrices[face] * vWorldPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
