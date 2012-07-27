/**\file materials.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
    material_t* mat;
    const materialvariantspecification_t* spec;
    boolean smooth;
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
    PathDirectoryNode* _directoryNode;

    /// Bound material.
    material_t* _material;

    /// Unique identifier for this binding.
    materialid_t _id;

    /// Extended info about this binding if present.
    materialbindinfo_t* _info;
} materialbind_t;

/// @return  Unique identifier associated with this.
materialid_t MaterialBind_Id(const materialbind_t* mb);

/// @return  Material associated with this else @c NULL.
material_t* MaterialBind_Material(const materialbind_t* mb);

/**
 * Change the Material associated with this binding. Note that only the
 * relationship between MaterialBind and Material is updated!
 *
 * \post If @a material differs from that currently associated with this,
 *     any MaterialBindInfo presently owned by this is destroyed (it is
 *     now invalid).
 *
 * @param  material  New Material to associate with this.
 * @return  Same as @a material for caller convenience.
 */
material_t* MaterialBind_SetMaterial(materialbind_t* mb, material_t* material);

/// @return  PathDirectory node associated with this.
PathDirectoryNode* MaterialBind_DirectoryNode(const materialbind_t* mb);

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

typedef struct materialanim_frame_s {
    material_t* material;
    ushort tics;
    ushort random;
} materialanim_frame_t;

typedef struct materialanim_s {
    int id;
    int flags;
    int index;
    int maxTimer;
    int timer;
    int count;
    materialanim_frame_t* frames;
} materialanim_t;

static int numgroups;
static materialanim_t* groups;

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
 * 2) Public material identifiers (materialid_t) are similarly eternal.
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
static uint materialCount;

static uint bindingCount = 0;

/// LUT which translates materialid_t to materialbind_t*. Index with materialid_t-1
static uint bindingIdMapSize = 0;
static materialbind_t** bindingIdMap = NULL;

// Material namespaces contain mappings between names and MaterialBind instances.
static PathDirectory* namespaces[MATERIALNAMESPACE_COUNT];

void Materials_Register(void)
{
    C_CMD("inspectmaterial", "s",   InspectMaterial)
    C_CMD("listmaterials",  NULL,   ListMaterials)
#if _DEBUG
    C_CMD("materialstats",  NULL,   PrintMaterialStats)
#endif
}

static void errorIfNotInited(const char* callerName)
{
    if(initedOk) return;
    Con_Error("%s: Materials collection is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static __inline PathDirectory* getDirectoryForNamespaceId(materialnamespaceid_t id)
{
    assert(VALID_MATERIALNAMESPACEID(id));
    return namespaces[id-MATERIALNAMESPACE_FIRST];
}

static materialnamespaceid_t namespaceIdForDirectory(PathDirectory* pd)
{
    materialnamespaceid_t id;
    assert(pd);

    for(id = MATERIALNAMESPACE_FIRST; id <= MATERIALNAMESPACE_LAST; ++id)
    {
        if(namespaces[id-MATERIALNAMESPACE_FIRST] == pd) return id;
    }
    // Should never happen.
    Con_Error("Materials::namespaceIdForDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
}

static materialnamespaceid_t namespaceIdForDirectoryNode(const PathDirectoryNode* node)
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
    const ddstring_t* namespaceName = Materials_NamespaceName(namespaceIdForDirectoryNode(node));
    ddstring_t* path = composePathForDirectoryNode(node, MATERIALS_PATH_DELIMITER);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(namespaceName));
    Str_Delete(path);
    return uri;
}

static materialanim_t* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups) return NULL;
    return &groups[number];
}

static boolean isInAnimGroup(const materialanim_t* group, const material_t* mat)
{
    int i;
    assert(group);

    if(!mat) return false;
    for(i = 0; i < group->count; ++i)
    {
        if(group->frames[i].material == mat)
            return true;
    }
    return false;
}

static materialvariantspecification_t* copyVariantSpecification(
    const materialvariantspecification_t* tpl)
{
    materialvariantspecification_t* spec = (materialvariantspecification_t*) malloc(sizeof *spec);
    if(!spec)
        Con_Error("Materials::copyVariantSpecification: Failed on allocation of %lu bytes for new MaterialVariantSpecification.", (unsigned long) sizeof *spec);
    memcpy(spec, tpl, sizeof *spec);
    return spec;
}

static int compareVariantSpecifications(const materialvariantspecification_t* a,
    const materialvariantspecification_t* b)
{
    assert(a && b);
    if(a == b) return 1;
    if(a->context != b->context) return 0;
    return GL_CompareTextureVariantSpecifications(a->primarySpec, b->primarySpec);
}

static materialvariantspecification_t* applyVariantSpecification(
    materialvariantspecification_t* spec, materialcontext_t mc,
    texturevariantspecification_t* primarySpec)
{
    assert(spec && (mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc)) && primarySpec);
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
        if(compareVariantSpecifications(node->spec, tpl))
            return node->spec;
    }
    if(!canCreate)
        return NULL;
    return linkVariantSpecification(copyVariantSpecification(tpl));
    }
}

