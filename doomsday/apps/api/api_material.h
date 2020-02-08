/** @file api_material.h Public API for materials.
 * @ingroup resource
 *
 * @author Copyright &copy; 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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
#include <doomsday/api_map.h>

/**
 * @defgroup material Materials
 * @ingroup resource
 */
///@{

DE_API_TYPEDEF(Material)
{
    de_api_t api;

    world_Material *(*ForTextureUri)(const Uri *textureUri);
    Uri *(*ComposeUri)(materialid_t materialId);
    materialid_t (*ResolveUri)(const Uri *uri);
    materialid_t (*ResolveUriCString)(const char *path);

}
DE_API_T(Material);

#ifndef DE_NO_API_MACROS_MATERIALS
#define DD_MaterialForTextureUri    _api_Material.ForTextureUri
#define Materials_ComposeUri        _api_Material.ComposeUri
#define Materials_ResolveUri        _api_Material.ResolveUri
#define Materials_ResolveUriCString _api_Material.ResolveUriCString
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Material);
#endif

///@}

#endif // DOOMSDAY_API_MATERIAL_H
