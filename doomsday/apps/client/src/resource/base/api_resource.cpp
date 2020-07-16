/** @file api_resource.cpp  Public API of the resource subsystem.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_RESOURCE
#include "de_base.h"
#include "api_resource.h"
#include "gl/gl_tex.h" // averagealpha_analysis_t, etc...
#ifdef __CLIENT__
#  include "render/r_draw.h" // Rend_PatchTextureSpec()
#  include "render/r_main.h" // texGammaLut
#endif

#include <doomsday/res/animgroups.h>
#include <doomsday/res/colorpalettes.h>
#include <doomsday/res/resources.h>
#include <doomsday/res/textures.h>
#include <de/legacy/texgamma.h>

using namespace de;

#undef Textures_UniqueId2
DE_EXTERN_C int Textures_UniqueId2(const uri_s *_uri, dd_bool quiet)
{
    LOG_AS("Textures_UniqueId");
    if(!_uri) return -1;
    const res::Uri &uri = reinterpret_cast<const res::Uri &>(*_uri);

    try
    {
        return res::Textures::get().textureManifest(uri).uniqueId();
    }
    catch(const Resources::MissingResourceManifestError &)
    {
        // Log but otherwise ignore this error.
        if(!quiet)
        {
            LOG_RES_WARNING("Unknown texture %s") << uri;
        }
    }
    return -1;
}

#undef Textures_UniqueId
DE_EXTERN_C int Textures_UniqueId(const uri_s *uri)
{
    return Textures_UniqueId2(uri, false);
}

#undef R_CreateAnimGroup
DE_EXTERN_C int R_CreateAnimGroup(int flags)
{
    return res::AnimGroups::get().newAnimGroup(flags & ~AGF_PRECACHE).id();
}

#undef R_AddAnimGroupFrame
DE_EXTERN_C void R_AddAnimGroupFrame(int groupId, const uri_s *textureUri_, int tics, int randomTics)
{
    LOG_AS("R_AddAnimGroupFrame");

    if(!textureUri_) return;
    const res::Uri &textureUri = reinterpret_cast<const res::Uri &>(*textureUri_);

    try
    {
        if(res::AnimGroup *group = res::AnimGroups::get().animGroup(groupId))
        {
            group->newFrame(res::Textures::get().textureManifest(textureUri), tics, randomTics);
        }
        else
        {
            LOG_DEBUG("Unknown anim group #%i, ignoring.") << groupId;
        }
    }
    catch(const Resources::MissingResourceManifestError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ". Failed adding texture \"%s\" to group #%i, ignoring.")
            << textureUri << groupId;
    }
}

#undef R_CreateColorPalette
DE_EXTERN_C colorpaletteid_t R_CreateColorPalette(const char *colorFormatDescriptor,
    const char *nameCStr, const uint8_t *colorData, int colorCount)
{
    DE_ASSERT(nameCStr != 0 && colorFormatDescriptor != 0 && colorData != 0);

    LOG_AS("R_CreateColorPalette");

    try
    {
        using namespace res;

        String name(nameCStr);
        if(name.isEmpty())
        {
            LOG_RES_WARNING("Invalid/zero-length name specified, ignoring.");
            return 0;
        }

        List<Vec3ub> colors =
            ColorTableReader::read(colorFormatDescriptor, colorCount, colorData);

        auto &palettes = Resources::get().colorPalettes();

        // Replacing an existing palette?
        if(palettes.hasColorPalette(name))
        {
            ColorPalette &palette = palettes.colorPalette(name);
            palette.replaceColorTable(colors);
            return palette.id();
        }

        // A new palette.
        ColorPalette *palette = new ColorPalette(colors);
        palettes.addColorPalette(*palette, name);

        return palette->id();
    }
    catch(const res::ColorTableReader::FormatError &er)
    {
        LOG_RES_WARNING("Error creating/replacing color palette '%s':\n")
            << nameCStr << er.asText();
    }
    return 0;
}

#undef R_CreateColorPaletteTranslation
DE_EXTERN_C void R_CreateColorPaletteTranslation(colorpaletteid_t paletteId,
    const ddstring_s *translationId, const uint8_t *mappings_)
{
    DE_ASSERT(mappings_ != 0);

    LOG_AS("R_CreateColorPaletteTranslation");

    try
    {
        res::ColorPalette &palette = App_Resources().colorPalettes().colorPalette(paletteId);

        // Convert the mapping table.
        const int colorCount  = palette.colorCount();
        res::ColorPaletteTranslation mappings(colorCount);
        for(int i = 0; i < colorCount; ++i)
        {
            const int palIdx = mappings_[i];
            DE_ASSERT(palIdx >= 0 && palIdx < colorCount);
            mappings[i] = palIdx;
        }

        // Create/update this translation.
        palette.newTranslation(Str_Text(translationId), mappings);
    }
    catch(const Resources::MissingResourceError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING("Error creating/replacing color palette '%u' translation '%s':\n")
            << paletteId << Str_Text(translationId) << er.asText();
    }
    catch(const res::ColorPalette::InvalidTranslationIdError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING("Error creating/replacing color palette '%u' translation '%s':\n")
            << paletteId << Str_Text(translationId) << er.asText();
    }
}

#undef R_GetColorPaletteNumForName
DE_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(const char *name)
{
    LOG_AS("R_GetColorPaletteNumForName");
    try
    {
        return App_Resources().colorPalettes().colorPalette(name).id();
    }
    catch(const Resources::MissingResourceError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    return 0; // Not found.
}

#undef R_GetColorPaletteNameForNum
DE_EXTERN_C const char *R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    LOG_AS("R_GetColorPaletteNameForNum");
    try
    {
        res::ColorPalette &palette = App_Resources().colorPalettes().colorPalette(id);
        return App_Resources().colorPalettes().colorPaletteName(palette);
    }
    catch(const Resources::MissingResourceError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    return 0; // Not found.
}

#undef R_GetColorPaletteRGBubv
DE_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t paletteId, int colorIdx, uint8_t rgb[3],
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
        Vec3ub palColor = App_Resources().colorPalettes().colorPalette(paletteId)[colorIdx];
        rgb[0] = palColor.x;
        rgb[1] = palColor.y;
        rgb[2] = palColor.z;
        if(applyTexGamma)
        {
            rgb[0] = R_TexGammaLut(rgb[0]);
            rgb[1] = R_TexGammaLut(rgb[1]);
            rgb[2] = R_TexGammaLut(rgb[2]);
        }
    }
    catch(const Resources::MissingResourceError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
}

#undef R_GetColorPaletteRGBf
DE_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t paletteId, int colorIdx, float rgb[3],
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
        res::ColorPalette &palette = App_Resources().colorPalettes().colorPalette(paletteId);
        if(applyTexGamma)
        {
            Vec3ub palColor = palette[colorIdx];
            rgb[0] = R_TexGammaLut(palColor.x) * reciprocal255;
            rgb[1] = R_TexGammaLut(palColor.y) * reciprocal255;
            rgb[2] = R_TexGammaLut(palColor.z) * reciprocal255;
        }
        else
        {
            Vec3f palColor = palette.colorf(colorIdx);
            rgb[0] = palColor.x;
            rgb[1] = palColor.y;
            rgb[2] = palColor.z;
        }
    }
    catch(const Resources::MissingResourceError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
}

#undef R_ComposePatchPath
DE_EXTERN_C AutoStr *R_ComposePatchPath(patchid_t id)
{
    LOG_AS("R_ComposePatchPath");
    try
    {
        res::TextureManifest &manifest = res::Textures::get().textureScheme("Patches").findByUniqueId(id);
        return AutoStr_FromTextStd(manifest.path());
    }
    catch(const res::TextureScheme::NotFoundError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    return AutoStr_NewStd();
}

#undef R_ComposePatchUri
DE_EXTERN_C uri_s *R_ComposePatchUri(patchid_t id)
{
    try
    {
        res::TextureManifest &manifest = res::Textures::get().textureScheme("Patches").findByUniqueId(id);
        return reinterpret_cast<uri_s *>(new res::Uri(manifest.composeUri()));
    }
    catch(const res::TextureScheme::NotFoundError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    return reinterpret_cast<uri_s *>(new res::Uri());
}

#undef R_DeclarePatch
DE_EXTERN_C patchid_t R_DeclarePatch(const char *encodedName)
{
    return res::Textures::get().declarePatch(encodedName);
}

#undef R_GetPatchInfo
DE_EXTERN_C dd_bool R_GetPatchInfo(patchid_t id, patchinfo_t *info)
{
    DE_ASSERT(info);
    LOG_AS("R_GetPatchInfo");

    de::zapPtr(info);
    if(!id) return false;

    try
    {
        res::Texture &tex = res::Textures::get().textureScheme("Patches").findByUniqueId(id).texture();

#ifdef __CLIENT__
        // Ensure we have up to date information about this patch.
        const TextureVariantSpec &texSpec =
            Rend_PatchTextureSpec(0 | (tex.isFlagged(res::Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                    | (tex.isFlagged(res::Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
        static_cast<ClientTexture &>(tex).prepareVariant(texSpec);
#endif

        info->id = id;
        info->flags.isCustom = tex.isFlagged(res::Texture::Custom);

        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t *>(tex.analysisDataPointer(res::Texture::AverageAlphaAnalysis));
        info->flags.isEmpty = aa && IS_ZERO(aa->alpha);

        info->geometry.size.width  = tex.width();
        info->geometry.size.height = tex.height();

        info->geometry.origin.x = tex.origin().x;
        info->geometry.origin.y = tex.origin().y;

        /// @todo fixme: kludge:
        info->extraOffset[0] = info->extraOffset[1] = (tex.isFlagged(res::Texture::UpscaleAndSharpen)? -1 : 0);
        // Kludge end.
        return true;
    }
    catch(const res::TextureManifest::MissingTextureError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring");
    }
    catch(const res::TextureScheme::NotFoundError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring");
    }
    return false;
}

DE_DECLARE_API(R) =
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
