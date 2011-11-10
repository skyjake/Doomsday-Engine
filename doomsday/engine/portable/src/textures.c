/**\file textures.c
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

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"

#include "m_misc.h" // For M_NumDigits
#include "fs_util.h" // For F_PrettyPath
#include "r_world.h" // For ddMapSetup

#include "gl_texmanager.h"
#include "pathdirectory.h"

#include "textures.h"

D_CMD(ListTextures);
D_CMD(InspectTexture);
#if _DEBUG
D_CMD(PrintTextureStats);
#endif

/// LUT which translates textureid_t to texture_t*. Index with textureid_t-1
static uint textureIdMapSize;
static texture_t** textureIdMap;

// Texture namespaces contain mappings between names and Texture instances.
static PathDirectory* namespaces[TEXTURENAMESPACE_COUNT];

void Textures_Register(void)
{
    C_CMD("inspecttexture", NULL, InspectTexture)
    C_CMD("listtextures", NULL, ListTextures)
#if _DEBUG
    C_CMD("texturestats", NULL, PrintTextureStats)
#endif
}

static __inline PathDirectory* getDirectoryForNamespaceId(texturenamespaceid_t id)
{
    assert(VALID_TEXTURENAMESPACEID(id));
    return namespaces[id-TEXTURENAMESPACE_FIRST];
}

static texturenamespaceid_t namespaceIdForDirectory(PathDirectory* pd)
{
    texturenamespaceid_t id;
    assert(pd);

    for(id = TEXTURENAMESPACE_FIRST; id <= TEXTURENAMESPACE_LAST; ++id)
    {
        if(namespaces[id-TEXTURENAMESPACE_FIRST] == pd) return id;
    }
    // Only reachable if attempting to find the id for a Texture that is not
    // in the collection, or the collection has not yet been initialized.
    Con_Error("Textures::namespaceIdForDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
}

static __inline boolean validTextureId(textureid_t id)
{
    return (id > 0 && id <= textureIdMapSize);
}

static __inline texture_t* getTextureForId(textureid_t id)
{
    assert(validTextureId(id));
    return textureIdMap[id-1/*1-based index*/];
}

static textureid_t findIdForTexture(const texture_t* tex)
{
    uint i;
    for(i = 0; i < textureIdMapSize; ++i)
    {
        if(textureIdMap[i] == tex)
            return (textureid_t)(i+1); // 1-based index.
    }
    return 0; // Not linked.
}

static texturenamespaceid_t namespaceIdForDirectoryNode(const PathDirectoryNode* node)
{
    return namespaceIdForDirectory(PathDirectoryNode_Directory(node));
}

/// @return  Newly composed path for @a node. Must be released with Str_Delete()
static ddstring_t* composePathForDirectoryNode(const PathDirectoryNode* node, char delimiter)
{
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, delimiter);
}

/// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
static Uri* composeUriForDirectoryNode(const PathDirectoryNode* node)
{
    const ddstring_t* namespaceName = Textures_NamespaceName(namespaceIdForDirectoryNode(node));
    ddstring_t* path = composePathForDirectoryNode(node, TEXTURES_PATH_DELIMITER);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(namespaceName));
    Str_Delete(path);
    return uri;
}

static void unlinkTextureFromIdMap(const texture_t* tex)
{
    if(!textureIdMapSize) return;

    if(tex != textureIdMap[textureIdMapSize-1])
    {
        textureid_t id = findIdForTexture(tex);
        if(!id) return; // Not linked.

        id -= 1; // 1-based index
        memmove(textureIdMap + id, textureIdMap + id + 1, sizeof *textureIdMap * (textureIdMapSize - 1 - id));
    }

    textureIdMap = (texture_t**) realloc(textureIdMap, sizeof *textureIdMap * --textureIdMapSize);
    if(!textureIdMap && textureIdMapSize)
        Con_Error("Textures::unlinkTextureFromIdMap: Failed on reallocation of %lu bytes when resizing list.", (unsigned long) sizeof *textureIdMap * textureIdMapSize);
}

/**
 * @defgroup validateTextureUriFlags  Validate Texture Uri Flags
 * @{
 */
