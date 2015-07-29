/** @file api_material.cpp  Material API.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

#undef DD_MaterialForTextureUri
DENG_EXTERN_C Material *DD_MaterialForTextureUri(uri_s const *textureUri)
{
    if(!textureUri) return nullptr;  // Not found.

    try
    {
        de::Uri uri = App_ResourceSystem().textureManifest(reinterpret_cast<de::Uri const &>(*textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return &App_ResourceSystem().material(uri);
    }
    catch(MaterialManifest::MissingMaterialError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch(ResourceSystem::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch(res::System::MissingResourceManifestError const &)
    {}  // Ignore this error.

    return nullptr;  // Not found.
}

#undef Materials_ComposeUri
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    MaterialManifest &manifest = App_ResourceSystem().toMaterialManifest(materialId);
    return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
}

#undef Materials_ResolveUri
DENG_EXTERN_C materialid_t Materials_ResolveUri(struct uri_s const *uri)
{
    try
    {
        return App_ResourceSystem().materialManifest(*reinterpret_cast<de::Uri const *>(uri)).id();
    }
    catch(res::System::MissingResourceManifestError const &)
    {}  // Ignore this error.
    return NOMATERIALID;
}

#undef Materials_ResolveUriCString
DENG_EXTERN_C materialid_t Materials_ResolveUriCString(char const *uriCString)
{
    if(uriCString && uriCString[0])
    {
        try
        {
            return App_ResourceSystem().materialManifest(de::Uri(uriCString, RC_NULL)).id();
        }
        catch(res::System::MissingResourceManifestError const &)
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
