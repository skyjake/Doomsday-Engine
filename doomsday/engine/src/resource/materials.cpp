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
#include "de_resource.h"

#include "resource/materials.h"
#include "resource/materialsnapshot.h"

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

D_CMD(InspectMaterial);
D_CMD(ListMaterials);
#if _DEBUG
D_CMD(PrintMaterialStats);
#endif

namespace de {

typedef UserDataPathTree MaterialRepository;

/**
 * Contains extended info about a material binding (see MaterialBind).
 * @note POD object.
 */
struct MaterialBindInfo
{
    ded_decor_t *decorationDefs[2];
    ded_detailtexture_t *detailtextureDefs[2];
    ded_ptcgen_t *ptcgenDefs[2];
    ded_reflection_t *reflectionDefs[2];
};

class MaterialBind
{
public:
    MaterialBind(MaterialRepository::Node &_direcNode, materialid_t id)
        : direcNode(&_direcNode), asocMaterial(0), guid(id), extInfo(0)
    {}

    ~MaterialBind()
    {
        MaterialBindInfo *detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);
    }

    /// @return  Unique identifier associated with this.
    materialid_t id() const { return guid; }

    /// @return  MaterialRepository node associated with this.
    MaterialRepository::Node &directoryNode() const { return *direcNode; }

    /// @return  Material associated with this else @c NULL.
    material_t *material() const { return asocMaterial; }

    /// @return  Extended info owned by this else @c NULL.
    MaterialBindInfo *info() const { return extInfo; }

    /**
     * Attach extended info data to this. If existing info is present it is replaced.
     * MaterialBind is given ownership of the info.
     * @param info  Extended info data to attach.
     */
    MaterialBind &attachInfo(MaterialBindInfo &info);

    /**
     * Detach any extended info owned by this and relinquish ownership to the caller.
     * @return  Extended info or else @c NULL if not present.
     */
    MaterialBindInfo *detachInfo();

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
    MaterialBind &setMaterial(material_t *material);

    /// @return  Detail texture definition associated with this else @c NULL
    ded_detailtexture_t *detailTextureDef() const;

    /// @return  Decoration definition associated with this else @c NULL
    ded_decor_t *decorationDef() const;

    /// @return  Particle generator definition associated with this else @c NULL
    ded_ptcgen_t *ptcGenDef() const;

    /// @return  Reflection definition associated with this else @c NULL
    ded_reflection_t *reflectionDef() const;

private:
    /// This binding's node in the directory.
    MaterialRepository::Node *direcNode;

    /// Material associated with this.
    material_t *asocMaterial;

    /// Unique identifier.
    materialid_t guid;

    /// Extended info about this binding. Will be attached upon successfull preparation
    /// of the first derived variant of the associated Material.
    MaterialBindInfo *extInfo;
};

typedef struct materialvariantspecificationlistnode_s {
    struct materialvariantspecificationlistnode_s *next;
    materialvariantspecification_t *spec;
} MaterialVariantSpecificationListNode;

typedef MaterialVariantSpecificationListNode VariantSpecificationList;

typedef struct materiallistnode_s {
    struct materiallistnode_s *next;
    material_t *mat;
} MaterialListNode;
typedef MaterialListNode MaterialList;

typedef struct variantcachequeuenode_s {
    struct variantcachequeuenode_s *next;
    material_t *mat;
    materialvariantspecification_t const *spec;
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

static void animateAllGroups();

static bool initedOk;
static VariantSpecificationList *variantSpecs;

static VariantCacheQueue *variantCacheQueue;

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
static blockset_t *materialsBlockSet;
static MaterialList *materials;
static uint materialCount;

static uint bindingCount;

/// LUT which translates materialid_t to MaterialBind*. Index with materialid_t-1
static uint bindingIdMapSize;
static MaterialBind **bindingIdMap;

// Material schemes contain mappings between names and MaterialBind instances.
static MaterialRepository *schemes[MATERIALSCHEME_COUNT];

void Materials::consoleRegister()
{
    C_CMD("inspectmaterial",    "s",    InspectMaterial)
    C_CMD("listmaterials",      NULL,   ListMaterials)

#ifdef DENG_DEBUG
    C_CMD("materialstats",      NULL,   PrintMaterialStats)
#endif
}

static void errorIfNotInited(char const *callerName)
{
    if(initedOk) return;
    throw Error("Materials", String("Collection is not presently initialized (%1).").arg(callerName));
}

static inline MaterialRepository &schemeById(materialschemeid_t id)
{
    DENG2_ASSERT(VALID_MATERIALSCHEMEID(id));
    DENG2_ASSERT(schemes[id - MATERIALSCHEME_FIRST]);
    return *schemes[id - MATERIALSCHEME_FIRST];
}

static materialschemeid_t schemeIdForDirectory(PathTree const &directory)
{
    for(uint i = uint(MATERIALSCHEME_FIRST); i <= uint(MATERIALSCHEME_LAST); ++i)
    {
        uint idx = i - MATERIALSCHEME_FIRST;
        if(schemes[idx] == &directory) return materialschemeid_t(i);
    }

    // Should never happen...
    throw Error("Materials::schemeIdForDirectory", QString("Failed to determine id for directory %p.").arg(dintptr(&directory)));
}

/// @return  Newly composed Uri for @a node. Must be deleted when no longer needed.
static de::Uri composeUriForDirectoryNode(MaterialRepository::Node const& node)
{
    return de::Uri(Materials::schemeName(schemeIdForDirectory(node.tree())), node.path());
}

static MaterialAnim* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups) return NULL;
    return &groups[number];
}

static bool isInAnimGroup(MaterialAnim const &group, material_t const &mat)
{
    for(int i = 0; i < group.count; ++i)
    {
        if(group.frames[i].material == &mat)
            return true;
    }
    return false;
}

