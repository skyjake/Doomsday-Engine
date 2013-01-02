#ifndef DOOMSDAY_API_MAP_EDIT_H
#define DOOMSDAY_API_MAP_EDIT_H

#include <de/str.h>
#include <de/types.h>
#include "api_base.h"

/// @defgroup mapEdit Map Editor
/// @ingroup map
///@{

DENG_API_TYPEDEF(MPE)
{
    de_api_t api;

    boolean         (*Begin)(const char* mapUri);
    boolean         (*End)(void);

    uint            (*VertexCreate)(coord_t x, coord_t y);
    boolean         (*VertexCreatev)(size_t num, coord_t* values, uint* indices);
    uint            (*SidedefCreate)(short flags, const ddstring_t* topMaterial, float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue, const ddstring_t* middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen, float middleBlue, float middleAlpha, const ddstring_t* bottomMaterial, float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
    uint            (*LinedefCreate)(uint v1, uint v2, uint frontSector, uint backSector, uint frontSide, uint backSide, int flags);
    uint            (*SectorCreate)(float lightlevel, float red, float green, float blue);
    uint            (*PlaneCreate)(uint sector, coord_t height, const ddstring_t* materialUri, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ);
    uint            (*PolyobjCreate)(uint* lines, uint linecount, int tag, int sequenceType, coord_t originX, coord_t originY);
    boolean         (*GameObjProperty)(const char* objName, uint idx, const char* propName, valuetype_t type, void* data);
}
DENG_API_T(MPE);

#ifndef DENG_NO_API_MACROS_MAP_EDIT
#define MPE_Begin           _api_MPE.Begin
#define MPE_End             _api_MPE.End
#define MPE_VertexCreate    _api_MPE.VertexCreate
#define MPE_VertexCreatev   _api_MPE.VertexCreatev
#define MPE_SidedefCreate   _api_MPE.SidedefCreate
#define MPE_LinedefCreate   _api_MPE.LinedefCreate
#define MPE_SectorCreate    _api_MPE.SectorCreate
#define MPE_PlaneCreate     _api_MPE.PlaneCreate
#define MPE_PolyobjCreate   _api_MPE.PolyobjCreate
#define MPE_GameObjProperty _api_MPE.GameObjProperty
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(MPE);
#endif

///@}

#endif // DOOMSDAY_API_MAP_EDIT_H
