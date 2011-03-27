/**\file p_materialmanager.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Materials collection, namespaces, bindings and other management.
 */

#include <ctype.h> // For tolower()

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

#include "blockset.h"
#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"

#define MATERIALS_BLOCK_ALLOC (32) // Num materials to allocate per block.
#define MATERIALNAMESPACE_HASH_SIZE (512)

typedef struct materialvariantspecificationlist_node_s {
    struct materialvariantspecificationlist_node_s* next;
    materialvariantspecification_t* spec;
} materialvariantspecificationlist_node_t;

typedef materialvariantspecificationlist_node_t variantspecificationlist_t;

typedef struct materiallist_node_s {
    struct materiallist_node_s* next;
    material_t* mat;
} materiallist_node_t;

typedef materiallist_node_t materiallist_t;

typedef struct variantcachequeue_node_s {
    struct variantcachequeue_node_s* next;
    materialvariantspecification_t* spec;
    material_t* mat;
} variantcachequeue_node_t;

typedef variantcachequeue_node_t variantcachequeue_t;

typedef struct materialbind_s {
    char            name[9];
    material_t*     mat;
    materialnamespaceid_t mnamespace;
    byte            prepared;
    struct ded_detailtexture_s* detail[2];
    struct ded_decor_s* decoration[2];
    struct ded_ptcgen_s* ptcGen[2];
    struct ded_reflection_s* reflection[2];

    uint            hashNext; // 1-based index
} materialbind_t;

typedef struct animframe_s {
    material_t*     mat;
    ushort          tics;
    ushort          random;
} animframe_t;

typedef struct animgroup_s {
    int             id;
    int             flags;
    int             index;
    int             maxTimer;
    int             timer;
    int             count;
    animframe_t*    frames;
} animgroup_t;

static int numgroups;
static animgroup_t* groups;

D_CMD(ListMaterials);

static void animateAnimGroups(void);

extern boolean ddMapSetup;

materialnum_t numMaterialBinds = 0;

static boolean initedOk = false;
static variantspecificationlist_t* variantSpecs;

static variantcachequeue_t* variantCacheQueue;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the materials themselves and in of the system itself:
 *
 * 1) Pointers to Material are eternal, they are always valid and continue
 *    to reference the same logical material data even after engine reset.
 * 2) Public material identifiers (materialnum_t) are similarly eternal.
 *    Note that they are used to index the material name bindings array.
 * 3) Dynamic creation/update of materials.
 * 4) Material name bindings are semi-independant from the materials. There
 *    may be multiple name bindings for a given material (aliases).
 *    The only requirement is that the name is unique among materials in
 *    a given material mnamespace.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (a hashing scheme is used).
 */
static blockset_t* materialsBlockSet;
static materiallist_t* materials;

static materialbind_t* materialBinds;
static materialnum_t maxMaterialBinds;
static uint hashTable[MATERIALNAMESPACE_COUNT][MATERIALNAMESPACE_HASH_SIZE];

void P_MaterialsRegister(void)
{
    C_CMD("listmaterials",  NULL,     ListMaterials);
}

static const ddstring_t* nameForMaterialNamespaceId(materialnamespaceid_t id)
{
    static const ddstring_t emptyString = { "" };
    static const ddstring_t namespaces[MATERIALNAMESPACE_COUNT] = {
        /* MN_SYSTEM */     { MN_SYSTEM_NAME },
        /* MN_FLATS */      { MN_FLATS_NAME },
        /* MN_TEXTURES */   { MN_TEXTURES_NAME },
        /* MN_SPRITES */    { MN_SPRITES_NAME }
    };
    if(VALID_MATERIALNAMESPACE(id))
        return &namespaces[id-MATERIALNAMESPACE_FIRST];
    return &emptyString;
}

static materialnamespaceid_t materialNamespaceIdForTextureNamespaceId(texturenamespaceid_t id)
{
    static const materialnamespaceid_t namespaceIds[TEXTURENAMESPACE_COUNT] = {
        /* TN_SYSTEM */    { MN_SYSTEM },
        /* TN_FLATS */     { MN_FLATS },
        /* TN_TEXTURES */  { MN_TEXTURES },
        /* TN_SPRITES */   { MN_SPRITES },
        /* TN_PATCHES */   { MN_ANY } // No materials for these yet.
    };
    if(VALID_TEXTURENAMESPACE(id))
        return namespaceIds[id-TEXTURENAMESPACE_FIRST];
    return MATERIALNAMESPACE_COUNT; // Unknown.
}

static animgroup_t* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups)
        return NULL;

    return &groups[number];
}

static boolean isInAnimGroup(animgroup_t* group, const material_t* mat)
{
    if(mat && group)
    {
        int i;
        for(i = 0; i < group->count; ++i)
            if(group->frames[i].mat == mat)
                return true;
    }
    return false;
}

static materialvariantspecification_t* copyVariantSpecification(
    const materialvariantspecification_t* tpl)
{
    materialvariantspecification_t* spec;
    if(NULL == (spec = (materialvariantspecification_t*) malloc(sizeof(*spec))))
        Con_Error("Materials::copyVariantSpecification: Failed on allocation of "
                  "%lu bytes for new MaterialVariantSpecification.",
                  (unsigned int) sizeof(*spec));
    memcpy(spec, tpl, sizeof(materialvariantspecification_t));
    return spec;
}

static int compareVariantSpecifications(const materialvariantspecification_t* a,
    const materialvariantspecification_t* b)
{
    if(a->context != b->context)
        return 1;
    return GL_CompareTextureVariantSpecifications(a->primarySpec, b->primarySpec);
}

static materialvariantspecification_t* applyVariantSpecification(
    materialvariantspecification_t* spec, materialvariantusagecontext_t mc,
    texturevariantspecification_t* primarySpec)
{
    assert(spec && (mc == MC_UNKNOWN || VALID_MATERIALVARIANTUSAGECONTEXT(mc)) && primarySpec);
    spec->context = mc;
    spec->primarySpec = primarySpec;
    return spec;
}

static materialvariantspecification_t* linkVariantSpecification(
    materialvariantspecification_t* spec)
{
    assert(initedOk && spec);
    {
    materialvariantspecificationlist_node_t* node;
    if(NULL == (node = (materialvariantspecificationlist_node_t*) malloc(sizeof(*node))))
        Con_Error("Materials::linkVariantSpecification: Failed on allocation of "
                  "%lu bytes for new MaterialVariantSpecificationListNode.",
                  (unsigned int) sizeof(*node));
    node->spec = spec;
    node->next = variantSpecs;
    variantSpecs = (variantspecificationlist_t*)node;
    return spec;
    }
}

static materialvariantspecification_t* findVariantSpecification(
    const materialvariantspecification_t* tpl, boolean canCreate)
{
    assert(initedOk && tpl);
    {
    materialvariantspecificationlist_node_t* node;
    for(node = variantSpecs; node; node = node->next)
    {
        if(!compareVariantSpecifications(node->spec, tpl))
            return node->spec;
    }
    if(!canCreate)
        return NULL;
    return linkVariantSpecification(copyVariantSpecification(tpl));
    }
}