static materialvariantspecification_t* getVariantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    static materialvariantspecification_t tpl;
    assert(initedOk && (mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc)));
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
    choosevariantworker_paramaters_t* p = (choosevariantworker_paramaters_t*) paramaters;
    const materialvariantspecification_t* cand = MaterialVariant_Spec(variant);
    assert(p);

    if(compareVariantSpecifications(cand, p->spec))
    {
        // This will do fine.
        p->chosen = variant;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

static materialvariant_t* chooseVariant(material_t* mat, const materialvariantspecification_t* spec)
{
    choosevariantworker_paramaters_t params;
    assert(mat && spec);

    params.spec = spec;
    params.chosen = NULL;
    Material_IterateVariants(mat, chooseVariantWorker, &params);
    return params.chosen;
}

static materialbind_t* getMaterialBindForId(materialid_t id)
{
    if(0 == id || id > bindingCount) return NULL;
    return bindingIdMap[id-1];
}

static void updateMaterialBindInfo(materialbind_t* mb, boolean canCreate)
{
    materialbindinfo_t* info = MaterialBind_Info(mb);
    material_t* mat = MaterialBind_Material(mb);
    materialid_t matId = Materials_Id(mat);
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
    info->decorationDefs[0] = Def_GetDecoration(matId, 0, isCustom);
    info->decorationDefs[1] = Def_GetDecoration(matId, 1, isCustom);

    // Reflection (aka shiny surface).
    info->reflectionDefs[0] = Def_GetReflection(matId, 0, isCustom);
    info->reflectionDefs[1] = Def_GetReflection(matId, 1, isCustom);

    // Generator (particles).
    info->ptcgenDefs[0] = Def_GetGenerator(matId, 0, isCustom);
    info->ptcgenDefs[1] = Def_GetGenerator(matId, 1, isCustom);

    // Detail texture.
    info->detailtextureDefs[0] = Def_GetDetailTex(matId, 0, isCustom);
    info->detailtextureDefs[1] = Def_GetDetailTex(matId, 1, isCustom);
}

static boolean newMaterialBind(const Uri* uri, material_t* material)
{
    PathDirectory* matDirectory = getDirectoryForNamespaceId(Materials_ParseNamespace(Str_Text(Uri_Scheme(uri))));
    PathDirectoryNode* node;
    materialbind_t* mb;

    node = PathDirectory_Insert(matDirectory, Str_Text(Uri_Path(uri)), MATERIALS_PATH_DELIMITER);

    // Is this a new binding?
    mb = (materialbind_t*) PathDirectoryNode_UserData(node);
    if(!mb)
    {
        // Acquire a new unique identifier for this binding.
        const materialid_t bindId = ++bindingCount;

        mb = (materialbind_t*) malloc(sizeof *mb);
        if(!mb)
        {
            Con_Error("Materials::newMaterialBind: Failed on allocation of %lu bytes for new MaterialBind.",
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
            Material_SetPrimaryBind(material, bindId);
        }

        // Add the new binding to the bindings index/map.
        if(bindingCount > bindingIdMapSize)
        {
            // Allocate more memory.
            bindingIdMapSize += MATERIALS_BINDINGMAP_BLOCK_ALLOC;
            bindingIdMap = (materialbind_t**) realloc(bindingIdMap, sizeof *bindingIdMap * bindingIdMapSize);
            if(!bindingIdMap)
                Con_Error("Materials::newMaterialBind: Failed on (re)allocation of %lu bytes enlarging MaterialBind map.", (unsigned long) sizeof *bindingIdMap * bindingIdMapSize);
        }
        bindingIdMap[bindingCount-1] = mb; /* 1-based index */
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
    materialCount++;
    return mat;
}

/**
 * Link the material into the global list of materials.
 * @pre material is NOT already present in the global list.
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

void Materials_Init(void)
{
    int i;
    if(initedOk) return; // Already been here.

    VERBOSE( Con_Message("Initializing Materials collection...\n") )

    variantSpecs = NULL;
    variantCacheQueue = NULL;

    materialsBlockSet = BlockSet_New(sizeof(material_t), MATERIALS_BLOCK_ALLOC);
    materials = NULL;
    materialCount = 0;

    bindingCount = 0;

    bindingIdMap = NULL;
    bindingIdMapSize = 0;

    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        namespaces[i] = PathDirectory_New();
    }

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
    while(materials)
    {
        materiallist_node_t* next = materials->next;
        Material_Destroy(materials->mat);
        free(materials);
        materials = next;
    }
    BlockSet_Delete(materialsBlockSet);
    materialsBlockSet = NULL;
    materialCount = 0;
}

static int clearBinding(PathDirectoryNode* node, void* paramaters)
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
    int i;
    assert(initedOk);

    for(i = 0; i < MATERIALNAMESPACE_COUNT; ++i)
    {
        PathDirectory_Iterate(namespaces[i], PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBinding);
        PathDirectory_Delete(namespaces[i]), namespaces[i] = NULL;
    }

    // Clear the binding index/map.
    if(bindingIdMap)
    {
        free(bindingIdMap), bindingIdMap = NULL;
        bindingIdMapSize = 0;
    }
    bindingCount = 0;
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

materialnamespaceid_t Materials_ParseNamespace(const char* str)
{
    if(!str || 0 == strlen(str)) return MN_ANY;

    if(!stricmp(str, MN_TEXTURES_NAME)) return MN_TEXTURES;
    if(!stricmp(str, MN_FLATS_NAME))    return MN_FLATS;
    if(!stricmp(str, MN_SPRITES_NAME))  return MN_SPRITES;
    if(!stricmp(str, MN_SYSTEM_NAME))   return MN_SYSTEM;

    return MN_INVALID; // Unknown.
}

const ddstring_t* Materials_NamespaceName(materialnamespaceid_t id)
{
    static const ddstring_t namespaces[1+MATERIALNAMESPACE_COUNT] = {
        /* No namespace name */ { "" },
        /* MN_SYSTEM */         { MN_SYSTEM_NAME },
        /* MN_FLATS */          { MN_FLATS_NAME },
        /* MN_TEXTURES */       { MN_TEXTURES_NAME },
        /* MN_SPRITES */        { MN_SPRITES_NAME }
    };
    if(VALID_MATERIALNAMESPACEID(id))
        return namespaces + 1 + (id - MATERIALNAMESPACE_FIRST);
    return namespaces + 0;
}

materialnamespaceid_t Materials_Namespace(materialid_t id)
{
    materialbind_t* bind = getMaterialBindForId(id);
    if(!bind)
    {
#if _DEBUG
        Con_Message("Warning:Materials::Namespace: Attempted with unbound materialId #%u, returning 'any' namespace.\n", id);
#endif
        return MN_ANY;
    }
    return namespaceIdForDirectoryNode(MaterialBind_DirectoryNode(bind));
}

static int clearBindingDefinitionLinks(const PathDirectoryNode* node, void* paramaters)
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

    errorIfNotInited("Materials::ClearDefinitionLinks");

    for(node = materials; node; node = node->next)
    {
        material_t* mat = node->mat;
        Material_SetDefinition(mat, NULL);
    }

    for(namespaceId = MATERIALNAMESPACE_FIRST; namespaceId <= MATERIALNAMESPACE_LAST; ++namespaceId)
    {
        PathDirectory* matDirectory = getDirectoryForNamespaceId(namespaceId);
        PathDirectory_Iterate_Const(matDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBindingDefinitionLinks);
    }
}