#define VTUF_ALLOW_NAMESPACE_ANY 0x1 /// The Scheme component of the uri may be of zero-length; signifying "any namespace".
/**@}*/

/**
 * @param uri  Uri to be validated.
 * @param flags  @see validateTextureUriFlags
 * @param quiet  @c true= Do not output validation remarks to the log.
 * @return  @c true if @a Uri passes validation.
 */
static boolean validateTextureUri2(const Uri* uri, int flags, boolean quiet)
{
    texturenamespaceid_t namespaceId;

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Invalid path '%s' in Texture uri \"%s\".\n", Str_Text(Uri_Path(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
        }
        return false;
    }

    namespaceId = Textures_ParseNamespace(Str_Text(Uri_Scheme(uri)));
    if(!((flags & VTUF_ALLOW_NAMESPACE_ANY) && namespaceId == TN_ANY) &&
       !VALID_TEXTURENAMESPACEID(namespaceId))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Unknown namespace '%s' in Texture uri \"%s\".\n", Str_Text(Uri_Scheme(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
        }
        return false;
    }

    return true;
}

static boolean validateTextureUri(const Uri* uri, int flags)
{
    return validateTextureUri2(uri, flags, false);
}

/**
 * Given a directory and path, search the Textures collection for a match.
 *
 * @param directory  Namespace-specific PathDirectory to search in.
 * @param path  Path of the texture to search for.
 * @return  Found Texture else @c 0
 */
static texture_t* findTextureForPath(PathDirectory* texDirectory, const char* path)
{
    PathDirectoryNode* node = PathDirectory_Find(texDirectory,
        PCF_NO_BRANCH|PCF_MATCH_FULL, path, TEXTURES_PATH_DELIMITER);
    if(node)
    {
        return (texture_t*)PathDirectoryNode_UserData(node);
    }
    return NULL; // Not found.
}

/// \assume @a uri has already been validated and is well-formed.
static texture_t* findTextureForUri(const Uri* uri)
{
    texturenamespaceid_t namespaceId = Textures_ParseNamespace(Str_Text(Uri_Scheme(uri)));
    const char* path = Str_Text(Uri_Path(uri));
    texture_t* tex = NULL;

    if(namespaceId != TN_ANY)
    {
        // Caller wants a texture in a specific namespace.
        tex = findTextureForPath(getDirectoryForNamespaceId(namespaceId), path);
    }
    else
    {
        // Caller does not care which namespace.
        // Check for the texture in these namespaces in priority order.
        static const texturenamespaceid_t order[] = {
            TN_SPRITES,
            TN_TEXTURES,
            TN_FLATS,
            TN_PATCHES,
            TN_SYSTEM,
            TN_DETAILS,
            TN_REFLECTIONS,
            TN_MASKS,
            TN_MODELSKINS,
            TN_MODELREFLECTIONSKINS,
            TN_LIGHTMAPS,
            TN_FLAREMAPS,
        };
        int n = 0;
        do
        {
            tex = findTextureForPath(getDirectoryForNamespaceId(order[n]), path);
        } while(!tex && order[++n] != MN_ANY);
    }
    return tex;
}

