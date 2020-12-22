/** @file api_resource.h Public API for resources.
 * @ingroup resource
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DOOMSDAY_API_RESOURCE_H
#define DOOMSDAY_API_RESOURCE_H

#include "apis.h"
#include "api_uri.h"
#include "dd_share.h"

DE_API_TYPEDEF(R)
{
    de_api_t api;

    /**
     * @param encodedName  Percent-encoded name for the patch.
     */
    patchid_t (*DeclarePatch)(const char *encodedName);

    /**
     * Retrieve extended info for the patch associated with @a id.
     *
     * @param id  Unique identifier of the patch to lookup.
     * @param info  Extend info will be written here if found.
     * @return  @c true: Extended info for this patch was found.
     */
    dd_bool (*GetPatchInfo)(patchid_t id, patchinfo_t *info);

    /// @return  Uri for the patch associated with @a id. Should be released with Uri_Delete()
    Uri* (*ComposePatchUri)(patchid_t id);

    /// @return  Path for the patch associated with @a id. A zero-length string is
    ///          returned if the id is invalid/unknown.
    AutoStr* (*ComposePatchPath)(patchid_t id);

    /**
     * Create a new animation group.
     * @return  Logical (unique) identifier reference associated with the new group.
     */
    int (*CreateAnimGroup)(int flags);

    /**
     * Append a new @a texture frame to the identified @a animGroupNum.
     *
     * @param animGroupNum  Logical identifier reference to the group being modified.
     * @param texture  Texture frame to be inserted into the group.
     * @param tics  Base duration of the new frame in tics.
     * @param randomTics  Extra frame duration in tics (randomized on each cycle).
     */
    void (*AddAnimGroupFrame)(int groupNum, const Uri *texture, int tics, int randomTics);

    /**
     * Add a new (named) color palette.
     *
     * @param fmt  Format string describes the format of @p data.
     *      Expected form: "C#C#C"
     *      C = color component, one of R, G, B.
     *      # = bits per component.
     *
     * @param name  Unique name by which the palette will be known.
     * @param colorData  Color component triplets (at least @a colorCount * 3 values).
     * @param colorCount  Number of colors.
     *
     * @return  Color palette id.
     */
    colorpaletteid_t (*CreateColorPalette)(const char *fmt, const char *name, const uint8_t *colorData, int colorCount);

    /**
     * Add a new translation map to the specified color palette.
     *
     * @param paletteId      Color palette identifier.
     * @param translationId  Unique identifier to associate with the translation.
     * @param mappings       Table of color palette index mappings. A copy is made.
     *                       It is assumed that this table contains a mapping for
     *                       each color index in the palette.
     */
    void (*CreateColorPaletteTranslation)(colorpaletteid_t paletteId, const Str *translationId, const uint8_t *mappings);

    /**
     * Given a color palette name, look up the associated identifier.
     *
     * @param name Unique name of the palette to locate.
     * @return  Identifier of the palette associated with this name, else @c 0
     */
    colorpaletteid_t (*GetColorPaletteNumForName)(const char *name);

    /**
     * Given a color palette id, look up the specified unique name.
     *
     * @param id  Id of the color palette to locate.
     * @return  Pointer to the unique name associated with the specified id else @c NULL
     */
    char const* (*GetColorPaletteNameForNum)(colorpaletteid_t id);

    /**
     * Given a color palette index, calculate the equivalent RGB color.
     *
     * @param id  Id of the ColorPalette to use.
     * @param colorIdx  ColorPalette color index.
     * @param rgb  Final color will be written back here.
     * @param applyTexGamma  @c true: the texture gamma ramp should be applied.
     */
    void (*GetColorPaletteRGBf)(colorpaletteid_t id, int colorIdx, float rgb[3], dd_bool applyTexGamma);

    /**
     * Given a color palette index, calculate the equivalent RGB color.
     *
     * @param id  Id of the ColorPalette to use.
     * @param colorIdx  ColorPalette color index.
     * @param rgb  Final color will be written back here.
     * @param applyTexGamma  @c true= the texture gamma ramp should be applied.
     */
    void (*GetColorPaletteRGBubv)(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], dd_bool applyTexGamma);

    int (*TextureUniqueId)(const Uri *uri); /*quiet=false*/
    int (*TextureUniqueId2)(const Uri *uri, dd_bool quiet);
}
DE_API_T(R);

#ifndef DE_NO_API_MACROS_RESOURCE
#define R_DeclarePatch                  _api_R.DeclarePatch
#define R_GetPatchInfo                  _api_R.GetPatchInfo
#define R_ComposePatchUri               _api_R.ComposePatchUri
#define R_ComposePatchPath              _api_R.ComposePatchPath
#define R_CreateAnimGroup               _api_R.CreateAnimGroup
#define R_AddAnimGroupFrame             _api_R.AddAnimGroupFrame
#define R_CreateColorPalette            _api_R.CreateColorPalette
#define R_CreateColorPaletteTranslation _api_R.CreateColorPaletteTranslation
#define R_GetColorPaletteNumForName     _api_R.GetColorPaletteNumForName
#define R_GetColorPaletteNameForNum     _api_R.GetColorPaletteNameForNum
#define R_GetColorPaletteRGBf           _api_R.GetColorPaletteRGBf
#define R_GetColorPaletteRGBubv         _api_R.GetColorPaletteRGBubv
#define Textures_UniqueId               _api_R.TextureUniqueId
#define Textures_UniqueId2              _api_R.TextureUniqueId2
#endif

#ifdef __DOOMSDAY__
DE_USING_API(R);
#endif

#endif // DOOMSDAY_API_RESOURCE_H
