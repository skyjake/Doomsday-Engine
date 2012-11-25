/**
 * @file materials.cpp
 * Materials collection, schemes, bindings and other management. @ingroup resource
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

#include "resource/texture.h"
#include "resource/texturevariant.h"
#include "resource/materialvariant.h"

#include <de/Error>
#include <de/Log>
#include <de/PathTree>
#include <de/memory.h>
#include <de/memoryblockset.h>
#include <de/memoryzone.h>

/// Number of materials to block-allocate.
#define MATERIALS_BLOCK_ALLOC (32)

/// Number of elements to block-allocate in the material index to materialbind map.
#define MATERIALS_BINDINGMAP_BLOCK_ALLOC (32)

typedef de::UserDataPathTree MaterialRepository;

/**
 * Contains extended info about a material binding (see MaterialBind).
 * @note POD object.
 */
struct MaterialBindInfo
{
    ded_decor_t* decorationDefs[2];
    ded_detailtexture_t* detailtextureDefs[2];
    ded_ptcgen_t* ptcgenDefs[2];
    ded_reflection_t* reflectionDefs[2];
};

class MaterialBind
{
public:
    MaterialBind(MaterialRepository::Node& _direcNode, materialid_t id)
        : direcNode(&_direcNode), asocMaterial(0), guid(id), extInfo(0)
    {}

    ~MaterialBind()
    {
        MaterialBindInfo* detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);
    }

    /// @return  Unique identifier associated with this.
    materialid_t id() const { return guid; }

    /// @return  MaterialRepository node associated with this.
    MaterialRepository::Node& directoryNode() const { return *direcNode; }

    /// @return  Material associated with this else @c NULL.
    material_t* material() const { return asocMaterial; }

    /// @return  Extended info owned by this else @c NULL.
    MaterialBindInfo* info() const { return extInfo; }

    /**
     * Attach extended info data to this. If existing info is present it is replaced.
     * MaterialBind is given ownership of the info.
     * @param info  Extended info data to attach.
     */
    MaterialBind& attachInfo(MaterialBindInfo& info);

    /**
     * Detach any extended info owned by this and relinquish ownership to the caller.
     * @return  Extended info or else @c NULL if not present.
     */
    MaterialBindInfo* detachInfo();

    /**
     * Change the Material associated with this binding.
     *
     * @note Only the relationship from MaterialBind to @a material changes!
     *
     * @post If @a material differs from that currently associated with this, any
     *       MaterialBindInfo presently owned by this will destroyed (its invalid).
     *
     * @param  material  New Material to associate with this.
     * @return  This instance.
     */
    MaterialBind& setMaterial(material_t* material);

    /// @return  Detail texture definition associated with this else @c NULL
    ded_detailtexture_t* detailTextureDef() const;

    /// @return  Decoration definition associated with this else @c NULL
    ded_decor_t* decorationDef() const;

    /// @return  Particle generator definition associated with this else @c NULL
    ded_ptcgen_t* ptcGenDef() const;

    /// @return  Reflection definition associated with this else @c NULL
    ded_reflection_t* reflectionDef() const;

private:
    /// This binding's node in the directory.
    MaterialRepository::Node* direcNode;

    /// Material associated with this.
    material_t* asocMaterial;

    /// Unique identifier.
    materialid_t guid;

    /// Extended info about this binding. Will be attached upon successfull preparation
    /// of the first derived variant of the associated Material.
    MaterialBindInfo* extInfo;
};

typedef struct materialvariantspecificationlistnode_s {
    struct materialvariantspecificationlistnode_s* next;
    materialvariantspecification_t* spec;
} MaterialVariantSpecificationListNode;

typedef MaterialVariantSpecificationListNode VariantSpecificationList;

typedef struct materiallistnode_s {
    struct materiallistnode_s* next;
    material_t* mat;
} MaterialListNode;
typedef MaterialListNode MaterialList;

typedef struct variantcachequeuenode_s {
    struct variantcachequeuenode_s* next;
    material_t* mat;
    materialvariantspecification_t const* spec;
    bool smooth;
} VariantCacheQueueNode;

typedef VariantCacheQueueNode VariantCacheQueue;

static void updateMaterialBindInfo(MaterialBind& mb, bool canCreate);

typedef struct materialanim_frame_s {
    material_t* material;
    ushort tics;
    ushort random;
} MaterialAnimFrame;

typedef struct materialanim_s {
    int id;
    int flags;
    int index;
    int maxTimer;
    int timer;
    int count;
    MaterialAnimFrame* frames;
} MaterialAnim;

static int numgroups;
static MaterialAnim* groups;

D_CMD(InspectMaterial);
D_CMD(ListMaterials);
#if _DEBUG
D_CMD(PrintMaterialStats);
#endif

static void animateAnimGroups(void);

static boolean initedOk;
static VariantSpecificationList* variantSpecs;

static VariantCacheQueue* variantCacheQueue;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the materials themselves and in of the system itself:
 *
 * 1) Pointers to Material are eternal, they are always valid and continue
 *    to reference the same logical material data even after engine reset.
 * 2) Public material identifiers (materialid_t) are similarly eternal.
 *    Note that they are used to index the material name bindings map.
 * 3) Dynamic creation/update of materials.
 * 4) Material name bindings are semi-independant from the materials. There
 *    may be multiple name bindings for a given material (aliases).
 *    The only requirement is that their symbolic names must be unique among
 *    those in the same scheme.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (hashing is used).
 */
static blockset_t* materialsBlockSet;
static MaterialList* materials;
static uint materialCount;

static uint bindingCount;

/// LUT which translates materialid_t to MaterialBind*. Index with materialid_t-1
static uint bindingIdMapSize;
static MaterialBind** bindingIdMap;

// Material schemes contain mappings between names and MaterialBind instances.
static MaterialRepository* schemes[MATERIALSCHEME_COUNT];

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
    throw de::Error("Materials", de::String("Collection is not presently initialized (%1).")
                                    .arg(callerName));
}

static inline MaterialRepository& schemeById(materialschemeid_t id)
{
    DENG2_ASSERT(VALID_MATERIALSCHEMEID(id));
    DENG2_ASSERT(schemes[id-MATERIALSCHEME_FIRST]);
    return *schemes[id-MATERIALSCHEME_FIRST];
}

