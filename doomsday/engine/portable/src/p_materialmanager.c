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
#include "pathdirectory.h"

/// Number of materials to block-allocate.
#define MATERIALS_BLOCK_ALLOC (32)

/// Number of elements to block-allocate in the material index to materialbind map.
#define MATERIALS_BINDINGMAP_BLOCK_ALLOC (32)

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
 * Info attached to a MaterialBind upon successfull preparation of the first
 * derived variant of the associated Material.
 */
typedef struct materialbindinfo_s {
    ded_decor_t* decorationDefs[2];
    ded_detailtexture_t* detailtextureDefs[2];
    ded_ptcgen_t* ptcgenDefs[2];
    ded_reflection_t* reflectionDefs[2];
} materialbindinfo_t;

typedef struct materialbind_s {
    /// Pointer to this binding's node in the directory.
    struct pathdirectory_node_s* _directoryNode;

    /// Bound material.
    material_t* _material;

    /// Unique identifier for this binding.
    materialnum_t _id;

    /// Extended info about this binding if present.
    materialbindinfo_t* _info;
} materialbind_t;

/// @return  Unique identifier associated with this.
materialnum_t MaterialBind_Id(const materialbind_t* mb);

/// @return  Material associated with this else @c NULL.
material_t* MaterialBind_Material(const materialbind_t* mb);

/// @return  PathDirectory node associated with this.
struct pathdirectory_node_s* MaterialBind_DirectoryNode(const materialbind_t* mb);

/// @return  Unique identifier of the namespace within which this binding resides.
materialnamespaceid_t MaterialBind_NamespaceId(const materialbind_t* mb);

/// @return  Symbolic name/path-to this binding. Must be destroyed with Str_Delete().
ddstring_t* MaterialBind_ComposePath(const materialbind_t* mb);

/// @return  Fully qualified/absolute Uri to this binding. Must be destroyed with Uri_Delete().
Uri* MaterialBind_ComposeUri(const materialbind_t* mb);

/// @return  Extended info owned by this else @c NULL.
materialbindinfo_t* MaterialBind_Info(const materialbind_t* mb);

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

static void updateMaterialBindInfo(materialbind_t* mb, boolean canCreate);

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
#if _DEBUG
D_CMD(PrintMaterialStats);
#endif

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
static materialbind_t** bindings;

static pathdirectory_t* namespaces[MATERIALNAMESPACE_COUNT];

void P_MaterialsRegister(void)
{
    C_CMD("inspectmaterial", "s",   InspectMaterial)
    C_CMD("listmaterials",  NULL,   ListMaterials)
#if _DEBUG
    C_CMD("materialstats",  NULL,   PrintMaterialStats)
#endif
}

static __inline pathdirectory_t* directoryForMaterialNamespaceId(materialnamespaceid_t id)
{
    assert(VALID_MATERIALNAMESPACEID(id));
    return namespaces[id-MATERIALNAMESPACE_FIRST];
}