static materialvariantspecification_t* getVariantSpecificationForContext(
    materialvariantusagecontext_t mc, int flags, byte border, int tClass, int tMap)
{
    static materialvariantspecification_t tpl;
    assert(initedOk && (mc == MC_UNKNOWN || VALID_MATERIALVARIANTUSAGECONTEXT(mc)));
    {
    texturevariantspecification_t* primarySpec;
    texturevariantusagecontext_t primaryContext;
    switch(mc)
    {
    case MC_UI:             primaryContext = TC_UI;                 break;
    case MC_MAPSURFACE:     primaryContext = TC_MAPSURFACE_DIFFUSE; break;
    case MC_SPRITE:         primaryContext = TC_SPRITE_DIFFUSE;     break;
    case MC_MODELSKIN:      primaryContext = TC_MODELSKIN_DIFFUSE;  break;
    case MC_PSPRITE:        primaryContext = TC_PSPRITE_DIFFUSE;    break;
    case MC_SKYSPHERE:      primaryContext = TC_SKYSPHERE_DIFFUSE;  break;
    default:                primaryContext = TC_UNKNOWN;            break;
    }
    primarySpec = GL_TextureVariantSpecificationForContext(primaryContext, flags, border, tClass, tMap);
    applyVariantSpecification(&tpl, mc, primarySpec);
    return findVariantSpecification(&tpl, true);
    }
}

static void destroyVariantSpecifications(void)
{
    assert(initedOk);
    while(variantSpecs)
    {
        materialvariantspecificationlist_node_t* next = variantSpecs->next;
        free(variantSpecs->spec);
        free(variantSpecs);
        variantSpecs = next;
    }
}

typedef struct {
    const materialvariantspecification_t* spec;
    materialvariant_t* chosen;
} choosevariantworker_paramaters_t;

