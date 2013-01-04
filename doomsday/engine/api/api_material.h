#ifndef DOOMSDAY_API_MATERIAL_H
#define DOOMSDAY_API_MATERIAL_H

#include "api_base.h"
#include "api_uri.h"

/**
 * @defgroup material Materials
 * @ingroup resource
 */
///@{

DENG_API_TYPEDEF(Material)
{
    de_api_t api;

    materialid_t (*IdForTextureUri)(Uri const *textureUri);
    Uri *(*ComposeUri)(materialid_t materialId);
    materialid_t (*ResolveUri)(const Uri* uri);
    materialid_t (*ResolveUriCString)(const char* path);

}
DENG_API_T(Material);

#ifndef DENG_NO_API_MACROS_MATERIALS
#define DD_MaterialForTextureUri    _api_Material.IdForTextureUri
#define Materials_ComposeUri        _api_Material.ComposeUri
#define Materials_ResolveUri        _api_Material.ResolveUri
#define Materials_ResolveUriCString _api_Material.ResolveUriCString
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Material);
#endif

///@}

#endif // DOOMSDAY_API_MATERIAL_H
