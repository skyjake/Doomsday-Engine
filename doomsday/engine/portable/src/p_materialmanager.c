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
#include "de_filesys.h"
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

/**
 * Info is attached to a MaterialBind upon successfully preparing the first
 * derived variant of the associated Material.
 */
typedef struct materialbindinfo_s {
    ded_decor_t* decorationDefs[2];
    ded_detailtexture_t* detailtextureDefs[2];
    ded_ptcgen_t* ptcgenDefs[2];
    ded_reflection_t* reflectionDefs[2];
} materialbindinfo_t;

typedef struct materialbind_s {
    material_t*     _material;
    ddstring_t      _name;
    materialnamespaceid_t _mnamespace;
    materialbindinfo_t* _info;

    byte            prepared;
} materialbind_t;

/// @return  Material associated with this else @c NULL
material_t* MaterialBind_Material(const materialbind_t* mb);

/// @return  Symbolic name.
const ddstring_t* MaterialBind_Name(const materialbind_t* mb);

/// @return  Namespace in which this is present.
materialnamespaceid_t MaterialBind_Namespace(const materialbind_t* mb);

/// @return  Extended info owned by this else @c NULL
materialbindinfo_t* MaterialBind_Info(materialbind_t* mb);

/**
 * Attach extended info data to this. If existing info is present it is replaced.
 * MaterialBind is given ownership of the info.
 * @param info  Extended info data to attach.
 */
void MaterialBind_AttachInfo(materialbind_t* mb, materialbindinfo_t* info);

/**
 * Detach any extended info owned by this and relinquish ownership to the caller.
 * @return  Extended info or else @c NULL if not present.
 */
materialbindinfo_t* MaterialBind_DetachInfo(materialbind_t* mb);

/// @return  Detail texture definition associated with this else @c NULL
ded_detailtexture_t* MaterialBind_DetailTextureDef(const materialbind_t* mb);

/// @return  Decoration definition associated with this else @c NULL
ded_decor_t* MaterialBind_DecorationDef(const materialbind_t* mb);

/// @return  Particle generator definition associated with this else @c NULL
ded_ptcgen_t* MaterialBind_PtcGenDef(const materialbind_t* mb);

/// @return  Reflection definition associated with this else @c NULL
ded_reflection_t* MaterialBind_ReflectionDef(const materialbind_t* mb);

static void updateMaterialBindInfo(materialbind_t* mb);

#define MATERIALNAMESPACE_NAMEHASH_SIZE (512)

typedef struct materialnamespace_namehash_node_s {
    struct materialnamespace_namehash_node_s* next;
    materialnum_t bindId;
} materialnamespace_namehash_node_t;
typedef materialnamespace_namehash_node_t* materialnamespace_namehash_t[MATERIALNAMESPACE_NAMEHASH_SIZE];

typedef struct materialnamespace_s {
    materialnamespace_namehash_t nameHash;
} materialnamespace_t;

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

D_CMD(InspectMaterial);
D_CMD(ListMaterials);

static void animateAnimGroups(void);

extern boolean ddMapSetup;

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
 *    The only requirement is that their symbolic names must be unique among
 *    those in the same namespace.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (a hashing scheme is used).
 */
static blockset_t* materialsBlockSet;
static materiallist_t* materials;

static materialnum_t bindingsCount;
static materialnum_t bindingsMax;
static materialbind_t* bindings;

static materialnamespace_t namespaces[MATERIALNAMESPACE_COUNT];

void P_MaterialsRegister(void)
{
    C_CMD("inspectmaterial", "s",   InspectMaterial);
    C_CMD("listmaterials",  NULL,   ListMaterials);
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
    if(VALID_MATERIALNAMESPACEID(id))
        return &namespaces[id-MATERIALNAMESPACE_FIRST];
    return &emptyString;
}

