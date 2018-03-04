#ifndef GLOOM_FLAGS_H
#define GLOOM_FLAGS_H

const float Pi = 3.141592653589793;

const uint Surface_WorldSpaceXZToTexCoords = 0x01u;
const uint Surface_WorldSpaceYToTexCoord   = 0x02u;
const uint Surface_FlipTexCoordY           = 0x04u;
const uint Surface_AnchorTopPlane          = 0x08u;
const uint Surface_TextureOffset           = 0x10u;

const int Texture_Diffuse            = 0; // RGB: Diffuse  | A: Opacity
const int Texture_SpecularGloss      = 1; // RGB: Specular | A: Gloss
const int Texture_Emissive           = 2; // RGB: Emissive
const int Texture_NormalDisplacement = 3; // RGB: Normal   | A: Displacement

const float Material_MaxReflectionBias = 5.0;
const float Material_MaxReflectionBlur = 10.0;

#define testFlag(flags, f) (((flags) & (f)) != 0u)

#endif // GLOOM_FLAGS_H