void Materials_Rebuild(material_t* mat, ded_material_t* def)
{
    uint i;
    if(!initedOk || !mat || !def) return;

    /// \todo We should be able to rebuild the variants.
    Material_DestroyVariants(mat);
    Material_SetDefinition(mat, def);

    // Update bindings.
    for(i = 0; i < bindingCount; ++i)
    {
        materialbind_t* mb = bindingIdMap[i];
        if(!mb || MaterialBind_Material(mb) != mat) continue;

        updateMaterialBindInfo(mb, false /*do not create, only update if present*/);
    }
}

void Materials_PurgeCacheQueue(void)
{
    errorIfNotInited("Materials::PurgeCacheQueue");
    while(variantCacheQueue)
    {
        variantcachequeue_node_t* next = variantCacheQueue->next;
        free(variantCacheQueue);
        variantCacheQueue = next;
    }
}

void Materials_ProcessCacheQueue(void)
{
    errorIfNotInited("Materials::ProcessCacheQueue");
    while(variantCacheQueue)
    {
        variantcachequeue_node_t* node = variantCacheQueue, *next = node->next;
        Materials_Prepare(node->mat, node->spec, node->smooth);
        free(node);
        variantCacheQueue = next;
    }
}

material_t* Materials_ToMaterial(materialid_t id)
{
    materialbind_t* mb;
    if(!initedOk) return NULL;
    mb = getMaterialBindForId(id);
    if(!mb) return NULL;
    return MaterialBind_Material(mb);
}

materialid_t Materials_Id(material_t* mat)
{
    materialbind_t* bind;
    if(!initedOk || !mat) return NOMATERIALID;
    bind = getMaterialBindForId(Material_PrimaryBind(mat));
    if(!bind) return NOMATERIALID;
    return MaterialBind_Id(bind);
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

    namespaceId = Materials_ParseNamespace(Str_Text(Uri_Scheme(uri)));
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
 * @return  Found Material else @c NULL
 */
static materialbind_t* findMaterialBindForPath(PathDirectory* matDirectory, const char* path)
{
    PathDirectoryNode* node = PathDirectory_Find(matDirectory,
        PCF_NO_BRANCH|PCF_MATCH_FULL, path, MATERIALS_PATH_DELIMITER);
    if(node)
    {
        return (materialbind_t*) PathDirectoryNode_UserData(node);
    }
    return NULL; // Not found.
}

/// @pre @a uri has already been validated and is well-formed.
static materialbind_t* findMaterialBindForUri(const Uri* uri)
{
    materialnamespaceid_t namespaceId = Materials_ParseNamespace(Str_Text(Uri_Scheme(uri)));
    const char* path = Str_Text(Uri_Path(uri));
    materialbind_t* bind = NULL;
    if(namespaceId != MN_ANY)
    {
        // Caller wants a material in a specific namespace.
        bind = findMaterialBindForPath(getDirectoryForNamespaceId(namespaceId), path);
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
            bind = findMaterialBindForPath(getDirectoryForNamespaceId(order[n]), path);
        } while(!bind && order[++n] != MN_ANY);
    }
    return bind;
}

materialid_t Materials_ResolveUri2(const Uri* uri, boolean quiet)
{
    materialbind_t* bind;
    if(!initedOk || !uri) return NOMATERIALID;
    if(!validateMaterialUri2(uri, VMUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning: Materials::ResolveUri: \"%s\" failed to validate, returning NOMATERIALID.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
#endif
        return NOMATERIALID;
    }

    // Perform the search.
    bind = findMaterialBindForUri(uri);
    if(bind) return MaterialBind_Id(bind);

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning: Materials::ResolveUri: \"%s\" not found, returning NOMATERIALID.\n", Str_Text(path));
        Str_Delete(path);
    }
    return NOMATERIALID;
}

