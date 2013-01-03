#define DENG_NO_API_MACROS_MATERIALS

#include "de_base.h"
#include "de_resource.h"
#include "api_material.h"

materialid_t DD_MaterialForTextureUri(uri_s const *_textureUri)
{
    if(!_textureUri) return NOMATERIALID;

    try
    {
        de::Uri uri = App_Textures()->find(reinterpret_cast<de::Uri const &>(*_textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return App_Materials()->resolveUri2(uri, true/*quiet please*/);
    }
    catch(de::Textures::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(de::Textures::NotFoundError const &)
    {} // Ignore this error.

    return NOMATERIALID;
}

// materials.cpp
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId);

DENG_DECLARE_API(Material) =
{
    { DE_API_MATERIALS_latest },
    DD_MaterialForTextureUri,
    Materials_ComposeUri
};