static int destroyTexture(PathDirectoryNode* node, void* paramaters)
{
    texture_t* tex = (texture_t*)PathDirectoryNode_DetachUserData(node);
    if(tex)
    {
        GL_ReleaseGLTexturesByTexture(tex);
        unlinkTextureFromIdMap(tex);

        switch(Textures_Namespace(tex))
        {
        case TN_SYSTEM: {
            systex_t* sTex = (systex_t*)Texture_DetachUserData(tex);
            if(sTex)
            {
                Uri_Delete(sTex->external);
                free(sTex);
            }
            break;
          }
        case TN_FLATS: {
            flat_t* flat = (flat_t*)Texture_DetachUserData(tex);
            if(flat) free(flat);
            break;
          }
        case TN_TEXTURES: {
            patchcompositetex_t* pcTex = (patchcompositetex_t*)Texture_DetachUserData(tex);
            if(pcTex)
            {
                Str_Free(&pcTex->name);
                if(pcTex->patches) free(pcTex->patches);
                free(pcTex);
            }
            break;
          }
        case TN_SPRITES: {
            spritetex_t* sprTex = (spritetex_t*)Texture_DetachUserData(tex);
            if(sprTex) free(sprTex);
            break;
          }
        case TN_PATCHES: {
            patchtex_t* pTex = (patchtex_t*)Texture_DetachUserData(tex);
            if(pTex) free(pTex);
            break;
          }
        case TN_DETAILS: {
            detailtex_t* dTex = (detailtex_t*)Texture_DetachUserData(tex);
            if(dTex)
            {
                //if(dTex->path) Uri_Delete(dTex->path);
                free(dTex);
            }
            break;
          }
        case TN_REFLECTIONS: {
            shinytex_t* sTex = (shinytex_t*)Texture_DetachUserData(tex);
            if(sTex)
            {
                //if(sTex->external) Uri_Delete(sTex->external);
                free(sTex);
            }
            break;
          }
        case TN_MASKS: {
            masktex_t* mTex = (masktex_t*)Texture_DetachUserData(tex);
            if(mTex)
            {
                //if(mTex->external) Uri_Delete(mTex->external);
                free(mTex);
            }
            break;
          }
        case TN_MODELSKINS: // fall-through
        case TN_MODELREFLECTIONSKINS: {
            skinname_t* skin = (skinname_t*)Texture_DetachUserData(tex);
            if(skin)
            {
                Uri_Delete(skin->path);
                free(skin);
            }
            break;
          }
        case TN_LIGHTMAPS: {
            lightmap_t* lmap = (lightmap_t*)Texture_DetachUserData(tex);
            if(lmap)
            {
                //if(lmap->external) Uri_Delete(lmap->external);
                free(lmap);
            }
            break;
          }
        case TN_FLAREMAPS: {
            flaretex_t* fTex = (flaretex_t*)Texture_DetachUserData(tex);
            if(fTex)
            {
                //if(fTex->external) Uri_Delete(fTex->external);
                free(fTex);
            }
            break;
          }
        default:
            Con_Error("Textures::destroyTexture: Internal error, invalid namespace id %i.", (int)Textures_Namespace(tex));
            exit(1); // Unreachable.
        }
        Texture_Delete(tex);
    }
    return 0; // Continue iteration.
}

void Textures_Init(void)
{
    int i;

    textureIdMap = NULL;
    textureIdMapSize = 0;

    for(i = 0; i < TEXTURENAMESPACE_COUNT; ++i)
    {
        namespaces[i] = PathDirectory_New();
    }
}

void Textures_Shutdown(void)
{
    int i;

    Textures_Clear();

    for(i = 0; i < TEXTURENAMESPACE_COUNT; ++i)
    {
        PathDirectory_Delete(namespaces[i]), namespaces[i] = NULL;
    }

    // Clear the texture index/map.
    if(textureIdMap)
    {
        free(textureIdMap), textureIdMap = NULL;
    }
    textureIdMapSize = 0;
}

texturenamespaceid_t Textures_ParseNamespace(const char* str)
{
    if(!str || 0 == strlen(str)) return TN_ANY;

    if(!stricmp(str, TN_TEXTURES_NAME))             return TN_TEXTURES;
    if(!stricmp(str, TN_FLATS_NAME))                return TN_FLATS;
    if(!stricmp(str, TN_SPRITES_NAME))              return TN_SPRITES;
    if(!stricmp(str, TN_PATCHES_NAME))              return TN_PATCHES;
    if(!stricmp(str, TN_SYSTEM_NAME))               return TN_SYSTEM;
    if(!stricmp(str, TN_DETAILS_NAME))              return TN_DETAILS;
    if(!stricmp(str, TN_REFLECTIONS_NAME))          return TN_REFLECTIONS;
    if(!stricmp(str, TN_MASKS_NAME))                return TN_MASKS;
    if(!stricmp(str, TN_MODELSKINS_NAME))           return TN_MASKS;
    if(!stricmp(str, TN_MODELREFLECTIONSKINS_NAME)) return TN_MODELREFLECTIONSKINS;
    if(!stricmp(str, TN_LIGHTMAPS_NAME))            return TN_LIGHTMAPS;
    if(!stricmp(str, TN_FLAREMAPS_NAME))            return TN_FLAREMAPS;

    return TN_INVALID; // Unknown.
}