static int chooseVariantWorker(materialvariant_t* variant, void* paramaters)
{
    assert(initedOk && variant && paramaters);
    {
    choosevariantworker_paramaters_t* p = (choosevariantworker_paramaters_t*) paramaters;
    const materialvariantspecification_t* cand = MaterialVariant_Spec(variant);
    if(!compareVariantSpecifications(cand, p->spec))
    {   // This will do fine.
        p->chosen = variant;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
    }
}

static materialvariant_t* chooseVariant(material_t* mat,
    const materialvariantspecification_t* spec)
{
    assert(initedOk && mat && spec);
    {
    choosevariantworker_paramaters_t params;
    params.spec = spec;
    params.chosen = NULL;
    Material_IterateVariants(mat, chooseVariantWorker, &params);
    return params.chosen;
    }
}

static __inline materialbind_t* bindForMaterial(const material_t* mat)
{
    uint bindId = Material_BindId(mat);
    if(0 != bindId) return &materialBinds[bindId-1];
    return 0;
}

static int compareMaterialBindByName(const void* e1, const void* e2)
{
    return stricmp((*(const materialbind_t**)e1)->name, (*(const materialbind_t**)e2)->name);
}

/**
 * This is a hash function. Given a material name it generates a
 * somewhat-random number between 0 and MATERIALNAMESPACE_HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashForName(const char* name)
{
    ushort              key = 0;
    int                 i;

    // Stop when the name ends.
    for(i = 0; *name; ++i, name++)
    {
        if(i == 0)
            key ^= (int) (*name);
        else if(i == 1)
            key *= (int) (*name);
        else if(i == 2)
        {
            key -= (int) (*name);
            i = -1;
        }
    }

    return key % MATERIALNAMESPACE_HASH_SIZE;
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the material to search for. Must have been
 *                      transformed to all lower case.
 * @param mnamespace    Specific MG_* material namespace NOT @c MN_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForName(const char* name, uint hash,
    materialnamespaceid_t mnamespace)
{
    // Go through the candidates.
    if(hashTable[mnamespace-MATERIALNAMESPACE_FIRST][hash])
    {
        materialbind_t* mb = &materialBinds[hashTable[mnamespace-MATERIALNAMESPACE_FIRST][hash] - 1];
        for(;;)
        {
            material_t* mat = mb->mat;
            if(!strncmp(mb->name, name, 8))
                return ((mb) - materialBinds) + 1;
            if(!mb->hashNext)
                break;

            mb = &materialBinds[mb->hashNext - 1];
        }
    }
    return 0; // Not found.
}

static void newMaterialNameBinding(material_t* mat, const char* name,
    materialnamespaceid_t mnamespace, uint hash)
{
    materialbind_t* mb;

    if(++numMaterialBinds > maxMaterialBinds)
    {   // Allocate more memory.
        maxMaterialBinds += MATERIALS_BLOCK_ALLOC;
        materialBinds = (materialbind_t*) realloc(materialBinds, sizeof(*materialBinds) * maxMaterialBinds);
    }

    // Add the new material to the end.
    mb = &materialBinds[numMaterialBinds - 1];
    memset(mb, 0, sizeof(*mb));
    strncpy(mb->name, name, 8);
    mb->name[8] = '\0';
    mb->mat = mat;
    mb->mnamespace = mnamespace;
    mb->prepared = 0;
    // We also hash the name for faster searching.
    mb->hashNext = hashTable[mnamespace-MATERIALNAMESPACE_FIRST][hash];
    hashTable[mnamespace-MATERIALNAMESPACE_FIRST][hash] = (mb - materialBinds) + 1;

    Material_SetBindId(mat, (mb - materialBinds) + 1);
}

static material_t* allocMaterial(void)
{
    material_t* mat = BlockSet_Allocate(materialsBlockSet);
    Material_Initialize(mat);
    return mat;
}

/**
 * Link the material into the global list of materials.
 * \assume material is NOT already present in the global list.
 */
static material_t* linkMaterialToGlobalList(material_t* mat)
{
    materiallist_node_t* node;
    if(NULL == (node = malloc(sizeof(*node))))
        Con_Error("linkMaterialToGlobalList: Failed on allocation of %lu bytes for "
                  "new MaterialList::Node.", (unsigned int) sizeof(*node));
    node->mat = mat;
    node->next = materials;
    materials = node;
    return mat;
}

static material_t* createMaterial(int width, int height, short flags,
    material_env_class_t envClass, const texture_t* tex, ded_material_t* def)
{
    assert(def);
    {
    material_t* mat = linkMaterialToGlobalList(allocMaterial());

    mat->_flags = flags;
    mat->_isCustom = !Texture_IsFromIWAD(tex);
    mat->_width = width;
    mat->_height = height;
    mat->_envClass = envClass;
    mat->_def = def;
    return mat;
    }
}

static material_t* getMaterialByNum(materialnum_t num)
{
    if(num < numMaterialBinds)
        return materialBinds[num].mat;
    return 0;
}

void Materials_Initialize(void)
{
    if(initedOk)
        return; // Already been here.

    variantSpecs = NULL;
    variantCacheQueue = NULL;

    materialsBlockSet = BlockSet_Construct(sizeof(material_t), MATERIALS_BLOCK_ALLOC);
    materials = NULL;

    materialBinds = NULL;
    numMaterialBinds = maxMaterialBinds = 0;

    // Clear the name bind hash tables.
    memset(hashTable, 0, sizeof(hashTable));

    initedOk = true;
}

static int destroyVariant(materialvariant_t* variant, void* paramaters)
{
    MaterialVariant_Destruct(variant);
    return 1; // Continue iteration.
}

void Materials_Shutdown(void)
{
    if(!initedOk)
        return;

    Materials_PurgeCacheQueue();

    while(materials)
    {
        materiallist_node_t* next = materials->next;
        Material_DestroyVariants(materials->mat);
        free(materials);
        materials = next;
    }

    BlockSet_Destruct(materialsBlockSet);
    materialsBlockSet = NULL;

    destroyVariantSpecifications();

    // Destroy the bindings.
    if(materialBinds)
    {
        free(materialBinds);
        materialBinds = NULL;
    }
    numMaterialBinds = maxMaterialBinds = 0;

    initedOk = false;
}

void Materials_LinkAssociatedDefinitions(void)
{
    uint i;
    for(i = 0; i < numMaterialBinds; ++i)
    {
        materialbind_t* mb = &materialBinds[i];
        if(mb->mnamespace == MN_SYSTEM)
            continue;

        // Surface decorations (lights and models).
        mb->decoration[0] = Def_GetDecoration(mb->mat, 0);
        mb->decoration[1] = Def_GetDecoration(mb->mat, 1);

        // Reflection (aka shiny surface).
        mb->reflection[0] = Def_GetReflection(mb->mat, 0);
        mb->reflection[1] = Def_GetReflection(mb->mat, 1);

        // Generator (particles).
        mb->ptcGen[0] = Def_GetGenerator(mb->mat, 0);
        mb->ptcGen[1] = Def_GetGenerator(mb->mat, 1);

        // Detail texture.
        mb->detail[0] = Def_GetDetailTex(mb->mat, 0);
        mb->detail[1] = Def_GetDetailTex(mb->mat, 1);
    }
}

void Materials_PurgeCacheQueue(void)
{
    if(!initedOk)
        Con_Error("Materials::PurgeCacheQueue: Materials collection not yet initialized.");
    while(variantCacheQueue)
    {
        variantcachequeue_node_t* next = variantCacheQueue->next;
        free(variantCacheQueue);
        variantCacheQueue = next;
    }
}

void Materials_ProcessCacheQueue(void)
{
    if(!initedOk)
        Con_Error("Materials::PurgeCacheQueue: Materials collection not yet initialized.");
    while(variantCacheQueue)
    {
        variantcachequeue_node_t* next = variantCacheQueue->next;
        Materials_Prepare(NULL, variantCacheQueue->mat, true, variantCacheQueue->spec);
        free(variantCacheQueue);
        variantCacheQueue = next;
    }
}

static int releaseGLTexturesForMaterialWorker(materialvariant_t* variant,
    void* paramaters)
{
    assert(variant);
    {
    int i, layerCount = Material_LayerCount(MaterialVariant_GeneralCase(variant));
    for(i = 0; i < layerCount; ++i)
    {
        const materialvariant_layer_t* ml = MaterialVariant_Layer(variant, i);
        GL_ReleaseGLTexturesForTexture(GL_ToTexture(ml->tex));
    }
    return 0; // Continue iteration.
    }
}

static void releaseGLTexturesForMaterial(material_t* mat)
{
    Material_IterateVariants(mat, releaseGLTexturesForMaterialWorker, NULL);
}

void Materials_DeleteGLTextures(const char* namespaceName)
{
    materialnamespaceid_t matNamespace = MN_ANY;

    if(namespaceName && namespaceName[0])
    {
        matNamespace = DD_ParseMaterialNamespace(namespaceName);
        if(!VALID_MATERIALNAMESPACE(matNamespace))
        {
#if _DEBUG
            Con_Message("Warning:Materials_DeleteGLTextures: Attempt to delete in "
                "unknown namespace (%s), ignoring.\n", namespaceName);
#endif
            return;
        }
    }

    if(matNamespace == MN_ANY)
    {   // Delete the lot.
        GL_ReleaseGLTexturesByNamespace(TN_ANY);
        return;
    }

    if(!VALID_MATERIALNAMESPACE(matNamespace))
        Con_Error("Materials_DeleteGLTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) matNamespace);

    if(materialBinds)
    {
        uint i;
        for(i = 0; i < MATERIALNAMESPACE_HASH_SIZE; ++i)
            if(hashTable[matNamespace-MATERIALNAMESPACE_FIRST][i])
            {
                materialbind_t* mb = &materialBinds[hashTable[matNamespace-MATERIALNAMESPACE_FIRST][i] - 1];

                for(;;)
                {
                    releaseGLTexturesForMaterial(mb->mat);

                    if(!mb->hashNext)
                        break;
                    mb = &materialBinds[mb->hashNext - 1];
                }
            }
    }
}

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace)
{
    return nameForMaterialNamespaceId(materialNamespaceIdForTextureNamespaceId(texNamespace));
}

/**
 * Given a unique material number return the associated material.
 *
 * @param num           Unique material number.
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* Materials_ToMaterial(materialnum_t num)
{
    if(!initedOk)
        return NULL;
    if(num != 0)
        return getMaterialByNum(num - 1); // 1-based index.
    return NULL;
}

/**
 * Retrieve the unique material number for the given material.
 *
 * @param mat           The material to retrieve the unique number of.
 *
 * @return              The associated unique number.
 */
materialnum_t Materials_ToMaterialNum(material_t* mat)
{
    if(mat)
    {
        materialbind_t* mb;
        if((mb = bindForMaterial(mat)))
            return (mb - materialBinds) + 1; // 1-based index.
    }
    return 0;
}

material_t* Materials_New(const dduri_t* rawName, int width, int height,
    short flags, textureid_t tex, int texOriginX, int texOriginY)
{
    materialnamespaceid_t mnamespace;
    material_env_class_t envClass;
    materialnum_t oldMat;
    material_t* mat;
    char name[9];
    uint hash;
    int n;

    if(!initedOk)
        return NULL;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || Str_IsEmpty(Uri_Path(rawName)) || !Str_CompareIgnoreCase(Uri_Path(rawName), "-"))
    {
#if _DEBUG
Con_Message("Materials_New: Warning, attempted to create material with "
            "NULL name\n.");
#endif
        return NULL;
    }

    // Prepare 'name'.
    { const char* c = Str_Text(Uri_Path(rawName));
    for(n = 0; *c && n < 8; ++n, c++)
        name[n] = tolower(*c);
    }
    name[n] = '\0';
    hash = hashForName(name);

    mnamespace = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(rawName)));
    // Check if we've already created a material for this.
    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace. This is only valid if we
        // can find a material by this name using a priority search order.
        oldMat = getMaterialNumForName(name, hash, MN_SPRITES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_TEXTURES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_FLATS);
    }
    else
    {
        if(!VALID_MATERIALNAMESPACE(mnamespace))
        {
#if _DEBUG
Con_Message("Materials_New: Warning, attempted to create material in "
            "unknown namespace '%i'.\n", (int) mnamespace);
#endif
            return NULL;
        }

        oldMat = getMaterialNumForName(name, hash, mnamespace);
    }

    envClass = S_MaterialClassForName(rawName);

    if(oldMat)
    {   // We are updating an existing material.
        materialbind_t* mb = &materialBinds[oldMat - 1];
        material_t* mat = mb->mat;

        // Update the (possibly new) meta data.
        mat->_flags = flags;
        if(tex)
            mat->layers[0].tex = tex;
        if(width > 0)
            mat->_width = width;
        if(height > 0)
            mat->_height = height;
        //mat->def = 0;
        mat->_envClass = envClass;
        mat->_inAnimGroup = false;

        mat->_isCustom = false;
        { uint i;
        for(i = 0; i < mat->numLayers; ++i)
        {
            if(Texture_IsFromIWAD(GL_ToTexture(mat->layers[i].tex)))
                continue;
            mat->_isCustom = true;
            break;
        }}

        Materials_ClearTranslation(mb->mat);
        return mat;
    }

    if(mnamespace == MN_ANY)
    {
#if _DEBUG
        Con_Message("Warning: Attempted to create Material in unknown namespace, ignoring.\n");
#endif
        return NULL;
    }

    /**
     * A new material.
     */

    // Only create complete materials.
    // \todo Doing this here isn't ideal.
    if(tex == 0 || !(width > 0) || !(height > 0))
        return NULL;

    mat = createMaterial(width, height, flags, envClass, GL_ToTexture(tex), NULL);
    newMaterialNameBinding(mat, name, mnamespace, hash);
    return mat;
}