static materialnamespaceid_t materialNamespaceIdForTextureNamespaceId(texturenamespaceid_t id)
{
    static const materialnamespaceid_t namespaceIds[TEXTURENAMESPACE_COUNT] = {
        /* TN_SYSTEM */    MN_SYSTEM,
        /* TN_FLATS */     MN_FLATS,
        /* TN_TEXTURES */  MN_TEXTURES,
        /* TN_SPRITES */   MN_SPRITES,
        /* TN_PATCHES */   MN_ANY // No materials for these yet.
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

static boolean isInAnimGroup(const animgroup_t* group, const material_t* mat)
{
    assert(group);
    if(NULL != mat)
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
        Con_Error("Materials::copyVariantSpecification: Failed on allocation of %lu bytes for "
            "new MaterialVariantSpecification.", (unsigned long) sizeof(*spec));
    memcpy(spec, tpl, sizeof(materialvariantspecification_t));
    return spec;
}

static int compareVariantSpecifications(const materialvariantspecification_t* a,
    const materialvariantspecification_t* b)
{
    assert(a && b);
    if(a == b)
        return 0;
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
        Con_Error("Materials::linkVariantSpecification: Failed on allocation of %lu bytes for "
            "new MaterialVariantSpecificationListNode.", (unsigned long) sizeof(*node));
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
    materialvariantusagecontext_t mc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
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
    primarySpec = GL_TextureVariantSpecificationForContext(primaryContext, flags,
        border, tClass, tMap, wrapS, wrapT, minFilter, magFilter, anisoFilter, mipmapped,
        gammaCorrection, noStretch, toAlpha);
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

static materialbind_t* bindByIndex(uint bindId)
{
    if(0 != bindId) return &bindings[bindId-1];
    return 0;
}

static int compareMaterialBindByName(const void* e1, const void* e2)
{
    return stricmp(Str_Text(MaterialBind_Name(*(const materialbind_t**)e1)),
                   Str_Text(MaterialBind_Name(*(const materialbind_t**)e2)));
}

/**
 * This is a hash function. Given a material name it generates a
 * somewhat-random number between 0 and MATERIALNAMESPACE_NAMEHASH_SIZE.
 *
 * @return  The generated hash index.
 */
static uint hashForName(const char* name)
{
    ushort key = 0;
    size_t i = 0;
    // Stop when name ends.
    for(; *name; name++)
    {
        switch(i)
        {
        case 0: key ^= (int) (*name); break;
        case 1: key *= (int) (*name); break;
        case 2: key -= (int) (*name); break;
        }
        i = (i == 2? 0 : i+1);
    }
    return key % MATERIALNAMESPACE_NAMEHASH_SIZE;
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name  Name of the material to search for. Must have been transformed
 *      to all lower case.
 * @param namespaceId  Specific MG_* material namespace NOT @c MN_ANY.
 * @return  Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForName(const char* name, uint hash,
    materialnamespaceid_t namespaceId)
{
    materialnamespace_t* mn = &namespaces[namespaceId-MATERIALNAMESPACE_FIRST];
    materialnamespace_namehash_node_t* node;
    for(node = (materialnamespace_namehash_node_t*)mn->nameHash[hash];
        NULL != node; node = node->next)
    {
        materialbind_t* mb = bindByIndex(node->bindId);
        if(!strncmp(Str_Text(MaterialBind_Name(mb)), name, 8))
            return node->bindId;
    }
    return 0; // Not found.
}

static void updateMaterialBindInfo(materialbind_t* mb)
{
    assert(initedOk && mb);
    {
    materialbindinfo_t* info = MaterialBind_Info(mb);
    boolean attachInfo = false;
    
    if(NULL == info)
    {
        info = (materialbindinfo_t*) malloc(sizeof(*info));
        if(NULL == info)
            Con_Error("MaterialBind::LinkDefinitions: Failed on allocation of %lu bytes for "
                "new MaterialBindInfo.", (unsigned long) sizeof(*info));
        attachInfo = true;
    }

    // Surface decorations (lights and models).
    info->decorationDefs[0] = Def_GetDecoration(mb->_material, 0);
    info->decorationDefs[1] = Def_GetDecoration(mb->_material, 1);

    // Reflection (aka shiny surface).
    info->reflectionDefs[0] = Def_GetReflection(mb->_material, 0);
    info->reflectionDefs[1] = Def_GetReflection(mb->_material, 1);

    // Generator (particles).
    info->ptcgenDefs[0] = Def_GetGenerator(mb->_material, 0);
    info->ptcgenDefs[1] = Def_GetGenerator(mb->_material, 1);

    // Detail texture.
    info->detailtextureDefs[0] = Def_GetDetailTex(mb->_material, 0);
    info->detailtextureDefs[1] = Def_GetDetailTex(mb->_material, 1);

    if(attachInfo)
        MaterialBind_AttachInfo(mb, info);
    }
}

static void newMaterialNameBinding(material_t* material, const char* name,
    materialnamespaceid_t namespaceId, uint hash)
{
    materialnamespace_namehash_node_t* node;
    materialnamespace_t* mn;
    materialbind_t* mb;

    if(++bindingsCount > bindingsMax)
    {   // Allocate more memory.
        bindingsMax += MATERIALS_BLOCK_ALLOC;
        bindings = (materialbind_t*) realloc(bindings, sizeof(*bindings) * bindingsMax);
    }

    mb = &bindings[bindingsCount - 1];
    Str_Init(&mb->_name); Str_Set(&mb->_name, name);
    mb->_material = material;
    mb->_mnamespace = namespaceId;
    mb->_info = NULL;

    mb->prepared = 0;

    // We also hash the name for faster searching.
    mn = &namespaces[namespaceId-MATERIALNAMESPACE_FIRST];
    node = (materialnamespace_namehash_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("Materials::newMaterialNameBinding: Failed on allocation of %lu bytes for"
            "new MaterialNamespace::NameHash::Node.", (unsigned long) sizeof(*node));
    node->next = mn->nameHash[hash];
    node->bindId = bindingsCount; // 1-based index;
    mn->nameHash[hash] = node;

    Material_SetBindId(material, bindingsCount /* 1-based index */);
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
            "new MaterialList::Node.", (unsigned long) sizeof(*node));
    node->mat = mat;
    node->next = materials;
    materials = node;
    return mat;
}

static __inline material_t* getMaterialByIndex(materialnum_t num)
{
    if(num < bindingsCount)
        return MaterialBind_Material(&bindings[num]);
    Con_Error("getMaterialByIndex: Invalid index #%u.", (unsigned int) num);
    return NULL; // Unreachable.
}

void Materials_Initialize(void)
{
    if(initedOk)
        return; // Already been here.

    variantSpecs = NULL;
    variantCacheQueue = NULL;

    bindings = NULL;
    bindingsCount = bindingsMax = 0;

    materialsBlockSet = BlockSet_New(sizeof(material_t), MATERIALS_BLOCK_ALLOC);
    materials = NULL;

    // Clear the name bind hash tables.
    { int i;
    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        materialnamespace_t* mn = &namespaces[i];
        memset(mn->nameHash, 0, sizeof(mn->nameHash));
    }}

    initedOk = true;
}

static int destroyVariant(materialvariant_t* variant, void* paramaters)
{
    MaterialVariant_Delete(variant);
    return 1; // Continue iteration.
}

static void destroyMaterials(void)
{
    assert(initedOk);
    while(NULL != materials)
    {
        materiallist_node_t* next = materials->next;
        Material_DestroyVariants(materials->mat);
        free(materials);
        materials = next;
    }
    BlockSet_Delete(materialsBlockSet);
    materialsBlockSet = NULL;
}

static void destroyBindings(void)
{
    assert(initedOk);

    // Empty the namespace namehash tables.
    { int i;
    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        materialnamespace_t* mn = &namespaces[i];
        int j;
        for(j = 0; j < MATERIALNAMESPACE_NAMEHASH_SIZE; ++j)
        {
            materialnamespace_namehash_node_t* next;
            while(NULL != mn->nameHash[j])
            {
                next = mn->nameHash[j]->next;
                free(mn->nameHash[j]);
                mn->nameHash[j] = next;
            }
        }
    }}

    // Destroy the bindings themselves.
    if(NULL != bindings)
    {
        materialnum_t i;
        for(i = 0; i < bindingsCount; ++i)
        {
            materialbind_t* mb = &bindings[i];
            materialbindinfo_t* info = MaterialBind_DetachInfo(mb);
            if(NULL != info)
                free(info);
            Str_Free(&mb->_name);
        }
        free(bindings);
        bindings = NULL;
    }
    bindingsCount = bindingsMax = 0;
}

void Materials_Shutdown(void)
{
    if(!initedOk)
        return;

    Materials_PurgeCacheQueue();

    destroyBindings();
    destroyMaterials();
    destroyVariantSpecifications();

    initedOk = false;
}

void Materials_ClearDefinitionLinks(void)
{
    assert(initedOk);
    {
    materiallist_node_t* node;
    materialnum_t i;
    for(node = materials; node; node = node->next)
    {
        material_t* mat = node->mat;
        Material_SetDefinition(mat, NULL);
    }
    for(i = 0; i < bindingsCount; ++i)
    {
        materialbind_t* mb = &bindings[i];
        materialbindinfo_t* info = MaterialBind_Info(mb);
        if(NULL == info) continue;
        info->decorationDefs[0]     = info->decorationDefs[1] = NULL;
        info->detailtextureDefs[0]  = info->detailtextureDefs[1] = NULL;
        info->ptcgenDefs[0]         = info->ptcgenDefs[1] = NULL;
        info->reflectionDefs[0]     = info->reflectionDefs[1] = NULL;
    }
    }
}

void Materials_Rebuild(material_t* mat, ded_material_t* def)
{
    assert(initedOk);
    {
    uint bindId;
    materialbind_t* mb;

    if(NULL == mat || NULL == def)
        return;

    /// \todo We should be able to rebuild the variants.
    Material_DestroyVariants(mat);
    Material_SetDefinition(mat, def);
    Material_SetFlags(mat, def->flags);
    Material_SetDimensions(mat, def->width, def->height);
    Material_SetEnvironmentClass(mat, S_MaterialClassForName(def->id));

    // Textures are updated automatically at prepare-time, so just clear them.
    Material_SetDetailTexture(mat, NULL);
    Material_SetShinyTexture(mat, NULL);
    Material_SetShinyMaskTexture(mat, NULL);

    bindId = Material_BindId(mat);
    if(0 == bindId)
        return;
    mb = bindByIndex(bindId);
    if(NULL == MaterialBind_Info(mb))
        return;
    updateMaterialBindInfo(mb);
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
        if(0 == ml->tex) continue;

        GL_ReleaseGLTexturesForTexture(GL_ToTexture(ml->tex));
    }
    return 0; // Continue iteration.
    }
}

static void releaseGLTexturesForMaterial(material_t* mat)
{
    Material_IterateVariants(mat, releaseGLTexturesForMaterialWorker, NULL);
}

void Materials_ReleaseGLTextures(const char* namespaceName)
{
    materialnamespaceid_t namespaceId = MN_ANY;

    if(namespaceName && namespaceName[0])
    {
        namespaceId = DD_ParseMaterialNamespace(namespaceName);
        if(!VALID_MATERIALNAMESPACEID(namespaceId))
        {
#if _DEBUG
            Con_Message("Warning:Materials_ReleaseGLTextures: Attempt to delete in "
                "unknown namespace (%s), ignoring.\n", namespaceName);
#endif
            return;
        }
    }

    if(namespaceId == MN_ANY)
    {   // Delete the lot.
        GL_ReleaseGLTexturesByNamespace(TN_ANY);
        return;
    }

    if(!VALID_MATERIALNAMESPACEID(namespaceId))
        Con_Error("Materials_ReleaseGLTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) namespaceId);

    if(bindings)
    {
        materialnamespace_t* mn = &namespaces[namespaceId-MATERIALNAMESPACE_FIRST];
        int i;
        for(i = 0; i < MATERIALNAMESPACE_NAMEHASH_SIZE; ++i)
        {
            materialnamespace_namehash_node_t* node;

            if(NULL == mn->nameHash[i]) continue;

            for(node = (materialnamespace_namehash_node_t*) mn->nameHash[i];
                NULL != node; node = node->next)
            {
                materialbind_t* mb = bindByIndex(node->bindId);
                material_t* material = MaterialBind_Material(mb);
                releaseGLTexturesForMaterial(material);
            }
        }
    }
}

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace)
{
    return nameForMaterialNamespaceId(materialNamespaceIdForTextureNamespaceId(texNamespace));
}

material_t* Materials_ToMaterial(materialnum_t num)
{
    if(!initedOk)
        return NULL;
    if(num != 0 && num <= bindingsCount)
        return getMaterialByIndex(num - 1); // 1-based index.
    return NULL;
}

materialnum_t Materials_ToMaterialNum(material_t* mat)
{
    if(mat)
    {
        materialbind_t* mb = bindByIndex(Material_BindId(mat));
        if(NULL != mb)
            return (mb - bindings) + 1; // 1-based index.
    }
    return 0;
}

material_t* Materials_CreateFromDef(ded_material_t* def)
{
    assert(def);
    {
    materialnamespaceid_t namespaceId = MN_ANY;
    int width = def->width, height = def->height;
    const Uri* rawName = def->id;
    const texture_t* tex = NULL;
    byte flags = def->flags;
    material_t* mat;
    char name[9];
    uint hash;

    if(!initedOk)
        return NULL;

    if(def->layers[0].stageCount.num > 0)
    {
        const ded_material_layer_t* l = &def->layers[0];
        if(l->stages[0].texture) // Not unused.
        {
            if(!(tex = GL_TextureByUri(l->stages[0].texture)))
            {
                ddstring_t* materialPath = Uri_ToString(def->id);
                ddstring_t* texturePath = Uri_ToString(l->stages[0].texture);
                VERBOSE( Con_Message("Warning, unknown texture '%s' in Material '%s' (layer %i stage %i).\n",
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
        Con_Message("Warning, attempted to create Material with invalid path \"%s\", ignoring.\n",
            Str_Text(path));
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

    namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(rawName)));
    // Check if we've already created a material for this.
    if(!VALID_MATERIALNAMESPACEID(namespaceId))
    {
#if _DEBUG
        Con_Message("Warning, attempted to create Material in unknown Namespace '%i', ignoring.\n",
            (int) namespaceId);
#endif
        return NULL;
    }

    { materialnum_t matNum = getMaterialNumForName(name, hash, namespaceId);
    if(0 != matNum)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(rawName);
        Con_Message("Warning, a Material with the path \"%s\" already exists, returning existing.\n",
            Str_Text(path));
        Str_Delete(path);
#endif
        return getMaterialByIndex(matNum);
    }}

    // Only create complete Materials.
    if(NULL == tex)
        return NULL;

    // A new Material.
    mat = linkMaterialToGlobalList(allocMaterial());
    mat->_flags = flags;
    mat->_isCustom = !Texture_IsFromIWAD(tex);
    mat->_def    = def;
    mat->_width  = MAX_OF(0, width);
    mat->_height = MAX_OF(0, height);
    mat->_envClass = S_MaterialClassForName(rawName);
    newMaterialNameBinding(mat, name, namespaceId, hash);

    return mat;
    }
}

static materialnum_t Materials_CheckNumForPath2(const Uri* uri)
{
    assert(initedOk && uri);
    {
    const char* rawName = Str_Text(Uri_Path(uri));
    materialnamespaceid_t namespaceId;
    uint hash;
    char name[9];

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(Str_IsEmpty(Uri_Path(uri)) || !Str_CompareIgnoreCase(Uri_Path(uri), "-"))
        return 0;

    namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
    if(namespaceId != MN_ANY && !VALID_MATERIALNAMESPACEID(namespaceId))
    {
#if _DEBUG
Con_Message("Materials_ToMaterial2: Internal error, invalid namespace '%i'\n",
            (int) namespaceId);
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

    if(namespaceId == MN_ANY)
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
    return getMaterialNumForName(name, hash, namespaceId);
    }
}

static materialnum_t Materials_NumForPath2(const Uri* path)
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

materialnum_t Materials_IndexForUri(const Uri* path)
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
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        materialnum_t result = Materials_IndexForUri(uri);
        Uri_Delete(uri);
        return result;
    }
    return 0;
}

/// \note Part of the Doomsday public API.
const ddstring_t* Materials_GetSymbolicName(material_t* mat)
{
    materialnum_t num;
    if(!initedOk)
        return NULL;
    if(NULL == mat)
        return NULL;
    if(0 == (num = Materials_ToMaterialNum(mat)))
        return NULL; // Should never happen. 
    return MaterialBind_Name(&bindings[num-1]);
}

Uri* Materials_GetUri(material_t* mat)
{
    materialbind_t* mb;
    Uri* uri;
    ddstring_t path;

    if(!mat)
    {
#if _DEBUG
        Con_Message("Warning:Materials_GetUri: Attempted with invalid reference (mat==0), returning 0.\n");
#endif
        return 0;
    }

    Str_Init(&path);
    mb = bindByIndex(Material_BindId(mat));
    if(NULL != mb)
    {
        Str_Appendf(&path, "%s:%s", Str_Text(nameForMaterialNamespaceId(MaterialBind_Namespace(mb))),
            Str_Text(Materials_GetSymbolicName(mat)));
    }
    uri = Uri_NewWithPath2(Str_Text(&path), RC_NULL);
    Str_Free(&path);
    return uri;
}

static void pushVariantCacheQueue(material_t* mat, materialvariantspecification_t* spec)
{
    assert(initedOk && mat && spec);
    {
    variantcachequeue_node_t* node = (variantcachequeue_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("Materials::pushVariantCacheQueue: Failed on allocation of %lu bytes for "
            "new VariantCacheQueueNode.", (unsigned long) sizeof(*node));
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

static texture_t* findDetailTextureLinkedToMaterialBinding(const materialbind_t* mb)
{
    assert(mb);
    {
    const ded_detailtexture_t* def;
    detailtex_t* dTex;
    if(NULL != (def  = MaterialBind_DetailTextureDef(mb)) &&
       NULL != (dTex = R_FindDetailTextureForName(def->detailTex, def->isExternal)))
    {
        return GL_ToTexture(dTex->id);
    }
    return NULL;
    }
}

static texture_t* findShinyTextureLinkedToMaterialBinding(const materialbind_t* mb)
{
    assert(mb);
    {
    const ded_reflection_t* def;
    shinytex_t* sTex;
    if(NULL != (def  = MaterialBind_ReflectionDef(mb)) &&
       NULL != (sTex = R_FindShinyTextureForName(def->shinyMap)))
    {
        return GL_ToTexture(sTex->id);
    }
    return NULL;
    }
}

static texture_t* findShinyMaskTextureLinkedToMaterialBinding(const materialbind_t* mb)
{
    assert(mb);
    {
    const ded_reflection_t* def;
    masktex_t* mTex;
    if(NULL != (def  = MaterialBind_ReflectionDef(mb)) &&
       NULL != (mTex = R_FindMaskTextureForName(def->maskMap)))
    {
        return GL_ToTexture(mTex->id);
    }
    return NULL;
    }
}

static void updateMaterialTextureLinks(material_t* mat)
{
    assert(mat);
    {
    materialbind_t* mb;
    uint bindId = Material_BindId(mat);

    if(0 == bindId)
        return;

    mb = bindByIndex(bindId);
    // Do we need to construct and attach the info data?
    if(NULL == MaterialBind_Info(mb))
        updateMaterialBindInfo(mb);

    Material_SetDetailTexture(mat,    findDetailTextureLinkedToMaterialBinding(mb));
    Material_SetShinyTexture(mat,     findShinyTextureLinkedToMaterialBinding(mb));
    Material_SetShinyMaskTexture(mat, findShinyMaskTextureLinkedToMaterialBinding(mb));
    }
}

static void setTexUnit(material_snapshot_t* ss, byte unit, const texturevariant_t* tex,
    blendmode_t blendMode, int magMode, float sScale, float tScale, float sOffset,
    float tOffset, float alpha)
{
    assert(ss);
    {
    material_textureunit_t* tu = &MSU(ss, unit);

    if(NULL != tex)
    {
        tu->tex.texture = TextureVariant_GeneralCase(tex);
        tu->tex.spec = TextureVariant_Spec(tex);
        tu->tex.glName = TextureVariant_GLName(tex);
        TextureVariant_Coords(tex, &tu->tex.s, &tu->tex.t);
    }
    else
    {
        tu->tex.texture = NULL;
        tu->tex.spec = NULL;
        tu->tex.glName = 0;
        tu->tex.s = tu->tex.t = 0;
    }

    tu->magMode = magMode;
    tu->blendMode = blendMode;
    tu->alpha = MINMAX_OF(0, alpha, 1);
    V2_Set(tu->scale, sScale, tScale);
    V2_Set(tu->offset, sOffset, tOffset);
    }
}

void Materials_InitSnapshot(material_snapshot_t* ss)
{
    assert(ss);
    { int i;
    for(i = 0; i < MATERIALVARIANT_MAXLAYERS; ++i)
        setTexUnit(ss, i, NULL, BM_NORMAL, GL_LINEAR, 1, 1, 0, 0, 0);
    }
    V3_Set(ss->topColor, 1, 1, 1);
    V3_Set(ss->color, 1, 1, 1);
    V3_Set(ss->colorAmplified, 1, 1, 1);
}

materialvariant_t* Materials_Prepare(material_snapshot_t* snapshot, material_t* mat,
    boolean smoothed, materialvariantspecification_t* spec)
{
    assert(mat && spec);
    {
    struct materialtextureunit_s {
        const texturevariant_t* tex;
    } static texUnits[NUM_MATERIAL_TEXTURE_UNITS];
    const ded_reflection_t* shinyTexDef = NULL;
    const ded_detailtexture_t* detailDef = NULL;
    const ded_decor_t* decorDef = NULL;
    materialvariant_t* variant;
    materialbind_t* mb;

    memset(texUnits, 0, sizeof(texUnits));

    // Have we already registered a suitable variant?
    variant = Materials_ChooseVariant(mat, spec);
    if(NULL == variant)
    {   // We need to create at least one variant.
        variant = Material_AddVariant(mat, MaterialVariant_New(mat, spec));

        // Create all other required variants for any Materials in all linked groups.
        if(Material_IsGroupAnimated(mat))
        {
            int i, k;
            for(i = 0; i < numgroups; ++i)
            {
                const animgroup_t* group = &groups[i];
                if(!isInAnimGroup(group, mat)) continue;

                for(k = 0; k < group->count; ++k)
                {
                    material_t* other = group->frames[k].mat;
                    if(mat != other && NULL == Materials_ChooseVariant(other, spec))
                    {
                        Material_AddVariant(other, MaterialVariant_New(other, spec));
                    }
                }
            }
        }
    }

    if(smoothed)
    {
        variant = MaterialVariant_TranslationCurrent(variant);
        mat = MaterialVariant_GeneralCase(variant);
    }
    mb = bindByIndex(Material_BindId(mat));

    // Ensure all resources needed to visualize this Material's layers have been prepared.
    { int i, layerCount = Material_LayerCount(mat);
    for(i = 0; i < layerCount; ++i)
    {
        const materialvariant_layer_t* ml = MaterialVariant_Layer(variant, i);
        preparetextureresult_t result;
        texture_t* tex;

        if(0 == ml->tex) continue;

        // Pick the instance matching the specified context.
        tex = GL_ToTexture(ml->tex);
        texUnits[i].tex = GL_PrepareTextureVariant2(tex, spec->primarySpec, &result);

        if(0 == i && NULL != mb &&
           (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
        {
            // Primary texture was (re)prepared.
            /// Update the prepared status. This should be moved out of the binding.
            mb->prepared = result == PTR_UPLOADED_ORIGINAL? 1 : 2;
            updateMaterialTextureLinks(mat);

            // Are we inheriting the logical dimensions from the texture?
            if(0 == Material_Width(mat) && 0 == Material_Height(mat))
                Material_SetDimensions(mat, Texture_Width(tex), Texture_Height(tex));
        }
    }}

    if(NULL != mb)
    {
        decorDef    = MaterialBind_DecorationDef(mb);
        detailDef   = MaterialBind_DetailTextureDef(mb);
        shinyTexDef = MaterialBind_ReflectionDef(mb);

        // Do we need to prepare a DetailTexture?
        if(detailDef && r_detail)
        {
            texture_t* tex = Material_DetailTexture(mat);
            if(NULL != tex)
            {
                float contrast = detailDef->strength * detailFactor;
                texturevariantspecification_t* texSpec =
                    GL_DetailTextureVariantSpecificationForContext(contrast);

                texUnits[MTU_DETAIL].tex = GL_PrepareTextureVariant(tex, texSpec);
            }
        }

        // Do we need to prepare a shiny texture (and possibly a mask)?
        if(shinyTexDef && useShinySurfaces)
        {
            texture_t* tex = Material_ShinyTexture(mat);
            if(tex != NULL)
            {
                texturevariantspecification_t* texSpec =
                    GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                        TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                        false, false, false, false);

                texUnits[MTU_REFLECTION].tex = GL_PrepareTextureVariant(tex, texSpec);

                // We are only interested in a mask if we have a shiny texture.
                if(NULL != texUnits[MTU_REFLECTION].tex && NULL != (tex = Material_ShinyMaskTexture(mat)))
                {
                    texSpec = GL_TextureVariantSpecificationForContext(
                        TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                        -1, -1, -1, true, false, false, false);

                    texUnits[MTU_REFLECTION_MASK].tex = GL_PrepareTextureVariant(tex, texSpec);
                }
            }
        }
    }

    // If we aren't taking a snapshot; get out of here.
    if(!snapshot)
        return variant;

    Materials_InitSnapshot(snapshot);

    if(0 == Material_Width(mat) && 0 == Material_Height(mat))
        return variant;

    snapshot->width = Material_Width(mat);
    snapshot->height = Material_Height(mat);
    snapshot->glowing = MaterialVariant_Layer(variant, 0)->glow * glowFactor;
    snapshot->isOpaque = NULL != texUnits[MTU_PRIMARY].tex &&
        !TextureVariant_IsMasked(texUnits[MTU_PRIMARY].tex);

    // Setup the primary texture unit.
    if(NULL != texUnits[MTU_PRIMARY].tex)
    {
        const texturevariant_t* tex = texUnits[MTU_PRIMARY].tex;
        int magMode = glmode[texMagMode];
        float sScale, tScale;

        if(TN_SPRITES == Texture_Namespace(TextureVariant_GeneralCase(tex)))
            magMode = filterSprites? GL_LINEAR : GL_NEAREST;
        sScale = 1.f / snapshot->width;
        tScale = 1.f / snapshot->height;

        setTexUnit(snapshot, MTU_PRIMARY, tex, BM_NORMAL, magMode,
            sScale, tScale, MaterialVariant_Layer(variant, 0)->texOrigin[0],
            MaterialVariant_Layer(variant, 0)->texOrigin[1], 1);
    }

    /**
     * If skymasked, we need only need to update the primary tex unit
     * (this is due to it being visible when skymask debug drawing is
     * enabled).
     */
    if(!Material_IsSkyMasked(mat))
    {
        // Setup the detail texture unit?
        if(NULL != texUnits[MTU_DETAIL].tex && detailDef && snapshot->isOpaque)
        {
            const texturevariant_t* tex = texUnits[MTU_DETAIL].tex;
            float width  = Texture_Width(TextureVariant_GeneralCase(tex));
            float height = Texture_Height(TextureVariant_GeneralCase(tex));
            float scale = MAX_OF(1, detailDef->scale);

            // Apply the global scaling factor.
            if(detailScale > .0001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, tex, BM_NORMAL,
                texMagMode?GL_LINEAR:GL_NEAREST, 1.f / width * scale, 1.f / height * scale, 0, 0, 1);
        }

        // Setup the shiny texture units?
        if(NULL != shinyTexDef)
        {
            if(NULL != texUnits[MTU_REFLECTION].tex)
            {
                const texturevariant_t* tex = texUnits[MTU_REFLECTION].tex;
                setTexUnit(snapshot, MTU_REFLECTION, tex, shinyTexDef->blendMode,
                    GL_LINEAR, 1, 1, 0, 0, shinyTexDef->shininess);
            }

            if(NULL != texUnits[MTU_REFLECTION_MASK].tex)
            {
                const texturevariant_t* tex = texUnits[MTU_REFLECTION_MASK].tex;

                setTexUnit(snapshot, MTU_REFLECTION_MASK, tex, BM_NORMAL,
                    snapshot->units[MTU_PRIMARY].magMode,
                    1.f / (snapshot->width * Texture_Width(TextureVariant_GeneralCase(tex))),
                    1.f / (snapshot->height * Texture_Height(TextureVariant_GeneralCase(tex))),
                    snapshot->units[MTU_PRIMARY].offset[0], snapshot->units[MTU_PRIMARY].offset[1], 1);
            }
        }
    }

    if(MC_MAPSURFACE == spec->context && NULL != shinyTexDef)
    {
        snapshot->shinyMinColor[CR] = shinyTexDef->minColor[CR];
        snapshot->shinyMinColor[CG] = shinyTexDef->minColor[CG];
        snapshot->shinyMinColor[CB] = shinyTexDef->minColor[CB];
    }

    if(MC_SKYSPHERE == spec->context && NULL != texUnits[MTU_PRIMARY].tex)
    {
        const texturevariant_t* tex = texUnits[MTU_PRIMARY].tex;
        const averagecolor_analysis_t* avgTopColor = (const averagecolor_analysis_t*)
            Texture_Analysis(TextureVariant_GeneralCase(tex), TA_SKY_SPHEREFADEOUT);
        assert(avgTopColor);
        snapshot->topColor[CR] = avgTopColor->color[CR];
        snapshot->topColor[CG] = avgTopColor->color[CG];
        snapshot->topColor[CB] = avgTopColor->color[CB];
    }

    if((MC_MAPSURFACE == spec->context || MC_SKYSPHERE == spec->context) &&
       NULL != texUnits[MTU_PRIMARY].tex)
    {
        const texturevariant_t* tex = texUnits[MTU_PRIMARY].tex;
        const ambientlight_analysis_t* ambientLight = (const ambientlight_analysis_t*)
            Texture_Analysis(TextureVariant_GeneralCase(tex), TA_MAP_AMBIENTLIGHT);
        assert(ambientLight);
        snapshot->color[CR] = ambientLight->color[CR];
        snapshot->color[CG] = ambientLight->color[CG];
        snapshot->color[CB] = ambientLight->color[CB];
        snapshot->colorAmplified[CR] = ambientLight->colorAmplified[CR];
        snapshot->colorAmplified[CG] = ambientLight->colorAmplified[CG];
        snapshot->colorAmplified[CB] = ambientLight->colorAmplified[CB];
    }

    return variant;
    }
}

const ded_decor_t* Materials_DecorationDef(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindByIndex(Material_BindId(Materials_ToMaterial(num)));
        if(!mb->prepared)
            Materials_Prepare(NULL, MaterialBind_Material(mb), false,
                Materials_VariantSpecificationForContext(MC_MAPSURFACE,
                    0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false));
        return MaterialBind_DecorationDef(mb);
    }
    return 0;
}

const ded_ptcgen_t* Materials_PtcGenDef(materialnum_t num)
{
    if(num > 0)
    {
        const materialbind_t* mb = bindByIndex(Material_BindId(Materials_ToMaterial(num)));
        if(!mb->prepared)
            Materials_Prepare(NULL, MaterialBind_Material(mb), false,
                Materials_VariantSpecificationForContext(MC_MAPSURFACE,
                    0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false));
        return MaterialBind_PtcGenDef(mb);
    }
    return 0;
}

uint Materials_Count(void)
{
    if(initedOk)
        return bindingsCount;
    return 0;
}

struct materialvariantspecification_s* Materials_VariantSpecificationForContext(
    materialvariantusagecontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    if(!initedOk)
        Con_Error("Materials::VariantSpecificationForContext: Materials collection "
            "not yet initialized.");
    return getVariantSpecificationForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
        minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection, noStretch, toAlpha);
}

materialvariant_t* Materials_ChooseVariant(material_t* mat,
    const materialvariantspecification_t* spec)
{
    if(!initedOk)
        Con_Error("Materials::ChooseVariant: Materials collection not yet initialized.");
    return chooseVariant(mat, spec);
}

static int printVariantInfo(materialvariant_t* variant, void* paramaters)
{
    assert(variant && paramaters);
    {
    int* variantIdx = (int*)paramaters;
    materialvariant_t* next = MaterialVariant_TranslationNext(variant);
    int i, layers = Material_LayerCount(MaterialVariant_GeneralCase(variant));

    Con_Printf("Variant #%i: Spec:%p\n", *variantIdx, (void*)MaterialVariant_Spec(variant));

    // Print translation info:
    if(Material_HasTranslation(MaterialVariant_GeneralCase(variant)))
    {
        materialvariant_t* cur = MaterialVariant_TranslationCurrent(variant);
        float inter = MaterialVariant_TranslationPoint(variant);
        Uri* curUri = Materials_GetUri(MaterialVariant_GeneralCase(cur));
        ddstring_t* curPath = Uri_ToString(curUri);
        Uri* nextUri = Materials_GetUri(MaterialVariant_GeneralCase(next));
        ddstring_t* nextPath = Uri_ToString(nextUri);

        Con_Printf("  Translation: Current:\"%s\" Next:\"%s\" Inter:%f\n",
            F_PrettyPath(Str_Text(curPath)), F_PrettyPath(Str_Text(nextPath)), inter);

        Uri_Delete(curUri);
        Str_Delete(curPath);
        Uri_Delete(nextUri);
        Str_Delete(nextPath);
    }

    // Print layer info:
    for(i = 0; i < layers; ++i)
    {
        const materialvariant_layer_t* l = MaterialVariant_Layer(variant, i);
        Uri* uri = GL_NewUriForTexture(GL_ToTexture(l->tex));
        ddstring_t* path = Uri_ToString(uri);

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:(\"%s\" uid:%i)"
            "\n      Offset: %.2f x %.2f Glow:%.2f\n",
            i, l->stage, (int)l->tics, F_PrettyPath(Str_Text(path)), l->tex,
            l->texOrigin[0], l->texOrigin[1], l->glow);

        Uri_Delete(uri);
        Str_Delete(path);
    }

    ++(*variantIdx);

    return 0; // Continue iteration.
    }
}

static void printMaterialInfo(materialbind_t* mb)
{
    material_t* mat = MaterialBind_Material(mb);
    Uri* uri = Materials_GetUri(mat);
    ddstring_t* path = Uri_ToString(uri);
    int variantIdx = 0;

    Con_Printf("Material \"%s\" [%p] uid:%u origin:%s"
        "\nDimensions: %d x %d Layers:%i InGroup:%s Drawable:%s EnvClass:%s"
        "\nDecorated:%s Detailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
        F_PrettyPath(Str_Text(path)), (void*) mat, (uint) Material_BindId(mat),
        !Material_IsCustom(mat)     ? "iwad" : (Material_Definition(mat)->autoGenerated? "addon" : "def"),
        Material_Width(mat), Material_Height(mat), Material_LayerCount(mat),
        Material_IsGroupAnimated(mat)? "yes" : "no",
        Material_IsDrawable(mat)     ? "yes" : "no",
        Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialClassName(Material_EnvironmentClass(mat)),
        Material_HasDecorations(mat) ? "yes" : "no",
        Material_DetailTexture(mat)  ? "yes" : "no",
        Material_HasGlow(mat)        ? "yes" : "no",
        Material_ShinyTexture(mat)   ? "yes" : "no",
        Material_ShinyMaskTexture(mat)? "(masked)" : "",
        Material_IsSkyMasked(mat)    ? "yes" : "no");

    Material_IterateVariants(mat, printVariantInfo, (void*)&variantIdx);

    Str_Delete(path);
    Uri_Delete(uri);
}

static void printMaterialOverview(const materialbind_t* mb, boolean printNamespace)
{
    int numDigits = M_NumDigits(bindingsCount);
    material_t* mat = MaterialBind_Material(mb);
    Uri* uri = Materials_GetUri(mat);
    const ddstring_t* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));

    Con_Printf(" %*u: %-*s %5d x %-5d %-8s %s\n", numDigits, (unsigned int) Material_BindId(mat),
        printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
        Material_Width(mat), Material_Height(mat),
        Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialClassName(Material_EnvironmentClass(mat)),
        !Material_IsCustom(mat) ? "iwad" : (Material_Definition(mat)->autoGenerated? "addon" : "def"));

    Uri_Delete(uri);
    if(printNamespace)
        Str_Delete((ddstring_t*)path);
}

static materialbind_t** collectMaterials(materialnamespaceid_t namespaceId,
    const char* like, size_t* count, materialbind_t** storage)
{
    size_t n = 0;

    if(VALID_MATERIALNAMESPACEID(namespaceId))
    {
        if(bindings)
        {
            materialnamespace_t* mn = &namespaces[namespaceId-MATERIALNAMESPACE_FIRST];
            int i;
            for(i = 0; i < MATERIALNAMESPACE_NAMEHASH_SIZE; ++i)
            {
                materialnamespace_namehash_node_t* node;

                if(NULL == mn->nameHash[i]) continue;

                for(node = (materialnamespace_namehash_node_t*) mn->nameHash[i];
                    NULL != node; node = node->next)
                {
                    materialbind_t* mb = bindByIndex(node->bindId);
                    if(!(like && like[0] && strnicmp(Str_Text(MaterialBind_Name(mb)), like, strlen(like))))
                    {
                        if(storage)
                            storage[n++] = mb;
                        else
                            ++n;
                    }
                }
            }
        }
    }
    else
    {   // Any.
        materialnum_t i;
        for(i = 0; i < bindingsCount; ++i)
        {
            materialbind_t* mb = &bindings[i];
            if(like && like[0] && strnicmp(Str_Text(MaterialBind_Name(mb)), like, strlen(like)))
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
    return collectMaterials(namespaceId, like, count, storage);
}

static size_t printMaterials2(materialnamespaceid_t namespaceId, const char* like,
    boolean printNamespace)
{
    int numDigits = M_NumDigits(bindingsCount);
    size_t count = 0;
    materialbind_t** foundMaterials = collectMaterials(namespaceId, like, &count, 0);

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known materials in namespace '%s'", Str_Text(nameForMaterialNamespaceId(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known materials");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    if(!foundMaterials)
        return 0;

    // Print the result index key.
    Con_Printf(" %*s: %-*s %12s  envclass origin\n", numDigits, "uid",
        printNamespace? 22 : 14, printNamespace? "namespace:name" : "name",
        "dimensions");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundMaterials, count, sizeof(*foundMaterials), compareMaterialBindByName);

    { materialbind_t* const* ptr;
    for(ptr = foundMaterials; *ptr; ++ptr)
    {
        const materialbind_t* mb = *ptr;
        printMaterialOverview(mb, printNamespace);
    }}

    free(foundMaterials);
    return count;
}

static void printMaterials(materialnamespaceid_t namespaceId, const char* like)
{
    size_t printTotal = 0;
    // Do we care which namespace?
    if(namespaceId == MN_ANY && like && like[0])
    {
        printTotal = printMaterials2(namespaceId, like, true);
        Con_PrintRuler();
    }
    // Only one namespace to print?
    else if(VALID_MATERIALNAMESPACEID(namespaceId))
    {
        printTotal = printMaterials2(namespaceId, like, false);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each namespace separately.
        int i;
        for(i = MATERIALNAMESPACE_FIRST; i <= MATERIALNAMESPACE_LAST; ++i)
        {
            size_t printed = printMaterials2((materialnamespaceid_t)i, like, false);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Material" : "Materials");
}

boolean Materials_MaterialLinkedToAnimGroup(int groupNum, material_t* mat)
{
    animgroup_t* group = getAnimGroup(groupNum);
    if(NULL != group)
        return isInAnimGroup(group, mat);
    return false;
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

    if(0 == num || num > bindingsCount)
    {
        Con_Message("Materials_AddAnimGroupFrame: Invalid material num '%i'\n.", num);
        return;
    }
    mat = getMaterialByIndex(num - 1); // 1-based index.

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
    materialvariantspecification_t* spec = MaterialVariant_Spec(variant);
    materialvariant_t* current = Materials_Prepare(NULL, params->current, false, spec);
    materialvariant_t* next    = Materials_Prepare(NULL, params->next,    false, spec);

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
        float interp;

        /*{ ded_material_t* def = Material_Definition(mat);
        if(def && def->layers[0].stageCount.num > 1)
        {
            if(GL_TextureByUri(def->layers[0].stages[0].texture))
                continue; // Animated elsewhere.
        }}*/

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

material_t* MaterialBind_Material(const materialbind_t* mb)
{
    assert(mb);
    return mb->_material;
}

const ddstring_t* MaterialBind_Name(const materialbind_t* mb)
{
    assert(mb);
    return &mb->_name;
}

materialnamespaceid_t MaterialBind_Namespace(const materialbind_t* mb)
{
    assert(mb);
    return mb->_mnamespace;
}

materialbindinfo_t* MaterialBind_Info(materialbind_t* mb)
{
    assert(mb);
    return mb->_info;
}

void MaterialBind_AttachInfo(materialbind_t* mb, materialbindinfo_t* info)
{
    assert(mb);
    if(NULL == info)
        Con_Error("MaterialBind::AttachInfo: Attempted with invalid info.");
    if(NULL != mb->_info)
    {
#if _DEBUG
        ddstring_t name; Str_Init(&name);
        Str_Appendf(&name, "%s:%s", Str_Text(MaterialBind_Name(mb)),
            Str_Text(nameForMaterialNamespaceId(MaterialBind_Namespace(mb))));
        Con_Message("Warning:MaterialBind::AttachInfo: Info already present for \"%s\", replacing.",
            Str_Text(&name));
        Str_Free(&name);
#endif
        free(mb->_info);
    }
    mb->_info = info;
}

materialbindinfo_t* MaterialBind_DetachInfo(materialbind_t* mb)
{
    assert(mb);
    {
    materialbindinfo_t* info = mb->_info;
    mb->_info = NULL;
    return info;
    }
}

ded_detailtexture_t* MaterialBind_DetailTextureDef(const materialbind_t* mb)
{
    assert(mb);
    if(0 == mb->prepared || NULL == mb->_info)
        return NULL;
    return mb->_info->detailtextureDefs[mb->prepared-1];
}

ded_decor_t* MaterialBind_DecorationDef(const materialbind_t* mb)
{
    assert(mb);
    if(0 == mb->prepared || NULL == mb->_info)
        return NULL;
    return mb->_info->decorationDefs[mb->prepared-1];
}

ded_ptcgen_t* MaterialBind_PtcGenDef(const materialbind_t* mb)
{
    assert(mb);
    if(0 == mb->prepared || NULL == mb->_info)
        return NULL;
    return mb->_info->ptcgenDefs[mb->prepared-1];
}

ded_reflection_t* MaterialBind_ReflectionDef(const materialbind_t* mb)
{
    assert(mb);
    if(0 == mb->prepared || NULL == mb->_info)
        return NULL;
    return mb->_info->reflectionDefs[mb->prepared-1];
}

D_CMD(ListMaterials)
{
    materialnamespaceid_t namespaceId = MN_ANY;
    const char* like = NULL;
    Uri* uri = NULL;

    // "listmaterials [namespace] [name]"
    if(argc > 2)
    {
        uri = Uri_New();
        Uri_SetScheme(uri, argv[1]);
        Uri_SetPath(uri, argv[2]);

        namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
        if(!VALID_MATERIALNAMESPACEID(namespaceId))
        {
            Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(uri)));
            Uri_Delete(uri);
            return false;
        }
        like = Str_Text(Uri_Path(uri));
    }
    // "listmaterials [namespace:name]" i.e., a partial Uri
    else if(argc > 1)
    {
        uri = Uri_NewWithPath2(argv[1], RC_NULL);
        if(!Str_IsEmpty(Uri_Scheme(uri)))
        {
            namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
            if(!VALID_MATERIALNAMESPACEID(namespaceId))
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
            namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Path(uri)));

            if(!VALID_MATERIALNAMESPACEID(namespaceId))
            {
                namespaceId = MN_ANY;
                like = argv[1];
            }
        }
    }

    printMaterials(namespaceId, like);

    if(uri != NULL)
        Uri_Delete(uri);
    return true;
}

D_CMD(InspectMaterial)
{
    Uri* search = Uri_NewWithPath2(argv[1], RC_NULL);
    material_t* mat;

    if(!Str_IsEmpty(Uri_Scheme(search)))
    {
        materialnamespaceid_t namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(search)));
        if(!VALID_MATERIALNAMESPACEID(namespaceId))
        {
            Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(search)));
            Uri_Delete(search);
            return false;
        }
    }

    mat = Materials_ToMaterial(Materials_IndexForUri(search));
    if(mat)
    {
        printMaterialInfo(bindByIndex(Material_BindId(mat)));
    }
    else
    {
        ddstring_t* path = Uri_ToString(search);
        Con_Printf("Unknown material \"%s\".\n", Str_Text(path));
        Str_Delete(path);
    }
    Uri_Delete(search);
    return true;
}