static materialvariantspecification_t *dupVariantSpecification(
    materialvariantspecification_t const &tpl)
{
    materialvariantspecification_t *spec = (materialvariantspecification_t *) M_Malloc(sizeof *spec);
    if(!spec) Con_Error("Materials::copyVariantSpecification: Failed on allocation of %lu bytes for new MaterialVariantSpecification.", (unsigned long) sizeof *spec);
    std::memcpy(spec, &tpl, sizeof *spec);
    return spec;
}

static int compareVariantSpecifications(materialvariantspecification_t const &a,
    materialvariantspecification_t const &b)
{
    if(&a == &b) return 1;
    if(a.context != b.context) return 0;
    return GL_CompareTextureVariantSpecifications(a.primarySpec, b.primarySpec);
}

static void applyVariantSpecification(materialvariantspecification_t &spec, materialcontext_t mc,
    texturevariantspecification_t *primarySpec)
{
    DENG2_ASSERT(mc == MC_UNKNOWN || VALID_MATERIALCONTEXT(mc) && primarySpec);
    spec.context     = mc;
    spec.primarySpec = primarySpec;
}

static materialvariantspecification_t *linkVariantSpecification(
    materialvariantspecification_t &spec)
{
    DENG2_ASSERT(initedOk);
    MaterialVariantSpecificationListNode * node = (MaterialVariantSpecificationListNode *) M_Malloc(sizeof(*node));
    if(!node) Con_Error("Materials::linkVariantSpecification: Failed on allocation of %lu bytes for new MaterialVariantSpecificationListNode.", (unsigned long) sizeof(*node));
    node->spec = &spec;
    node->next = variantSpecs;
    variantSpecs = (VariantSpecificationList *)node;
    return &spec;
}

static materialvariantspecification_t *findVariantSpecification(
    materialvariantspecification_t const &tpl, bool canCreate)
{
    DENG2_ASSERT(initedOk);
    for(MaterialVariantSpecificationListNode* node = variantSpecs; node; node = node->next)
    {
        if(compareVariantSpecifications(*node->spec, tpl))
        {
            return node->spec;
        }
    }

    if(!canCreate) return 0;

    return linkVariantSpecification(*dupVariantSpecification(tpl));
}

static materialvariantspecification_t *getVariantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
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
        GL_TextureVariantSpecificationForContext(primaryContext, flags, border, tClass, tMap, wrapS, wrapT,
                                                 minFilter, magFilter, anisoFilter, mipmapped,
                                                 gammaCorrection, noStretch, toAlpha);

    applyVariantSpecification(tpl, mc, primarySpec);
    return findVariantSpecification(tpl, true);
}

static void destroyVariantSpecifications()
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
    materialvariantspecification_t const *spec;
    MaterialVariant* chosen;
} choosevariantworker_parameters_t;