const ddstring_t* Textures_NamespaceName(texturenamespaceid_t id)
{
    static const ddstring_t namespaceNames[TEXTURENAMESPACE_COUNT+1] = {
        /* No namespace name */         { "" },
        /* TN_SYSTEM */                 { TN_SYSTEM_NAME },
        /* TN_FLATS */                  { TN_FLATS_NAME  },
        /* TN_TEXTURES */               { TN_TEXTURES_NAME },
        /* TN_SPRITES */                { TN_SPRITES_NAME },
        /* TN_PATCHES */                { TN_PATCHES_NAME },
        /* TN_DETAILS */                { TN_DETAILS_NAME },
        /* TN_REFLECTIONS */            { TN_REFLECTIONS_NAME },
        /* TN_MASKS */                  { TN_MASKS_NAME },
        /* TN_MODELSKINS */             { TN_MODELSKINS_NAME },
        /* TN_MODELREFLECTIONSKINS */   { TN_MODELREFLECTIONSKINS_NAME },
        /* TN_LIGHTMAPS */              { TN_LIGHTMAPS_NAME },
        /* TN_FLAREMAPS */              { TN_FLAREMAPS_NAME }
    };
    if(VALID_TEXTURENAMESPACEID(id))
        return namespaceNames + 1 + (id - TEXTURENAMESPACE_FIRST);
    return namespaceNames + 0;
}

uint Textures_Size(void)
{
    return textureIdMapSize;
}

uint Textures_Count(texturenamespaceid_t namespaceId)
{
    PathDirectory* texDirectory;
    if(!VALID_TEXTURENAMESPACEID(namespaceId) || !Textures_Size()) return 0;
    texDirectory = getDirectoryForNamespaceId(namespaceId);
    if(!texDirectory) return 0;
    return PathDirectory_Size(texDirectory);
}

void Textures_Clear(void)
{
    if(!Textures_Size()) return;

    Textures_ClearRuntime();
    Textures_ClearSystem();
}

void Textures_ClearRuntime(void)
{
    if(!Textures_Size()) return;

    Textures_ClearByNamespace(TN_FLATS);
    Textures_ClearByNamespace(TN_TEXTURES);
    Textures_ClearByNamespace(TN_PATCHES);
    Textures_ClearByNamespace(TN_SPRITES);
    Textures_ClearByNamespace(TN_DETAILS);
    Textures_ClearByNamespace(TN_REFLECTIONS);
    Textures_ClearByNamespace(TN_MASKS);
    Textures_ClearByNamespace(TN_MODELSKINS);
    Textures_ClearByNamespace(TN_MODELREFLECTIONSKINS);
    Textures_ClearByNamespace(TN_LIGHTMAPS);
    Textures_ClearByNamespace(TN_FLAREMAPS);

    GL_PruneTextureVariantSpecifications();
}

void Textures_ClearSystem(void)
{
    if(!Textures_Size()) return;

    Textures_ClearByNamespace(TN_SYSTEM);
    GL_PruneTextureVariantSpecifications();
}

void Textures_ClearByNamespace(texturenamespaceid_t texNamespace)
{
    texturenamespaceid_t from, to, iter;
    if(!Textures_Size()) return;

    if(texNamespace == TN_ANY)
    {
        from = TEXTURENAMESPACE_FIRST;
        to   = TEXTURENAMESPACE_LAST;
    }
    else if(VALID_TEXTURENAMESPACEID(texNamespace))
    {
        from = to = texNamespace;
    }
    else
    {
        Con_Error("Textures_ClearByNamespace: Invalid texture namespace %i.", (int) texNamespace);
        exit(1); // Unreachable.
    }

    for(iter = from; iter <= to; ++iter)
    {
        PathDirectory* texDirectory = getDirectoryForNamespaceId(iter);
        PathDirectory_Iterate(texDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, destroyTexture);
        PathDirectory_Clear(texDirectory);
    }
}

