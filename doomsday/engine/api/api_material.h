/** @file api_material.h Public API for materials.
 * @ingroup resource
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

#ifndef DOOMSDAY_API_MATERIAL_H
#define DOOMSDAY_API_MATERIAL_H

#include "api_base.h"
#include "api_uri.h"

/**
 * @defgroup material Materials
 * @ingroup resource
 */
///@{

#if !defined __DOOMSDAY__ && !defined DENG_INTERNAL_DATA_ACCESS
typedef struct material_s { int type; } material_t;
#endif

DENG_API_TYPEDEF(Material)
{
    de_api_t api;

    material_t *(*ForTextureUri)(Uri const *textureUri);
    Uri *(*ComposeUri)(materialid_t materialId);
    materialid_t (*ResolveUri)(const Uri* uri);
    materialid_t (*ResolveUriCString)(const char* path);

}
DENG_API_T(Material);

#ifndef DENG_NO_API_MACROS_MATERIALS
#define DD_MaterialForTextureUri    _api_Material.ForTextureUri
#define Materials_ComposeUri        _api_Material.ComposeUri
#define Materials_ResolveUri        _api_Material.ResolveUri
#define Materials_ResolveUriCString _api_Material.ResolveUriCString
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Material);
#endif

///@}

#endif // DOOMSDAY_API_MATERIAL_H