static materialnamespaceid_t namespaceIdForMaterialDirectory(pathdirectory_t* pd)
{
    materialnamespaceid_t id;
    assert(pd);
    for(id = MATERIALNAMESPACE_FIRST; id <= MATERIALNAMESPACE_LAST; ++id)
        if(namespaces[id-MATERIALNAMESPACE_FIRST] == pd) return id;
    // Should never happen.
    Con_Error("Materials::namespaceIdForMaterialDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
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

static materialbind_t* bindById(uint bindId)
{
    if(0 == bindId || bindId > bindingsCount) return 0;
    return bindings[bindId-1];
}

static void updateMaterialBindInfo(materialbind_t* mb, boolean canCreate)
{
    assert(initedOk && mb);
    {
    materialbindinfo_t* info = MaterialBind_Info(mb);
    material_t* mat = MaterialBind_Material(mb);
    materialnum_t matNum = Materials_ToMaterialNum(mat);
    boolean isCustom = (mat? Material_IsCustom(mat) : false);

    if(!info)
    {
        if(!canCreate) return;

        // Create new info and attach to this binding.
        info = (materialbindinfo_t*) malloc(sizeof *info);
        if(!info)
            Con_Error("MaterialBind::LinkDefinitions: Failed on allocation of %lu bytes for "
                "new MaterialBindInfo.", (unsigned long) sizeof *info);
        MaterialBind_AttachInfo(mb, info);
    }

    // Surface decorations (lights and models).
    info->decorationDefs[0] = Def_GetDecoration(matNum, 0, isCustom);
    info->decorationDefs[1] = Def_GetDecoration(matNum, 1, isCustom);

    // Reflection (aka shiny surface).
    info->reflectionDefs[0] = Def_GetReflection(matNum, 0, isCustom);
    info->reflectionDefs[1] = Def_GetReflection(matNum, 1, isCustom);

    // Generator (particles).
    info->ptcgenDefs[0] = Def_GetGenerator(matNum, 0, isCustom);
    info->ptcgenDefs[1] = Def_GetGenerator(matNum, 1, isCustom);

    // Detail texture.
    info->detailtextureDefs[0] = Def_GetDetailTex(matNum, 0, isCustom);
    info->detailtextureDefs[1] = Def_GetDetailTex(matNum, 1, isCustom);
    }
}

static boolean newMaterialBind(const Uri* uri, material_t* material)
{
    pathdirectory_t* matDirectory = directoryForMaterialNamespaceId(DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri))));
    struct pathdirectory_node_s* node;
    materialbind_t* mb;

    node = PathDirectory_Insert(matDirectory, Str_Text(Uri_Path(uri)), MATERIALDIRECTORY_DELIMITER);

    // Is this a new binding?
    mb = (materialbind_t*) PathDirectoryNode_UserData(node);
    if(!mb)
    {
        // Acquire a new unique identifier for this binding.
        const materialnum_t bindId = ++bindingsCount;

        mb = (materialbind_t*) malloc(sizeof *mb);
        if(!mb)
        {
            Con_Error("Materials::newMaterialUriBinding: Failed on allocation of %lu bytes for new MaterialBind.",
                (unsigned long) sizeof *mb);
            exit(1); // Unreachable.
        }

        // Init the new binding.
        mb->_material = NULL;
        mb->_info = NULL;

        mb->_directoryNode = node;
        PathDirectoryNode_AttachUserData(node, mb);

        mb->_id = bindId;
        if(material)
        {
            Material_SetPrimaryBindId(material, bindId);
        }

        // Add the new binding to the bindings index/map.
        if(bindingsCount > bindingsMax)
        {
            // Allocate more memory.
            bindingsMax += MATERIALS_BINDINGMAP_BLOCK_ALLOC;
            bindings = (materialbind_t**) realloc(bindings, sizeof *bindings * bindingsMax);
            if(!bindings)
                Con_Error("Materials:newMaterialUriBinding: Failed on (re)allocation of %lu bytes for MaterialBind map.",
                    (unsigned long) sizeof *bindings * bindingsMax);
        }
        bindings[bindingsCount-1] = mb; /* 1-based index */
    }

    // (Re)configure the binding.
    mb->_material = material;
    updateMaterialBindInfo(mb, false/*do not create, only update if present*/);

    return true;
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
    materiallist_node_t* node = (materiallist_node_t*)malloc(sizeof *node);
    if(!node)
        Con_Error("linkMaterialToGlobalList: Failed on allocation of %lu bytes for "
            "new MaterialList::Node.", (unsigned long) sizeof *node);

    node->mat = mat;
    node->next = materials;
    materials = node;
    return mat;
}