static materialschemeid_t schemeIdForDirectory(de::PathTree const &directory)
{
    for(uint i = uint(MATERIALSCHEME_FIRST); i <= uint(MATERIALSCHEME_LAST); ++i)
    {
        uint idx = i - MATERIALSCHEME_FIRST;
        if(schemes[idx] == &directory) return materialschemeid_t(i);
    }

    // Should never happen.
    throw de::Error("Materials::schemeIdForDirectory",
                    de::String().sprintf("Failed to determine id for directory %p.", (void*)&directory));
}

/// @return  Newly composed Uri for @a node. Must be deleted when no longer needed.
static de::Uri composeUriForDirectoryNode(MaterialRepository::Node const& node)
{
    Str const* schemeName = Materials_SchemeName(schemeIdForDirectory(node.tree()));
    return de::Uri(node.composePath()).setScheme(Str_Text(schemeName));
}

static MaterialAnim* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups) return NULL;
    return &groups[number];
}

static bool isInAnimGroup(MaterialAnim const& group, material_t const* mat)
{
    if(!mat) return false;
    for(int i = 0; i < group.count; ++i)
    {
        if(group.frames[i].material == mat)
            return true;
    }
    return false;
}

static materialvariantspecification_t* copyVariantSpecification(
    const materialvariantspecification_t* tpl)
{
    materialvariantspecification_t* spec = (materialvariantspecification_t*) M_Malloc(sizeof *spec);
    if(!spec)
        Con_Error("Materials::copyVariantSpecification: Failed on allocation of %lu bytes for new MaterialVariantSpecification.", (unsigned long) sizeof *spec);
    memcpy(spec, tpl, sizeof *spec);
    return spec;
}

static int compareVariantSpecifications(const materialvariantspecification_t* a,
    const materialvariantspecification_t* b)
{
    DENG2_ASSERT(a && b);
    if(a == b) return 1;
    if(a->context != b->context) return 0;
    return GL_CompareTextureVariantSpecifications(a->primarySpec, b->primarySpec);
}

static materialvariantspecification_t* applyVariantSpecification(
    materialvariantspecification_t* spec, materialcontext_t mc,
    texturevariantspecification_t* primarySpec)
{
    DENG2_ASSERT(spec && (mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc)) && primarySpec);
    spec->context = mc;
    spec->primarySpec = primarySpec;
    return spec;
}

static materialvariantspecification_t* linkVariantSpecification(
    materialvariantspecification_t* spec)
{
    DENG2_ASSERT(initedOk && spec);
    {
    MaterialVariantSpecificationListNode* node;
    if(NULL == (node = (MaterialVariantSpecificationListNode*) M_Malloc(sizeof(*node))))
        Con_Error("Materials::linkVariantSpecification: Failed on allocation of %lu bytes for "
            "new MaterialVariantSpecificationListNode.", (unsigned long) sizeof(*node));
    node->spec = spec;
    node->next = variantSpecs;
    variantSpecs = (VariantSpecificationList*)node;
    return spec;
    }
}

static materialvariantspecification_t* findVariantSpecification(
    const materialvariantspecification_t* tpl, boolean canCreate)
{
    DENG2_ASSERT(initedOk && tpl);
    MaterialVariantSpecificationListNode* node;
    for(node = variantSpecs; node; node = node->next)
    {
        if(compareVariantSpecifications(node->spec, tpl))
            return node->spec;
    }
    if(!canCreate)
        return NULL;
    return linkVariantSpecification(copyVariantSpecification(tpl));
}

static materialvariantspecification_t* getVariantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    static materialvariantspecification_t tpl;

    DENG2_ASSERT(initedOk && (mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc)));

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

    texturevariantspecification_t* primarySpec =
            GL_TextureVariantSpecificationForContext(primaryContext, flags,
                border, tClass, tMap, wrapS, wrapT, minFilter, magFilter, anisoFilter, mipmapped,
                gammaCorrection, noStretch, toAlpha);
    applyVariantSpecification(&tpl, mc, primarySpec);
    return findVariantSpecification(&tpl, true);
}

static void destroyVariantSpecifications(void)
{
    DENG2_ASSERT(initedOk);
    while(variantSpecs)
    {
        MaterialVariantSpecificationListNode* next = variantSpecs->next;
        M_Free(variantSpecs->spec);
        M_Free(variantSpecs);
        variantSpecs = next;
    }
}

typedef struct {
    const materialvariantspecification_t* spec;
    MaterialVariant* chosen;
} choosevariantworker_parameters_t;

