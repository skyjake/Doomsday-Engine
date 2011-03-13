/**\file gltexture.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_REFRESH_GLTEXTURE_H
#define LIBDENG_REFRESH_GLTEXTURE_H

struct gltexturevariant_s;
struct gltexture_variantlist_node_s;

/**
 * gltexture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */

#define GLTEXTURE_TYPE_STRING(t)     ((t) == GLT_FLAT? "flat" : \
    (t) == GLT_DOOMTEXTURE? "doomtexture" : \
    (t) == GLT_DOOMPATCH? "doompatch" : \
    (t) == GLT_SPRITE? "sprite" : \
    (t) == GLT_DETAIL? "detailtex" : \
    (t) == GLT_SHINY? "shinytex" : \
    (t) == GLT_MASK? "masktex" : \
    (t) == GLT_MODELSKIN? "modelskin" : \
    (t) == GLT_MODELSHINYSKIN? "modelshinyskin" : \
    (t) == GLT_LIGHTMAP? "lightmap" : \
    (t) == GLT_FLARE? "flaretex" : "systemtex")

typedef struct gltexture_s {
    gltextureid_t id;
    char name[9];
    gltexture_type_t type;
    int ofTypeID;
    struct gltexture_variantlist_node_s* variants;
    uint hashNext; // 1-based index
} gltexture_t;

void GLTexture_Destruct(gltexture_t* tex);

/**
 * Add a new prepared variant to the list of resources for this GLTexture.
 * GLTexture takes ownership of the variant.
 *
 * @param variant  Variant instance to add to the resource list.
 */
void GLTexture_AddVariant(gltexture_t* tex, struct gltexturevariant_s* variant);

const char* GLTexture_Name(const gltexture_t* tex);

float GLTexture_GetWidth(const gltexture_t* tex);

float GLTexture_GetHeight(const gltexture_t* tex);

boolean GLTexture_IsFromIWAD(const gltexture_t* tex);

/**
 * Sets the minification mode of the specified gltexture.
 *
 * @param tex  The gltexture to be updated.
 * @param minMode  The GL minification mode to set.
 */
void GLTexture_SetMinMode(gltexture_t* tex, int minMode);

/**
 * Deletes all GL texture instances for the specified gltexture.
 */
void GLTexture_ReleaseTextures(gltexture_t* tex);

int GLTexture_IterateInstances(gltexture_t* tex,
    int (*callback)(struct gltexturevariant_s* instance, void* paramaters),
    void* paramaters);

struct gltexturevariant_s* GLTexture_Prepare(gltexture_t* tex, void* context,
    byte* result);

#endif /* LIBDENG_REFRESH_GLTEXTURE_H */