material_t* Materials_NewFromDef(ded_material_t* def)
{
    assert(def);
    {
    materialnamespaceid_t mnamespace = MN_ANY; // No change.
    const texture_t* tex = NULL; // No change.
    float width = -1, height = -1; // No change.
    material_env_class_t envClass;
    const dduri_t* rawName;
    materialnum_t oldMat;
    material_t* mat;
    char name[9];
    byte flags;
    uint hash;

    if(!initedOk)
        return 0;

    // Sanitize so that when updating we only change what is requested.
    if(def->width > 0)
        width = MAX_OF(1, def->width);
    if(def->height > 0)
        height = MAX_OF(1, def->height);
    rawName = def->id;
    flags = def->flags;

    if(def->layers[0].stageCount.num > 0)
    {
        const ded_material_layer_t* l = &def->layers[0];
        if(l->stages[0].texture) // Not unused.
        {
            if(!(tex = GL_TextureByUri(l->stages[0].texture)))
            {
                ddstring_t* materialPath = Uri_ToString(def->id);
                ddstring_t* texturePath = Uri_ToString(l->stages[0].texture);
                VERBOSE( Con_Message("Warning: Unknown texture '%s' in Material '%s' (layer %i stage %i).\n",
                         Str_Text(texturePath), Str_Text(materialPath), 0, 0) );
                Str_Delete(materialPath);
                Str_Delete(texturePath);
            }
        }
    }

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || Str_IsEmpty(Uri_Path(rawName)) || !Str_CompareIgnoreCase(Uri_Path(rawName), "-"))
    {
#if _DEBUG
ddstring_t* path = Uri_ToString(rawName);
Con_Message("Warning: Attempted to create/update Material with invalid path \"%s\", ignoring.\n", Str_Text(path));
Str_Delete(path);
#endif
        return 0;
    }

    // Prepare 'name'.
    { const char* c = Str_Text(Uri_Path(rawName));
    int n;
    for(n = 0; *c && n < 8; ++n, ++c)
        name[n] = tolower(*c);
    name[n] = '\0';
    }
    hash = hashForName(name);

    mnamespace = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(rawName)));
    // Check if we've already created a material for this.
    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace. This is only valid if we
        // can find a material by this name using a priority search order.
        oldMat = getMaterialNumForName(name, hash, MN_SPRITES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_TEXTURES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_FLATS);
    }
    else
    {
        if(!VALID_MATERIALNAMESPACE(mnamespace))
        {
#if _DEBUG
Con_Message("Warning: Attempted to create/update Material in unknown Namespace '%i', ignoring.\n",
            (int) mnamespace);
#endif
            return 0;
        }

        oldMat = getMaterialNumForName(name, hash, mnamespace);
    }

    envClass = S_MaterialClassForName(rawName);

    if(oldMat)
    {   // We are updating an existing material.
        materialbind_t* mb = &materialBinds[oldMat - 1];
        material_t* mat = mb->mat;

        // Update the (possibly new) meta data.
        mat->_flags = flags;
        if(tex)
            mat->layers[0].tex = Texture_Id(tex);
        if(width > 0)
            mat->_width = width;
        if(height > 0)
            mat->_height = height;
        mat->_def = def;
        mat->_envClass = envClass;
        mat->_inAnimGroup = false;

        mat->_isCustom = false;
        { uint i;
        for(i = 0; i < mat->numLayers; ++i)
        {
            if(Texture_IsFromIWAD(GL_ToTexture(mat->layers[0].tex)))
                continue;
            mat->_isCustom = true;
            break;
        }}

        Materials_ClearTranslation(mb->mat);
        return mat;
    }

    if(mnamespace == MN_ANY)
    {
#if _DEBUG
        Con_Message("Warning: Attempted to create Material in unknown namespace, ignoring.\n");
#endif
        return NULL;
    }

    /**
     * A new material.
     */

    // Only create complete materials.
    // \todo Doing this here isn't ideal.
    if(tex == 0 || !(width > 0) || !(height > 0))
        return NULL;

    mat = createMaterial(width, height, flags, envClass, tex, def);
    newMaterialNameBinding(mat, name, mnamespace, hash);
    return mat;
    }
}

