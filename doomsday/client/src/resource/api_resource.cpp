#define DENG_NO_API_MACROS_RESOURCE
#include "de_base.h"
#include "api_resource.h"
#include "Textures"
#include "resource/animgroups.h"
#include "gl/gl_tex.h" // averagealpha_analysis_t, etc...
#ifdef __CLIENT__
#  include "render/r_draw.h" // Rend_PatchTextureSpec()
#endif

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

#undef R_CreateAnimGroup
DENG_EXTERN_C int R_CreateAnimGroup(int flags)
{
    return App_ResourceSystem().newAnimGroup(flags & ~AGF_PRECACHE).id();
}

#undef R_AddAnimGroupFrame
DENG_EXTERN_C void R_AddAnimGroupFrame(int groupId, uri_s const *textureUri_, int tics, int randomTics)
{
    LOG_AS("R_AddAnimGroupFrame");

    if(!textureUri_) return;
    de::Uri const &textureUri = reinterpret_cast<de::Uri const &>(*textureUri_);

    try
    {
        if(de::AnimGroup *group = App_ResourceSystem().animGroup(groupId))
        {
            group->newFrame(App_Textures().find(textureUri), tics, randomTics);
        }
        else
        {
            LOG_DEBUG("Unknown anim group #%i, ignoring.") << groupId;
        }
    }
    catch(de::Textures::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ". Failed adding texture \"%s\" to group #%i, ignoring.")
            << textureUri << groupId;
    }
}

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

#undef R_ComposePatchPath
DENG_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id)
{
    try
    {
        de::TextureManifest &manifest = App_Textures().scheme("Patches").findByUniqueId(id);
        return AutoStr_FromTextStd(manifest.path().toUtf8().constData());
    }
    catch(de::TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return AutoStr_NewStd();
}

#undef R_ComposePatchUri
DENG_EXTERN_C uri_s *R_ComposePatchUri(patchid_t id)
{
    try
    {
        de::TextureManifest &manifest = App_Textures().scheme("Patches").findByUniqueId(id);
        return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
    }
    catch(de::TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return reinterpret_cast<uri_s *>(new de::Uri());
}

#undef R_DeclarePatch
DENG_EXTERN_C patchid_t R_DeclarePatch(char const *encodedName)
{
    return App_ResourceSystem().declarePatch(encodedName);
}

#undef R_GetPatchInfo
DENG_EXTERN_C boolean R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    DENG2_ASSERT(info);
    LOG_AS("R_GetPatchInfo");

    std::memset(info, 0, sizeof(patchinfo_t));
    if(!id) return false;

    try
    {
        de::Texture &tex = App_Textures().scheme("Patches").findByUniqueId(id).texture();

#ifdef __CLIENT__
        // Ensure we have up to date information about this patch.
        texturevariantspecification_t &texSpec =
            Rend_PatchTextureSpec(0 | (tex.isFlagged(de::Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                    | (tex.isFlagged(de::Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
        tex.prepareVariant(texSpec);
#endif

        info->id = id;
        info->flags.isCustom = tex.isFlagged(de::Texture::Custom);

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex.analysisDataPointer(de::Texture::AverageAlphaAnalysis));
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);

        info->geometry.size.width  = tex.width();
        info->geometry.size.height = tex.height();

        info->geometry.origin.x = tex.origin().x;
        info->geometry.origin.y = tex.origin().y;

        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (tex.isFlagged(de::Texture::UpscaleAndSharpen)? -1 : 0);
        // Kludge end.
        return true;
    }
    catch(de::TextureManifest::MissingTextureError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(de::TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return false;
}

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
