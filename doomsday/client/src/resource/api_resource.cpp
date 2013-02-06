#define DENG_NO_API_MACROS_RESOURCE
#include "api_resource.h"
#include "dd_main.h"
#include "Textures"

#undef Textures_UniqueId2
DENG_EXTERN_C int Textures_UniqueId2(Uri const *_uri, boolean quiet)
{
    LOG_AS("Textures_UniqueId");
    if(!_uri) return -1;
    de::Uri const &uri = reinterpret_cast<de::Uri const &>(*_uri);

    try
    {
        return App_Textures().find(uri).uniqueId();
    }
    catch(de::Textures::NotFoundError const &)
    {
        // Log but otherwise ignore this error.
        if(!quiet)
        {
            LOG_WARNING("Unknown texture %s.") << uri;
        }
    }
    return -1;
}

#undef Textures_UniqueId
DENG_EXTERN_C int Textures_UniqueId(Uri const *uri)
{
    return Textures_UniqueId2(uri, false);
}

/*
 * animgroups.cpp:
 */
#undef R_CreateAnimGroup
DENG_EXTERN_C int R_CreateAnimGroup(int flags);
#undef R_AddAnimGroupFrame
DENG_EXTERN_C void R_AddAnimGroupFrame(int animGroupNum, Uri const *texture, int tics, int randomTics);

/*
 * colorpalettes.cpp:
 */
#undef R_CreateColorPalette
DENG_EXTERN_C colorpaletteid_t R_CreateColorPalette(char const *fmt, char const *name, uint8_t const *colorData, int colorCount);

#undef R_GetColorPaletteNameForNum
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id);

#undef R_GetColorPaletteNumForName
DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name);

#undef R_GetColorPaletteRGBubv
DENG_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean correctGamma);

#undef R_GetColorPaletteRGBf
DENG_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean correctGamma);

/*
 * r_data.cpp:
 */
#undef R_DeclarePatch
DENG_EXTERN_C patchid_t R_DeclarePatch(char const *name);

#undef R_GetPatchInfo
DENG_EXTERN_C boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info);

#undef R_ComposePatchUri
DENG_EXTERN_C struct uri_s *R_ComposePatchUri(patchid_t id);

#undef R_ComposePatchPath
DENG_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id);

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