texture_t* Textures_ToTexture(textureid_t id)
{
    if(!validTextureId(id))
    {
#if _DEBUG
        if(id != 0)
        {
            Con_Message("Warning:Textures_ToTexture: Failed to locate texture for id #%i, returning NULL.\n", id);
        }
#endif
        return NULL;
    }
    return getTextureForId(id);
}

texture_t* Textures_TextureForUri2(const Uri* uri, boolean quiet)
{
    texture_t* tex;
    if(!uri || !Textures_Size()) return NULL;

    if(!validateTextureUri2(uri, VTUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning:Textures::TextureForUri: Uri \"%s\" failed to validate, returing NULL.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
#endif
        return NULL;
    }

    // Perform the search.
    tex = findTextureForUri(uri);
    if(tex) return tex;

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Textures::TextureForUri: \"%s\" not found!\n", Str_Text(path));
        Str_Delete(path);
    }
    return NULL;
}

texture_t* Textures_TextureForUri(const Uri* uri)
{
    return Textures_TextureForUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

texture_t* Textures_TextureForUriCString2(const char* path, boolean quiet)
{
    if(path && path[0])
    {
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        texture_t* tex = Textures_TextureForUri2(uri, quiet);
        Uri_Delete(uri);
        return tex;
    }
    return NULL;
}

texture_t* Textures_TextureForUriCString(const char* path)
{
    return Textures_TextureForUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

texture_t* Textures_CreateWithDimensions(const Uri* uri, int flags, int width, int height, void* userData)
{
    PathDirectory* texDirectory;
    PathDirectoryNode* node;
    texture_t* tex;
    assert(uri);

    // We require a properly formed uri.
    if(!validateTextureUri2(uri, 0, (verbose >= 1)))
    {
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning, failed to create Texture \"%s\", ignoring.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
        return NULL;
    }

    // Have we already created a texture for this?
    tex = findTextureForUri(uri);
    if(tex)
    {
        /// \todo Do not update textures here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:Textures::CreateWithDimensions: A Texture with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        Texture_SetFlags(tex, flags);
        Texture_SetDimensions(tex, width, height);
        Texture_AttachUserData(tex, userData);
        /// \fixme Materials and Surfaces should be notified of this!
        return tex;
    }

    // A new texture.
    texDirectory = getDirectoryForNamespaceId(Textures_ParseNamespace(Str_Text(Uri_Scheme(uri))));
    node = PathDirectory_Insert(texDirectory, Str_Text(Uri_Path(uri)), TEXTURES_PATH_DELIMITER);

    tex = Texture_NewWithDimensions(node, flags, width, height, userData);
    PathDirectoryNode_AttachUserData(node, tex);

    // Link the new texture into the id map.
    textureIdMap = (texture_t**) realloc(textureIdMap, sizeof *textureIdMap * ++textureIdMapSize);
    if(!textureIdMap)
        Con_Error("Textures::CreateWithDimensions: Failed on (re)allocation of %lu bytes enlarging Textures list.", (unsigned long) sizeof *textureIdMap * textureIdMapSize);
    textureIdMap[textureIdMapSize - 1] = tex;

    return tex;
}

texture_t* Textures_Create(const Uri* uri, int flags, void* userData)
{
    return Textures_CreateWithDimensions(uri, flags, 0, 0, userData);
}

textureid_t Textures_Id(texture_t* tex)
{
    if(!tex)
    {
#if _DEBUG
        Con_Message("Warning:Textures::Id: Attempted with invalid reference [%p], returning invalid id.\n", (void*)tex);
#endif
        return 0;
    }
    return findIdForTexture(tex);
}

texturenamespaceid_t Textures_Namespace(texture_t* tex)
{
    if(!tex)
    {
#if _DEBUG
        Con_Message("Warning:Textures::Namespace: Attempted with invalid reference [%p], returning invalid id.\n", (void*)tex);
#endif
        return TN_INVALID;
    }
    return namespaceIdForDirectoryNode(Texture_DirectoryNode(tex));
}

ddstring_t* Textures_ComposePath(texture_t* tex)
{
    if(!tex)
    {
#if _DEBUG
        Con_Message("Warning:Textures::ComposePath: Attempted with invalid reference [%p], returning null-object.\n", (void*)tex);
#endif
        return Str_New();
    }
    return composePathForDirectoryNode(Texture_DirectoryNode(tex), TEXTURES_PATH_DELIMITER);
}

Uri* Textures_ComposeUri(texture_t* tex)
{
    if(!tex)
    {
#if _DEBUG
        Con_Message("Warning:Textures::ComposeUri: Attempted with invalid reference [%p], returning null-object.\n", (void*)tex);
#endif
        return Uri_New();
    }
    return composeUriForDirectoryNode(Texture_DirectoryNode(tex));
}

typedef struct {
    int (*callback)(texture_t* tex, void* paramaters);
    void* paramaters;
} texturesiterateworker_params_t;

static int texturesIterateWorker(PathDirectoryNode* node, void* paramaters)
{
    texturesiterateworker_params_t* p = (texturesiterateworker_params_t*)paramaters;
    texture_t* tex = (texture_t*)PathDirectoryNode_UserData(node);
    assert(node && p);
    if(tex)
    {
        return p->callback(tex, p->paramaters);
    }
    return 0; // Continue iteration.
}

int Textures_Iterate2(texturenamespaceid_t texNamespace,
    int (*callback)(texture_t* tex, void* paramaters), void* paramaters)
{
    texturenamespaceid_t from, to, iter;
    texturesiterateworker_params_t p;
    int result = 0;

    if(!callback) return result;

    if(VALID_TEXTURENAMESPACEID(texNamespace))
    {
        from = to = texNamespace;
    }
    else
    {
        from = TEXTURENAMESPACE_FIRST;
        to   = TEXTURENAMESPACE_LAST;
    }

    p.callback = callback;
    p.paramaters = paramaters;
    for(iter = from; iter <= to; ++iter)
    {
        PathDirectory* texDirectory = getDirectoryForNamespaceId(iter);
        result = PathDirectory_Iterate2(texDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, texturesIterateWorker, (void*)&p);
        if(result) break;
    }

    return result;
}

int Textures_Iterate(texturenamespaceid_t texNamespace, int (*callback)(texture_t* tex, void* paramaters))
{
    return Textures_Iterate2(texNamespace, callback, NULL);
}

static void printTextureInfo(texture_t* tex)
{
    Uri* uri = Textures_ComposeUri(tex);
    ddstring_t* path = Uri_ToString(uri);
    //int variantIdx = 0;

    Con_Printf("Texture \"%s\" [%p] uid:%u origin:%s",
        F_PrettyPath(Str_Text(path)), (void*) tex, (uint) Textures_Id(tex),
        Texture_IsCustom(tex)? "addon" : "game");
    if(!Texture_IsNull(tex))
    {
        Con_Printf("\nDimensions: %d x %d\n", Texture_Width(tex), Texture_Height(tex));
    }
    else
    {
        Con_Printf(" (not yet loaded)\n");
    }

    //Texture_IterateVariants(tex, printVariantInfo, (void*)&variantIdx);

    Str_Delete(path);
    Uri_Delete(uri);
}

static void printTextureOverview(texture_t* tex, boolean printNamespace)
{
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Textures_Size()));
    Uri* uri = Textures_ComposeUri(tex);
    const ddstring_t* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));

    Con_FPrintf(Texture_IsNull(tex)? CPF_LIGHT : CPF_WHITE,
        "%-*s %*u %s\n", printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
        numUidDigits, (unsigned int) Textures_Id(tex), Texture_IsCustom(tex)? "addon" : "game");

    Uri_Delete(uri);
    if(printNamespace)
        Str_Delete((ddstring_t*)path);
}