/// @note Part of the Doomsday public API.
materialid_t Materials_ResolveUri(const Uri* uri)
{
    return Materials_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

materialid_t Materials_ResolveUriCString2(const char* path, boolean quiet)
{
    if(path && path[0])
    {
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        materialid_t matId = Materials_ResolveUri2(uri, quiet);
        Uri_Delete(uri);
        return matId;
    }
    return NOMATERIALID;
}

/// @note Part of the Doomsday public API.
materialid_t Materials_ResolveUriCString(const char* path)
{
    return Materials_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

ddstring_t* Materials_ComposePath(materialid_t id)
{
    materialbind_t* bind = getMaterialBindForId(id);
    if(!bind)
    {
#if _DEBUG
        Con_Message("Warning:Materials::ComposePath: Attempted with unbound materialId #%u, returning null-object.\n", id);
#endif
        return Str_New();
    }
    return composePathForDirectoryNode(MaterialBind_DirectoryNode(bind), MATERIALS_PATH_DELIMITER);
}

/// @note Part of the Doomsday public API.
Uri* Materials_ComposeUri(materialid_t id)
{
    materialbind_t* bind = getMaterialBindForId(id);
    if(!bind)
    {
#if _DEBUG
        Con_Message("Warning:Materials::ComposeUri: Attempted with unbound materialId #%u, returning null-object.\n", id);
#endif
        return Uri_New();
    }
    return composeUriForDirectoryNode(MaterialBind_DirectoryNode(bind));
}

material_t* Materials_CreateFromDef(ded_material_t* def)
{
    const Uri* uri = def->uri;
    materialbind_t* bind;
    textureid_t texId;
    material_t* mat;
    assert(def);

    if(!initedOk) return NULL;

    // We require a properly formed uri.
    if(!validateMaterialUri2(uri, 0, (verbose >= 1)))
    {
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning: Failed creating Material \"%s\" from definition %p, ignoring.\n", Str_Text(uriStr), (void*)def);
        Str_Delete(uriStr);
        return NULL;
    }

    // Have we already created a material for this?
    bind = findMaterialBindForUri(uri);
    if(bind && MaterialBind_Material(bind))
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:Materials::CreateFromDef: A Material with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return MaterialBind_Material(bind);
    }

    // Ensure the primary layer has a valid texture reference.
    texId = NOTEXTUREID;
    if(def->layers[0].stageCount.num > 0)
    {
        const ded_material_layer_t* l = &def->layers[0];
        if(l->stages[0].texture) // Not unused.
        {
            texId = Textures_ResolveUri2(l->stages[0].texture, true/*quiet please*/);
            if(texId == NOTEXTUREID)
            {
                ddstring_t* materialPath = Uri_ToString(def->uri);
                ddstring_t* texturePath = Uri_ToString(l->stages[0].texture);
                Con_Message("Warning: Unknown texture \"%s\" in Material \"%s\" (layer %i stage %i).\n", Str_Text(texturePath), Str_Text(materialPath), 0, 0);
                Str_Delete(materialPath);
                Str_Delete(texturePath);
            }
        }
    }
    if(texId == NOTEXTUREID) return NULL;

    // A new Material.
    mat = linkMaterialToGlobalList(allocMaterial());
    mat->_flags = def->flags;
    mat->_isCustom = Texture_IsCustom(Textures_ToTexture(texId));
    mat->_def = def;
    Size2_SetWidthHeight(mat->_size, MAX_OF(0, def->width), MAX_OF(0, def->height));
    mat->_envClass = S_MaterialEnvClassForUri(uri);

    if(!bind)
    {
        newMaterialBind(uri, mat);
    }
    else
    {
        MaterialBind_SetMaterial(bind, mat);
    }

    return mat;
}

static void pushVariantCacheQueue(material_t* mat, const materialvariantspecification_t* spec, boolean smooth)
{
    variantcachequeue_node_t* node;
    assert(initedOk && mat && spec);

    node = (variantcachequeue_node_t*) malloc(sizeof *node);
    if(!node)
        Con_Error("Materials::pushVariantCacheQueue: Failed on allocation of %lu bytes for new VariantCacheQueueNode.", (unsigned long) sizeof *node);

    node->mat = mat;
    node->spec = spec;
    node->smooth = smooth;
    node->next = variantCacheQueue;
    variantCacheQueue = node;
}

void Materials_Precache2(material_t* mat, const materialvariantspecification_t* spec,
    boolean smooth, boolean cacheGroup)
{
    errorIfNotInited("Materials::Precache");

    if(!mat || ! spec)
    {
        DEBUG_Message(("Materials_Precache: Invalid arguments mat:%p, spec:%p, ignoring.\n", mat, spec));
        return;
    }

    // Don't precache when playing demo.
    if(isDedicated || playback) return;

    // Already in the queue?
    { variantcachequeue_node_t* node;
    for(node = variantCacheQueue; node; node = node->next)
    {
        if(mat == node->mat && spec == node->spec) return;
    }}

    pushVariantCacheQueue(mat, spec, smooth);

    if(cacheGroup && Material_IsGroupAnimated(mat))
    {
        // Material belongs in one or more animgroups; precache the group.
        int i, k;
        for(i = 0; i < numgroups; ++i)
        {
            if(!isInAnimGroup(&groups[i], mat)) continue;

            for(k = 0; k < groups[i].count; ++k)
                Materials_Precache2(groups[i].frames[k].material, spec, smooth, false);
        }
    }
}

void Materials_Precache(material_t* mat, const materialvariantspecification_t* spec, boolean smooth)
{
    Materials_Precache2(mat, spec, smooth, true);
}

void Materials_Ticker(timespan_t time)
{
    materiallist_node_t* node;

    // The animation will only progress when the game is not paused.
    if(clientPaused || novideo) return;

    node = materials;
    while(node)
    {
        Material_Ticker(node->mat, time);
        node = node->next;
    }

    if(DD_IsSharpTick())
    {
        animateAnimGroups();
    }
}

static Texture* findDetailTextureForDef(const ded_detailtexture_t* def)
{
    assert(def);
    return R_FindDetailTextureForResourcePath(def->detailTex);
}

static Texture* findShinyTextureForDef(const ded_reflection_t* def)
{
    assert(def);
    return R_FindReflectionTextureForResourcePath(def->shinyMap);
}

static Texture* findShinyMaskTextureForDef(const ded_reflection_t* def)
{
    assert(def);
    return R_FindMaskTextureForResourcePath(def->maskMap);
}

