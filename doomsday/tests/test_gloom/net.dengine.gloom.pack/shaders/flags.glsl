#ifndef GLOOM_FLAGS_H
#define GLOOM_FLAGS_H

const uint Surface_WorldSpaceXZToTexCoords = 0x01u;
const uint Surface_WorldSpaceYToTexCoord   = 0x02u;
const uint Surface_FlipTexCoordY           = 0x04u;
const uint Surface_AnchorTopPlane          = 0x08u;
const uint Surface_TextureOffset           = 0x10u;

#define testFlag(flags, f) (((flags) & (f)) != 0u)

#endif // GLOOM_FLAGS_H