static materialnum_t Materials_CheckNumForPath2(const dduri_t* uri)
{
    assert(initedOk && uri);
    {
    const char* rawName = Str_Text(Uri_Path(uri));
    materialnamespaceid_t mnamespace;
    uint hash;
    char name[9];

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(Str_IsEmpty(Uri_Path(uri)) || !Str_CompareIgnoreCase(Uri_Path(uri), "-"))
        return 0;

    mnamespace = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
    if(mnamespace != MN_ANY && !VALID_MATERIALNAMESPACE(mnamespace))
    {
#if _DEBUG
Con_Message("Materials_ToMaterial2: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return 0;
    }

    // Prepare 'name'.
    { int n;
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    }
    hash = hashForName(name);

    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace.
        materialnum_t       matNum;

        // Check for the material in these namespaces, in priority order.
        if((matNum = getMaterialNumForName(name, hash, MN_SPRITES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_TEXTURES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_FLATS)))
            return matNum;

        return 0; // Not found.
    }

    // Caller wants a material in a specific namespace.
    return getMaterialNumForName(name, hash, mnamespace);
    }
}

static materialnum_t Materials_NumForPath2(const dduri_t* path)
{
    materialnum_t result;
    if(!initedOk)
        return 0;
    result = Materials_CheckNumForPath2(path);
    // Not found?
    if(verbose && result == 0 && !ddMapSetup) // Don't announce during map setup.
    {
        ddstring_t* nicePath = Uri_ToString(path);
        Con_Message("Materials::NumForName: \"%s\" not found!\n", Str_Text(nicePath));
        Str_Delete(nicePath);
    }
    return result;
}

materialnum_t Materials_IndexForUri(const dduri_t* path)
{
    if(path)
    {
        return Materials_CheckNumForPath2(path);
    }
    return 0;
}

materialnum_t Materials_IndexForName(const char* path)
{
    if(path && path[0])
    {
        dduri_t* uri = Uri_Construct2(path, RC_NULL);
        materialnum_t result = Materials_IndexForUri(uri);
        Uri_Destruct(uri);
        return result;
    }
    return 0;
}

/**
 * Given a unique material identifier, lookup the associated name.
 * \note Part of the Doomsday public API.
 *
 * @param mat           The material to lookup the name for.
 *
 * @return              The associated name.
 */
const char* Materials_GetSymbolicName(material_t* mat)
{
    materialnum_t num;
    if(!initedOk)
        return NULL;
    if(mat && (num = Materials_ToMaterialNum(mat)))
        return materialBinds[num-1].name;
    return "NOMAT"; // Should never happen.
}

dduri_t* Materials_GetUri(material_t* mat)
{
    materialbind_t* mb;
    dduri_t* uri;
    ddstring_t path;

    if(!mat)
    {
#if _DEBUG
        Con_Message("Warning:Materials_GetUri: Attempted with invalid reference (mat==0), returning 0.\n");
#endif
        return 0;
    }

    Str_Init(&path);
    mb = bindForMaterial(mat);
    if(NULL != mb)
    {
        Str_Appendf(&path, "%s:%s", Str_Text(nameForMaterialNamespaceId(mb->mnamespace)), Materials_GetSymbolicName(mat));
    }
    uri = Uri_Construct2(Str_Text(&path), RC_NULL);
    Str_Free(&path);
    return uri;
}