static void updateMaterialTextureLinks(materialbind_t* mb)
{
    material_t* mat = MaterialBind_Material(mb);
    const float black[3] = { 0, 0, 0 };
    const ded_detailtexture_t* dtlDef;
    const ded_reflection_t* refDef;

    // We may need to need to construct and attach the info.
    updateMaterialBindInfo(mb, true /* create if not present */);

    if(!mat) return;

    dtlDef = MaterialBind_DetailTextureDef(mb);
    Material_SetDetailTexture(mat,  (dtlDef? findDetailTextureForDef(dtlDef) : NULL));
    Material_SetDetailStrength(mat, (dtlDef? dtlDef->strength : 0));
    Material_SetDetailScale(mat,    (dtlDef? dtlDef->scale : 0));

    refDef = MaterialBind_ReflectionDef(mb);
    Material_SetShinyTexture(mat,     (refDef? findShinyTextureForDef(refDef) : NULL));
    Material_SetShinyMaskTexture(mat, (refDef? findShinyMaskTextureForDef(refDef) : NULL));
    Material_SetShinyBlendmode(mat,   (refDef? refDef->blendMode : BM_ADD));
    Material_SetShinyMinColor(mat,    (refDef? refDef->minColor : black));
    Material_SetShinyStrength(mat,    (refDef? refDef->shininess : 0));
}

static void setTexUnit(materialsnapshot_t* ms, byte unit, TextureVariant* texture,
    blendmode_t blendMode, float sScale, float tScale, float sOffset,
    float tOffset, float opacity)
{
    rtexmapunit_t* tu;
    assert(ms && unit < NUM_MATERIAL_TEXTURE_UNITS);

    ms->textures[unit] = texture;
    tu = &ms->units[unit];
    tu->texture.variant = texture;
    tu->texture.flags = TUF_TEXTURE_IS_MANAGED;
    tu->blendMode = blendMode;
    V2f_Set(tu->scale, sScale, tScale);
    V2f_Set(tu->offset, sOffset, tOffset);
    tu->opacity = MINMAX_OF(0, opacity, 1);
}

void Materials_InitSnapshot(materialsnapshot_t* ms)
{
    int i;
    assert(ms);

    for(i = 0; i < NUM_MATERIAL_TEXTURE_UNITS; ++i)
    {
        Rtu_Init(&ms->units[i]);
        ms->textures[i] = NULL;
    }

    ms->material = NULL;
    ms->size.width = ms->size.height = 0;
    ms->glowing = 0;
    ms->isOpaque = true;
    V3f_Set(ms->shinyMinColor, 0, 0, 0);
}