static int chooseVariantWorker(MaterialVariant* variant, void* parameters)
{
    choosevariantworker_parameters_t* p = (choosevariantworker_parameters_t*) parameters;
    const materialvariantspecification_t* cand = MaterialVariant_Spec(variant);
    DENG2_ASSERT(p);

    if(compareVariantSpecifications(cand, p->spec))
    {
        // This will do fine.
        p->chosen = variant;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

static MaterialVariant* chooseVariant(material_t* mat, const materialvariantspecification_t* spec)
{
    choosevariantworker_parameters_t params;
    DENG2_ASSERT(mat && spec);

    params.spec = spec;
    params.chosen = NULL;
    Material_IterateVariants(mat, chooseVariantWorker, &params);
    return params.chosen;
}

static MaterialBind* getMaterialBindForId(materialid_t id)
{
    if(0 == id || id > bindingCount) return NULL;
    return bindingIdMap[id-1];
}

static void updateMaterialBindInfo(MaterialBind& mb, bool canCreateInfo)
{
    MaterialBindInfo* info = mb.info();
    material_t* mat = mb.material();
    materialid_t matId = Materials_Id(mat);
    bool isCustom = (mat? Material_IsCustom(mat) : false);

    if(!info)
    {
        if(!canCreateInfo) return;

        // Create new info and attach to this binding.
        info = (MaterialBindInfo*) M_Malloc(sizeof *info);
        if(!info) Con_Error("MaterialBind::LinkDefinitions: Failed on allocation of %lu bytes for new MaterialBindInfo.", (unsigned long) sizeof *info);

        mb.attachInfo(*info);
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

static bool newMaterialBind(de::Uri& uri, material_t* material)
{
    MaterialRepository& matDirectory = schemeById(Materials_ParseSchemeName(uri.schemeCStr()));
    MaterialRepository::Node* node;
    MaterialBind* mb;

    node = matDirectory.insert(uri.path());

    // Is this a new binding?
    mb = reinterpret_cast<MaterialBind*>(node->userPointer());
    if(!mb)
    {
        // Acquire a new unique identifier for this binding.
        materialid_t const bindId = ++bindingCount;

        mb = new MaterialBind(*node, bindId);
        if(!mb)
        {
            throw de::Error("Materials::newMaterialBind",
                            de::String("Failed on allocation of %1 bytes for new MaterialBind.")
                                .arg((unsigned long) sizeof *mb));
        }
        node->setUserPointer(mb);

        if(material)
        {
            Material_SetPrimaryBind(material, bindId);
        }

        // Add the new binding to the bindings index/map.
        if(bindingCount > bindingIdMapSize)
        {
            // Allocate more memory.
            bindingIdMapSize += MATERIALS_BINDINGMAP_BLOCK_ALLOC;
            bindingIdMap = (MaterialBind**) M_Realloc(bindingIdMap, sizeof *bindingIdMap * bindingIdMapSize);
            if(!bindingIdMap)
                Con_Error("Materials::newMaterialBind: Failed on (re)allocation of %lu bytes enlarging MaterialBind map.", (unsigned long) sizeof *bindingIdMap * bindingIdMapSize);
        }
        bindingIdMap[bindingCount-1] = mb; /* 1-based index */
    }

    // (Re)configure the binding.
    mb->setMaterial(material);
    updateMaterialBindInfo(*mb, false/*do not create, only update if present*/);

    return true;
}

static material_t* allocMaterial(void)
{
    material_t* mat = (material_t*)BlockSet_Allocate(materialsBlockSet);
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
    MaterialListNode* node = (MaterialListNode*)M_Malloc(sizeof *node);
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

    for(i = 0; i < MATERIALSCHEME_COUNT; ++i)
    {
        schemes[i] = new MaterialRepository();
    }

    initedOk = true;
}

static void destroyMaterials(void)
{
    DENG2_ASSERT(initedOk);
    while(materials)
    {
        MaterialListNode* next = materials->next;
        Material_Destroy(materials->mat);
        M_Free(materials);
        materials = next;
    }
    BlockSet_Delete(materialsBlockSet);
    materialsBlockSet = NULL;
    materialCount = 0;
}

static void destroyBindings(void)
{
    int i;
    DENG2_ASSERT(initedOk);

    for(i = 0; i < MATERIALSCHEME_COUNT; ++i)
    {
        if(!schemes[i]) continue;

        de::PathTreeIterator<MaterialRepository> iter(schemes[i]->leafNodes());
        while(iter.hasNext())
        {
            MaterialBind* mb = reinterpret_cast<MaterialBind*>(iter.next().userPointer());
            if(mb)
            {
                // Detach our user data from this node.
                iter.value().setUserPointer(0);
                delete mb;
            }
        }
        delete schemes[i]; schemes[i] = NULL;
    }

    // Clear the binding index/map.
    if(bindingIdMap)
    {
        M_Free(bindingIdMap); bindingIdMap = NULL;
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

materialschemeid_t Materials_ParseSchemeName(const char* str)
{
    if(!str || 0 == strlen(str)) return MS_ANY;

    if(!stricmp(str, MS_TEXTURES_NAME)) return MS_TEXTURES;
    if(!stricmp(str, MS_FLATS_NAME))    return MS_FLATS;
    if(!stricmp(str, MS_SPRITES_NAME))  return MS_SPRITES;
    if(!stricmp(str, MS_SYSTEM_NAME))   return MS_SYSTEM;

    return MS_INVALID; // Unknown.
}

Str const* Materials_SchemeName(materialschemeid_t id)
{
    static de::Str const names[1+MATERIALSCHEME_COUNT] = {
        /* No scheme name */    "",
        /* MS_SYSTEM */         MS_SYSTEM_NAME,
        /* MS_FLATS */          MS_FLATS_NAME,
        /* MS_TEXTURES */       MS_TEXTURES_NAME,
        /* MS_SPRITES */        MS_SPRITES_NAME
    };
    if(VALID_MATERIALSCHEMEID(id))
        return names[1 + (id - MATERIALSCHEME_FIRST)];
    return names[0];
}

materialschemeid_t Materials_Scheme(materialid_t id)
{
    MaterialBind* bind = getMaterialBindForId(id);
    if(!bind)
    {
        DEBUG_Message(("Warning: Materials::Scheme: Attempted with unbound materialId #%u, returning 'any' scheme.\n", id));
        return MS_ANY;
    }
    return schemeIdForDirectory(bind->directoryNode().tree());
}

static void clearBindingDefinitionLinks(MaterialBind* mb)
{
    DENG2_ASSERT(mb);
    MaterialBindInfo* info = mb->info();
    if(info)
    {
        info->decorationDefs[0]    = info->decorationDefs[1]    = NULL;
        info->detailtextureDefs[0] = info->detailtextureDefs[1] = NULL;
        info->ptcgenDefs[0]        = info->ptcgenDefs[1]        = NULL;
        info->reflectionDefs[0]    = info->reflectionDefs[1]    = NULL;
    }
}

void Materials_ClearDefinitionLinks(void)
{
    errorIfNotInited("Materials::ClearDefinitionLinks");

    for(MaterialListNode* node = materials; node; node = node->next)
    {
        material_t* mat = node->mat;
        Material_SetDefinition(mat, NULL);
    }

    for(uint i = uint(MATERIALSCHEME_FIRST); i <= uint(MATERIALSCHEME_LAST); ++i)
    {
        MaterialRepository& matDirectory = schemeById(materialschemeid_t(i));

        de::PathTreeIterator<MaterialRepository> iter(matDirectory.leafNodes());
        while(iter.hasNext())
        {
            MaterialBind* mb = reinterpret_cast<MaterialBind*>(iter.next().userPointer());
            if(mb)
            {
                clearBindingDefinitionLinks(mb);
            }
        }
    }
}

void Materials_Rebuild(material_t* mat, ded_material_t* def)
{
    if(!initedOk || !mat || !def) return;

    /// @todo We should be able to rebuild the variants.
    Material_DestroyVariants(mat);
    Material_SetDefinition(mat, def);

    // Update bindings.
    for(uint i = 0; i < bindingCount; ++i)
    {
        MaterialBind* mb = bindingIdMap[i];
        if(!mb || mb->material() != mat) continue;

        updateMaterialBindInfo(*mb, false /*do not create, only update if present*/);
    }
}

void Materials_PurgeCacheQueue(void)
{
    errorIfNotInited("Materials::PurgeCacheQueue");
    while(variantCacheQueue)
    {
        VariantCacheQueueNode* next = variantCacheQueue->next;
        M_Free(variantCacheQueue);
        variantCacheQueue = next;
    }
}

void Materials_ProcessCacheQueue(void)
{
    errorIfNotInited("Materials::ProcessCacheQueue");
    while(variantCacheQueue)
    {
        VariantCacheQueueNode* node = variantCacheQueue, *next = node->next;
        Materials_Prepare(node->mat, node->spec, node->smooth);
        M_Free(node);
        variantCacheQueue = next;
    }
}

material_t* Materials_ToMaterial(materialid_t id)
{
    MaterialBind* mb;
    if(!initedOk) return NULL;
    mb = getMaterialBindForId(id);
    if(!mb) return NULL;
    return mb->material();
}

materialid_t Materials_Id(material_t* mat)
{
    MaterialBind* bind;
    if(!initedOk || !mat) return NOMATERIALID;
    bind = getMaterialBindForId(Material_PrimaryBind(mat));
    if(!bind) return NOMATERIALID;
    return bind->id();
}

/**
 * @defgroup validateMaterialUriFlags  Validate Material Uri Flags
 * @ingroup flags
 */
///@{
#define VMUF_ALLOW_ANY_SCHEME           0x1 ///< The Scheme component of the uri may be of zero-length; signifying "any scheme".
///@}

/**
 * @param uri  Uri to be validated.
 * @param flags  @ref validateMaterialUriFlags
 * @param quiet  @c true= Do not output validation remarks to the log.
 * @return  @c true if @a Uri passes validation.
 */
static bool validateMaterialUri(de::Uri const& uri, int flags, boolean quiet=false)
{
    materialschemeid_t schemeId;

    if(uri.isEmpty())
    {
        if(!quiet)
        {
            LOG_MSG("Invalid path in material URI \"%s\".") <<uri;
        }
        return false;
    }

    schemeId = Materials_ParseSchemeName(uri.schemeCStr());
    if(!((flags & VMUF_ALLOW_ANY_SCHEME) && schemeId == MS_ANY) &&
       !VALID_MATERIALSCHEMEID(schemeId))
    {
        if(!quiet)
        {
            LOG_MSG("Unknown scheme in material URI \"%s\".") << uri;
        }
        return false;
    }

    return true;
}

/**
 * Given a directory and path, search the Materials collection for a match.
 *
 * @param directory  Scheme-specific MaterialRepository to search in.
 * @param path  Path of the material to search for.
 * @return  Found Material else @c NULL
 */
static MaterialBind* findMaterialBindForPath(MaterialRepository& matDirectory, de::String path)
{
    try
    {
        MaterialRepository::Node& node = matDirectory.find(path, de::PathTree::NoBranch | de::PathTree::MatchFull);
        return reinterpret_cast<MaterialBind*>(node.userPointer());
    }
    catch(MaterialRepository::NotFoundError const&)
    {} // Ignore this error.
    return NULL; // Not found.
}

/// @pre @a uri has already been validated and is well-formed.
static MaterialBind* findMaterialBindForUri(de::Uri const& uri)
{
    materialschemeid_t schemeId = Materials_ParseSchemeName(uri.schemeCStr());
    de::String const& path = uri.path();
    MaterialBind* bind = 0;
    if(schemeId != MS_ANY)
    {
        // Caller wants a material in a specific scheme.
        bind = findMaterialBindForPath(schemeById(schemeId), path);
    }
    else
    {
        // Caller does not care which scheme.
        // Check for the material in these schemes in priority order.
        static const materialschemeid_t order[] = {
            MS_SPRITES, MS_TEXTURES, MS_FLATS, MS_ANY
        };
        int n = 0;
        do
        {
            bind = findMaterialBindForPath(schemeById(order[n]), path);
        } while(!bind && order[++n] != MS_ANY);
    }
    return bind;
}

materialid_t Materials_ResolveUri2(Uri const* _uri, boolean quiet)
{
    LOG_AS("Materials::resolveUri");

    if(!initedOk || !_uri) return NOMATERIALID;

    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);
    if(!validateMaterialUri(uri, VMUF_ALLOW_ANY_SCHEME, true /*quiet please*/))
    {
#if _DEBUG
        LOG_WARNING("\"%s\" failed validation, returning NOMATERIALID.") << uri;
#endif
        return NOMATERIALID;
    }

    // Perform the search.
    MaterialBind* bind = findMaterialBindForUri(uri);
    if(bind) return bind->id();

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        LOG_DEBUG("\"%s\" not found, returning NOMATERIALID.") << uri;
    }
    return NOMATERIALID;
}

/// @note Part of the Doomsday public API.
materialid_t Materials_ResolveUri(Uri const* uri)
{
    return Materials_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

materialid_t Materials_ResolveUriCString2(char const* path, boolean quiet)
{
    if(path && path[0])
    {
        de::Uri uri = de::Uri(path, RC_NULL);
        return Materials_ResolveUri2(reinterpret_cast<uri_s*>(&uri), quiet);
    }
    return NOMATERIALID;
}

/// @note Part of the Doomsday public API.
materialid_t Materials_ResolveUriCString(char const* path)
{
    return Materials_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

AutoStr* Materials_ComposePath(materialid_t id)
{
    LOG_AS("Materials::composePath");

    MaterialBind* bind = getMaterialBindForId(id);
    if(bind)
    {
        MaterialRepository::Node& node = bind->directoryNode();
        QByteArray path = node.composePath().toUtf8();
        return AutoStr_FromTextStd(path.constData());
    }

    LOG_WARNING("Attempted with unbound materialId #%u, returning null-object.") << id;
    return AutoStr_New();
}

/// @note Part of the Doomsday public API.
Uri* Materials_ComposeUri(materialid_t id)
{
    LOG_AS("Materials::composeuri");

    MaterialBind* bind = getMaterialBindForId(id);
    if(bind)
    {
        MaterialRepository::Node& node = bind->directoryNode();
        return reinterpret_cast<Uri*>(new de::Uri(composeUriForDirectoryNode(node)));
    }

#if _DEBUG
    if(id != NOMATERIALID)
    {
        LOG_WARNING("Attempted with unbound materialId #%u, returning null-object.") << id;
    }
#endif
    return Uri_New();
}

material_t* Materials_CreateFromDef(ded_material_t* def)
{
    DENG2_ASSERT(def);

    LOG_AS("Materials::createFromDef");

    if(!initedOk || !def->uri) return NULL;
    de::Uri& uri = reinterpret_cast<de::Uri&>(*def->uri);

    // We require a properly formed uri.
    if(!validateMaterialUri(uri, 0, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Material \"%s\" from definition %p, ignoring.") << uri << de::dintptr(def);
        return NULL;
    }

    // Have we already created a material for this?
    MaterialBind* bind = findMaterialBindForUri(uri);
    if(bind && bind->material())
    {
        LOG_DEBUG("A Material with uri \"%s\" already exists, returning existing.") << uri;
        return bind->material();
    }

    // Ensure the primary layer has a valid texture reference.
    textureid_t texId = NOTEXTUREID;
    if(def->layers[0].stageCount.num > 0)
    {
        ded_material_layer_t const& layer = def->layers[0];
        if(layer.stages[0].texture) // Not unused.
        {
            texId = Textures_ResolveUri2(layer.stages[0].texture, true/*quiet please*/);
            if(texId == NOTEXTUREID)
            {
                LOG_WARNING("Unknown texture \"%s\" in Material \"%s\" (layer %i stage %i).")
                    << reinterpret_cast<de::Uri*>(layer.stages[0].texture)
                    << reinterpret_cast<de::Uri*>(def->uri)
                    << 0 << 0;
            }
        }
    }
    if(texId == NOTEXTUREID) return NULL;

    // A new Material.
    material_t* mat = linkMaterialToGlobalList(allocMaterial());
    mat->_flags = def->flags;
    mat->_isCustom = Texture_IsCustom(Textures_ToTexture(texId));
    mat->_def = def;
    Size2_SetWidthHeight(mat->_size, MAX_OF(0, def->width), MAX_OF(0, def->height));
    mat->_envClass = S_MaterialEnvClassForUri(reinterpret_cast<Uri const*>(&uri));

    if(!bind)
    {
        newMaterialBind(uri, mat);
    }
    else
    {
        bind->setMaterial(mat);
    }

    return mat;
}

static void pushVariantCacheQueue(material_t* mat, materialvariantspecification_t const* spec, bool smooth)
{
    DENG2_ASSERT(initedOk && mat && spec);

    VariantCacheQueueNode* node = (VariantCacheQueueNode*) M_Malloc(sizeof *node);
    if(!node) Con_Error("Materials::pushVariantCacheQueue: Failed on allocation of %lu bytes for new VariantCacheQueueNode.", (unsigned long) sizeof *node);

    node->mat = mat;
    node->spec = spec;
    node->smooth = smooth;
    node->next = variantCacheQueue;
    variantCacheQueue = node;
}

void Materials_Precache2(material_t* mat, materialvariantspecification_t const* spec,
    boolean smooth, boolean cacheGroup)
{
    errorIfNotInited("Materials_Precache2");

    if(!mat || ! spec)
    {
        DEBUG_Message(("Materials::precache: Invalid arguments mat:%p, spec:%p, ignoring.\n", mat, spec));
        return;
    }

    // Don't precache when playing demo.
    if(isDedicated || playback) return;

    // Already in the queue?
    for(VariantCacheQueueNode* node = variantCacheQueue; node; node = node->next)
    {
        if(mat == node->mat && spec == node->spec) return;
    }

    pushVariantCacheQueue(mat, spec, CPP_BOOL(smooth));

    if(cacheGroup && Material_IsGroupAnimated(mat))
    {
        // Material belongs in one or more animgroups; precache the group.
        for(int i = 0; i < numgroups; ++i)
        {
            if(!isInAnimGroup(groups[i], mat)) continue;

            for(int k = 0; k < groups[i].count; ++k)
                Materials_Precache2(groups[i].frames[k].material, spec, smooth, false);
        }
    }
}

void Materials_Precache(material_t* mat, materialvariantspecification_t const* spec, boolean smooth)
{
    Materials_Precache2(mat, spec, smooth, true);
}

void Materials_Ticker(timespan_t time)
{
    // The animation will only progress when the game is not paused.
    if(clientPaused || novideo) return;

    for(MaterialListNode* node = materials; node; node = node->next)
    {
        Material_Ticker(node->mat, time);
    }

    if(DD_IsSharpTick())
    {
        animateAnimGroups();
    }
}

static Texture* findDetailTextureForDef(ded_detailtexture_t const& def)
{
    return R_FindDetailTextureForResourcePath(def.detailTex);
}

static Texture* findShinyTextureForDef(ded_reflection_t const& def)
{
    return R_FindReflectionTextureForResourcePath(def.shinyMap);
}

static Texture* findShinyMaskTextureForDef(ded_reflection_t const& def)
{
    return R_FindMaskTextureForResourcePath(def.maskMap);
}

static void updateMaterialTextureLinks(MaterialBind& mb)
{
    material_t* mat = mb.material();

    // We may need to need to construct and attach the info.
    updateMaterialBindInfo(mb, true /* create if not present */);

    if(!mat) return;

    ded_detailtexture_t const* dtlDef = mb.detailTextureDef();
    Material_SetDetailTexture(mat,  (dtlDef? findDetailTextureForDef(*dtlDef) : NULL));
    Material_SetDetailStrength(mat, (dtlDef? dtlDef->strength : 0));
    Material_SetDetailScale(mat,    (dtlDef? dtlDef->scale : 0));

    ded_reflection_t const* refDef = mb.reflectionDef();
    Material_SetShinyTexture(mat,     (refDef? findShinyTextureForDef(*refDef) : NULL));
    Material_SetShinyMaskTexture(mat, (refDef? findShinyMaskTextureForDef(*refDef) : NULL));
    Material_SetShinyBlendmode(mat,   (refDef? refDef->blendMode : BM_ADD));
    float const black[3] = { 0, 0, 0 };
    Material_SetShinyMinColor(mat,    (refDef? refDef->minColor : black));
    Material_SetShinyStrength(mat,    (refDef? refDef->shininess : 0));
}

static void setTexUnit(materialsnapshot_t* ms, byte unit, TextureVariant* texture,
    blendmode_t blendMode, float sScale, float tScale, float sOffset,
    float tOffset, float opacity)
{
    rtexmapunit_t* tu;
    DENG2_ASSERT(ms && unit < NUM_MATERIAL_TEXTURE_UNITS);

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
    DENG2_ASSERT(ms);

    for(int i = 0; i < NUM_MATERIAL_TEXTURE_UNITS; ++i)
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
materialsnapshot_t const* updateMaterialSnapshot(MaterialVariant* variant,
    materialsnapshot_t* snapshot)
{
    static struct materialtextureunit_s {
        TextureVariant* tex;
    } texUnits[NUM_MATERIAL_TEXTURE_UNITS];
    material_t* mat = MaterialVariant_GeneralCase(variant);
    materialvariantspecification_t const* spec = MaterialVariant_Spec(variant);
    DENG2_ASSERT(snapshot);

    memset(texUnits, 0, sizeof texUnits);

    // Ensure all resources needed to visualize this Material's layers have been prepared.
    int layerCount = Material_LayerCount(mat);
    for(int i = 0; i < layerCount; ++i)
    {
        materialvariant_layer_t const* ml = MaterialVariant_Layer(variant, i);
        preparetextureresult_t result;

        if(!ml->texture) continue;

        // Pick the instance matching the specified context.
        texUnits[i].tex = GL_PrepareTextureVariant2(ml->texture, spec->primarySpec, &result);

        if(0 == i && (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
        {
            // Primary texture was (re)prepared.
            Material_SetPrepared(mat, result == PTR_UPLOADED_ORIGINAL? 1 : 2);

            if(MaterialBind* bind = getMaterialBindForId(Material_PrimaryBind(mat)))
            {
                updateMaterialTextureLinks(*bind);
            }

            // Are we inheriting the logical dimensions from the texture?
            if(0 == Material_Width(mat) && 0 == Material_Height(mat))
            {
                Size2Raw texSize(Texture_Width(ml->texture),
                                 Texture_Height(ml->texture));
                Material_SetSize(mat, &texSize);
            }
        }
    }

    // Do we need to prepare a DetailTexture?
    Texture* tex = Material_DetailTexture(mat);
    if(tex)
    {
        float const contrast = Material_DetailStrength(mat) * detailFactor;
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

const materialsnapshot_t* Materials_PrepareVariant2(MaterialVariant* variant, boolean updateSnapshot)
{
    // Acquire the snapshot we are interested in.
    materialsnapshot_t* snapshot = MaterialVariant_Snapshot(variant);
    if(!snapshot)
    {
        // Time to allocate the snapshot.
        snapshot = (materialsnapshot_t*)M_Malloc(sizeof *snapshot);
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

const materialsnapshot_t* Materials_PrepareVariant(MaterialVariant* variant)
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
    MaterialBind* mb = getMaterialBindForId(Material_PrimaryBind(mat));
    return mb->decorationDef();
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
    MaterialBind* mb = getMaterialBindForId(Material_PrimaryBind(mat));
    return mb->ptcGenDef();
}

uint Materials_Size(void)
{
    return materialCount;
}

uint Materials_Count(materialschemeid_t schemeId)
{
    if(!VALID_MATERIALSCHEMEID(schemeId) || !Materials_Size()) return 0;
    return schemeById(schemeId).size();
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

MaterialVariant* Materials_ChooseVariant(material_t* mat,
    const materialvariantspecification_t* spec, boolean smoothed, boolean canCreate)
{
    MaterialVariant* variant;

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

static int printVariantInfo(MaterialVariant* variant, void* parameters)
{
    int* variantIdx = (int*)parameters;
    MaterialVariant* next = MaterialVariant_TranslationNext(variant);
    int i, layers = Material_LayerCount(MaterialVariant_GeneralCase(variant));
    DENG2_ASSERT(variantIdx);

    Con_Printf("Variant #%i: Spec:%p\n", *variantIdx, (void*)MaterialVariant_Spec(variant));

    // Print translation info:
    if(Material_HasTranslation(MaterialVariant_GeneralCase(variant)))
    {
        MaterialVariant* cur = MaterialVariant_TranslationCurrent(variant);
        float inter = MaterialVariant_TranslationPoint(variant);
        Uri* curUri = Materials_ComposeUri(Materials_Id(MaterialVariant_GeneralCase(cur)));
        AutoStr* curPath = Uri_ToString(curUri);
        Uri* nextUri = Materials_ComposeUri(Materials_Id(MaterialVariant_GeneralCase(next)));
        AutoStr* nextPath = Uri_ToString(nextUri);

        Con_Printf("  Translation: Current:\"%s\" Next:\"%s\" Inter:%f\n",
                   F_PrettyPath(Str_Text(curPath)), F_PrettyPath(Str_Text(nextPath)), inter);

        Uri_Delete(curUri);
        Uri_Delete(nextUri);
    }

    // Print layer info:
    for(i = 0; i < layers; ++i)
    {
        const materialvariant_layer_t* l = MaterialVariant_Layer(variant, i);
        Uri* uri = Textures_ComposeUri(Textures_Id(l->texture));
        AutoStr* path = Uri_ToString(uri);

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:(\"%s\" uid:%u)"
                   "\n      Offset: %.2f x %.2f Glow:%.2f\n",
                   i, l->stage, (int)l->tics, F_PrettyPath(Str_Text(path)), Textures_Id(l->texture),
                   l->texOrigin[0], l->texOrigin[1], l->glow);

        Uri_Delete(uri);
    }

    ++(*variantIdx);

    return 0; // Continue iteration.
}

static void printMaterialInfo(material_t* mat)
{
    Uri* uri = Materials_ComposeUri(Materials_Id(mat));
    AutoStr* path = Uri_ToString(uri);
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

    Uri_Delete(uri);
}

static void printMaterialOverview(material_t* mat, bool printScheme)
{
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials_Size()));
    Uri* uri = Materials_ComposeUri(Materials_Id(mat));
    AutoStr* path = (printScheme? Uri_ToString(uri) : Str_PercentDecode(AutoStr_FromTextStd(Str_Text(Uri_Path(uri)))));

    Con_Printf("%-*s %*u %s\n", printScheme? 22 : 14, F_PrettyPath(Str_Text(path)),
               numUidDigits, Materials_Id(mat),
               !Material_IsCustom(mat) ? "game" : (Material_Definition(mat)->autoGenerated? "addon" : "def"));

    Uri_Delete(uri);
}

/**
 * @todo A horridly inefficent algorithm. This should be implemented in MaterialRepository
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the material search/listing console commands
 * so is not hugely important right now.
 */
static MaterialRepository::Node** collectDirectoryNodes(materialschemeid_t schemeId,
    de::String like, int* count, MaterialRepository::Node** storage)
{
    materialschemeid_t fromId, toId;

    if(VALID_MATERIALSCHEMEID(schemeId))
    {
        // Only consider materials in this scheme.
        fromId = toId = schemeId;
    }
    else
    {
        // Consider materials in any scheme.
        fromId = MATERIALSCHEME_FIRST;
        toId   = MATERIALSCHEME_LAST;
    }

    int idx = 0;
    for(uint i = uint(fromId); i <= uint(toId); ++i)
    {
        MaterialRepository& matDirectory = schemeById(materialschemeid_t(i));

        de::PathTreeIterator<MaterialRepository> iter(matDirectory.leafNodes());
        while(iter.hasNext())
        {
            MaterialRepository::Node& node = iter.next();
            if(!like.isEmpty())
            {
                de::String path = node.composePath();
                if(!path.beginsWith(like, Qt::CaseInsensitive)) continue;
            }

            if(storage)
            {
                // Store mode.
                storage[idx++] = &node;
            }
            else
            {
                // Count mode.
                ++idx;
            }
        }
    }

    if(storage)
    {
        storage[idx] = 0; // Terminate.
        if(count) *count = idx;
        return storage;
    }

    if(idx == 0)
    {
        if(count) *count = 0;
        return NULL;
    }

    storage = (MaterialRepository::Node**)M_Malloc(sizeof *storage * (idx+1));
    if(!storage)
        Con_Error("Materials::collectDirectoryNodes: Failed on allocation of %lu bytes for new MaterialRepository::Node collection.", (unsigned long) (sizeof* storage * (idx+1)));
    return collectDirectoryNodes(schemeId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(void const* a, void const* b)
{
    // Decode paths before determining a lexicographical delta.
    MaterialRepository::Node const& nodeA = **(MaterialRepository::Node const**)a;
    MaterialRepository::Node const& nodeB = **(MaterialRepository::Node const**)b;
    QByteArray pathAUtf8 = nodeA.composePath().toUtf8();
    QByteArray pathBUtf8 = nodeB.composePath().toUtf8();
    AutoStr* pathA = Str_PercentDecode(AutoStr_FromTextStd(pathAUtf8.constData()));
    AutoStr* pathB = Str_PercentDecode(AutoStr_FromTextStd(pathBUtf8.constData()));
    return Str_CompareIgnoreCase(pathA, Str_Text(pathB));
}

static size_t printMaterials2(materialschemeid_t schemeId, char const* like,
    bool printSchemeName)
{
    int numFoundDigits, numUidDigits, idx, count = 0;
    MaterialRepository::Node** foundMaterials = collectDirectoryNodes(schemeId, like, &count, NULL);
    MaterialRepository::Node** iter;

    if(!foundMaterials) return 0;

    if(!printSchemeName)
        Con_FPrintf(CPF_YELLOW, "Known materials in scheme '%s'", Str_Text(Materials_SchemeName(schemeId)));
    else // Any scheme.
        Con_FPrintf(CPF_YELLOW, "Known materials");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials_Size()));
    Con_Printf(" %*s: %-*s %*s origin\n", numFoundDigits, "idx",
        printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path", numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundMaterials, (size_t)count, sizeof *foundMaterials, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundMaterials; *iter; ++iter)
    {
        MaterialRepository::Node const* node = *iter;
        MaterialBind* mb = reinterpret_cast<MaterialBind*>(node->userPointer());
        material_t* mat = mb->material();
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printMaterialOverview(mat, printSchemeName);
    }

    M_Free(foundMaterials);
    return count;
}

static void printMaterials(materialschemeid_t schemeId, const char* like)
{
    size_t printTotal = 0;
    // Do we care which scheme?
    if(schemeId == MS_ANY && like && like[0])
    {
        printTotal = printMaterials2(schemeId, like, true);
        Con_PrintRuler();
    }
    // Only one scheme to print?
    else if(VALID_MATERIALSCHEMEID(schemeId))
    {
        printTotal = printMaterials2(schemeId, like, false);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each scheme separately.
        int i;
        for(i = MATERIALSCHEME_FIRST; i <= MATERIALSCHEME_LAST; ++i)
        {
            size_t printed = printMaterials2((materialschemeid_t)i, like, false);
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
    MaterialAnim* group = getAnimGroup(groupNum);
    if(!group) return false;
    return isInAnimGroup(*group, mat);
}

boolean Materials_HasDecorations(material_t* mat)
{
    if(novideo) return false;

    DENG2_ASSERT(mat);
    /// @todo We should not need to prepare to determine this.
    /// Nor should we need to process the group each time. Cache this decision.
    if(Materials_DecorationDef(mat)) return true;
    if(Material_IsGroupAnimated(mat))
    {
        int g, i, numGroups = Materials_AnimGroupCount();
        for(g = 0; g < numGroups; ++g)
        {
            MaterialAnim* group = &groups[g];

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
    // Allocating one by one is inefficient, but it doesn't really matter.
    groups = (MaterialAnim*)Z_Realloc(groups, sizeof(*groups) * (numgroups + 1), PU_APPSTATIC);

    // Init the new group.
    MaterialAnim* group = &groups[numgroups];
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

void Materials_ClearAnimGroups(void)
{
    if(numgroups <= 0) return;

    for(int i = 0; i < numgroups; ++i)
    {
        MaterialAnim* group = &groups[i];
        Z_Free(group->frames);
    }

    Z_Free(groups);
    groups = NULL;
    numgroups = 0;
}

void Materials_AddAnimGroupFrame(int groupNum, struct material_s* mat, int tics, int randomTics)
{
    MaterialAnim* group = getAnimGroup(groupNum);

    if(!group)
    {
        DEBUG_Message(("Materials::AddAnimGroupFrame: Unknown anim group '%i', ignoring.\n", groupNum));
        return;
    }

    if(!mat)
    {
        DEBUG_Message(("Warning::Materials::AddAnimGroupFrame: Invalid material (ref=0), ignoring.\n"));
        return;
    }

    // Mark the material as being in an animgroup.
    Material_SetGroupAnimated(mat, true);

    // Allocate a new animframe.
    group->frames = (MaterialAnimFrame*)Z_Realloc(group->frames, sizeof(*group->frames) * ++group->count, PU_APPSTATIC);

    MaterialAnimFrame* frame = &group->frames[group->count - 1];
    frame->material = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean Materials_IsPrecacheAnimGroup(int groupNum)
{
    MaterialAnim* group = getAnimGroup(groupNum);
    if(!group) return false;
    return ((group->flags & AGF_PRECACHE) != 0);
}

#if 0
static int clearVariantTranslationWorker(MaterialVariant* variant, void* /*parameters*/)
{
    MaterialVariant_SetTranslation(variant, variant, variant);
    return 0; // Continue iteration.
}

static void Materials_ClearTranslation(material_t* mat)
{
    DENG2_ASSERT(initedOk);
    Material_IterateVariants(mat, clearVariantTranslationWorker, NULL);
}
#endif

typedef struct {
    material_t* current, *next;
} setmaterialtranslationworker_parameters_t;

static int setVariantTranslationWorker(MaterialVariant* variant, void* parameters)
{
    setmaterialtranslationworker_parameters_t* p = (setmaterialtranslationworker_parameters_t*) parameters;
    const materialvariantspecification_t* spec = MaterialVariant_Spec(variant);
    MaterialVariant* current, *next;
    DENG2_ASSERT(p);

    current = Materials_ChooseVariant(p->current, spec, false, true/*create if necessary*/);
    next    = Materials_ChooseVariant(p->next,    spec, false, true/*create if necessary*/);
    MaterialVariant_SetTranslation(variant, current, next);
    return 0; // Continue iteration.
}

static int setVariantTranslationPointWorker(MaterialVariant* variant, void* parameters)
{
    float* interPtr = (float*)parameters;
    DENG2_ASSERT(interPtr);

    MaterialVariant_SetTranslationPoint(variant, *interPtr);
    return 0; // Continue iteration.
}

void Materials_AnimateAnimGroup(MaterialAnim* group)
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
            setmaterialtranslationworker_parameters_t params;

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

static int resetVariantGroupAnimWorker(MaterialVariant* mat, void* /*parameters*/)
{
    MaterialVariant_ResetAnim(mat);
    return 0; // Continue iteration.
}

void Materials_ResetAnimGroups(void)
{
    MaterialListNode* node;
    MaterialAnim* group;
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

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    if(!Materials_Size())
    {
        Con_Message("There are currently no materials defined/loaded.\n");
        return true;
    }

    materialschemeid_t schemeId = MS_ANY;
    char const* like = 0;
    de::Uri uri;

    // "listmaterials [scheme] [path]"
    if(argc > 2)
    {
        uri.setScheme(argv[1]).setPath(argv[2]);

        schemeId = DD_ParseMaterialSchemeName(uri.schemeCStr());
        if(!VALID_MATERIALSCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
            return false;
        }
        like = uri.pathCStr();
    }
    // "listmaterials [scheme:path]" (i.e., a partial Uri)
    else if(argc > 1)
    {
        uri.setUri(argv[1], RC_NULL);
        if(!uri.scheme().isEmpty())
        {
            schemeId = DD_ParseMaterialSchemeName(uri.schemeCStr());
            if(!VALID_MATERIALSCHEMEID(schemeId))
            {
                Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
                return false;
            }

            if(!uri.path().isEmpty())
                like = uri.pathCStr();
        }
        else
        {
            schemeId = DD_ParseMaterialSchemeName(uri.pathCStr());

            if(!VALID_MATERIALSCHEMEID(schemeId))
            {
                schemeId = MS_ANY;
                like = argv[1];
            }
        }
    }

    printMaterials(schemeId, like);

    return true;
}

MaterialBind& MaterialBind::setMaterial(material_t* newMaterial)
{
    if(asocMaterial != newMaterial)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        MaterialBindInfo* detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);

        // Associate with the new Material.
        asocMaterial = newMaterial;
    }
    return *this;
}

MaterialBind& MaterialBind::attachInfo(MaterialBindInfo& info)
{
    LOG_AS("MaterialBind::attachInfo");
    if(extInfo)
    {
#if _DEBUG
        de::Uri* uri = reinterpret_cast<de::Uri*>(Materials_ComposeUri(guid));
        LOG_DEBUG("Info already present for \"%s\", will replace.") << uri;
        delete uri;
#endif
        M_Free(extInfo);
    }
    extInfo = &info;
    return *this;
}

MaterialBindInfo* MaterialBind::detachInfo()
{
    MaterialBindInfo* retInfo = extInfo;
    extInfo = NULL;
    return retInfo;
}

ded_detailtexture_t* MaterialBind::detailTextureDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return NULL;
    return extInfo->detailtextureDefs[Material_Prepared(asocMaterial)-1];
}

ded_decor_t* MaterialBind::decorationDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return NULL;
    return extInfo->decorationDefs[Material_Prepared(asocMaterial)-1];
}

ded_ptcgen_t* MaterialBind::ptcGenDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return NULL;
    return extInfo->ptcgenDefs[Material_Prepared(asocMaterial)-1];
}

ded_reflection_t* MaterialBind::reflectionDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return NULL;
    return extInfo->reflectionDefs[Material_Prepared(asocMaterial)-1];
}

D_CMD(InspectMaterial)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc);

    // Path is assumed to be in a human-friendly, non-encoded representation.
    Str path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, argv[1]));

    de::Uri search = de::Uri(Str_Text(&path), RC_NULL);
    Str_Free(&path);

    if(!search.scheme().isEmpty())
    {
        materialschemeid_t schemeId = DD_ParseMaterialSchemeName(search.schemeCStr());
        if(!VALID_MATERIALSCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", search.schemeCStr());
            return false;
        }
    }

    material_t* mat = Materials_ToMaterial(Materials_ResolveUri(reinterpret_cast<Uri*>(&search)));
    if(mat)
    {
        printMaterialInfo(mat);
    }
    else
    {
        AutoStr* path = Uri_ToString(reinterpret_cast<Uri*>(&search));
        Con_Printf("Unknown material \"%s\".\n", Str_Text(path));
    }
    return true;
}

#if _DEBUG
D_CMD(PrintMaterialStats)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc); DENG2_UNUSED(argv);

    Con_FPrintf(CPF_YELLOW, "Material Statistics:\n");
    for(uint i = uint(MATERIALSCHEME_FIRST); i <= uint(MATERIALSCHEME_LAST); ++i)
    {
        materialschemeid_t schemeId = materialschemeid_t(i);
        MaterialRepository& matDirectory = schemeById(schemeId);
        uint size = matDirectory.size();

        Con_Printf("Scheme: %s (%u %s)\n", Str_Text(Materials_SchemeName(schemeId)), size, size==1? "material":"materials");
        matDirectory.debugPrintHashDistribution();
        matDirectory.debugPrint();
    }
    return true;
}
#endif