static void pushVariantCacheQueue(material_t* mat, materialvariantspecification_t* spec)
{
    assert(initedOk && mat && spec);
    {
    variantcachequeue_node_t* node = (variantcachequeue_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("Materials::pushVariantCacheQueue: Failed on allocation of "
                  "%lu bytes for new VariantCacheQueueNode.",
                  (unsigned int) sizeof(*node));
    node->mat = mat;
    node->spec = spec;
    node->next = variantCacheQueue;
    variantCacheQueue = node;
    }
}

void Materials_Precache2(material_t* mat, materialvariantspecification_t* spec,
    boolean cacheGroup)
{
    assert(initedOk && mat && spec);

    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Already in the queue?
    { variantcachequeue_node_t* node;
    for(node = variantCacheQueue; node; node = node->next)
        if(mat == node->mat && spec == node->spec)
            return;
    }

    pushVariantCacheQueue(mat, spec);

    if(cacheGroup && Material_IsGroupAnimated(mat))
    {   // Material belongs in one or more animgroups; precache the group.
        int i;
        for(i = 0; i < numgroups; ++i)
        {
            if(!isInAnimGroup(&groups[i], mat)) continue;

            { int k;
            for(k = 0; k < groups[i].count; ++k)
                Materials_Precache2(groups[i].frames[k].mat, spec, false);
            }
        }
    }
}

void Materials_Precache(material_t* mat, materialvariantspecification_t* spec)
{
    Materials_Precache2(mat, spec, true);
}

void Materials_Ticker(timespan_t time)
{
    static trigger_t fixed = { 1.0 / 35, 0 };

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

    { materiallist_node_t* node = materials;
    while(node)
    {
        Material_Ticker(node->mat, time);
        node = node->next;
    }}

    if(!M_RunTrigger(&fixed, time))
        return;

    animateAnimGroups();
}

/**
 * Subroutine of Materials_Prepare().
 */
static __inline void setTexUnit(material_snapshot_t* ss, byte unit,
                                blendmode_t blendMode, int magMode,
                                const texturevariant_t* tex,
                                float sScale, float tScale,
                                float sOffset, float tOffset, float alpha)
{
    material_textureunit_t* mtp = &ss->units[unit];

    mtp->tex = tex;
    mtp->magMode = magMode;
    mtp->blendMode = blendMode;
    mtp->alpha = MINMAX_OF(0, alpha, 1);
    mtp->scale[0] = sScale;
    mtp->scale[1] = tScale;
    mtp->offset[0] = sOffset;
    mtp->offset[1] = tOffset;
}

byte Materials_Prepare(material_snapshot_t* snapshot, material_t* mat,
    boolean smoothed, materialvariantspecification_t* spec)
{
    assert(mat && spec);
    {
    const texturevariant_t* layerTextures[MATERIALVARIANT_MAXLAYERS];
    const texturevariant_t* detailTex = NULL, *shinyTex = NULL, *shinyMaskTex = NULL;
    const ded_detailtexture_t* detail = NULL;
    const ded_reflection_t* reflection = NULL;
    const ded_decor_t* decor = NULL;
    materialvariant_t* variant;
    materialbind_t* mb;
    byte tmpResult = 0;
    uint i;

    // Have we already registered a suitable variant?
    variant = Materials_ChooseVariant(mat, spec);
    if(NULL == variant)
    {   // We need to create at least one variant.
        variant = Material_AddVariant(mat, MaterialVariant_Construct(mat, spec));
        if(Material_IsGroupAnimated(mat))
        {   // Create all other required variants for any Materials in any linked
            // animation groups.
            int i;
            for(i = 0; i < numgroups; ++i)
            {
                if(!isInAnimGroup(&groups[i], mat)) continue;

                { int k;
                for(k = 0; k < groups[i].count; ++k)
                {
                    material_t* other = groups[i].frames[k].mat;
                    if(mat != other && NULL == Materials_ChooseVariant(other, spec))
                    {
                        Material_AddVariant(other, MaterialVariant_Construct(other, spec));
                    }
                }}
            }
        }
    }

    if(smoothed)
    {
        variant = MaterialVariant_TranslationCurrent(variant);
        mat = MaterialVariant_GeneralCase(variant);
    }

    // Ensure all resources needed to visualize this material are loaded.
    { int i, layerCount = Material_LayerCount(mat);
    for(i = 0; i < layerCount; ++i)
    {
        const materialvariant_layer_t* ml = MaterialVariant_Layer(variant, i);
        byte result;

        // Pick the instance matching the specified context.
        layerTextures[i] = GL_PrepareTexture(ml->tex, spec->primarySpec, &result);

        if(result)
            tmpResult = result;
    }}

    if((mb = bindForMaterial(mat)))
    {
        if(tmpResult)
            mb->prepared = tmpResult;

        decor = mb->decoration[mb->prepared-1];
        detail = mb->detail[mb->prepared-1];
        reflection = mb->reflection[mb->prepared-1];

        if(mb->prepared)
        {   // A texture was loaded.
            // Do we need to prepare a detail texture?
            if(detail && r_detail)
            {
                detailtex_t* dTex;

                /**
                 * \todo No need to look up the detail texture record every time!
                 * This will change anyway once the texture for the detailtex is
                 * linked to (and prepared) via the layers (above).
                 */

                if((dTex = R_GetDetailTexture(detail->detailTex, detail->isExternal)))
                {
                    float contrast = detail->strength * detailFactor;

                    // Pick an instance matching the specified context.
                    detailTex = GL_PrepareTexture(dTex->id, GL_DetailTextureVariantSpecificationForContext(contrast), 0);
                }
            }

            // Do we need to prepare a shiny texture (and possibly a mask)?
            if(reflection && useShinySurfaces)
            {
                shinytex_t* sTex;
                masktex_t* mTex;

                /**
                 * \todo No need to look up the shiny texture record every time!
                 * This will change anyway once the texture for the shinytex is
                 * linked to (and prepared) via the layers (above).
                 */

                if((sTex = R_GetShinyTexture(reflection->shinyMap)))
                {
                    // Pick an instance matching the specified context.
                    shinyTex = GL_PrepareTexture(sTex->id,
                        GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                            0, 0, 0, 0), 0);
                }

                if(shinyTex && // Don't bother searching unless the above succeeds.
                   (mTex = R_GetMaskTexture(reflection->maskMap)))
                {
                    // Pick an instance matching the specified context.
                    shinyMaskTex = GL_PrepareTexture(mTex->id,
                        GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTIONMASK,
                            0, 0, 0, 0), 0);
                }
            }
        }
    }

    // If we arn't taking a snapshot, get out of here.
    if(!snapshot)
        return tmpResult;

    /**
     * Take a snapshot:
     */

    // Reset to the default state.
    for(i = 0; i < MATERIALVARIANT_MAXLAYERS; ++i)
        setTexUnit(snapshot, i, BM_NORMAL, GL_LINEAR, 0, 1, 1, 0, 0, 0);

    snapshot->width = Material_Width(mat);
    snapshot->height = Material_Height(mat);
    snapshot->glowing = MaterialVariant_Layer(variant, 0)->glow * glowingTextures;
    snapshot->isDecorated = (decor? true : false);

    // Setup the primary texturing pass.
    if(0 != MaterialVariant_Layer(variant, 0)->tex)
    {
        const texture_t* tex = GL_ToTexture(MaterialVariant_Layer(variant, 0)->tex);
        int magMode = glmode[texMagMode];
        vec2_t scale;

        if(TN_SPRITES == Texture_Namespace(tex))
            magMode = filterSprites? GL_LINEAR : GL_NEAREST;
        V2_Set(scale, 1.f / snapshot->width, 1.f / snapshot->height);

        setTexUnit(snapshot, MTU_PRIMARY, BM_NORMAL, magMode, layerTextures[0],
            scale[0], scale[1], MaterialVariant_Layer(variant, 0)->texOrigin[0],
            MaterialVariant_Layer(variant, 0)->texOrigin[1], 1);

        snapshot->isOpaque = !TextureVariant_IsMasked(layerTextures[0]);

        if(TC_SKYSPHERE_DIFFUSE == TS_NORMAL(spec->primarySpec)->context)
        {
            const averagecolor_analysis_t* avgTopColor = (const averagecolor_analysis_t*)
                TextureVariant_Analysis(layerTextures[0], TA_SKY_SPHEREFADECOLOR);
            assert(avgTopColor);
            snapshot->topColor[CR] = avgTopColor->color[CR];
            snapshot->topColor[CG] = avgTopColor->color[CG];
            snapshot->topColor[CB] = avgTopColor->color[CB];
        }
        else
        {
            snapshot->topColor[CR] = snapshot->topColor[CG] = snapshot->topColor[CB] = 1;
        }

        if(TC_MAPSURFACE_DIFFUSE == TS_NORMAL(spec->primarySpec)->context ||
           TC_SKYSPHERE_DIFFUSE  == TS_NORMAL(spec->primarySpec)->context)
        {
            const ambientlight_analysis_t* ambientLight = (const ambientlight_analysis_t*)
                TextureVariant_Analysis(layerTextures[0], TA_MAP_AMBIENTLIGHT);
            assert(ambientLight);
            snapshot->color[CR] = ambientLight->color[CR];
            snapshot->color[CG] = ambientLight->color[CG];
            snapshot->color[CB] = ambientLight->color[CB];
            snapshot->colorAmplified[CR] = ambientLight->colorAmplified[CR];
            snapshot->colorAmplified[CG] = ambientLight->colorAmplified[CG];
            snapshot->colorAmplified[CB] = ambientLight->colorAmplified[CB];
        }
        else
        {
            snapshot->color[CR] = snapshot->color[CG] = snapshot->color[CB] = 1;
            snapshot->colorAmplified[CR] = snapshot->colorAmplified[CG] = snapshot->colorAmplified[CB] = 1;
        }
    }

    /**
     * If skymasked, we need only need to update the primary tex unit
     * (this is due to it being visible when skymask debug drawing is
     * enabled).
     */
    if(!Material_IsSkyMasked(mat))
    {
        // Setup the detail texturing pass?
        if(detailTex && detail && snapshot->isOpaque)
        {
            float width, height, scale;

            width  = Texture_Width(TextureVariant_GeneralCase(detailTex));
            height = Texture_Height(TextureVariant_GeneralCase(detailTex));
            scale  = MAX_OF(1, detail->scale);
            // Apply the global scaling factor.
            if(detailScale > .001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, BM_NORMAL, texMagMode?GL_LINEAR:GL_NEAREST,
                detailTex, 1.f / width * scale, 1.f / height * scale, 0, 0, 1);
        }

        // Setup the reflection (aka shiny) texturing pass(es)?
        if(shinyTex && reflection)
        {
            snapshot->shiny.minColor[CR] = reflection->minColor[CR];
            snapshot->shiny.minColor[CG] = reflection->minColor[CG];
            snapshot->shiny.minColor[CB] = reflection->minColor[CB];

            setTexUnit(snapshot, MTU_REFLECTION, reflection->blendMode, GL_LINEAR,
                shinyTex, 1, 1, 0, 0, reflection->shininess);

            if(shinyMaskTex)
                setTexUnit(snapshot, MTU_REFLECTION_MASK, BM_NORMAL, snapshot->units[MTU_PRIMARY].magMode, shinyMaskTex,
                           1.f / (snapshot->width * maskTextures[Texture_TypeIndex(TextureVariant_GeneralCase(shinyMaskTex))]->width),
                           1.f / (snapshot->height * maskTextures[Texture_TypeIndex(TextureVariant_GeneralCase(shinyMaskTex))]->height),
                           snapshot->units[MTU_PRIMARY].offset[0], snapshot->units[MTU_PRIMARY].offset[1], 1);
        }
    }

    return tmpResult;
    }
}