/// @return  Same as @a snapshot for caller convenience.
const materialsnapshot_t* updateMaterialSnapshot(materialvariant_t* variant,
    materialsnapshot_t* snapshot)
{
    static struct materialtextureunit_s {
        TextureVariant* tex;
    } texUnits[NUM_MATERIAL_TEXTURE_UNITS];
    material_t* mat = MaterialVariant_GeneralCase(variant);
    const materialvariantspecification_t* spec = MaterialVariant_Spec(variant);
    int i, layerCount;
    Texture* tex;
    assert(snapshot);

    memset(texUnits, 0, sizeof texUnits);

    // Ensure all resources needed to visualize this Material's layers have been prepared.
    layerCount = Material_LayerCount(mat);
    for(i = 0; i < layerCount; ++i)
    {
        const materialvariant_layer_t* ml = MaterialVariant_Layer(variant, i);
        preparetextureresult_t result;

        if(!ml->texture) continue;

        // Pick the instance matching the specified context.
        texUnits[i].tex = GL_PrepareTextureVariant2(ml->texture, spec->primarySpec, &result);

        if(0 == i && (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
        {
            materialbind_t* bind = getMaterialBindForId(Material_PrimaryBind(mat));

            // Primary texture was (re)prepared.
            Material_SetPrepared(mat, result == PTR_UPLOADED_ORIGINAL? 1 : 2);

            if(bind)
            {
                updateMaterialTextureLinks(bind);
            }

            // Are we inheriting the logical dimensions from the texture?
            if(0 == Material_Width(mat) && 0 == Material_Height(mat))
            {
                Size2Raw texSize;
                Material_SetSize(mat, Size2_Raw(Texture_Size(ml->texture), &texSize));
            }
        }
    }

    // Do we need to prepare a DetailTexture?
    tex = Material_DetailTexture(mat);
    if(tex)
    {
        const float contrast = Material_DetailStrength(mat) * detailFactor;
        texturevariantspecification_t* texSpec = GL_DetailTextureVariantSpecificationForContext(contrast);
        texUnits[MTU_DETAIL].tex = GL_PrepareTextureVariant(tex, texSpec);
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    tex = Material_ShinyTexture(mat);
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

    MaterialVariant_SetSnapshotPrepareFrame(variant, frameCount);

    Materials_InitSnapshot(snapshot);
    snapshot->material = variant;
    memcpy(&snapshot->size, Material_Size(mat), sizeof snapshot->size);

    if(0 == snapshot->size.width && 0 == snapshot->size.height) return snapshot;

    snapshot->glowing = MaterialVariant_Layer(variant, 0)->glow * glowFactor;
    snapshot->isOpaque = NULL != texUnits[MTU_PRIMARY].tex &&
        !TextureVariant_IsMasked(texUnits[MTU_PRIMARY].tex);

    // Setup the primary texture unit.
    if(texUnits[MTU_PRIMARY].tex)
    {
        TextureVariant* tex = texUnits[MTU_PRIMARY].tex;
        const float sScale = 1.f / snapshot->size.width;
        const float tScale = 1.f / snapshot->size.height;

        setTexUnit(snapshot, MTU_PRIMARY, tex, BM_NORMAL,
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
            TextureVariant* tex = texUnits[MTU_DETAIL].tex;
            const float width  = Texture_Width(TextureVariant_GeneralCase(tex));
            const float height = Texture_Height(TextureVariant_GeneralCase(tex));
            float scale = Material_DetailScale(mat);

            // Apply the global scaling factor.
            if(detailScale > .0001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, tex, BM_NORMAL,
                       1.f / width * scale, 1.f / height * scale, 0, 0, 1);
        }

        // Setup the shiny texture units?
        if(texUnits[MTU_REFLECTION].tex)
        {
            TextureVariant* tex = texUnits[MTU_REFLECTION].tex;
            const blendmode_t blendmode = Material_ShinyBlendmode(mat);
            const float strength = Material_ShinyStrength(mat);

            setTexUnit(snapshot, MTU_REFLECTION, tex, blendmode, 1, 1, 0, 0, strength);
        }

        if(texUnits[MTU_REFLECTION_MASK].tex)
        {
            TextureVariant* tex = texUnits[MTU_REFLECTION_MASK].tex;

            setTexUnit(snapshot, MTU_REFLECTION_MASK, tex, BM_NORMAL,
                1.f / (snapshot->size.width * Texture_Width(TextureVariant_GeneralCase(tex))),
                1.f / (snapshot->size.height * Texture_Height(TextureVariant_GeneralCase(tex))),
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

    return snapshot;
}

const materialsnapshot_t* Materials_PrepareVariant2(materialvariant_t* variant, boolean updateSnapshot)
{
    // Acquire the snapshot we are interested in.
    materialsnapshot_t* snapshot = MaterialVariant_Snapshot(variant);
    if(!snapshot)
    {
        // Time to allocate the snapshot.
        snapshot = (materialsnapshot_t*)malloc(sizeof *snapshot);
        if(!snapshot)
            Con_Error("Materials::Prepare: Failed on allocation of %lu bytes for new MaterialSnapshot.", (unsigned long) sizeof *snapshot);
        snapshot = MaterialVariant_AttachSnapshot(variant, snapshot);
        Materials_InitSnapshot(snapshot);
        snapshot->material = variant;

        // Update the snapshot right away.
        updateSnapshot = true;
    }
    else if(MaterialVariant_SnapshotPrepareFrame(variant) != frameCount)
    {
        // Time to update the snapshot.
        updateSnapshot = true;
    }

    // If we aren't updating a snapshot; get out of here.
    if(!updateSnapshot) return snapshot;

    // We have work to do...
    return updateMaterialSnapshot(variant, snapshot);
}

const materialsnapshot_t* Materials_PrepareVariant(materialvariant_t* variant)
{
    return Materials_PrepareVariant2(variant, false/*do not force a snapshot update*/);
}

const materialsnapshot_t* Materials_Prepare2(material_t* mat, const materialvariantspecification_t* spec,
    boolean smooth, boolean updateSnapshot)
{
    return Materials_PrepareVariant2(Materials_ChooseVariant(mat, spec, smooth, true), updateSnapshot);
}

const materialsnapshot_t* Materials_Prepare(material_t* mat, const materialvariantspecification_t* spec,
    boolean smooth)
{
    return Materials_Prepare2(mat, spec, smooth, false/*do not force a snapshot update*/);
}

const ded_decor_t* Materials_DecorationDef(material_t* mat)
{
    if(!mat) return NULL;
    if(!Material_Prepared(mat))
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        Materials_Prepare(mat, spec, false);
    }
    return MaterialBind_DecorationDef(getMaterialBindForId(Material_PrimaryBind(mat)));
}

const ded_ptcgen_t* Materials_PtcGenDef(material_t* mat)
{
    if(!mat || isDedicated) return NULL;
    if(!Material_Prepared(mat))
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        Materials_Prepare(mat, spec, false);
    }
    return MaterialBind_PtcGenDef(getMaterialBindForId(Material_PrimaryBind(mat)));
}

uint Materials_Size(void)
{
    return materialCount;
}

uint Materials_Count(materialnamespaceid_t namespaceId)
{
    PathDirectory* matDirectory;
    if(!VALID_MATERIALNAMESPACEID(namespaceId) || !Materials_Size()) return 0;
    matDirectory = getDirectoryForNamespaceId(namespaceId);
    if(!matDirectory) return 0;
    return PathDirectory_Size(matDirectory);
}

const struct materialvariantspecification_s* Materials_VariantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    errorIfNotInited("Materials::VariantSpecificationForContext");
    return getVariantSpecificationForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
                                             minFilter, magFilter, anisoFilter,
                                             mipmapped, gammaCorrection, noStretch, toAlpha);
}

materialvariant_t* Materials_ChooseVariant(material_t* mat,
    const materialvariantspecification_t* spec, boolean smoothed, boolean canCreate)
{
    materialvariant_t* variant;

    errorIfNotInited("Materials::ChooseVariant");

    variant = chooseVariant(mat, spec);
    if(!variant)
    {
        if(!canCreate) return NULL;
        variant = Material_AddVariant(mat, MaterialVariant_New(mat, spec));
    }

    if(smoothed)
    {
        variant = MaterialVariant_TranslationCurrent(variant);
    }

    return variant;
}

static int printVariantInfo(materialvariant_t* variant, void* paramaters)
{
    int* variantIdx = (int*)paramaters;
    materialvariant_t* next = MaterialVariant_TranslationNext(variant);
    int i, layers = Material_LayerCount(MaterialVariant_GeneralCase(variant));
    assert(variantIdx);

    Con_Printf("Variant #%i: Spec:%p\n", *variantIdx, (void*)MaterialVariant_Spec(variant));

    // Print translation info:
    if(Material_HasTranslation(MaterialVariant_GeneralCase(variant)))
    {
        materialvariant_t* cur = MaterialVariant_TranslationCurrent(variant);
        float inter = MaterialVariant_TranslationPoint(variant);
        Uri* curUri = Materials_ComposeUri(Materials_Id(MaterialVariant_GeneralCase(cur)));
        ddstring_t* curPath = Uri_ToString(curUri);
        Uri* nextUri = Materials_ComposeUri(Materials_Id(MaterialVariant_GeneralCase(next)));
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
        Uri* uri = Textures_ComposeUri(Textures_Id(l->texture));
        ddstring_t* path = Uri_ToString(uri);

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:(\"%s\" uid:%u)"
            "\n      Offset: %.2f x %.2f Glow:%.2f\n",
            i, l->stage, (int)l->tics, F_PrettyPath(Str_Text(path)), Textures_Id(l->texture),
            l->texOrigin[0], l->texOrigin[1], l->glow);

        Uri_Delete(uri);
        Str_Delete(path);
    }

    ++(*variantIdx);

    return 0; // Continue iteration.
}

static void printMaterialInfo(material_t* mat)
{
    Uri* uri = Materials_ComposeUri(Materials_Id(mat));
    ddstring_t* path = Uri_ToString(uri);
    int variantIdx = 0;

    Con_Printf("Material \"%s\" [%p] uid:%u origin:%s"
        "\nSize: %d x %d Layers:%i InGroup:%s Drawable:%s EnvClass:%s"
        "\nDecorated:%s Detailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
        F_PrettyPath(Str_Text(path)), (void*) mat, Materials_Id(mat),
        !Material_IsCustom(mat)     ? "game" : (Material_Definition(mat)->autoGenerated? "addon" : "def"),
        Material_Width(mat), Material_Height(mat), Material_LayerCount(mat),
        Material_IsGroupAnimated(mat)? "yes" : "no",
        Material_IsDrawable(mat)     ? "yes" : "no",
        Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(mat)),
        Materials_HasDecorations(mat) ? "yes" : "no",
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
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials_Size()));
    Uri* uri = Materials_ComposeUri(Materials_Id(mat));
    ddstring_t* path = (printNamespace? Uri_ToString(uri) : Str_PercentDecode(Str_Set(Str_New(), Str_Text(Uri_Path(uri)))));

    Con_Printf("%-*s %*u %s\n", printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
        numUidDigits, Materials_Id(mat),
        !Material_IsCustom(mat) ? "game" : (Material_Definition(mat)->autoGenerated? "addon" : "def"));

    Uri_Delete(uri);
    Str_Delete((ddstring_t*)path);
}

