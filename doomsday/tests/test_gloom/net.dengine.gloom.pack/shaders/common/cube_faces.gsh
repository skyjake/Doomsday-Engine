#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uCubeFaceMatrices[6];

out vec4 vWorldPos;

void main()
{
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i) {
            vWorldPos = gl_in[i].gl_Position;
            gl_Position = uCubeFaceMatrices[face] * vWorldPos;
            /*
            float dp = dot(surface.normal, uLightDir);
            if (dp > 0.0) {
                // Apply a slight bias to backfaces to avoid peter panning.
                //gl_Position.z *= 1.0 + 0.1*(1.0 - dp);
                gl_Position.z -= 0.004;
            }
            */
            EmitVertex();
        }
        EndPrimitive();
    }
}
