#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uCubeFaceMatrices[6];

in      vec2 gmUV[];
in      vec3 gmTSLightDir[];
flat in uint gmMaterial[];

out vec4 vWorldPos;
out vec2 vUV;
out vec3 vTSLightDir;
flat out uint vMaterial;

void main()
{
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i) {
            vWorldPos = gl_in[i].gl_Position;
            gl_Position = uCubeFaceMatrices[face] * vWorldPos;
            vUV         = gmUV[i];
            vTSLightDir = gmTSLightDir[i];
            vMaterial   = gmMaterial[i];
            EmitVertex();
        }
        EndPrimitive();
    }
}