/**
 * \todo A horridly inefficent algorithm. This should be implemented in PathDirectory
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the material search/listing console commands
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

static PathDirectoryNode** collectDirectoryNodes(materialnamespaceid_t namespaceId,
    const char* like, int* count, PathDirectoryNode** storage)
{
    collectdirectorynodeworker_paramaters_t p;
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

    p.delimiter = MATERIALS_PATH_DELIMITER;
    p.like = like;
    p.idx = 0;
    p.storage = storage;
    for(iterId  = fromId; iterId <= toId; ++iterId)
    {
        PathDirectory* matDirectory = getDirectoryForNamespaceId(iterId);
        PathDirectory_Iterate2(matDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
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
        Con_Error("Materials::collectDirectoryNodes: Failed on allocation of %lu bytes for new PathDirectoryNode collection.", (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectDirectoryNodes(namespaceId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(const void* nodeA, const void* nodeB)
{
    // Decode paths before determining a lexicographical delta.
    ddstring_t* a = Str_PercentDecode(composePathForDirectoryNode(*(const PathDirectoryNode**)nodeA, MATERIALS_PATH_DELIMITER));
    ddstring_t* b = Str_PercentDecode(composePathForDirectoryNode(*(const PathDirectoryNode**)nodeB, MATERIALS_PATH_DELIMITER));
    int delta = stricmp(Str_Text(a), Str_Text(b));
    Str_Delete(b);
    Str_Delete(a);
    return delta;
}

static size_t printMaterials2(materialnamespaceid_t namespaceId, const char* like,
    boolean printNamespace)
{
    int numFoundDigits, numUidDigits, idx, count = 0;
    PathDirectoryNode** foundMaterials = collectDirectoryNodes(namespaceId, like, &count, NULL);
    PathDirectoryNode** iter;

    if(!foundMaterials) return 0;

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known materials in namespace '%s'", Str_Text(Materials_NamespaceName(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known materials");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials_Size()));
    Con_Printf(" %*s: %-*s %*s origin\n", numFoundDigits, "idx",
        printNamespace? 22 : 14, printNamespace? "namespace:path" : "path", numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundMaterials, (size_t)count, sizeof *foundMaterials, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundMaterials; *iter; ++iter)
    {
        const PathDirectoryNode* node = *iter;
        material_t* mat = MaterialBind_Material((materialbind_t*)PathDirectoryNode_UserData(node));
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printMaterialOverview(mat, printNamespace);
    }

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

boolean Materials_IsMaterialInAnimGroup(material_t* mat, int groupNum)
{
    materialanim_t* group = getAnimGroup(groupNum);
    if(!group) return false;
    return isInAnimGroup(group, mat);
}

boolean Materials_HasDecorations(material_t* mat)
{
    if(novideo) return false;

    assert(mat);
    /// @todo We should not need to prepare to determine this.
    /// Nor should we need to process the group each time. Cache this decision.
    if(Materials_DecorationDef(mat)) return true;
    if(Material_IsGroupAnimated(mat))
    {
        int g, i, numGroups = Materials_AnimGroupCount();
        for(g = 0; g < numGroups; ++g)
        {
            materialanim_t* group = &groups[g];

            // Precache groups don't apply.
            if(Materials_IsPrecacheAnimGroup(g)) continue;
            // Is this material in this group?
            if(!Materials_IsMaterialInAnimGroup(mat, g)) continue;

            // If any material in this group has decorations then this
            // material is considered to be decorated also.
            for(i = 0; i < group->count; ++i)
            {
                if(Materials_DecorationDef(group->frames[i].material)) return true;
            }
        }
    }
    return false;
}

int Materials_AnimGroupCount(void)
{
    return numgroups;
}

int Materials_CreateAnimGroup(int flags)
{
    materialanim_t* group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups = Z_Realloc(groups, sizeof(materialanim_t) * (numgroups + 1), PU_APPSTATIC);

    // Init the new group.
    group = &groups[numgroups];
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

void Materials_ClearAnimGroups(void)
{
    int i;
    if(numgroups <= 0) return;

    for(i = 0; i < numgroups; ++i)
    {
        materialanim_t* group = &groups[i];
        Z_Free(group->frames);
    }

    Z_Free(groups);
    groups = NULL;
    numgroups = 0;
}

void Materials_AddAnimGroupFrame(int groupNum, struct material_s* mat, int tics, int randomTics)
{
    materialanim_t* group = getAnimGroup(groupNum);
    materialanim_frame_t* frame;

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
    group->frames = Z_Realloc(group->frames, sizeof(materialanim_frame_t) * ++group->count, PU_APPSTATIC);

    frame = &group->frames[group->count - 1];

    frame->material = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean Materials_IsPrecacheAnimGroup(int groupNum)
{
    materialanim_t* group = getAnimGroup(groupNum);
    if(!group) return false;
    return ((group->flags & AGF_PRECACHE) != 0);
}

static int clearVariantTranslationWorker(materialvariant_t* variant, void* paramaters)
{
    MaterialVariant_SetTranslation(variant, variant, variant);
    return 0; // Continue iteration.
}

static void Materials_ClearTranslation(material_t* mat)
{
    assert(initedOk);
    Material_IterateVariants(mat, clearVariantTranslationWorker, NULL);
}

typedef struct {
    material_t* current, *next;
} setmaterialtranslationworker_paramaters_t;

static int setVariantTranslationWorker(materialvariant_t* variant, void* paramaters)
{
    setmaterialtranslationworker_paramaters_t* p = (setmaterialtranslationworker_paramaters_t*) paramaters;
    const materialvariantspecification_t* spec = MaterialVariant_Spec(variant);
    materialvariant_t* current, *next;
    assert(p);

    current = Materials_ChooseVariant(p->current, spec, false, true/*create if necessary*/);
    next    = Materials_ChooseVariant(p->next,    spec, false, true/*create if necessary*/);
    MaterialVariant_SetTranslation(variant, current, next);
    return 0; // Continue iteration.
}

