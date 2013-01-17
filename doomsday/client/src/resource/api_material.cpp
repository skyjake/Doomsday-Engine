#define DENG_NO_API_MACROS_MATERIALS

#include "de_base.h"
#include "de_resource.h"
#include "api_material.h"

#undef DD_MaterialForTextureUri
material_t *DD_MaterialForTextureUri(uri_s const *textureUri)
{
    if(!textureUri) return 0; // Not found.

    try
    {
        de::Uri uri = App_Textures()->find(reinterpret_cast<de::Uri const &>(*textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return App_Materials()->find(uri).material();
    }
    catch(de::Materials::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(de::Materials::NotFoundError const &)
    {} // Ignore this error.
    catch(de::Textures::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(de::Textures::NotFoundError const &)
    {} // Ignore this error.

    return 0; // Not found.
}

// materials.cpp
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId);
DENG_EXTERN_C materialid_t Materials_ResolveUri(Uri const *uri/*, quiet=!(verbose >= 1)*/);
DENG_EXTERN_C materialid_t Materials_ResolveUriCString(char const *uri/*, quiet=!(verbose >= 1)*/);

DENG_DECLARE_API(Material) =
{
    { DE_API_MATERIALS },
    DD_MaterialForTextureUri,
    Materials_ComposeUri,
    Materials_ResolveUri,
    Materials_ResolveUriCString
};
