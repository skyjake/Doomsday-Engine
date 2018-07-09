#ifndef GLOOM_FLAGS_H
#define GLOOM_FLAGS_H

const float Pi = 3.141592653589793;

const vec3 Axis_X = vec3(1.0, 0.0, 0.0);
const vec3 Axis_Y = vec3(0.0, 1.0, 0.0);
const vec3 Axis_Z = vec3(0.0, 0.0, 1.0);

const uint InvalidIndex = 0xffffffffu;

const uint Surface_WorldSpaceXZToTexCoords = 0x01u;
const uint Surface_WorldSpaceYToTexCoord   = 0x02u;
const uint Surface_FlipTexCoordY           = 0x04u;
const uint Surface_AnchorTopPlane          = 0x08u;
const uint Surface_TextureOffset           = 0x10u;
const uint Surface_LeftEdge                = 0x20u;
const uint Surface_RightEdge               = 0x40u;

const int Texture_Diffuse            = 0; // RGB: Diffuse  | A: Opacity
const int Texture_SpecularGloss      = 1; // RGB: Specular | A: Gloss
const int Texture_Emissive           = 2; // RGB: Emissive
const int Texture_NormalDisplacement = 3; // RGB: Normal   | A: Displacement

const float Material_MaxReflectionBias = 5.0;
const float Material_MaxReflectionBlur = 10.0;

const uint Metrics_AnimationMask  = 1u;
const uint Metrics_VerticalAspect = 2u;

const int Bloom_Horizontal = 0;
const int Bloom_Vertical   = 1;

#define testFlag(flags, f) (((flags) & (f)) != 0u)

#endif // GLOOM_FLAGS_H
