#ifndef GLOOM_FLAGS_H
#define GLOOM_FLAGS_H

const uint Surface_WorldSpaceXZToTexCoords = 1u;
const uint Surface_WorldSpaceYToTexCoord   = 2u;
const uint Surface_FlipTexCoordY           = 4u;
const uint Surface_AnchorTopPlane          = 8u;

#define testFlag(flags, f) (((flags) & (f)) != 0u)

#endif // GLOOM_FLAGS_H
