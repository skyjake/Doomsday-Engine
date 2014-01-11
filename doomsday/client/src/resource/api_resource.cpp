/** @file api_resource.cpp  Public API of the resource subsystem.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_RESOURCE
#include "de_base.h"
#include "api_resource.h"
#include "resource/resourcesystem.h"
#include "gl/gl_tex.h" // averagealpha_analysis_t, etc...
#ifdef __CLIENT__
#  include "render/r_draw.h" // Rend_PatchTextureSpec()
#endif
#include "render/r_main.h" // texGammaLut

using namespace de;

#undef Textures_UniqueId2
DENG_EXTERN_C int Textures_UniqueId2(uri_s const *_uri, dd_bool quiet)
{
    LOG_AS("Textures_UniqueId");
    if(!_uri) return -1;
    de::Uri const &uri = reinterpret_cast<de::Uri const &>(*_uri);

    try
    {
        return App_ResourceSystem().textureManifest(uri).uniqueId();
    }
    catch(ResourceSystem::MissingManifestError const &)
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
DENG_EXTERN_C int Textures_UniqueId(uri_s const *uri)
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
        if(AnimGroup *group = App_ResourceSystem().animGroup(groupId))
        {
            group->newFrame(App_ResourceSystem().textureManifest(textureUri), tics, randomTics);
        }
        else
        {
            LOG_DEBUG("Unknown anim group #%i, ignoring.") << groupId;
        }
    }
    catch(ResourceSystem::MissingManifestError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ". Failed adding texture \"%s\" to group #%i, ignoring.")
            << textureUri << groupId;
    }
}

#undef R_CreateColorPalette
DENG_EXTERN_C colorpaletteid_t R_CreateColorPalette(char const *colorFormatDescriptor,
    char const *nameCStr, uint8_t const *colorData, int colorCount)
{
    DENG2_ASSERT(nameCStr != 0 && colorFormatDescriptor != 0 && colorData != 0);

    LOG_AS("R_CreateColorPalette");

    String name(nameCStr);
    if(name.isEmpty())
    {
        LOG_WARNING("Invalid/zero-length name specified, ignoring.");
        return 0;
    }

    try
    {
        QVector<Vector3ub> colors =
            ColorTableReader::read(colorFormatDescriptor, colorCount, colorData);

        // Replacing an existing palette?
        if(App_ResourceSystem().hasColorPalette(name))
        {
            ColorPalette &palette = App_ResourceSystem().colorPalette(name);
            palette.replaceColorTable(colors);
            return palette.id();
        }

        // A new palette.
        ColorPalette *palette = new ColorPalette(colors);
        App_ResourceSystem().addColorPalette(*palette, name);

        return palette->id();
    }
    catch(ColorTableReader::FormatError const &er)
    {
        LOG_WARNING("Error creating/replacing color palette '%s':\n")
            << name << er.asText();
    }
    return 0;
}

#undef R_CreateColorPaletteTranslation
DENG_EXTERN_C void R_CreateColorPaletteTranslation(colorpaletteid_t paletteId,
    ddstring_s const *translationId, uint8_t const *mappings_)
{
    DENG2_ASSERT(mappings_ != 0);

    LOG_AS("R_CreateColorPaletteTranslation");

    try
    {
        ColorPalette &palette = App_ResourceSystem().colorPalette(paletteId);

        // Convert the mapping table.
        int const colorCount  = palette.colorCount();
        ColorPaletteTranslation mappings(colorCount);
        for(int i = 0; i < colorCount; ++i)
        {
            int const palIdx = mappings_[i];
            DENG2_ASSERT(palIdx >= 0 && palIdx < colorCount);
            mappings[i] = palIdx;
        }

        // Create/update this translation.
        palette.newTranslation(Str_Text(translationId), mappings);
    }
    catch(ResourceSystem::MissingResourceError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING("Error creating/replacing color palette '%u' translation '%s':\n")
            << paletteId << Str_Text(translationId) << er.asText();
    }
    catch(ColorPalette::InvalidTranslationIdError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING("Error creating/replacing color palette '%u' translation '%s':\n")
            << paletteId << Str_Text(translationId) << er.asText();
    }
}

#undef R_GetColorPaletteNumForName
DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name)
{
    LOG_AS("R_GetColorPaletteNumForName");
    try
    {
        return App_ResourceSystem().colorPalette(name).id();
    }
    catch(ResourceSystem::MissingResourceError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return 0; // Not found.
}

#undef R_GetColorPaletteNameForNum
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    LOG_AS("R_GetColorPaletteNameForNum");
    try
    {
        ColorPalette &palette = App_ResourceSystem().colorPalette(id);
        return App_ResourceSystem().colorPaletteName(palette).toUtf8().constData();
    }
    catch(ResourceSystem::MissingResourceError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return 0; // Not found.
}

#undef R_GetColorPaletteRGBubv
DENG_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t paletteId, int colorIdx, uint8_t rgb[3],
    dd_bool applyTexGamma)
{
    LOG_AS("R_GetColorPaletteRGBubv");

    if(!rgb) return;

    // Always interpret a negative color index as black.
    if(colorIdx < 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    try
    {
        Vector3ub palColor = App_ResourceSystem().colorPalette(paletteId)[colorIdx];
        rgb[0] = palColor.x;
        rgb[1] = palColor.y;
        rgb[2] = palColor.z;
        if(applyTexGamma)
        {
            rgb[0] = texGammaLut[rgb[0]];
            rgb[1] = texGammaLut[rgb[1]];
            rgb[2] = texGammaLut[rgb[2]];
        }
    }
    catch(ResourceSystem::MissingResourceError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
}

#undef R_GetColorPaletteRGBf
DENG_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t paletteId, int colorIdx, float rgb[3],
    dd_bool applyTexGamma)
{
    LOG_AS("R_GetColorPaletteRGBf");

    if(!rgb) return;

    // Always interpret a negative color index as black.
    if(colorIdx < 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    try
    {
        ColorPalette &palette = App_ResourceSystem().colorPalette(paletteId);
        if(applyTexGamma)
        {
            Vector3ub palColor = palette[colorIdx];
            rgb[0] = texGammaLut[palColor.x] * reciprocal255;
            rgb[1] = texGammaLut[palColor.y] * reciprocal255;
            rgb[2] = texGammaLut[palColor.z] * reciprocal255;
        }
        else
        {
            Vector3f palColor = palette.colorf(colorIdx);
            rgb[0] = palColor.x;
            rgb[1] = palColor.y;
            rgb[2] = palColor.z;
        }
    }
    catch(ResourceSystem::MissingResourceError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
}

#undef R_ComposePatchPath
DENG_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id)
{
    LOG_AS("R_ComposePatchPath");
    try
    {
        TextureManifest &manifest = App_ResourceSystem().textureScheme("Patches").findByUniqueId(id);
        return AutoStr_FromTextStd(manifest.path().toUtf8().constData());
    }
    catch(TextureScheme::NotFoundError const &er)
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
        TextureManifest &manifest = App_ResourceSystem().textureScheme("Patches").findByUniqueId(id);
        return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
    }
    catch(TextureScheme::NotFoundError const &er)
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
DENG_EXTERN_C dd_bool R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    DENG2_ASSERT(info);
    LOG_AS("R_GetPatchInfo");

    de::zapPtr(info);
    if(!id) return false;

    try
    {
        Texture &tex = App_ResourceSystem().textureScheme("Patches").findByUniqueId(id).texture();

#ifdef __CLIENT__
        // Ensure we have up to date information about this patch.
        TextureVariantSpec const &texSpec =
            Rend_PatchTextureSpec(0 | (tex.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                    | (tex.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
        tex.prepareVariant(texSpec);
#endif

        info->id = id;
        info->flags.isCustom = tex.isFlagged(Texture::Custom);

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex.analysisDataPointer(Texture::AverageAlphaAnalysis));
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);

        info->geometry.size.width  = tex.width();
        info->geometry.size.height = tex.height();

        info->geometry.origin.x = tex.origin().x;
        info->geometry.origin.y = tex.origin().y;

        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (tex.isFlagged(Texture::UpscaleAndSharpen)? -1 : 0);
        // Kludge end.
        return true;
    }
    catch(TextureManifest::MissingTextureError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(TextureScheme::NotFoundError const &er)
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
    R_CreateColorPaletteTranslation,
    R_GetColorPaletteNumForName,
    R_GetColorPaletteNameForNum,
    R_GetColorPaletteRGBf,
    R_GetColorPaletteRGBubv,
    Textures_UniqueId,
    Textures_UniqueId2,
};