const ded_reflection_t* Materials_Reflection(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(Materials_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false,
                Materials_VariantSpecificationForContext(MC_UNKNOWN, 0, 0, 0, 0));
        return mb->reflection[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_detailtexture_t* Materials_Detail(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(Materials_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false,
                Materials_VariantSpecificationForContext(MC_UNKNOWN, 0, 0, 0, 0));
        return mb->detail[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_decor_t* Materials_Decoration(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(Materials_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false,
                Materials_VariantSpecificationForContext(MC_UNKNOWN, 0, 0, 0, 0));
        return mb->decoration[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

const ded_ptcgen_t* Materials_PtcGen(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindForMaterial(Materials_ToMaterial(num));
        if(!mb->prepared)
            Materials_Prepare(NULL, mb->mat, false,
                Materials_VariantSpecificationForContext(MC_UNKNOWN, 0, 0, 0, 0));
        return mb->ptcGen[mb->prepared? mb->prepared-1:0];
    }
    return 0;
}

uint Materials_Count(void)
{
    if(initedOk)
        return numMaterialBinds;
    return 0;
}

struct materialvariantspecification_s* Materials_VariantSpecificationForContext(
    materialvariantusagecontext_t mc, int flags, byte border, int tClass, int tMap)
{
    if(!initedOk)
        Con_Error("Materials::VariantSpecificationForContext: Materials collection "
            "not yet initialized.");
    return getVariantSpecificationForContext(mc, flags, border, tClass, tMap);
}

materialvariant_t* Materials_ChooseVariant(material_t* mat,
    const materialvariantspecification_t* spec)
{
    if(!initedOk)
        Con_Error("Materials::ChooseVariant: Materials collection not yet initialized.");
    return chooseVariant(mat, spec);
}

static void printMaterialInfo(const materialbind_t* mb, boolean printNamespace)
{
    int numDigits = M_NumDigits(numMaterialBinds);
    const material_t* mat = mb->mat;

    Con_Printf(" %*u: \"", numDigits, (unsigned int) Material_BindId(mat));
    if(printNamespace)
        Con_Printf("%s:", Str_Text(nameForMaterialNamespaceId(mb->mnamespace)));
    Con_Printf("%s\" [%i, %i]", mb->name, Material_Width(mat), Material_Height(mat));
    Con_Printf("\n");
}

static materialbind_t** collectMaterials(materialnamespaceid_t mnamespace, const char* like,
    size_t* count, materialbind_t** storage)
{
    size_t n = 0;

    if(VALID_MATERIALNAMESPACE(mnamespace))
    {
        if(materialBinds)
        {
            uint i;
            for(i = 0; i < MATERIALNAMESPACE_HASH_SIZE; ++i)
                if(hashTable[mnamespace-MATERIALNAMESPACE_FIRST][i])
                {
                    materialnum_t num = hashTable[mnamespace-MATERIALNAMESPACE_FIRST][i] - 1;
                    materialbind_t* mb = &materialBinds[num];

                    for(;;)
                    {
                        if(!(like && like[0] && strnicmp(mb->name, like, strlen(like))))
                        {
                            if(storage)
                                storage[n++] = mb;
                            else
                                ++n;
                        }

                        if(!mb->hashNext)
                            break;
                        mb = &materialBinds[mb->hashNext - 1];
                    }
                }
        }
    }
    else
    {   // Any.
        materialnum_t i;
        for(i = 0; i < numMaterialBinds; ++i)
        {
            materialbind_t* mb = &materialBinds[i];
            if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
                continue;
            if(storage)
                storage[n++] = mb;
            else
                ++n;
        }
    }

    if(storage)
    {
        storage[n] = 0; // Terminate.
        if(count)
            *count = n;
        return storage;
    }

    if(n == 0)
    {
        if(count)
            *count = 0;
        return 0;
    }

    storage = malloc(sizeof(materialbind_t*) * (n+1));
    return collectMaterials(mnamespace, like, count, storage);
}

static size_t printMaterials2(materialnamespaceid_t mnamespace, const char* like)
{
    size_t count = 0;
    materialbind_t** foundMaterials = collectMaterials(mnamespace, like, &count, 0);

    if(VALID_MATERIALNAMESPACE(mnamespace))
        Con_FPrintf(CBLF_YELLOW, "Known Materials in \"%s\":\n", Str_Text(nameForMaterialNamespaceId(mnamespace)));
    else // Any namespace.
        Con_FPrintf(CBLF_YELLOW, "Known Materials:\n");

    if(!foundMaterials)
    {
        Con_Printf(" None found.\n");
        return 0;
    }

    // Print the result index key.
    if(VALID_MATERIALNAMESPACE(mnamespace))
    {
        Con_Printf(" uid: \"name\" [width, height]\n");
        Con_FPrintf(CBLF_RULER, "");
    }
    else
    {   // Any namespace.
        Con_Printf(" uid: \"(namespace:)name\" [width, height]\n");
        Con_FPrintf(CBLF_RULER, "");
    }

    // Sort and print the index.
    qsort(foundMaterials, count, sizeof(*foundMaterials), compareMaterialBindByName);

    { materialbind_t* const* ptr;
    for(ptr = foundMaterials; *ptr; ++ptr)
    {
        const materialbind_t* mb = *ptr;
        printMaterialInfo(mb, (mnamespace == MN_ANY));
    }}

    free(foundMaterials);
    return count;
}

static void printMaterials(materialnamespaceid_t mnamespace, const char* like)
{
    // Only one namespace to print?
    if(VALID_MATERIALNAMESPACE(mnamespace))
    {
        printMaterials2(mnamespace, like);
        return;
    }

    // Collect and sort in each namespace separately.
    { int i;
    for(i = MATERIALNAMESPACE_FIRST; i < MATERIALNAMESPACE_LAST; ++i)
    {
        if(printMaterials2((materialnamespaceid_t)i, like) != 0)
            Con_FPrintf(CBLF_RULER, "");
    }}
}

boolean Materials_MaterialLinkedToAnimGroup(int groupNum, material_t* mat)
{
    return isInAnimGroup(getAnimGroup(groupNum), mat);
}

int Materials_AnimGroupCount(void)
{
    return numgroups;
}

/**
 * Create a new animation group. Returns the group number.
 * \note Part of the Doomsday public API.
 */
int Materials_CreateAnimGroup(int flags)
{
    animgroup_t* group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups = Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_APPSTATIC);

    // Init the new group.
    group = &groups[numgroups];
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

/**
 * Called during engine reset to clear the existing animation groups.
 */
void Materials_DestroyAnimGroups(void)
{
    int                 i;

    if(numgroups > 0)
    {
        for(i = 0; i < numgroups; ++i)
        {
            animgroup_t*        group = &groups[i];
            Z_Free(group->frames);
        }

        Z_Free(groups);
        groups = NULL;
        numgroups = 0;
    }
}

/**
 * \note Part of the Doomsday public API.
 */
void Materials_AddAnimGroupFrame(int groupNum, materialnum_t num, int tics,
                      int randomTics)
{
    animgroup_t*        group;
    animframe_t*        frame;
    material_t*  mat;

    group = getAnimGroup(groupNum);
    if(!group)
        Con_Error("Materials_AddAnimGroupFrame: Unknown anim group '%i'\n.", groupNum);

    if(!num || !(mat = getMaterialByNum(num - 1)))
    {
        Con_Message("Materials_AddAnimGroupFrame: Invalid material num '%i'\n.", num);
        return;
    }

    // Mark the material as being in an animgroup.
    Material_SetGroupAnimated(mat, true);

    // Allocate a new animframe.
    group->frames =
        Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count,
                  PU_APPSTATIC);

    frame = &group->frames[group->count - 1];

    frame->mat = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean Materials_IsPrecacheAnimGroup(int groupNum)
{
    animgroup_t*        group;

    if((group = getAnimGroup(groupNum)))
    {
        return (group->flags & AGF_PRECACHE)? true : false;
    }

    return false;
}

static int clearVariantTranslationWorker(materialvariant_t* variant, void* paramaters)
{
    MaterialVariant_SetTranslation(variant, variant, variant);
    return 0; // Continue iteration.
}

void Materials_ClearTranslation(material_t* mat)
{
    if(!initedOk)
        Con_Error("Materials::ClearTranslation: Materials collection not yet initialized.");
    Material_IterateVariants(mat, clearVariantTranslationWorker, NULL);
}

typedef struct {
    material_t* current, *next;
} setmaterialtranslationworker_paramaters_t;

static int setVariantTranslationWorker(materialvariant_t* variant, void* paramaters)
{
    assert(variant && paramaters);
    {
    setmaterialtranslationworker_paramaters_t* params =
        (setmaterialtranslationworker_paramaters_t*) paramaters;
    const materialvariantspecification_t* spec = MaterialVariant_Spec(variant);
    materialvariant_t* current = Materials_ChooseVariant(params->current, spec);
    materialvariant_t* next    = Materials_ChooseVariant(params->next,    spec);

    MaterialVariant_SetTranslation(variant, current, next);
    return 0; // Continue iteration.
    }
}

static int setVariantTranslationPointWorker(materialvariant_t* variant, void* paramaters)
{
    assert(variant && paramaters);
    {
    float inter = *((float*)paramaters);
    MaterialVariant_SetTranslationPoint(variant, inter);
    return 0; // Continue iteration.
    }
}

void Materials_AnimateAnimGroup(animgroup_t* group)
{
    // The Precache groups are not intended for animation.
    if((group->flags & AGF_PRECACHE) || !group->count)
        return;

    if(--group->timer <= 0)
    {
        // Advance to next frame.
        int i, timer;

        group->index = (group->index + 1) % group->count;
        timer = (int) group->frames[group->index].tics;

        if(group->frames[group->index].random)
        {
            timer += (int) RNG_RandByte() % (group->frames[group->index].random + 1);
        }
        group->timer = group->maxTimer = timer;

        // Update translations.
        for(i = 0; i < group->count; ++i)
        {
            material_t* real = group->frames[i].mat;
            setmaterialtranslationworker_paramaters_t params;
            params.current = group->frames[(group->index + i    ) % group->count].mat;
            params.next    = group->frames[(group->index + i + 1) % group->count].mat;
            Material_IterateVariants(real, setVariantTranslationWorker, &params);

            // Just animate the first in the sequence?
            if(group->flags & AGF_FIRST_ONLY)
                break;
        }
        return;
    }

    // Update the interpolation point of animated group members.
    {int i;
    for(i = 0; i < group->count; ++i)
    {
        material_t* mat = group->frames[i].mat;
        ded_material_t* def = Material_Definition(mat);
        float interp;

        if(def && def->layers[0].stageCount.num > 1)
        {
            if(GL_TextureByUri(def->layers[0].stages[0].texture))
                continue; // Animated elsewhere.
        }

        if(group->flags & AGF_SMOOTH)
        {
            interp = 1 - group->timer / (float) group->maxTimer;
        }
        else
        {
            interp = 0;
        }

        Material_IterateVariants(mat, setVariantTranslationPointWorker, &interp);

        // Just animate the first in the sequence?
        if(group->flags & AGF_FIRST_ONLY)
            break;
    }}
}

static void animateAnimGroups(void)
{
    int i;
    for(i = 0; i < numgroups; ++i)
    {
        Materials_AnimateAnimGroup(&groups[i]);
    }
}

static int resetVariantGroupAnimWorker(materialvariant_t* mat, void* paramaters)
{
    MaterialVariant_ResetAnim(mat);
    return 0; // Continue iteration.
}

/**
 * All animation groups are reset back to their original state.
 * Called when setting up a map.
 */
void Materials_ResetAnimGroups(void)
{
    { materiallist_node_t* node;
    for(node = materials; node; node = node->next)
    {
        Material_IterateVariants(node->mat, resetVariantGroupAnimWorker, NULL);
    }}

    {int i;
    animgroup_t* group = groups;
    for(i = 0; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        group->timer = 0;
        group->maxTimer = 1;

        // The anim group should start from the first step using the
        // correct timings.
        group->index = group->count - 1;
    }}

    // This'll get every group started on the first step.
    animateAnimGroups();
}

D_CMD(ListMaterials)
{
    materialnamespaceid_t mnamespace = (argc > 1? DD_ParseMaterialNamespace(argv[1]) : MN_ANY);
    if(argc > 2 && !VALID_MATERIALNAMESPACE(mnamespace))
    {
        Con_Printf("Invalid namespace \"%s\".\n", argv[1]);
        return false;
    }
    printMaterials(mnamespace, (argc > 2? argv[2] : (argc > 1 && mnamespace == MN_ANY? argv[1] : NULL)));
    return true;
}
