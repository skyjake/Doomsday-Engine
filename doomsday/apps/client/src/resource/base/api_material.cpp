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

#define DENG_NO_API_MACROS_MATERIALS

#include "de_base.h"
#include "api_material.h"

#include <doomsday/res/Textures>
#include <doomsday/world/Materials>
#include <doomsday/world/MaterialManifest>

using namespace de;

#undef DD_MaterialForTextureUri
DENG_EXTERN_C world_Material *DD_MaterialForTextureUri(uri_s const *textureUri)
{
    if (!textureUri) return nullptr;  // Not found.

    try
    {
        de::Uri uri = res::Textures::get().textureManifest(reinterpret_cast<de::Uri const &>(*textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return reinterpret_cast<world_Material *>(&world::Materials::get().material(uri));
    }
    catch (world::MaterialManifest::MissingMaterialError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch (Resources::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch (Resources::MissingResourceManifestError const &)
    {}  // Ignore this error.

    return nullptr;  // Not found.
}

#undef Materials_ComposeUri
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    world::MaterialManifest &manifest = world::Materials::get().toMaterialManifest(materialId);
    return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
}

#undef Materials_ResolveUri
DENG_EXTERN_C materialid_t Materials_ResolveUri(struct uri_s const *uri)
{
    try
    {
        return world::Materials::get().materialManifest(*reinterpret_cast<de::Uri const *>(uri)).id();
    }
    catch (Resources::MissingResourceManifestError const &)
    {}  // Ignore this error.
    return NOMATERIALID;
}

#undef Materials_ResolveUriCString
DENG_EXTERN_C materialid_t Materials_ResolveUriCString(char const *uriCString)
{
    if (uriCString && uriCString[0])
    {
        try
        {
            return world::Materials::get().materialManifest(de::Uri(uriCString, RC_NULL)).id();
        }
        catch (Resources::MissingResourceManifestError const &)
        {}  // Ignore this error.
    }
    return NOMATERIALID;
}

DENG_DECLARE_API(Material) =
{
    { DE_API_MATERIALS },
    DD_MaterialForTextureUri,
    Materials_ComposeUri,
    Materials_ResolveUri,
    Materials_ResolveUriCString
};