/**
 * \todo A horridly inefficent algorithm. This should be implemented in PathDirectory
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the texture search/listing console commands
 * so is not hugely important right now.
 */
typedef struct {
    char delimiter;
    const char* like;
    int idx;
    PathDirectoryNode** storage;
} collectdirectorynodeworker_paramaters_t;

static int collectDirectoryNodeWorker(PathDirectoryNode* node, void* paramaters)
{
    collectdirectorynodeworker_paramaters_t* p = (collectdirectorynodeworker_paramaters_t*)paramaters;

    if(p->like && p->like[0])
    {
        ddstring_t* path = composePathForDirectoryNode(node, p->delimiter);
        int delta = strnicmp(Str_Text(path), p->like, strlen(p->like));
        Str_Delete(path);
        if(delta) return 0; // Continue iteration.
    }

    if(p->storage)
    {
        p->storage[p->idx++] = node;
    }
    else
    {
        ++p->idx;
    }

    return 0; // Continue iteration.
}

static PathDirectoryNode** collectDirectoryNodes(texturenamespaceid_t namespaceId,
    const char* like, int* count, PathDirectoryNode** storage)
{
    collectdirectorynodeworker_paramaters_t p;
    texturenamespaceid_t fromId, toId, iterId;

    if(VALID_TEXTURENAMESPACEID(namespaceId))
    {
        // Only consider textures in this namespace.
        fromId = toId = namespaceId;
    }
    else
    {
        // Consider textures in any namespace.
        fromId = TEXTURENAMESPACE_FIRST;
        toId   = TEXTURENAMESPACE_LAST;
    }

    p.delimiter = TEXTURES_PATH_DELIMITER;
    p.like = like;
    p.idx = 0;
    p.storage = storage;
    for(iterId  = fromId; iterId <= toId; ++iterId)
    {
        PathDirectory* texDirectory = getDirectoryForNamespaceId(iterId);
        PathDirectory_Iterate2(texDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
            PATHDIRECTORY_NOHASH, collectDirectoryNodeWorker, (void*)&p);
    }

    if(storage)
    {
        storage[p.idx] = 0; // Terminate.
        if(count) *count = p.idx;
        return storage;
    }

    if(p.idx == 0)
    {
        if(count) *count = 0;
        return NULL;
    }

    storage = (PathDirectoryNode**)malloc(sizeof *storage * (p.idx+1));
    if(!storage)
        Con_Error("Textures::collectDirectoryNodes: Failed on allocation of %lu bytes for new collection.", (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectDirectoryNodes(namespaceId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(const void* nodeA, const void* nodeB)
{
    ddstring_t* a = composePathForDirectoryNode(*(const PathDirectoryNode**)nodeA, TEXTURES_PATH_DELIMITER);
    ddstring_t* b = composePathForDirectoryNode(*(const PathDirectoryNode**)nodeB, TEXTURES_PATH_DELIMITER);
    int delta = stricmp(Str_Text(a), Str_Text(b));
    Str_Delete(b);
    Str_Delete(a);
    return delta;
}

/**
 * @defgroup printTextureFlags  Print Texture Flags
 * @{
 */
#define PTF_TRANSFORM_PATH_NO_NAMESPACE 0x1 /// Do not print the namespace.
/**@}*/

#define DEFAULT_PRINTTEXTUREFLAGS       0

/**
 * @param flags  @see printTextureFlags
 */
static size_t printTextures3(texturenamespaceid_t namespaceId, const char* like, int flags)
{
    const boolean printNamespace = !(flags & PTF_TRANSFORM_PATH_NO_NAMESPACE);
    int numFoundDigits, numUidDigits, idx, count = 0;
    PathDirectoryNode **foundTextures = collectDirectoryNodes(namespaceId, like, &count, NULL);
    PathDirectoryNode** iter;

    if(!foundTextures) return 0;

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known textures in namespace '%s'", Str_Text(Textures_NamespaceName(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known textures");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Textures_Size()));
    Con_Printf(" %*s: %-*s %*s origin\n", numFoundDigits, "idx",
        printNamespace? 22 : 14, printNamespace? "namespace:path" : "path",
        numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundTextures, (size_t)count, sizeof *foundTextures, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundTextures; *iter; ++iter)
    {
        const PathDirectoryNode* node = *iter;
        texture_t* tex = (texture_t*) PathDirectoryNode_UserData(node);
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printTextureOverview(tex, printNamespace);
    }

    free(foundTextures);
    return count;
}

static void printTextures2(texturenamespaceid_t namespaceId, const char* like, int flags)
{
    size_t printTotal = 0;
    // Do we care which namespace?
    if(namespaceId == TN_ANY && like && like[0])
    {
        printTotal = printTextures3(namespaceId, like, flags & ~PTF_TRANSFORM_PATH_NO_NAMESPACE);
        Con_PrintRuler();
    }
    // Only one namespace to print?
    else if(VALID_TEXTURENAMESPACEID(namespaceId))
    {
        printTotal = printTextures3(namespaceId, like, flags | PTF_TRANSFORM_PATH_NO_NAMESPACE);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each namespace separately.
        int i;
        for(i = TEXTURENAMESPACE_FIRST; i <= TEXTURENAMESPACE_LAST; ++i)
        {
            size_t printed = printTextures3((texturenamespaceid_t)i, like, flags| PTF_TRANSFORM_PATH_NO_NAMESPACE);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Texture" : "Textures");
}

static void printTextures(texturenamespaceid_t namespaceId, const char* like)
{
    printTextures2(namespaceId, like, DEFAULT_PRINTTEXTUREFLAGS);
}

D_CMD(ListTextures)
{
    texturenamespaceid_t namespaceId = TN_ANY;
    const char* like = NULL;
    Uri* uri = NULL;

    if(!Textures_Size())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    // "listtextures [namespace] [name]"
    if(argc > 2)
    {
        uri = Uri_New();
        Uri_SetScheme(uri, argv[1]);
        Uri_SetPath(uri, argv[2]);

        namespaceId = Textures_ParseNamespace(Str_Text(Uri_Scheme(uri)));
        if(!VALID_TEXTURENAMESPACEID(namespaceId))
        {
            Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(uri)));
            Uri_Delete(uri);
            return false;
        }
        like = Str_Text(Uri_Path(uri));
    }
    // "listtextures [namespace:name]" i.e., a partial Uri
    else if(argc > 1)
    {
        uri = Uri_NewWithPath2(argv[1], RC_NULL);
        if(!Str_IsEmpty(Uri_Scheme(uri)))
        {
            namespaceId = Textures_ParseNamespace(Str_Text(Uri_Scheme(uri)));
            if(!VALID_TEXTURENAMESPACEID(namespaceId))
            {
                Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(uri)));
                Uri_Delete(uri);
                return false;
            }

            if(!Str_IsEmpty(Uri_Path(uri)))
                like = Str_Text(Uri_Path(uri));
        }
        else
        {
            namespaceId = Textures_ParseNamespace(Str_Text(Uri_Path(uri)));
            if(!VALID_TEXTURENAMESPACEID(namespaceId))
            {
                namespaceId = TN_ANY;
                like = argv[1];
            }
        }
    }

    printTextures(namespaceId, like);

    if(uri) Uri_Delete(uri);
    return true;
}

