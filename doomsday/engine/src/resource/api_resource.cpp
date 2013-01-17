#define DENG_NO_API_MACROS_RESOURCE
#include "api_resource.h"

// animgroups.cpp
DENG_EXTERN_C int R_CreateAnimGroup(int flags);
DENG_EXTERN_C void R_AddAnimGroupFrame(int animGroupNum, Uri const *texture, int tics, int randomTics);

// colorpalettes.cpp
DENG_EXTERN_C colorpaletteid_t R_CreateColorPalette(char const *fmt, char const *name, uint8_t const *colorData, int colorCount);
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id);
DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name);
DENG_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean correctGamma);
DENG_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean correctGamma);

// r_data.cpp
DENG_EXTERN_C patchid_t R_DeclarePatch(char const *name);
DENG_EXTERN_C boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info);
DENG_EXTERN_C struct uri_s *R_ComposePatchUri(patchid_t id);
DENG_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id);

// textures.cpp
DENG_EXTERN_C int Textures_UniqueId2(Uri const *uri, boolean quiet);
DENG_EXTERN_C int Textures_UniqueId(Uri const *uri/*, quiet = false */);

DENG_DECLARE_API(R) =
{
    { DE_API_RESOURCE },
    R_DeclarePatch,
    R_GetPatchInfo,
    R_ComposePatchUri,
    R_ComposePatchPath,
    R_CreateAnimGroup,
    R_AddAnimGroupFrame,
    R_CreateColorPalette,
    R_GetColorPaletteNumForName,
    R_GetColorPaletteNameForNum,
    R_GetColorPaletteRGBf,
    R_GetColorPaletteRGBubv,
    Textures_UniqueId,
    Textures_UniqueId2,
};