static int setVariantTranslationPointWorker(materialvariant_t* variant, void* paramaters)
{
    float* interPtr = (float*)paramaters;
    assert(interPtr);

    MaterialVariant_SetTranslationPoint(variant, *interPtr);
    return 0; // Continue iteration.
}

void Materials_AnimateAnimGroup(materialanim_t* group)
{
    int i;

    // The Precache groups are not intended for animation.
    if((group->flags & AGF_PRECACHE) || !group->count) return;

    if(--group->timer <= 0)
    {
        // Advance to next frame.
        int timer;

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
            material_t* real = group->frames[i].material;
            setmaterialtranslationworker_paramaters_t params;

            params.current = group->frames[(group->index + i    ) % group->count].material;
            params.next    = group->frames[(group->index + i + 1) % group->count].material;
            Material_IterateVariants(real, setVariantTranslationWorker, &params);

            // Surfaces using this material may need to be updated.
            R_UpdateMapSurfacesOnMaterialChange(real);

            // Just animate the first in the sequence?
            if(group->flags & AGF_FIRST_ONLY) break;
        }
        return;
    }

    // Update the interpolation point of animated group members.
    for(i = 0; i < group->count; ++i)
    {
        material_t* mat = group->frames[i].material;
        float interp;

        /*{ ded_material_t* def = Material_Definition(mat);
        if(def && def->layers[0].stageCount.num > 1)
        {
            if(Textures_ResolveUri(def->layers[0].stages[0].texture))
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
        if(group->flags & AGF_FIRST_ONLY) break;
    }
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

void Materials_ResetAnimGroups(void)
{
    materiallist_node_t* node;
    materialanim_t* group;
    int i;

    for(node = materials; node; node = node->next)
    {
        Material_IterateVariants(node->mat, resetVariantGroupAnimWorker, NULL);
    }

    group = groups;
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
    }

    // This'll get every group started on the first step.
    animateAnimGroups();
}

materialid_t MaterialBind_Id(const materialbind_t* mb)
{
    assert(mb);
    return mb->_id;
}

material_t* MaterialBind_Material(const materialbind_t* mb)
{
    assert(mb);
    return mb->_material;
}

material_t* MaterialBind_SetMaterial(materialbind_t* mb, material_t* material)
{
    assert(mb);
    if(mb->_material != material)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        materialbindinfo_t* info = MaterialBind_DetachInfo(mb);
        if(info) free(mb->_info);

        // Associate with the new Material.
        mb->_material = material;
    }
    return mb->_material;
}

PathDirectoryNode* MaterialBind_DirectoryNode(const materialbind_t* mb)
{
    assert(mb);
    return mb->_directoryNode;
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
        Uri* uri = Materials_ComposeUri(mb->_id);
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
    materialbindinfo_t* info;
    assert(mb);
    info = mb->_info;
    mb->_info = NULL;
    return info;
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

    if(!Materials_Size())
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

    if(uri) Uri_Delete(uri);
    return true;
}

D_CMD(InspectMaterial)
{
    ddstring_t path;
    material_t* mat;
    Uri* search;

    // Path is assumed to be in a human-friendly, non-encoded representation.
    Str_Init(&path); Str_PercentEncode(Str_Set(&path, argv[1]));
    search = Uri_NewWithPath2(Str_Text(&path), RC_NULL);
    Str_Free(&path);

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

    mat = Materials_ToMaterial(Materials_ResolveUri(search));
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
        PathDirectory* matDirectory = getDirectoryForNamespaceId(namespaceId);
        uint size;

        if(!matDirectory) continue;

        size = PathDirectory_Size(matDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(Materials_NamespaceName(namespaceId)), size, size==1? "material":"materials");
        PathDirectory_PrintHashDistribution(matDirectory);
        PathDirectory_Print(matDirectory, MATERIALS_PATH_DELIMITER);
    }
    return true;
}
#endif