D_CMD(InspectTexture)
{
    Uri* search = Uri_NewWithPath2(argv[1], RC_NULL);
    texture_t* tex;

    if(!Str_IsEmpty(Uri_Scheme(search)))
    {
        texturenamespaceid_t namespaceId = Textures_ParseNamespace(Str_Text(Uri_Scheme(search)));
        if(!VALID_TEXTURENAMESPACEID(namespaceId))
        {
            Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(search)));
            Uri_Delete(search);
            return false;
        }
    }

    tex = Textures_TextureForUri(search);
    if(tex)
    {
        printTextureInfo(tex);
    }
    else
    {
        ddstring_t* path = Uri_ToString(search);
        Con_Printf("Unknown texture \"%s\".\n", Str_Text(path));
        Str_Delete(path);
    }
    Uri_Delete(search);
    return true;
}

#if _DEBUG
D_CMD(PrintTextureStats)
{
    texturenamespaceid_t namespaceId;

    if(!Textures_Size())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Texture Statistics:\n");
    for(namespaceId = TEXTURENAMESPACE_FIRST; namespaceId <= TEXTURENAMESPACE_LAST; ++namespaceId)
    {
        PathDirectory* texDirectory = getDirectoryForNamespaceId(namespaceId);
        uint size;

        if(!texDirectory) continue;

        size = PathDirectory_Size(texDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(Textures_NamespaceName(namespaceId)), size, size==1? "texture":"textures");
        PathDirectory_PrintHashDistribution(texDirectory);
        PathDirectory_Print(texDirectory, TEXTURES_PATH_DELIMITER);
    }
    return true;
}
#endif