void Materials_Initialize(void)
{
    if(initedOk) return; // Already been here.

    VERBOSE( Con_Message("Initializing Materials collection...\n") )

    variantSpecs = NULL;
    variantCacheQueue = NULL;

    materialsBlockSet = BlockSet_New(sizeof(material_t), MATERIALS_BLOCK_ALLOC);
    materials = NULL;

    bindings = NULL;
    bindingsCount = bindingsMax = 0;
    { int i;
    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        namespaces[i] = PathDirectory_New();
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

static int clearBinding(struct pathdirectory_node_s* node, void* paramaters)
{
    materialbind_t* mb = PathDirectoryNode_DetachUserData(node);
    materialbindinfo_t* info = MaterialBind_DetachInfo(mb);
    if(info)
    {
        free(info);
    }
    free(mb);
    return 0; // Continue iteration.
}

static void destroyBindings(void)
{
    assert(initedOk);

    { int i;
    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        PathDirectory_Iterate(namespaces[i], PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBinding);
        PathDirectory_Delete(namespaces[i]), namespaces[i] = NULL;
    }}

    // Clear the binding index/map.
    if(bindings)
    {
        free(bindings);
        bindings = NULL;
    }
    bindingsCount = bindingsMax = 0;
}

void Materials_Shutdown(void)
{
    if(!initedOk) return;

    Materials_PurgeCacheQueue();

    destroyBindings();
    destroyMaterials();
    destroyVariantSpecifications();

    initedOk = false;
}

static int clearBindingDefinitionLinks(const struct pathdirectory_node_s* node, void* paramaters)
{
    materialbind_t* mb = (materialbind_t*)PathDirectoryNode_UserData(node);
    materialbindinfo_t* info = MaterialBind_Info(mb);
    if(info)
    {
        info->decorationDefs[0]    = info->decorationDefs[1]    = NULL;
        info->detailtextureDefs[0] = info->detailtextureDefs[1] = NULL;
        info->ptcgenDefs[0]        = info->ptcgenDefs[1]        = NULL;
        info->reflectionDefs[0]    = info->reflectionDefs[1]    = NULL;
    }
    return 0; // Continue iteration.
}

void Materials_ClearDefinitionLinks(void)
{
    materiallist_node_t* node;
    materialnamespaceid_t namespaceId;

    assert(initedOk);
    for(node = materials; node; node = node->next)
    {
        material_t* mat = node->mat;
        Material_SetDefinition(mat, NULL);
    }

    for(namespaceId = MATERIALNAMESPACE_FIRST; namespaceId <= MATERIALNAMESPACE_LAST; ++namespaceId)
    {
        pathdirectory_t* matDirectory = directoryForMaterialNamespaceId(namespaceId);
        PathDirectory_Iterate_Const(matDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBindingDefinitionLinks);
    }
}

void Materials_Rebuild(material_t* mat, ded_material_t* def)
{
    if(!initedOk || !mat || !def) return;

    /// \todo We should be able to rebuild the variants.
    Material_DestroyVariants(mat);
    Material_SetDefinition(mat, def);
    Material_SetFlags(mat, def->flags);
    Material_SetDimensions(mat, def->width, def->height);
    Material_SetEnvironmentClass(mat, S_MaterialEnvClassForUri(def->id));

    // Textures are updated automatically at prepare-time, so just clear them.
    Material_SetDetailTexture(mat, NULL);
    Material_SetShinyTexture(mat, NULL);
    Material_SetShinyMaskTexture(mat, NULL);

    // Update bindings.
    { materialnum_t i;
    for(i = 0; i < bindingsCount; ++i)
    {
        materialbind_t* mb = bindings[i];
        if(MaterialBind_Material(mb) != mat) continue;

        updateMaterialBindInfo(mb, false /*do not create, only update if present*/);
    }}
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

static int releaseGLTexturesForMaterial(struct pathdirectory_node_s* node, void* paramaters)
{
    materialbind_t* mb = PathDirectoryNode_UserData(node);
    material_t* mat = MaterialBind_Material(mb);
    if(mat)
    {
        Material_IterateVariants(mat, releaseGLTexturesForMaterialWorker, NULL);
    }
    return 0; // Continue iteration.
}

void Materials_ReleaseGLTextures(materialnamespaceid_t namespaceId)
{
    pathdirectory_t* matDirectory;

    if(namespaceId == MN_ANY)
    {   // Delete the lot.
        GL_ReleaseGLTexturesByNamespace(TN_ANY);
        return;
    }

    if(!VALID_MATERIALNAMESPACEID(namespaceId))
        Con_Error("Materials_ReleaseGLTextures: Internal error, "
                  "invalid materialgroup '%i'.", (int) namespaceId);

    matDirectory = directoryForMaterialNamespaceId(namespaceId);
    PathDirectory_Iterate(matDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, releaseGLTexturesForMaterial);
}

const ddstring_t* Materials_NamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace)
{
    return nameForMaterialNamespaceId(materialNamespaceIdForTextureNamespaceId(texNamespace));
}

material_t* Materials_ToMaterial(materialnum_t num)
{
    materialbind_t* mb;
    if(!initedOk) return NULL;
    mb = bindById((uint)num);
    if(!mb) return NULL;
    return MaterialBind_Material(mb);
}

materialnum_t Materials_ToMaterialNum(material_t* mat)
{
    materialbind_t* mb;
    if(!initedOk) return 0;
    if(!mat) return 0;
    mb = bindById(Material_PrimaryBindId(mat));
    if(!mb) return 0;
    return MaterialBind_Id(mb);
}

materialbind_t* Materials_PrimaryBind(material_t* mat)
{
    return bindById(Material_PrimaryBindId(mat));
}

/**
 * @defgroup validateMaterialUriFlags  Validate Material Uri Flags
 * @{
 */
#define VMUF_ALLOW_NAMESPACE_ANY 0x1 /// The Scheme component of the uri may be of zero-length; signifying "any namespace".
/**@}*/

/**
 * @param uri  Uri to be validated.
 * @param flags  @see validateMaterialUriFlags
 * @param quiet  @c true= Do not output validation remarks to the log.
 * @return  @c true if @a Uri passes validation.
 */
static boolean validateMaterialUri2(const Uri* uri, int flags, boolean quiet)
{
    materialnamespaceid_t namespaceId;

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Invalid path '%s' in Material uri \"%s\".\n", Str_Text(Uri_Path(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
        }
        return false;
    }

    namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
    if(!((flags & VMUF_ALLOW_NAMESPACE_ANY) && namespaceId == MN_ANY) &&
       !VALID_MATERIALNAMESPACEID(namespaceId))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Unknown namespace '%s' in Material uri \"%s\".\n", Str_Text(Uri_Scheme(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
        }
        return false;
    }

    return true;
}

static boolean validateMaterialUri(const Uri* uri, int flags)
{
    return validateMaterialUri2(uri, flags, false);
}

/**
 * Given a directory and path, search the Materials collection for a match.
 *
 * @param directory  Namespace-specific PathDirectory to search in.
 * @param path  Path of the material to search for.
 * @return  Found Material else @c 0
 */
static material_t* findMaterialForPath(pathdirectory_t* matDirectory, const char* path)
{
    struct pathdirectory_node_s* node = DD_SearchPathDirectory(matDirectory,
        PCF_NO_BRANCH|PCF_MATCH_FULL, path, MATERIALDIRECTORY_DELIMITER);
    if(node)
    {
        return MaterialBind_Material((materialbind_t*) PathDirectoryNode_UserData(node));
    }
    return NULL; // Not found.
}

/// \assume @a uri has already been validated and is well-formed.
static material_t* findMaterialForUri(const Uri* uri)
{
    materialnamespaceid_t namespaceId = DD_ParseMaterialNamespace(Str_Text(Uri_Scheme(uri)));
    const char* path = Str_Text(Uri_Path(uri));
    material_t* mat = NULL;
    if(namespaceId != MN_ANY)
    {
        // Caller wants a material in a specific namespace.
        mat = findMaterialForPath(directoryForMaterialNamespaceId(namespaceId), path);
    }
    else
    {
        // Caller does not care which namespace.
        // Check for the material in these namespaces in priority order.
        static const materialnamespaceid_t order[] = {
            MN_SPRITES, MN_TEXTURES, MN_FLATS, MN_ANY
        };
        int n = 0;
        do
        {
            mat = findMaterialForPath(directoryForMaterialNamespaceId(order[n]), path);
        } while(!mat && order[++n] != MN_ANY);
    }
    return mat;
}

material_t* Materials_MaterialForUri2(const Uri* uri, boolean quiet)
{
    material_t* mat;
    if(!initedOk || !uri) return NULL;
    if(!validateMaterialUri2(uri, VMUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning:Materials::MaterialForUri: Uri \"%s\" failed to validate, returing NULL.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
#endif
        return NULL;
    }

    // Perform the search.
    mat = findMaterialForUri(uri);
    if(mat) return mat;

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Materials::MaterialForUri: \"%s\" not found!\n", Str_Text(path));
        Str_Delete(path);
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
material_t* Materials_MaterialForUri(const Uri* uri)
{
    return Materials_MaterialForUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);

}

material_t* Materials_MaterialForUriCString2(const char* path, boolean quiet)
{
    if(path && path[0])
    {
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        material_t* mat = Materials_MaterialForUri2(uri, quiet);
        Uri_Delete(uri);
        return mat;
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
material_t* Materials_MaterialForUriCString(const char* path)
{
    return Materials_MaterialForUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

/// \note Part of the Doomsday public API.
Uri* Materials_ComposeUri(material_t* mat)
{
    materialbind_t* mb;
    if(!mat)
    {
#if _DEBUG
        Con_Message("Warning:Materials::ComposeUri: Attempted with invalid index (num==0), returning null-object.\n");
#endif
        return Uri_New();
    }
    mb = Materials_PrimaryBind(mat);
    if(!mb)
    {
#if _DEBUG
        Con_Message("Warning:Materials::ComposeUri: Attempted with non-bound material [%p], returning null-object.\n", (void*)mat);
#endif
        return Uri_New();
    }
    return MaterialBind_ComposeUri(mb);
}

material_t* Materials_CreateFromDef(ded_material_t* def)
{
    assert(def);
    {
    const Uri* uri = def->id;
    const texture_t* tex;
    material_t* mat;

    if(!initedOk) return NULL;

    // We require a properly formed uri.
    if(!validateMaterialUri2(uri, 0, (verbose >= 1)))
    {
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning, failed to create Material \"%s\" from definition %p, ignoring.\n", Str_Text(uriStr), (void*)def);
        Str_Delete(uriStr);
        return NULL;
    }

    // Have we already created a material for this?
    mat = findMaterialForUri(uri);
    if(mat)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:Materials::CreateFromDef: A Material with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return mat;
    }

    // Ensure the primary layer has a valid texture reference.
    tex = NULL;
    if(def->layers[0].stageCount.num > 0)
    {
        const ded_material_layer_t* l = &def->layers[0];
        if(l->stages[0].texture) // Not unused.
        {
            tex = GL_TextureByUri(l->stages[0].texture);
            if(!tex)
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
    if(!tex) return NULL;

    // A new Material.
    mat = linkMaterialToGlobalList(allocMaterial());
    mat->_flags = def->flags;
    mat->_isCustom = !Texture_IsFromIWAD(tex);
    mat->_def    = def;
    mat->_width  = MAX_OF(0, def->width);
    mat->_height = MAX_OF(0, def->height);
    mat->_envClass = S_MaterialEnvClassForUri(uri);
    newMaterialBind(uri, mat);

    return mat;
    }
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
    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

    { materiallist_node_t* node = materials;
    while(node)
    {
        Material_Ticker(node->mat, time);
        node = node->next;
    }}

    if(DD_IsSharpTick())
    {
        animateAnimGroups();
    }
}

static texture_t* findDetailTextureForDef(const ded_detailtexture_t* def)
{
    assert(def);
    {
    detailtex_t* dTex = R_FindDetailTextureForName(def->detailTex, def->isExternal);
    if(!dTex) return NULL;
    return GL_ToTexture(dTex->id);
    }
}

static texture_t* findShinyTextureForDef(const ded_reflection_t* def)
{
    assert(def);
    {
    shinytex_t* sTex = R_FindShinyTextureForName(def->shinyMap);
    if(!sTex) return NULL;
    return GL_ToTexture(sTex->id);
    }
}

static texture_t* findShinyMaskTextureForDef(const ded_reflection_t* def)
{
    assert(def);
    {
    masktex_t* mTex = R_FindMaskTextureForName(def->maskMap);
    if(!mTex) return NULL;
    return GL_ToTexture(mTex->id);
    }
}

static void updateMaterialTextureLinks(materialbind_t* mb)
{
    material_t* mat = MaterialBind_Material(mb);

    // We may need to need to construct and attach the info.
    updateMaterialBindInfo(mb, true /* create if not present */);

    if(!mat) return;

    { const ded_detailtexture_t* def = MaterialBind_DetailTextureDef(mb);
    Material_SetDetailTexture(mat, (def? findDetailTextureForDef(def) : NULL));
    Material_SetDetailStrength(mat, (def? def->strength : 0));
    Material_SetDetailScale(mat, (def? def->scale : 0));
    }

    { const ded_reflection_t* def = MaterialBind_ReflectionDef(mb);
    float black[3] = { 0, 0, 0 };
    Material_SetShinyTexture(mat, (def? findShinyTextureForDef(def) : NULL));
    Material_SetShinyMaskTexture(mat, (def? findShinyMaskTextureForDef(def) : NULL));
    Material_SetShinyBlendmode(mat, (def? def->blendMode : BM_ADD));
    Material_SetShinyMinColor(mat, (def? def->minColor : black));
    Material_SetShinyStrength(mat, (def? def->shininess : 0));
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
    materialvariant_t* variant;

    memset(texUnits, 0, sizeof(texUnits));

    // Have we already registered a suitable variant?
    variant = Materials_ChooseVariant(mat, spec);
    if(!variant)
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
                    if(mat != other && !Materials_ChooseVariant(other, spec))
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

        if(0 == i && (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
        {
            materialbind_t* mb = Materials_PrimaryBind(mat);

            // Primary texture was (re)prepared.
            Material_SetPrepared(mat, result == PTR_UPLOADED_ORIGINAL? 1 : 2);

            if(mb)
            {
                updateMaterialTextureLinks(mb);
            }

            // Are we inheriting the logical dimensions from the texture?
            if(0 == Material_Width(mat) && 0 == Material_Height(mat))
            {
                Material_SetDimensions(mat, Texture_Width(tex), Texture_Height(tex));
            }
        }
    }}

    // Do we need to prepare a DetailTexture?
    if(r_detail)
    {
        texture_t* tex = Material_DetailTexture(mat);
        if(tex)
        {
            const float contrast = Material_DetailStrength(mat) * detailFactor;
            texturevariantspecification_t* texSpec = GL_DetailTextureVariantSpecificationForContext(contrast);
            texUnits[MTU_DETAIL].tex = GL_PrepareTextureVariant(tex, texSpec);
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(useShinySurfaces)
    {
        texture_t* tex = Material_ShinyTexture(mat);
        if(tex)
        {
            texturevariantspecification_t* texSpec =
                GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                    TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                    false, false, false, false);

            texUnits[MTU_REFLECTION].tex = GL_PrepareTextureVariant(tex, texSpec);

            // We are only interested in a mask if we have a shiny texture.
            if(texUnits[MTU_REFLECTION].tex && (tex = Material_ShinyMaskTexture(mat)))
            {
                texSpec = GL_TextureVariantSpecificationForContext(
                              TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                              -1, -1, -1, true, false, false, false);
                texUnits[MTU_REFLECTION_MASK].tex = GL_PrepareTextureVariant(tex, texSpec);
            }
        }
    }

    // If we aren't taking a snapshot; get out of here.
    if(!snapshot) return variant;

    Materials_InitSnapshot(snapshot);

    if(0 == Material_Width(mat) && 0 == Material_Height(mat))
        return variant;

    snapshot->width = Material_Width(mat);
    snapshot->height = Material_Height(mat);
    snapshot->glowing = MaterialVariant_Layer(variant, 0)->glow * glowFactor;
    snapshot->isOpaque = NULL != texUnits[MTU_PRIMARY].tex &&
        !TextureVariant_IsMasked(texUnits[MTU_PRIMARY].tex);

    // Setup the primary texture unit.
    if(texUnits[MTU_PRIMARY].tex)
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
        if(texUnits[MTU_DETAIL].tex && snapshot->isOpaque)
        {
            const texturevariant_t* tex = texUnits[MTU_DETAIL].tex;
            const float width  = Texture_Width(TextureVariant_GeneralCase(tex));
            const float height = Texture_Height(TextureVariant_GeneralCase(tex));
            float scale = Material_DetailScale(mat);

            // Apply the global scaling factor.
            if(detailScale > .0001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, tex, BM_NORMAL,
                texMagMode?GL_LINEAR:GL_NEAREST, 1.f / width * scale, 1.f / height * scale, 0, 0, 1);
        }

        // Setup the shiny texture units?
        if(texUnits[MTU_REFLECTION].tex)
        {
            const texturevariant_t* tex = texUnits[MTU_REFLECTION].tex;
            const blendmode_t blendmode = Material_ShinyBlendmode(mat);
            const float strength = Material_ShinyStrength(mat);

            setTexUnit(snapshot, MTU_REFLECTION, tex, blendmode, GL_LINEAR, 1, 1, 0, 0, strength);
        }

        if(texUnits[MTU_REFLECTION_MASK].tex)
        {
            const texturevariant_t* tex = texUnits[MTU_REFLECTION_MASK].tex;

            setTexUnit(snapshot, MTU_REFLECTION_MASK, tex, BM_NORMAL,
                snapshot->units[MTU_PRIMARY].magMode,
                1.f / (snapshot->width * Texture_Width(TextureVariant_GeneralCase(tex))),
                1.f / (snapshot->height * Texture_Height(TextureVariant_GeneralCase(tex))),
                snapshot->units[MTU_PRIMARY].offset[0], snapshot->units[MTU_PRIMARY].offset[1], 1);
        }
    }

    if(MC_MAPSURFACE == spec->context && texUnits[MTU_REFLECTION].tex)
    {
        const float* minColor = Material_ShinyMinColor(mat);
        snapshot->shinyMinColor[CR] = minColor[CR];
        snapshot->shinyMinColor[CG] = minColor[CG];
        snapshot->shinyMinColor[CB] = minColor[CB];
    }

    if(MC_SKYSPHERE == spec->context && texUnits[MTU_PRIMARY].tex)
    {
        const texturevariant_t* tex = texUnits[MTU_PRIMARY].tex;
        const averagecolor_analysis_t* avgTopColor = (const averagecolor_analysis_t*)
            Texture_Analysis(TextureVariant_GeneralCase(tex), TA_SKY_SPHEREFADEOUT);
        assert(avgTopColor);
        snapshot->topColor[CR] = avgTopColor->color[CR];
        snapshot->topColor[CG] = avgTopColor->color[CG];
        snapshot->topColor[CB] = avgTopColor->color[CB];
    }

    if((MC_MAPSURFACE == spec->context || MC_SKYSPHERE == spec->context) && texUnits[MTU_PRIMARY].tex)
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

const ded_decor_t* Materials_DecorationDef(material_t* mat)
{
    if(!mat) return 0;
    if(!Material_Prepared(mat))
        Materials_Prepare(NULL, mat, false,
            Materials_VariantSpecificationForContext(MC_MAPSURFACE,
                0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false));
    return MaterialBind_DecorationDef(Materials_PrimaryBind(mat));
}

const ded_ptcgen_t* Materials_PtcGenDef(material_t* mat)
{
    if(!mat) return 0;
    if(!Material_Prepared(mat))
        Materials_Prepare(NULL, mat, false,
            Materials_VariantSpecificationForContext(MC_MAPSURFACE,
                0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false));
    return MaterialBind_PtcGenDef(Materials_PrimaryBind(mat));
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
        Uri* curUri = Materials_ComposeUri(MaterialVariant_GeneralCase(cur));
        ddstring_t* curPath = Uri_ToString(curUri);
        Uri* nextUri = Materials_ComposeUri(MaterialVariant_GeneralCase(next));
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

static void printMaterialInfo(material_t* mat)
{
    Uri* uri = Materials_ComposeUri(mat);
    ddstring_t* path = Uri_ToString(uri);
    int variantIdx = 0;

    Con_Printf("Material \"%s\" [%p] uid:%u origin:%s"
        "\nDimensions: %d x %d Layers:%i InGroup:%s Drawable:%s EnvClass:%s"
        "\nDecorated:%s Detailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
        F_PrettyPath(Str_Text(path)), (void*) mat, (uint) Materials_ToMaterialNum(mat),
        !Material_IsCustom(mat)     ? "iwad" : (Material_Definition(mat)->autoGenerated? "addon" : "def"),
        Material_Width(mat), Material_Height(mat), Material_LayerCount(mat),
        Material_IsGroupAnimated(mat)? "yes" : "no",
        Material_IsDrawable(mat)     ? "yes" : "no",
        Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(mat)),
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

static void printMaterialOverview(material_t* mat, boolean printNamespace)
{
    int numDigits = M_NumDigits(Materials_Count());
    Uri* uri = Materials_ComposeUri(mat);
    const ddstring_t* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));

    Con_Printf(" %*u: %-*s %5d x %-5d %-8s %s\n", numDigits, (unsigned int) Materials_ToMaterialNum(mat),
        printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
        Material_Width(mat), Material_Height(mat),
        Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(mat)),
        !Material_IsCustom(mat) ? "iwad" : (Material_Definition(mat)->autoGenerated? "addon" : "def"));

    Uri_Delete(uri);
    if(printNamespace)
        Str_Delete((ddstring_t*)path);
}

/**
 * \todo A horridly inefficent algorithm. This should be implemented in PathDirectory
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the material search/listing console commands
 * so is not hugely important right now.
 */
typedef struct {
    const char* like;
    int idx;
    materialbind_t** storage;
} collectmaterialworker_paramaters_t;

static int collectMaterialWorker(const struct pathdirectory_node_s* node, void* paramaters)
{
    materialbind_t* mb = (materialbind_t*)PathDirectoryNode_UserData(node);
    collectmaterialworker_paramaters_t* p = (collectmaterialworker_paramaters_t*)paramaters;

    if(p->like && p->like[0])
    {
        Uri* uri = MaterialBind_ComposeUri(mb);
        int delta = strnicmp(Str_Text(Uri_Path(uri)), p->like, strlen(p->like));
        Uri_Delete(uri);
        if(delta) return 0; // Continue iteration.
    }

    if(p->storage)
    {
        p->storage[p->idx++] = mb;
    }
    else
    {
        ++p->idx;
    }

    return 0; // Continue iteration.
}

static materialbind_t** collectMaterials(materialnamespaceid_t namespaceId,
    const char* like, int* count, materialbind_t** storage)
{
    collectmaterialworker_paramaters_t p;
    materialnamespaceid_t fromId, toId, iterId;

    if(VALID_MATERIALNAMESPACEID(namespaceId))
    {
        // Only consider materials in this namespace.
        fromId = toId = namespaceId;
    }
    else
    {
        // Consider materials in any namespace.
        fromId = MATERIALNAMESPACE_FIRST;
        toId   = MATERIALNAMESPACE_LAST;
    }

    p.idx = 0;
    p.like = like;
    p.storage = storage;
    for(iterId  = fromId; iterId <= toId; ++iterId)
    {
        pathdirectory_t* matDirectory = directoryForMaterialNamespaceId(iterId);
        PathDirectory_Iterate2_Const(matDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
            PATHDIRECTORY_NOHASH, collectMaterialWorker, (void*)&p);
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
        return 0;
    }

    storage = (materialbind_t**)malloc(sizeof *storage * (p.idx+1));
    if(!storage)
        Con_Error("collectMaterials: Failed on allocation of %lu bytes for new collection.",
            (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectMaterials(namespaceId, like, count, storage);
}

static int compareMaterialBindByPath(const void* mbA, const void* mbB)
{
    Uri* a = MaterialBind_ComposeUri(*(const materialbind_t**)mbA);
    Uri* b = MaterialBind_ComposeUri(*(const materialbind_t**)mbB);
    int delta = stricmp(Str_Text(Uri_Path(a)), Str_Text(Uri_Path(b)));
    Uri_Delete(b);
    Uri_Delete(a);
    return delta;
}

static size_t printMaterials2(materialnamespaceid_t namespaceId, const char* like,
    boolean printNamespace)
{
    int numDigits = M_NumDigits(Materials_Count());
    int count = 0;
    materialbind_t** foundMaterials = collectMaterials(namespaceId, like, &count, NULL);

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
    qsort(foundMaterials, (size_t)count, sizeof *foundMaterials, compareMaterialBindByPath);

    { materialbind_t* const* ptr;
    for(ptr = foundMaterials; *ptr; ++ptr)
    {
        const materialbind_t* mb = *ptr;
        material_t* mat = MaterialBind_Material(mb);
        if(!mat) continue;
        printMaterialOverview(mat, printNamespace);
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

/// \note Part of the Doomsday public API.
void Materials_AddAnimGroupFrame(int groupNum, struct material_s* mat, int tics, int randomTics)
{
    animgroup_t* group = getAnimGroup(groupNum);
    animframe_t* frame;

    if(!group)
        Con_Error("Materials::AddAnimGroupFrame: Unknown anim group '%i', ignoring.\n", groupNum);

    if(!mat)
    {
#if _DEBUG
        Con_Message("Warning::Materials::AddAnimGroupFrame: Invalid material (ref=0), ignoring.\n");
#endif
        return;
    }

    // Mark the material as being in an animgroup.
    Material_SetGroupAnimated(mat, true);

    // Allocate a new animframe.
    group->frames = Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count, PU_APPSTATIC);

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

materialnum_t MaterialBind_Id(const materialbind_t* mb)
{
    assert(mb);
    return mb->_id;
}

material_t* MaterialBind_Material(const materialbind_t* mb)
{
    assert(mb);
    return mb->_material;
}

struct pathdirectory_node_s* MaterialBind_DirectoryNode(const materialbind_t* mb)
{
    assert(mb);
    return mb->_directoryNode;
}

materialnamespaceid_t MaterialBind_NamespaceId(const materialbind_t* mb)
{
    return namespaceIdForMaterialDirectory(PathDirectoryNode_Directory(MaterialBind_DirectoryNode(mb)));
}

ddstring_t* MaterialBind_ComposePath(const materialbind_t* mb)
{
    struct pathdirectory_node_s* node = mb->_directoryNode;
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, MATERIALDIRECTORY_DELIMITER);
}

Uri* MaterialBind_ComposeUri(const materialbind_t* mb)
{
    ddstring_t* path = MaterialBind_ComposePath(mb);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(nameForMaterialNamespaceId(MaterialBind_NamespaceId(mb))));
    Str_Delete(path);
    return uri;
}

materialbindinfo_t* MaterialBind_Info(const materialbind_t* mb)
{
    assert(mb);
    return mb->_info;
}

void MaterialBind_AttachInfo(materialbind_t* mb, materialbindinfo_t* info)
{
    assert(mb);
    if(!info)
        Con_Error("MaterialBind::AttachInfo: Attempted with invalid info.");
    if(mb->_info)
    {
#if _DEBUG
        Uri* uri = MaterialBind_ComposeUri(mb);
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:MaterialBind::AttachInfo: Info already present for \"%s\", replacing.", Str_Text(path));
        Str_Delete(path);
        Uri_Delete(uri);
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
    if(!mb->_info || !mb->_material || !Material_Prepared(mb->_material)) return NULL;
    return mb->_info->detailtextureDefs[Material_Prepared(mb->_material)-1];
}

ded_decor_t* MaterialBind_DecorationDef(const materialbind_t* mb)
{
    assert(mb);
    if(!mb->_info || !mb->_material || !Material_Prepared(mb->_material)) return NULL;
    return mb->_info->decorationDefs[Material_Prepared(mb->_material)-1];
}

ded_ptcgen_t* MaterialBind_PtcGenDef(const materialbind_t* mb)
{
    assert(mb);
    if(!mb->_info || !mb->_material || !Material_Prepared(mb->_material)) return NULL;
    return mb->_info->ptcgenDefs[Material_Prepared(mb->_material)-1];
}

ded_reflection_t* MaterialBind_ReflectionDef(const materialbind_t* mb)
{
    assert(mb);
    if(!mb->_info || !mb->_material || !Material_Prepared(mb->_material)) return NULL;
    return mb->_info->reflectionDefs[Material_Prepared(mb->_material)-1];
}

D_CMD(ListMaterials)
{
    materialnamespaceid_t namespaceId = MN_ANY;
    const char* like = NULL;
    Uri* uri = NULL;

    if(!Materials_Count())
    {
        Con_Message("There are currently no materials defined/loaded.\n");
        return true;
    }

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

    mat = Materials_MaterialForUri(search);
    if(mat)
    {
        printMaterialInfo(mat);
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

#if _DEBUG
D_CMD(PrintMaterialStats)
{
    materialnamespaceid_t namespaceId;
    Con_FPrintf(CPF_YELLOW, "Material Statistics:\n");
    for(namespaceId = MATERIALNAMESPACE_FIRST; namespaceId <= MATERIALNAMESPACE_LAST; ++namespaceId)
    {
        pathdirectory_t* matDirectory = directoryForMaterialNamespaceId(namespaceId);
        uint size;

        if(!matDirectory) continue;

        size = PathDirectory_Size(matDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(nameForMaterialNamespaceId(namespaceId)), size, size==1? "material":"materials");
        PathDirectory_PrintHashDistribution(matDirectory);
        PathDirectory_Print(matDirectory, MATERIALDIRECTORY_DELIMITER);
    }
    return true;
}
#endif