static int chooseVariantWorker(struct materialvariant_s *_variant, void *parameters)
{
    MaterialVariant *variant = reinterpret_cast<MaterialVariant *>(_variant);
    choosevariantworker_parameters_t *p = (choosevariantworker_parameters_t *) parameters;
    materialvariantspecification_t const &cand = variant->spec();
    DENG2_ASSERT(p);

    if(compareVariantSpecifications(cand, *p->spec))
    {
        // This will do fine.
        p->chosen = variant;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

static MaterialVariant *chooseVariant2(material_t &mat, materialvariantspecification_t const &spec)
{
    choosevariantworker_parameters_t params;
    params.spec   = &spec;
    params.chosen = NULL;
    Material_IterateVariants(&mat, chooseVariantWorker, &params);
    return params.chosen;
}

static MaterialBind *getMaterialBindForId(materialid_t id)
{
    if(0 == id || id > bindingCount) return NULL;
    return bindingIdMap[id-1];
}

static void updateMaterialBindInfo(MaterialBind &mb, bool canCreateInfo)
{
    MaterialBindInfo *info = mb.info();
    material_t *mat = mb.material();
    materialid_t matId = Materials::id(mat);
    bool isCustom = (mat? Material_IsCustom(mat) : false);

    if(!info)
    {
        if(!canCreateInfo) return;

        // Create new info and attach to this binding.
        info = (MaterialBindInfo *) M_Malloc(sizeof *info);
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
    MaterialRepository& matDirectory = schemeById(Materials::parseSchemeName(uri.schemeCStr()));
    MaterialRepository::Node* node;
    MaterialBind* mb;

    node = &matDirectory.insert(uri.path());

    // Is this a new binding?
    mb = reinterpret_cast<MaterialBind*>(node->userPointer());
    if(!mb)
    {
        // Acquire a new unique identifier for this binding.
        materialid_t const bindId = ++bindingCount;

        mb = new MaterialBind(*node, bindId);
        if(!mb)
        {
            throw Error("Materials::newMaterialBind",
                            String("Failed on allocation of %1 bytes for new MaterialBind.")
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

static material_t* allocMaterial()
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

void Materials::init()
{
    if(initedOk) return; // Already been here.

    LOG_VERBOSE("Initializing Materials collection...");

    variantSpecs = NULL;
    variantCacheQueue = NULL;

    materialsBlockSet = BlockSet_New(sizeof(material_t), MATERIALS_BLOCK_ALLOC);
    materials = NULL;
    materialCount = 0;

    bindingCount = 0;

    bindingIdMap = NULL;
    bindingIdMapSize = 0;

    for(int i = 0; i < MATERIALSCHEME_COUNT; ++i)
    {
        schemes[i] = new MaterialRepository();
    }

    initedOk = true;
}

static void clearMaterials()
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
    materialsBlockSet = 0;
    materialCount = 0;
}

static void destroyBindings()
{
    DENG2_ASSERT(initedOk);

    for(int i = 0; i < MATERIALSCHEME_COUNT; ++i)
    {
        if(!schemes[i]) continue;

        PathTreeIterator<MaterialRepository> iter(schemes[i]->leafNodes());
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
        delete schemes[i]; schemes[i] = 0;
    }

    // Clear the binding index/map.
    if(bindingIdMap)
    {
        M_Free(bindingIdMap); bindingIdMap = 0;
        bindingIdMapSize = 0;
    }
    bindingCount = 0;
}

void Materials::shutdown()
{
    if(!initedOk) return;

    purgeCacheQueue();

    destroyBindings();
    clearMaterials();
    destroyVariantSpecifications();

    initedOk = false;
}

materialschemeid_t Materials::parseSchemeName(char const *str)
{
    if(!str || 0 == qstrlen(str)) return MS_ANY;

    if(!qstricmp(str, "Textures")) return MS_TEXTURES;
    if(!qstricmp(str, "Flats"))    return MS_FLATS;
    if(!qstricmp(str, "Sprites"))  return MS_SPRITES;
    if(!qstricmp(str, "System"))   return MS_SYSTEM;

    return MS_INVALID; // Unknown.
}

String const &Materials::schemeName(materialschemeid_t id)
{
    static String const names[1 + MATERIALSCHEME_COUNT] = {
        /* No scheme name */    "",
        /* MS_SYSTEM */         "System",
        /* MS_FLATS */          "Flats",
        /* MS_TEXTURES */       "Textures",
        /* MS_SPRITES */        "Sprites"
    };
    if(VALID_MATERIALSCHEMEID(id))
        return names[1 + (id - MATERIALSCHEME_FIRST)];
    return names[0];
}

materialschemeid_t Materials::scheme(materialid_t id)
{
    LOG_AS("Materials::Scheme");

    MaterialBind* bind = getMaterialBindForId(id);
    if(!bind)
    {
        LOG_WARNING("Attempted with unbound materialId #%u, returning 'any' scheme.") << id;
        return MS_ANY;
    }
    return schemeIdForDirectory(bind->directoryNode().tree());
}

static void clearBindingDefinitionLinks(MaterialBind *mb)
{
    DENG2_ASSERT(mb);
    MaterialBindInfo *info = mb->info();
    if(info)
    {
        info->decorationDefs[0]    = info->decorationDefs[1]    = NULL;
        info->detailtextureDefs[0] = info->detailtextureDefs[1] = NULL;
        info->ptcgenDefs[0]        = info->ptcgenDefs[1]        = NULL;
        info->reflectionDefs[0]    = info->reflectionDefs[1]    = NULL;
    }
}

void Materials::clearDefinitionLinks()
{
    errorIfNotInited("Materials::ClearDefinitionLinks");

    for(MaterialListNode *node = materials; node; node = node->next)
    {
        material_t *mat = node->mat;
        Material_SetDefinition(mat, NULL);
    }

    for(uint i = uint(MATERIALSCHEME_FIRST); i <= uint(MATERIALSCHEME_LAST); ++i)
    {
        MaterialRepository &matDirectory = schemeById(materialschemeid_t(i));

        PathTreeIterator<MaterialRepository> iter(matDirectory.leafNodes());
        while(iter.hasNext())
        {
            MaterialBind* mb = reinterpret_cast<MaterialBind *>(iter.next().userPointer());
            if(mb)
            {
                clearBindingDefinitionLinks(mb);
            }
        }
    }
}

void Materials::rebuild(material_t *mat, ded_material_t *def)
{
    if(!initedOk || !mat || !def) return;

    /// @todo We should be able to rebuild the variants.
    Material_DestroyVariants(mat);
    Material_SetDefinition(mat, def);

    // Update bindings.
    for(uint i = 0; i < bindingCount; ++i)
    {
        MaterialBind *mb = bindingIdMap[i];
        if(!mb || mb->material() != mat) continue;

        updateMaterialBindInfo(*mb, false /*do not create, only update if present*/);
    }
}

void Materials::purgeCacheQueue()
{
    errorIfNotInited("Materials::PurgeCacheQueue");
    while(variantCacheQueue)
    {
        VariantCacheQueueNode *next = variantCacheQueue->next;
        M_Free(variantCacheQueue);
        variantCacheQueue = next;
    }
}

void Materials::processCacheQueue()
{
    errorIfNotInited("Materials::ProcessCacheQueue");
    while(variantCacheQueue)
    {
        VariantCacheQueueNode *node = variantCacheQueue, *next = node->next;
        prepare(*node->mat, *node->spec, node->smooth);
        M_Free(node);
        variantCacheQueue = next;
    }
}

material_t *Materials::toMaterial(materialid_t id)
{
    if(!initedOk) return 0;

    MaterialBind *mb = getMaterialBindForId(id);
    if(!mb) return 0;
    return mb->material();
}

materialid_t Materials::id(material_t *mat)
{
    if(!initedOk || !mat) return NOMATERIALID;

    MaterialBind *bind = getMaterialBindForId(Material_PrimaryBind(mat));
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
static bool validateMaterialUri(de::Uri const &uri, int flags, boolean quiet=false)
{
    if(uri.isEmpty())
    {
        if(!quiet)
        {
            LOG_MSG("Invalid path in material URI \"%s\".") <<uri;
        }
        return false;
    }

    materialschemeid_t schemeId = Materials::parseSchemeName(uri.schemeCStr());
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
static MaterialBind *findMaterialBindForPath(MaterialRepository &matDirectory, String path)
{
    try
    {
        MaterialRepository::Node &node = matDirectory.find(path, PathTree::NoBranch | PathTree::MatchFull);
        return reinterpret_cast<MaterialBind *>(node.userPointer());
    }
    catch(MaterialRepository::NotFoundError const &)
    {} // Ignore this error.
    return 0; // Not found.
}

/// @pre @a uri has already been validated and is well-formed.
static MaterialBind *findMaterialBindForUri(de::Uri const &uri)
{
    materialschemeid_t schemeId = Materials::parseSchemeName(uri.schemeCStr());
    String const &path = uri.path();
    MaterialBind *bind = 0;
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

materialid_t Materials::resolveUri2(de::Uri const &uri, bool quiet)
{
    LOG_AS("Materials::resolveUri");

    if(!initedOk) return NOMATERIALID;

    if(!validateMaterialUri(uri, VMUF_ALLOW_ANY_SCHEME, true /*quiet please*/))
    {
#if _DEBUG
        LOG_WARNING("\"%s\" failed validation, returning NOMATERIALID.") << uri;
#endif
        return NOMATERIALID;
    }

    // Perform the search.
    MaterialBind *bind = findMaterialBindForUri(uri);
    if(bind) return bind->id();

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        LOG_DEBUG("\"%s\" not found, returning NOMATERIALID.") << uri;
    }
    return NOMATERIALID;
}

materialid_t Materials::resolveUri(de::Uri const &uri)
{
    return resolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

String Materials::composePath(materialid_t id)
{
    LOG_AS("Materials::composePath");

    MaterialBind *bind = getMaterialBindForId(id);
    if(bind)
    {
        MaterialRepository::Node &node = bind->directoryNode();
        return node.path().toString();
    }

    LOG_WARNING("Attempted with unbound materialId #%u, returning null-object.") << id;
    return "";
}

de::Uri Materials::composeUri(materialid_t id)
{
    LOG_AS("Materials::composeuri");

    MaterialBind *bind = getMaterialBindForId(id);
    if(bind)
    {
        MaterialRepository::Node &node = bind->directoryNode();
        return composeUriForDirectoryNode(node);
    }

#if _DEBUG
    if(id != NOMATERIALID)
    {
        LOG_WARNING("Attempted with unbound materialId #%u, returning null-object.") << id;
    }
#endif
    return de::Uri();
}

material_t *Materials::newFromDef(ded_material_t *def)
{
    DENG2_ASSERT(def);

    LOG_AS("Materials::newFromDef");

    if(!initedOk || !def->uri) return 0;
    de::Uri &uri = reinterpret_cast<de::Uri &>(*def->uri);

    // We require a properly formed uri.
    if(!validateMaterialUri(uri, 0, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Material \"%s\" from definition %p, ignoring.") << uri << dintptr(def);
        return 0;
    }

    // Have we already created a material for this?
    MaterialBind *bind = findMaterialBindForUri(uri);
    if(bind && bind->material())
    {
        LOG_DEBUG("A Material with uri \"%s\" already exists, returning existing.") << uri;
        return bind->material();
    }

    // Ensure the primary layer has a valid texture reference.
    Texture *tex = 0;
    if(def->layers[0].stageCount.num > 0)
    {
        ded_material_layer_t const &layer = def->layers[0];
        de::Uri *texUri = reinterpret_cast<de::Uri *>(layer.stages[0].texture);
        if(texUri) // Not unused.
        {
            try
            {
                tex = App_Textures()->find(*texUri).texture();
            }
            catch(Textures::NotFoundError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_WARNING(er.asText() + ". Unknown texture \"%s\" in Material \"%s\" (layer %i stage %i), ignoring.")
                    << reinterpret_cast<de::Uri*>(layer.stages[0].texture)
                    << reinterpret_cast<de::Uri*>(def->uri)
                    << 0 << 0;
            }
        }
    }

    if(!tex) return 0;

    // A new Material.
    material_t *mat = linkMaterialToGlobalList(allocMaterial());
    mat->_flags = def->flags;
    mat->_isCustom = tex->flags().testFlag(Texture::Custom);
    mat->_def = def;
    Size2_SetWidthHeight(mat->_size, MAX_OF(0, def->width), MAX_OF(0, def->height));
    mat->_envClass = S_MaterialEnvClassForUri(reinterpret_cast<struct uri_s const *>(&uri));

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

static void pushVariantCacheQueue(material_t &mat, materialvariantspecification_t const &spec, bool smooth)
{
    DENG2_ASSERT(initedOk);

    VariantCacheQueueNode* node = (VariantCacheQueueNode *) M_Malloc(sizeof *node);
    if(!node) Con_Error("Materials::pushVariantCacheQueue: Failed on allocation of %lu bytes for new VariantCacheQueueNode.", (unsigned long) sizeof *node);

    node->mat    = &mat;
    node->spec   = &spec;
    node->smooth = smooth;
    node->next   = variantCacheQueue;
    variantCacheQueue = node;
}

void Materials::precache(material_t &mat, materialvariantspecification_t const &spec,
    bool smooth, bool cacheGroup)
{
    errorIfNotInited("Materials::precache");

    // Don't precache when playing demo.
    if(isDedicated || playback) return;

    // Already in the queue?
    for(VariantCacheQueueNode* node = variantCacheQueue; node; node = node->next)
    {
        if(&mat == node->mat && &spec == node->spec) return;
    }

    pushVariantCacheQueue(mat, spec, smooth);

    if(cacheGroup && Material_IsGroupAnimated(&mat))
    {
        // Material belongs in one or more animgroups; precache the group.
        for(int i = 0; i < numgroups; ++i)
        {
            if(!isInAnimGroup(groups[i], mat)) continue;

            for(int k = 0; k < groups[i].count; ++k)
            {
                precache(*groups[i].frames[k].material, spec, smooth, false /* do not cache groups */);
            }
        }
    }
}

void Materials::ticker(timespan_t time)
{
    // The animation will only progress when the game is not paused.
    if(clientPaused || novideo) return;

    for(MaterialListNode *node = materials; node; node = node->next)
    {
        Material_Ticker(node->mat, time);
    }

    if(DD_IsSharpTick())
    {
        animateAllGroups();
    }
}

static Texture *findTextureByResourceUri(String nameOfScheme, de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return 0;
    try
    {
        return App_Textures()->scheme(nameOfScheme).findByResourceUri(resourceUri).texture();
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

static Texture *findDetailTextureForDef(ded_detailtexture_t const &def)
{
    if(!def.detailTex) return 0;
    return findTextureByResourceUri("Details", reinterpret_cast<de::Uri const &>(*def.detailTex));
}

static Texture *findShinyTextureForDef(ded_reflection_t const &def)
{
    if(!def.shinyMap) return 0;
    return findTextureByResourceUri("Reflections", reinterpret_cast<de::Uri const &>(*def.shinyMap));
}

static Texture *findShinyMaskTextureForDef(ded_reflection_t const &def)
{
    if(!def.maskMap) return 0;
    return findTextureByResourceUri("Masks", reinterpret_cast<de::Uri const &>(*def.maskMap));
}

static void updateMaterialTextureLinks(MaterialBind &mb)
{
    material_t *mat = mb.material();

    // We may need to need to construct and attach the info.
    updateMaterialBindInfo(mb, true /* create if not present */);

    if(!mat) return;

    ded_detailtexture_t const *dtlDef = mb.detailTextureDef();
    Material_SetDetailTexture(mat,  reinterpret_cast<texture_s *>(dtlDef? findDetailTextureForDef(*dtlDef) : NULL));
    Material_SetDetailStrength(mat, (dtlDef? dtlDef->strength : 0));
    Material_SetDetailScale(mat,    (dtlDef? dtlDef->scale : 0));

    ded_reflection_t const *refDef = mb.reflectionDef();
    Material_SetShinyTexture(mat,     reinterpret_cast<texture_s *>(refDef? findShinyTextureForDef(*refDef) : NULL));
    Material_SetShinyMaskTexture(mat, reinterpret_cast<texture_s *>(refDef? findShinyMaskTextureForDef(*refDef) : NULL));
    Material_SetShinyBlendmode(mat,   (refDef? refDef->blendMode : BM_ADD));
    float const black[3] = { 0, 0, 0 };
    Material_SetShinyMinColor(mat,    (refDef? refDef->minColor : black));
    Material_SetShinyStrength(mat,    (refDef? refDef->shininess : 0));
}

void Materials::updateTextureLinks(materialid_t materialId)
{
    MaterialBind *bind = getMaterialBindForId(materialId);
    if(!bind) return;
    updateMaterialTextureLinks(*bind);
}

MaterialSnapshot const *Materials::prepareVariant(MaterialVariant &variant, bool updateSnapshot)
{
    // Acquire the snapshot we are interested in.
    MaterialSnapshot *snapshot = variant.snapshot();
    if(!snapshot)
    {
        // Time to allocate the snapshot.
        snapshot = new MaterialSnapshot(variant);
        variant.attachSnapshot(*snapshot);

        // Update the snapshot right away.
        updateSnapshot = true;
    }
    else if(variant.snapshotPrepareFrame() != frameCount)
    {
        // Time to update the snapshot.
        updateSnapshot = true;
    }

    // We have work to do?
    if(updateSnapshot)
    {
        variant.setSnapshotPrepareFrame(frameCount);
        snapshot->update();
    }

    return snapshot;
}

MaterialSnapshot const *Materials::prepare(material_t &mat, materialvariantspecification_t const &spec,
    bool smooth, bool updateSnapshot)
{
    return prepareVariant(*chooseVariant(mat, spec, smooth, true), updateSnapshot);
}

ded_decor_t const *Materials::decorationDef(material_t *mat)
{
    if(!mat) return 0;
    if(!Material_Prepared(mat))
    {
        prepare(*mat, *Rend_MapSurfaceDiffuseMaterialSpec(), false);
    }
    MaterialBind *mb = getMaterialBindForId(Material_PrimaryBind(mat));
    return mb->decorationDef();
}

ded_ptcgen_t const *Materials::ptcGenDef(material_t *mat)
{
    if(!mat || isDedicated) return 0;
    if(!Material_Prepared(mat))
    {
        prepare(*mat, *Rend_MapSurfaceDiffuseMaterialSpec(), false);
    }
    MaterialBind *mb = getMaterialBindForId(Material_PrimaryBind(mat));
    return mb->ptcGenDef();
}

uint Materials::size()
{
    return materialCount;
}

uint Materials::count(materialschemeid_t schemeId)
{
    if(!VALID_MATERIALSCHEMEID(schemeId) || !size()) return 0;
    return schemeById(schemeId).size();
}

struct materialvariantspecification_s const *Materials::variantSpecificationForContext(
    materialcontext_t mc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha)
{
    errorIfNotInited("Materials::variantSpecificationForContext");
    return getVariantSpecificationForContext(mc, flags, border, tClass, tMap, wrapS, wrapT,
                                             minFilter, magFilter, anisoFilter,
                                             mipmapped, gammaCorrection, noStretch, toAlpha);
}

MaterialVariant *Materials::chooseVariant(material_t &mat,
    materialvariantspecification_t const &spec, bool smoothed, bool canCreate)
{
    DENG_ASSERT(initedOk);

    MaterialVariant* variant = chooseVariant2(mat, spec);
    if(!variant)
    {
        if(!canCreate) return 0;

        MaterialVariant *variant = new MaterialVariant(mat, spec, *Material_Definition(&mat));
        Material_AddVariant(&mat, reinterpret_cast<materialvariant_s *>(variant));
    }

    if(smoothed)
    {
        variant = variant->translationCurrent();
    }

    return variant;
}

static int printVariantInfoWorker(struct materialvariant_s *_variant, void* parameters)
{
    MaterialVariant *variant = reinterpret_cast<MaterialVariant *>(_variant);
    int* variantIdx = (int *)parameters;
    MaterialVariant *next = variant->translationNext();
    int i, layers = Material_LayerCount(&variant->generalCase());
    DENG2_ASSERT(variantIdx);

    Con_Printf("Variant #%i: Spec:%p\n", *variantIdx, (void *) &variant->spec());

    // Print translation info:
    if(Material_HasTranslation(&variant->generalCase()))
    {
        MaterialVariant *cur = variant->translationCurrent();
        float inter = variant->translationPoint();
        QByteArray curPath  = Materials::composeUri(Materials::id(&cur->generalCase())).asText().toUtf8();
        QByteArray nextPath = Materials::composeUri(Materials::id(&next->generalCase())).asText().toUtf8();

        Con_Printf("  Translation: Current:\"%s\" Next:\"%s\" Inter:%f\n",
                   curPath.constData(), nextPath.constData(), inter);
    }

    // Print layer info:
    for(i = 0; i < layers; ++i)
    {
        MaterialVariant::Layer const &l = variant->layer(i);
        de::Uri uri = reinterpret_cast<Texture &>(*l.texture).manifest().composeUri();
        QByteArray path = uri.asText().toUtf8();

        Con_Printf("  #%i: Stage:%i Tics:%i Texture:\"%s\""
                   "\n      Offset: %.2f x %.2f Glow:%.2f\n",
                   i, l.stage, int(l.tics), path.constData(),
                   l.texOrigin[0], l.texOrigin[1], l.glow);
    }

    ++(*variantIdx);

    return 0; // Continue iteration.
}

static void printMaterialInfo(material_t *mat)
{
    QByteArray path = Materials::composeUri(Materials::id(mat)).asText().toUtf8();
    int variantIdx = 0;

    Con_Printf("Material \"%s\" [%p] uid:%u origin:%s"
               "\nSize: %d x %d Layers:%i InGroup:%s Drawable:%s EnvClass:%s"
               "\nDecorated:%s Detailed:%s Glowing:%s Shiny:%s%s SkyMasked:%s\n",
               path.constData(), (void*) mat, Materials::id(mat),
               !Material_IsCustom(mat)     ? "game" : (Material_Definition(mat)->autoGenerated? "addon" : "def"),
               Material_Width(mat), Material_Height(mat), Material_LayerCount(mat),
               Material_IsGroupAnimated(mat)? "yes" : "no",
               Material_IsDrawable(mat)     ? "yes" : "no",
               Material_EnvironmentClass(mat) == MEC_UNKNOWN? "N/A" : S_MaterialEnvClassName(Material_EnvironmentClass(mat)),
               Materials::hasDecorations(mat) ? "yes" : "no",
               Material_DetailTexture(mat)  ? "yes" : "no",
               Material_HasGlow(mat)        ? "yes" : "no",
               Material_ShinyTexture(mat)   ? "yes" : "no",
               Material_ShinyMaskTexture(mat)? "(masked)" : "",
               Material_IsSkyMasked(mat)    ? "yes" : "no");

    Material_IterateVariants(mat, printVariantInfoWorker, (void *)&variantIdx);
}

static void printMaterialOverview(material_t *mat, bool printScheme)
{
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials::size()));
    de::Uri uri = Materials::composeUri(Materials::id(mat));
    QByteArray path = (printScheme? uri.asText().toUtf8() : String(Str_Text(Str_PercentDecode(AutoStr_FromTextStd(uri.pathCStr())))).toUtf8());

    Con_Printf("%-*s %*u %s\n", printScheme? 22 : 14, path.constData(),
               numUidDigits, Materials::id(mat),
               !Material_IsCustom(mat) ? "game" : (Material_Definition(mat)->autoGenerated? "addon" : "def"));
}

/**
 * @todo A horridly inefficent algorithm. This should be implemented in MaterialRepository
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the material search/listing console commands
 * so is not hugely important right now.
 */
static MaterialRepository::Node **collectDirectoryNodes(materialschemeid_t schemeId,
    String like, int *count, MaterialRepository::Node **storage)
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
        MaterialRepository &matDirectory = schemeById(materialschemeid_t(i));

        PathTreeIterator<MaterialRepository> iter(matDirectory.leafNodes());
        while(iter.hasNext())
        {
            MaterialRepository::Node &node = iter.next();
            if(!like.isEmpty())
            {
                String path = node.path();
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

    storage = (MaterialRepository::Node **)M_Malloc(sizeof *storage * (idx+1));
    if(!storage) Con_Error("Materials::collectDirectoryNodes: Failed on allocation of %lu bytes for new MaterialRepository::Node collection.", (unsigned long) (sizeof* storage * (idx+1)));

    return collectDirectoryNodes(schemeId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(void const *a, void const *b)
{
    // Decode paths before determining a lexicographical delta.
    MaterialRepository::Node const &nodeA = **(MaterialRepository::Node const **)a;
    MaterialRepository::Node const &nodeB = **(MaterialRepository::Node const **)b;
    QByteArray pathAUtf8 = nodeA.path().toUtf8();
    QByteArray pathBUtf8 = nodeB.path().toUtf8();
    AutoStr *pathA = Str_PercentDecode(AutoStr_FromTextStd(pathAUtf8.constData()));
    AutoStr *pathB = Str_PercentDecode(AutoStr_FromTextStd(pathBUtf8.constData()));
    return Str_CompareIgnoreCase(pathA, Str_Text(pathB));
}

static size_t printMaterials2(materialschemeid_t schemeId, char const *like,
    bool printSchemeName)
{
    int numFoundDigits, numUidDigits, idx, count = 0;
    MaterialRepository::Node **foundMaterials = collectDirectoryNodes(schemeId, like, &count, NULL);
    MaterialRepository::Node **iter;

    if(!foundMaterials) return 0;

    if(!printSchemeName)
    {
        QByteArray schemeName = Materials::schemeName(schemeId).toUtf8();
        Con_FPrintf(CPF_YELLOW, "Known materials in scheme '%s'", schemeName.constData());
    }
    else // Any scheme.
    {
        Con_FPrintf(CPF_YELLOW, "Known materials");
    }

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Materials::size()));
    Con_Printf(" %*s: %-*s %*s origin\n", numFoundDigits, "idx",
        printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path", numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundMaterials, (size_t)count, sizeof *foundMaterials, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundMaterials; *iter; ++iter)
    {
        MaterialRepository::Node const *node = *iter;
        MaterialBind *mb = reinterpret_cast<MaterialBind *>(node->userPointer());
        material_t *mat = mb->material();
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printMaterialOverview(mat, printSchemeName);
    }

    M_Free(foundMaterials);
    return count;
}

static void printMaterials(materialschemeid_t schemeId, char const *like)
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
        for(int i = MATERIALSCHEME_FIRST; i <= MATERIALSCHEME_LAST; ++i)
        {
            size_t printed = printMaterials2(materialschemeid_t(i), like, false);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }

    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Material" : "Materials");
}

bool Materials::isMaterialInAnimGroup(material_t *mat, int groupNum)
{
    MaterialAnim *group = getAnimGroup(groupNum);
    if(!group) return false;
    return isInAnimGroup(*group, *mat);
}

bool Materials::hasDecorations(material_t *mat)
{
    DENG2_ASSERT(mat);

    if(novideo) return false;

    /// @todo We should not need to prepare to determine this.
    /// Nor should we need to process the group each time. Cache this decision.
    if(decorationDef(mat)) return true;

    if(Material_IsGroupAnimated(mat))
    {
        int g, i, numGroups = animGroupCount();
        for(g = 0; g < numGroups; ++g)
        {
            MaterialAnim *group = &groups[g];

            // Precache groups don't apply.
            if(isPrecacheAnimGroup(g)) continue;

            // Is this material in this group?
            if(!isMaterialInAnimGroup(mat, g)) continue;

            // If any material in this group has decorations then this
            // material is considered to be decorated also.
            for(i = 0; i < group->count; ++i)
            {
                if(Materials::decorationDef(group->frames[i].material)) return true;
            }
        }
    }
    return false;
}

int Materials::animGroupCount()
{
    return numgroups;
}

int Materials::newAnimGroup(int flags)
{
    // Allocating one by one is inefficient, but it doesn't really matter.
    groups = (MaterialAnim *)Z_Realloc(groups, sizeof(*groups) * (numgroups + 1), PU_APPSTATIC);

    // Init the new group.
    MaterialAnim *group = &groups[numgroups];
    std::memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

void Materials::clearAnimGroups()
{
    if(numgroups <= 0) return;

    for(int i = 0; i < numgroups; ++i)
    {
        MaterialAnim *group = &groups[i];
        Z_Free(group->frames);
    }

    Z_Free(groups); groups = 0;
    numgroups = 0;
}

void Materials::addAnimGroupFrame(int groupNum, struct material_s *mat, int tics, int randomTics)
{
    LOG_AS("Materials::addAnimGroupFrame");

    MaterialAnim *group = getAnimGroup(groupNum);
    if(!group)
    {
        LOG_WARNING("Unknown anim group '%i', ignoring.") << groupNum;
        return;
    }

    if(!mat)
    {
        LOG_WARNING("Invalid material (ref=0), ignoring.");
        return;
    }

    // Mark the material as being in an animgroup.
    Material_SetGroupAnimated(mat, true);

    // Allocate a new animframe.
    group->frames = (MaterialAnimFrame *)Z_Realloc(group->frames, sizeof(*group->frames) * ++group->count, PU_APPSTATIC);

    MaterialAnimFrame *frame = &group->frames[group->count - 1];
    frame->material = mat;
    frame->tics     = tics;
    frame->random   = randomTics;
}

bool Materials::isPrecacheAnimGroup(int groupNum)
{
    MaterialAnim *group = getAnimGroup(groupNum);
    if(!group) return false;
    return ((group->flags & AGF_PRECACHE) != 0);
}

typedef struct {
    material_t *current, *next;
} setmaterialtranslationworker_parameters_t;

static int setVariantTranslationWorker(struct materialvariant_s *_variant, void *parameters)
{
    MaterialVariant *variant = reinterpret_cast<MaterialVariant *>(_variant);
    setmaterialtranslationworker_parameters_t *p = (setmaterialtranslationworker_parameters_t *) parameters;
    materialvariantspecification_t const &spec = variant->spec();
    MaterialVariant *current, *next;
    DENG2_ASSERT(p);

    current = Materials::chooseVariant(*p->current, spec, false, true/*create if necessary*/);
    next    = Materials::chooseVariant(*p->next,    spec, false, true/*create if necessary*/);
    variant->setTranslation(current, next);
    return 0; // Continue iteration.
}

static int setVariantTranslationPointWorker(struct materialvariant_s *variant, void* parameters)
{
    float* interPtr = (float*)parameters;
    DENG2_ASSERT(interPtr);

    reinterpret_cast<MaterialVariant *>(variant)->setTranslationPoint(*interPtr);
    return 0; // Continue iteration.
}

static void animateGroup(MaterialAnim *group)
{
    // The Precache groups are not intended for animation.
    if((group->flags & AGF_PRECACHE) || !group->count) return;

    if(--group->timer <= 0)
    {
        // Advance to next frame.
        group->index = (group->index + 1) % group->count;
        int timer = group->frames[group->index].tics;

        if(group->frames[group->index].random)
        {
            timer += int(RNG_RandByte()) % (group->frames[group->index].random + 1);
        }
        group->timer = group->maxTimer = timer;

        // Update translations.
        for(int i = 0; i < group->count; ++i)
        {
            material_t *real = group->frames[i].material;
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
    for(int i = 0; i < group->count; ++i)
    {
        material_t *mat = group->frames[i].material;
        float interp;

        /*ded_material_t *def = Material_Definition(mat);
        if(def && def->layers[0].stageCount.num > 1)
        {
            de::Uri *texUri = reinterpret_cast<de::Uri *>(def->layers[0].stages[0].texture)
            if(texUri && Textures::resolveUri(*texUri))
                continue; // Animated elsewhere.
        }*/

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

static void animateAllGroups()
{
    for(int i = 0; i < numgroups; ++i)
    {
        animateGroup(&groups[i]);
    }
}

static int resetVariantGroupAnimWorker(struct materialvariant_s *mat, void* /*parameters*/)
{
    reinterpret_cast<MaterialVariant *>(mat)->resetAnim();
    return 0; // Continue iteration.
}

void Materials::resetAnimGroups()
{
    for(MaterialListNode *node = materials; node; node = node->next)
    {
        Material_IterateVariants(node->mat, resetVariantGroupAnimWorker, NULL);
    }

    MaterialAnim* group = groups;
    for(int i = 0; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count) continue;

        group->timer = 0;
        group->maxTimer = 1;

        // The anim group should start from the first step using the
        // correct timings.
        group->index = group->count - 1;
    }

    // This'll get every group started on the first step.
    animateAllGroups();
}

} // namespace de

D_CMD(ListMaterials)
{
    DENG2_UNUSED(src);

    if(!de::Materials::size())
    {
        Con_Message("There are currently no materials defined/loaded.\n");
        return true;
    }

    materialschemeid_t schemeId = MS_ANY;
    char const *like = 0;
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

    de::printMaterials(schemeId, like);

    return true;
}

D_CMD(InspectMaterial)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc);

    // Path is assumed to be in a human-friendly, non-encoded representation.
    de::Uri search(Str_Text(Str_PercentEncode(AutoStr_FromTextStd(argv[1]))), RC_NULL);

    if(!search.scheme().isEmpty())
    {
        materialschemeid_t schemeId = DD_ParseMaterialSchemeName(search.schemeCStr());
        if(!VALID_MATERIALSCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", search.schemeCStr());
            return false;
        }
    }

    material_t *mat = de::Materials::toMaterial(de::Materials::resolveUri(search));
    if(mat)
    {
        de::printMaterialInfo(mat);
    }
    else
    {
        AutoStr *path = Uri_ToString(reinterpret_cast<struct uri_s *>(&search));
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
        de::MaterialRepository& matDirectory = de::schemeById(schemeId);
        uint size = matDirectory.size();

        QByteArray schemeName = de::Materials::schemeName(schemeId).toUtf8();
        Con_Printf("Scheme: %s (%u %s)\n", schemeName.constData(), size, size==1? "material":"materials");
        matDirectory.debugPrintHashDistribution();
        matDirectory.debugPrint();
    }
    return true;
}
#endif

/*
 * C wrapper API:
 */

void Materials_Init()
{
    de::Materials::init();
}

void Materials_Shutdown()
{
    de::Materials::shutdown();
}

void Materials_Ticker(timespan_t elapsed)
{
    de::Materials::ticker(elapsed);
}

uint Materials_Size()
{
    return de::Materials::size();
}

materialid_t Materials_Id(material_t *material)
{
    return de::Materials::id(material);
}

material_t *Materials_ToMaterial(materialid_t materialId)
{
    return de::Materials::toMaterial(materialId);
}

struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    de::Uri uri = de::Materials::composeUri(materialId);
    return Uri_Dup(reinterpret_cast<uri_s *>(&uri));
}

boolean Materials_HasDecorations(material_t *material)
{
    return de::Materials::hasDecorations(material);
}

ded_ptcgen_t const *Materials_PtcGenDef(material_t *material)
{
    return de::Materials::ptcGenDef(material);
}

boolean Materials_IsMaterialInAnimGroup(material_t *material, int animGroupNum)
{
    return de::Materials::isMaterialInAnimGroup(material, animGroupNum);
}

materialid_t Materials_ResolveUri2(struct uri_s const *uri, boolean quiet)
{
    return de::Materials::resolveUri2(*reinterpret_cast<de::Uri const *>(uri), CPP_BOOL(quiet));
}

materialid_t Materials_ResolveUri(struct uri_s const *uri)
{
    return Materials_ResolveUri2(uri, !(verbose >= 1));
}

materialid_t Materials_ResolveUriCString2(char const *uriCString, boolean quiet)
{
    if(uriCString && uriCString[0])
    {
        return de::Materials::resolveUri2(de::Uri(uriCString, RC_NULL), CPP_BOOL(quiet));
    }
    return NOMATERIALID;
}

/// @note Part of the Doomsday public API.
materialid_t Materials_ResolveUriCString(char const *path)
{
    return Materials_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

int Materials_AnimGroupCount()
{
    return de::Materials::animGroupCount();
}

boolean Materials_IsPrecacheAnimGroup(int animGroupNum)
{
    return de::Materials::isPrecacheAnimGroup(animGroupNum);
}

namespace de {

MaterialBind &MaterialBind::setMaterial(material_t *newMaterial)
{
    if(asocMaterial != newMaterial)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        MaterialBindInfo *detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);

        // Associate with the new Material.
        asocMaterial = newMaterial;
    }
    return *this;
}

MaterialBind &MaterialBind::attachInfo(MaterialBindInfo &info)
{
    LOG_AS("MaterialBind::attachInfo");
    if(extInfo)
    {
#if _DEBUG
        de::Uri uri = Materials::composeUri(guid);
        LOG_DEBUG("Info already present for \"%s\", will replace.") << uri;
#endif
        M_Free(extInfo);
    }
    extInfo = &info;
    return *this;
}

MaterialBindInfo *MaterialBind::detachInfo()
{
    MaterialBindInfo *retInfo = extInfo;
    extInfo = 0;
    return retInfo;
}

ded_detailtexture_t *MaterialBind::detailTextureDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->detailtextureDefs[Material_Prepared(asocMaterial)-1];
}

ded_decor_t *MaterialBind::decorationDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->decorationDefs[Material_Prepared(asocMaterial)-1];
}

ded_ptcgen_t *MaterialBind::ptcGenDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->ptcgenDefs[Material_Prepared(asocMaterial)-1];
}

ded_reflection_t *MaterialBind::reflectionDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->reflectionDefs[Material_Prepared(asocMaterial)-1];
}

} // namespace de
