/** @file api_material.cpp  Material API.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_MATERIALS

#include "de_base.h"
#include "api_material.h"

#include <doomsday/res/textures.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/materialmanifest.h>

using namespace de;

#undef DD_MaterialForTextureUri
DE_EXTERN_C world_Material *DD_MaterialForTextureUri(const uri_s *textureUri)
{
    if (!textureUri) return nullptr;  // Not found.

    try
    {
        res::Uri uri = res::Textures::get().textureManifest(reinterpret_cast<const res::Uri &>(*textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return reinterpret_cast<world_Material *>(&world::Materials::get().material(uri));
    }
    catch (const world::MaterialManifest::MissingMaterialError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch (const Resources::UnknownSchemeError &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch (const Resources::MissingResourceManifestError &)
    {}  // Ignore this error.

    return nullptr;  // Not found.
}

#undef Materials_ComposeUri
DE_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    world::MaterialManifest &manifest = world::Materials::get().toMaterialManifest(materialId);
    return reinterpret_cast<uri_s *>(new res::Uri(manifest.composeUri()));
}

#undef Materials_ResolveUri
DE_EXTERN_C materialid_t Materials_ResolveUri(const struct uri_s *uri)
{
    try
    {
        return world::Materials::get().materialManifest(*reinterpret_cast<const res::Uri *>(uri)).id();
    }
    catch (const Resources::MissingResourceManifestError &)
    {}  // Ignore this error.
    return NOMATERIALID;
}

#undef Materials_ResolveUriCString
DE_EXTERN_C materialid_t Materials_ResolveUriCString(const char *uriCString)
{
    if (uriCString && uriCString[0])
    {
        try
        {
            return world::Materials::get().materialManifest(res::makeUri(uriCString)).id();
        }
        catch (const Resources::MissingResourceManifestError &)
        {}  // Ignore this error.
    }
    return NOMATERIALID;
}

DE_DECLARE_API(Material) =
{
    { DE_API_MATERIALS },
    DD_MaterialForTextureUri,
    Materials_ComposeUri,
    Materials_ResolveUri,
    Materials_ResolveUriCString
};
