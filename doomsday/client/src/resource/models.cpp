/** @file models.cpp  3D model resource management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_platform.h"
#include "resource/models.h"

#include "dd_main.h" // App_FileSystem()
#include "con_bar.h"
#include "con_main.h"
#include "def_main.h"

#include "filesys/fs_main.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"

#  include "render/billboard.h" // Rend_SpriteMaterialSpec()
#  include "render/r_main.h" // frameTimePos, precacheSkins
#  include "render/rend_model.h"
#endif

#include "world/p_object.h"

#include <de/App>
#include <de/ByteOrder>
#include <de/NativePath>
#include <de/StringPool>

#include <de/mathutil.h> // M_CycleIntoRange()
#include <de/memory.h>
#include <cmath>
#include <cstring> // memset

using namespace de;

byte useModels = true;
float rModelAspectMod = 1 / 1.2f; //.833334f;

typedef std::vector<ModelDef> ModelDefs;
static ModelDefs modefs;
static std::vector<int> stateModefs; // Index to the modefs array.

static StringPool *modelRepository; // Owns Model instances.

static Model *loadModel(String path);
static bool recogniseDmd(de::FileHandle &file);
static bool recogniseMd2(de::FileHandle &file);
static void loadDmd(de::FileHandle &file, Model &mdl);
static void loadMd2(de::FileHandle &file, Model &mdl);

static ModelDef *modelDefForState(int stateIndex, int select = 0)
{
    DENG2_ASSERT(stateIndex >= 0);
    DENG2_ASSERT(stateIndex < int(stateModefs.size()));

    if(stateModefs[stateIndex] < 0) return 0;

    DENG2_ASSERT(stateModefs[stateIndex] >= 0);
    DENG2_ASSERT(stateModefs[stateIndex] < int(modefs.size()));

    ModelDef *def = &modefs[stateModefs[stateIndex]];
    if(select)
    {
        // Choose the correct selector, or selector zero if the given one not available.
        int const mosel = select & DDMOBJ_SELECTOR_MASK;
        for(ModelDef *it = def; it; it = it->selectNext)
        {
            if(it->select == mosel)
            {
                return it;
            }
        }
    }

    return def;
}

static Model *modelForId(modelid_t modelId, bool canCreate = false)
{
    DENG2_ASSERT(modelRepository);
    Model *mdl = reinterpret_cast<Model*>(modelRepository->userPointer(modelId));
    if(!mdl && canCreate)
    {
        // Allocate a new model_t.
        mdl = new Model(modelId);
        modelRepository->setUserPointer(modelId, mdl);
    }
    return mdl;
}

static inline String const &findModelPath(modelid_t id)
{
    return modelRepository->stringRef(id);
}

Model *Models_Model(modelid_t id)
{
    return modelForId(id);
}

ModelDef *Models_ModelDef(int index)
{
    if(index >= 0 && index < int(modefs.size()))
    {
        return &modefs[index];
    }
    return 0;
}

ModelDef *Models_ModelDef(char const *id)
{
    if(!id || !id[0]) return 0;

    for(uint i = 0; i < modefs.size(); ++i)
    {
        if(!strcmp(modefs[i].id, id))
        {
            return &modefs[i];
        }
    }
    return 0;
}

float Models_ModelDefForMobj(mobj_t const *mo, ModelDef **modef, ModelDef **nextmodef)
{
    // On the client it is possible that we don't know the mobj's state.
    if(!mo->state) return -1;

    state_t &st = *mo->state;

    // By default there are no models.
    *nextmodef = NULL;
    *modef = modelDefForState(&st - states, mo->selector);
    if(!*modef) return -1; // No model available.

    float interp = -1;

    // World time animation?
    bool worldTime = false;
    if((*modef)->flags & MFF_WORLD_TIME_ANIM)
    {
        float duration = (*modef)->interRange[0];
        float offset   = (*modef)->interRange[1];

        // Validate/modify the values.
        if(duration == 0) duration = 1;

        if(offset == -1)
        {
            offset = M_CycleIntoRange(MOBJ_TO_ID(mo), duration);
        }

        interp = M_CycleIntoRange(App_World().time() / duration + offset, 1);
        worldTime = true;
    }
    else
    {
        // Calculate the currently applicable intermark.
        interp = 1.0f - (mo->tics - frameTimePos) / float( st.tics );
    }

/*#if _DEBUG
    if(mo->dPlayer)
    {
        qDebug() << "itp:" << interp << " mot:" << mo->tics << " stt:" << st.tics;
    }
#endif*/

    // First find the modef for the interpoint. Intermark is 'stronger' than interrange.

    // Scan interlinks.
    while((*modef)->interNext && (*modef)->interNext->interMark <= interp)
    {
        *modef = (*modef)->interNext;
    }

    if(!worldTime)
    {
        // Scale to the modeldef's interpolation range.
        interp = (*modef)->interRange[0] + interp * ((*modef)->interRange[1] -
                                                     (*modef)->interRange[0]);
    }

    // What would be the next model? Check interlinks first.
    if((*modef)->interNext)
    {
        *nextmodef = (*modef)->interNext;
    }
    else if(worldTime)
    {
        *nextmodef = modelDefForState(&st - states, mo->selector);
    }
    else if(st.nextState > 0) // Check next state.
    {
        // Find the appropriate state based on interrange.
        state_t *it = states + st.nextState;
        bool foundNext = false;
        if((*modef)->interRange[1] < 1)
        {
            // Current modef doesn't interpolate to the end, find the proper destination
            // modef (it isn't just the next one). Scan the states that follow (and
            // interlinks of each).
            bool stopScan = false;
            int max = 20; // Let's not be here forever...
            while(!stopScan)
            {
                if(!((!modelDefForState(it - states) ||
                      modelDefForState(it - states, mo->selector)->interRange[0] > 0) &&
                     it->nextState > 0))
                {
                    stopScan = true;
                }
                else
                {
                    // Scan interlinks, then go to the next state.
                    ModelDef *mdit;
                    if((mdit = modelDefForState(it - states, mo->selector)) && mdit->interNext)
                    {
                        forever
                        {
                            mdit = mdit->interNext;
                            if(mdit)
                            {
                                if(mdit->interRange[0] <= 0) // A new beginning?
                                {
                                    *nextmodef = mdit;
                                    foundNext = true;
                                }
                            }

                            if(!mdit || foundNext)
                            {
                                break;
                            }
                        }
                    }

                    if(foundNext)
                    {
                        stopScan = true;
                    }
                    else
                    {
                        it = states + it->nextState;
                    }
                }

                if(max-- <= 0)
                    stopScan = true;
            }
            // @todo What about max == -1? What should 'it' be then?
        }

        if(!foundNext)
        {
            *nextmodef = modelDefForState(it - states, mo->selector);
        }
    }

    // Is this group disabled?
    if(useModels >= 2 && (*modef)->group & useModels)
    {
        *modef = *nextmodef = 0;
        return -1;
    }

    return interp;
}

/**
 * Scales the given model so that it'll be 'destHeight' units tall. Measurements
 * are based on submodel zero. Scale is applied uniformly.
 */
static void scaleModel(ModelDef &mf, float destHeight, float offset)
{
    if(!mf.subCount()) return;

    submodeldef_t &smf = mf.subModelDef(0);

    // No model to scale?
    if(!smf.modelId) return;

    // Find the top and bottom heights.
    float top, bottom;
    float height = Models_Model(smf.modelId)->frame(smf.frame).horizontalRange(&top, &bottom);
    if(!height) height = 1;

    float scale = destHeight / height;

    mf.scale    = Vector3f(scale, scale, scale);
    mf.offset.y = -bottom * scale + offset;
}

static void scaleModelToSprite(ModelDef &mf, Sprite *sprite)
{
    if(!sprite) return;
    if(!sprite->hasViewAngle(0)) return;

    MaterialSnapshot const &ms = sprite->viewAngle(0).material->prepare(Rend_SpriteMaterialSpec());
    Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
    int off = de::max(0, -tex.origin().y - ms.height());
    scaleModel(mf, ms.height(), off);
}

static float calcModelVisualRadius(ModelDef *def)
{
    if(!def || !def->subModelId(0)) return 0;

    // Use the first frame bounds.
    Vector3f min, max;
    float maxRadius = 0;
    for(uint i = 0; i < def->subCount(); ++i)
    {
        if(!def->subModelId(i)) break;

        SubmodelDef &sub = def->subModelDef(i);

        Models_Model(sub.modelId)->frame(sub.frame).bounds(min, max);

        // Half the distance from bottom left to top right.
        float radius = (def->scale.x * (max.x - min.x) +
                        def->scale.z * (max.z - min.z)) / 3.5f;
        if(radius > maxRadius)
        {
            maxRadius = radius;
        }
    }

    return maxRadius;
}

/**
 * Create a new modeldef or find an existing one. This is for ID'd models.
 */
static ModelDef *getModelDefWithId(char const *id)
{
    // ID defined?
    if(!id || !id[0]) return 0;

    // First try to find an existing modef.
    ModelDef *md = Models_ModelDef(id);
    if(md) return md;

    // Get a new entry.
    modefs.push_back(ModelDef(id));
    return &modefs.back();
}

/**
 * Create a new modeldef or find an existing one. There can be only one model
 * definition associated with a state/intermark pair.
 */
static ModelDef *getModelDef(int state, float interMark, int select)
{
    // Is this a valid state?
    if(state < 0 || state >= countStates.num) return 0;

    // First try to find an existing modef.
    for(uint i = 0; i < modefs.size(); ++i)
    {
        if(modefs[i].state == &states[state] &&
           modefs[i].interMark == interMark && modefs[i].select == select)
        {
            // Models are loaded in reverse order; this one already has
            // a model.
            return NULL;
        }
    }

    modefs.push_back(ModelDef());

    // Set initial data.
    ModelDef *md = &modefs.back();
    md->state     = &states[state];
    md->interMark = interMark;
    md->select    = select;
    return md;
}

static String findSkinPath(Path const &skinPath, Path const &modelFilePath)
{
    DENG2_ASSERT(!skinPath.isEmpty());

    // Try the "first choice" directory first.
    if(!modelFilePath.isEmpty())
    {
        // The "first choice" directory is that in which the model file resides.
        try
        {
            return App_FileSystem().findPath(de::Uri("Models", modelFilePath.toString().fileNamePath() / skinPath.fileName()),
                                             RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        }
        catch(FS1::NotFoundError const &)
        {} // Ignore this error.
    }

    /// @throws FS1::NotFoundError if no resource was found.
    return App_FileSystem().findPath(de::Uri("Models", skinPath), RLF_DEFAULT,
                                     App_ResourceClass(RC_GRAPHIC));
}

/**
 * Allocate room for a new skin file name.
 */
static short defineSkinAndAddToModelIndex(Model &mdl, Path const &skinPath)
{
    if(Texture *tex = App_ResourceSystem().defineTexture("ModelSkins", de::Uri(skinPath)))
    {
        // A duplicate? (return existing skin number)
        for(int i = 0; i < mdl.skinCount(); ++i)
        {
            if(mdl.skin(i).texture == tex)
                return i;
        }

        // Add this new skin.
        mdl.newSkin(skinPath.toString()).texture = tex;
        return mdl.skinCount() - 1;
    }

    return -1;
}

/**
 * Creates a modeldef based on the given DED info. A pretty straightforward
 * operation. No interlinks are set yet. Autoscaling is done and the scale
 * factors set appropriately. After this has been called for all available
 * Model DEDs, each State that has a model will have a pointer to the one
 * with the smallest intermark (start of a chain).
 */
static void setupModel(ded_model_t &def)
{
    LOG_AS("setupModel");

    int const modelScopeFlags = def.flags | defs.modelFlags;
    int const statenum = Def_GetStateNum(def.state);

    // Is this an ID'd model?
    ModelDef *modef = getModelDefWithId(def.id);
    if(!modef)
    {
        // No, normal State-model.
        if(statenum < 0) return;

        modef = getModelDef(statenum + def.off, def.interMark, def.selector);
        if(!modef) return; // Can't get a modef, quit!
    }

    // Init modef info (state & intermark already set).
    modef->def       = &def;
    modef->group     = def.group;
    modef->flags     = modelScopeFlags;
    modef->offset    = def.offset;
    modef->offset.y += defs.modelOffset; // Common Y axis offset.
    modef->scale     = def.scale;
    modef->scale.y  *= defs.modelScale;  // Common Y axis scaling.
    modef->resize    = def.resize;
    modef->skinTics  = de::max(def.skinTics, 1);
    for(int i = 0; i < 2; ++i)
    {
        modef->interRange[i] = def.interRange[i];
    }

    // Submodels.
    modef->clearSubs();
    for(uint i = 0; i < def.subCount(); ++i)
    {
        ded_submodel_t const *subdef = &def.sub(i);
        submodeldef_t *sub = modef->addSub();

        sub->modelId = 0;

        if(!subdef->filename) continue;
        de::Uri const &searchPath = reinterpret_cast<de::Uri &>(*subdef->filename);
        if(searchPath.isEmpty()) continue;

        try
        {
            String foundPath = App_FileSystem().findPath(searchPath, RLF_DEFAULT,
                                                         App_ResourceClass(RC_MODEL));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            Model *mdl = loadModel(foundPath);
            if(!mdl) continue;

            sub->modelId    = mdl->modelId();
            sub->frame      = mdl->frameNumber(subdef->frame);
            if(sub->frame < 0) sub->frame = 0;
            sub->frameRange = de::max(1, subdef->frameRange); // Frame range must always be greater than zero.

            sub->alpha      = byte(255 - subdef->alpha * 255);
            sub->blendMode  = subdef->blendMode;

            // Submodel-specific flags cancel out model-scope flags!
            sub->setFlags(modelScopeFlags ^ subdef->flags);

            // Flags may override alpha and/or blendmode.
            if(sub->testFlag(MFF_BRIGHTSHADOW))
            {
                sub->alpha = byte(256 * .80f);
                sub->blendMode = BM_ADD;
            }
            else if(sub->testFlag(MFF_BRIGHTSHADOW2))
            {
                sub->blendMode = BM_ADD;
            }
            else if(sub->testFlag(MFF_DARKSHADOW))
            {
                sub->blendMode = BM_DARK;
            }
            else if(sub->testFlag(MFF_SHADOW2))
            {
                sub->alpha = byte(256 * .2f);
            }
            else if(sub->testFlag(MFF_SHADOW1))
            {
                sub->alpha = byte(256 * .62f);
            }

            // Extra blendmodes:
            if(sub->testFlag(MFF_REVERSE_SUBTRACT))
            {
                sub->blendMode = BM_REVERSE_SUBTRACT;
            }
            else if(sub->testFlag(MFF_SUBTRACT))
            {
                sub->blendMode = BM_SUBTRACT;
            }

            if(subdef->skinFilename && !Uri_IsEmpty(subdef->skinFilename))
            {
                // A specific file name has been given for the skin.
                String const &skinFilePath  = reinterpret_cast<de::Uri &>(*subdef->skinFilename).path();
                String const &modelFilePath = findModelPath(sub->modelId);
                try
                {
                    Path foundResourcePath(findSkinPath(skinFilePath, modelFilePath));

                    sub->skin = defineSkinAndAddToModelIndex(*mdl, foundResourcePath);
                }
                catch(FS1::NotFoundError const&)
                {
                    LOG_WARNING("Failed to locate skin \"%s\" for model \"%s\", ignoring.")
                        << reinterpret_cast<de::Uri &>(*subdef->skinFilename) << NativePath(modelFilePath).pretty();
                }
            }
            else
            {
                sub->skin = subdef->skin;
            }

            // Skin range must always be greater than zero.
            sub->skinRange = de::max(subdef->skinRange, 1);

            // Offset within the model.
            sub->offset = subdef->offset;

            if(subdef->shinySkin && !Uri_IsEmpty(subdef->shinySkin))
            {
                String const &skinFilePath  = reinterpret_cast<de::Uri &>(*subdef->shinySkin).path();
                String const &modelFilePath = findModelPath(sub->modelId);
                try
                {
                    de::Uri foundResourceUri(Path(findSkinPath(skinFilePath, modelFilePath)));

                    sub->shinySkin = App_ResourceSystem().defineTexture("ModelReflectionSkins", foundResourceUri);
                }
                catch(FS1::NotFoundError const &)
                {
                    LOG_WARNING("Failed to locate skin \"%s\" for model \"%s\", ignoring.")
                        << skinFilePath << NativePath(modelFilePath).pretty();
                }
            }
            else
            {
                sub->shinySkin = 0;
            }

            // Should we allow texture compression with this model?
            if(sub->testFlag(MFF_NO_TEXCOMP))
            {
                // All skins of this model will no longer use compression.
                mdl->setFlags(Model::NoTextureCompression);
            }
        }
        catch(FS1::NotFoundError const &)
        {
            LOG_WARNING("Failed to locate \"%s\", ignoring.") << searchPath;
        }
    }

    // Do scaling, if necessary.
    if(modef->resize)
    {
        scaleModel(*modef, modef->resize, modef->offset.y);
    }
    else if(modef->state && modef->testSubFlag(0, MFF_AUTOSCALE))
    {
        spritenum_t sprNum = Def_GetSpriteNum(def.sprite.id);
        int sprFrame       = def.spriteFrame;

        if(sprNum < 0)
        {
            // No sprite ID given.
            sprNum   = modef->state->sprite;
            sprFrame = modef->state->frame;
        }

        if(Sprite *sprite = App_ResourceSystem().spritePtr(sprNum, sprFrame))
        {
            scaleModelToSprite(*modef, sprite);
        }
    }

    if(modef->state)
    {
        int stateNum = modef->state - states;

        // Associate this modeldef with its state.
        if(stateModefs[stateNum] < 0)
        {
            // No modef; use this.
            stateModefs[stateNum] = Models_IndexOf(modef);
        }
        else
        {
            // Must check intermark; smallest wins!
            ModelDef *other = modelDefForState(stateNum);

            if((modef->interMark <= other->interMark && // Should never be ==
                modef->select == other->select) || modef->select < other->select) // Smallest selector?
            {
                stateModefs[stateNum] = Models_IndexOf(modef);
            }
        }
    }

    // Calculate the particle offset for each submodel.
    Vector3f min, max;
    for(uint i = 0; i < modef->subCount(); ++i)
    {
        SubmodelDef *sub = &modef->subModelDef(i);
        if(sub->modelId && sub->frame >= 0)
        {
            Models_Model(sub->modelId)->frame(sub->frame).bounds(min, max);
            modef->setParticleOffset(i, ((max + min) / 2 + sub->offset) * modef->scale + modef->offset);
        }
    }

    // Calculate visual radius for shadows.
    /// @todo fixme: use a separate property.
    /*if(def.shadowRadius)
    {
        modef->visualRadius = def.shadowRadius;
    }
    else*/
    {
        modef->visualRadius = calcModelVisualRadius(modef);
    }
}

static int destroyModelInRepository(StringPool::Id id, void * /*context*/)
{
    if(Model *model = reinterpret_cast<Model *>(modelRepository->userPointer(id)))
    {
        modelRepository->setUserPointer(id, 0);
        delete model;
    }
    return 0;
}

static void clearModelList()
{
    if(!modelRepository) return;

    modelRepository->iterate(destroyModelInRepository, 0);
}

void Models_Init()
{
    // Dedicated servers do nothing with models.
    if(isDedicated) return;
    if(CommandLine_Check("-nomd2")) return;

    LOG_VERBOSE("Initializing Models...");
    Time begunAt;

    modelRepository = new StringPool();

    clearModelList();
    modefs.clear();

    // There can't be more modeldefs than there are DED Models.
    for(uint i = 0; i < defs.models.size(); ++i)
    {
        modefs.push_back(ModelDef());
    }

    // Clear the modef pointers of all States.
    stateModefs.clear();
    for(int i = 0; i < countStates.num; ++i)
    {
        stateModefs.push_back(-1);
    }

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for(int i = int(defs.models.size()) - 1; i >= 0; --i)
    {
        if(!(i % 100))
        {
            // This may take a while, so keep updating the progress.
            Con_SetProgress(130 + 70*(defs.models.size() - i)/defs.models.size());
        }

        setupModel(defs.models[i]);
    }

    // Create interlinks. Note that the order in which the defs were loaded
    // is important. We want to allow "patch" definitions, right?

    // For each modeldef we will find the "next" def.
    for(int i = int(modefs.size()) - 1; i >= 0; --i)
    {
        ModelDef *me = &modefs[i];

        float minmark = 2; // max = 1, so this is "out of bounds".

        ModelDef *closest = 0;
        for(int k = int(modefs.size()) - 1; k >= 0; --k)
        {
            ModelDef *other = &modefs[k];

            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->interMark > me->interMark &&
               other->interMark < minmark)
            {
                minmark = other->interMark;
                closest = other;
            }
        }

        me->interNext = closest;
    }

    // Create selectlinks.
    for(int i = int(modefs.size()) - 1; i >= 0; --i)
    {
        ModelDef *me = &modefs[i];

        int minsel = DDMAXINT;

        ModelDef *closest = 0;

        // Start scanning from the next definition.
        for(int k = int(modefs.size()) - 1; k >= 0; --k)
        {
            ModelDef *other = &modefs[k];

            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->select > me->select && other->select < minsel &&
               other->interMark >= me->interMark)
            {
                minsel = other->select;
                closest = other;
            }
        }

        me->selectNext = closest;
    }

    LOG_INFO(String("Models_Init: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void Models_Shutdown()
{
    /// @todo Why only centralized memory deallocation? Bad (lazy) design...
    modefs.clear();
    stateModefs.clear();

    clearModelList();

    if(modelRepository)
    {
        delete modelRepository; modelRepository = 0;
    }
}

int Models_IndexOf(ModelDef const *modelDef)
{
    int index = int(modelDef - &modefs[0]);
    if(index >= 0 && index < int(modefs.size()))
    {
        return index;
    }
    return -1;
}

void Models_SetFrame(ModelDef &modef, int frame)
{
    for(uint i = 0; i < modef.subCount(); ++i)
    {
        submodeldef_t &subdef = modef.subModelDef(i);
        if(subdef.modelId == NOMODELID) continue;

        // Modify the modeldef itself: set the current frame.
        Model *mdl = Models_Model(subdef.modelId);
        DENG2_ASSERT(mdl != 0);
        subdef.frame = frame % mdl->frameCount();
    }
}

void Models_Cache(ModelDef *modelDef)
{
    if(!modelDef) return;

    for(uint sub = 0; sub < modelDef->subCount(); ++sub)
    {
        submodeldef_t &subdef = modelDef->subModelDef(sub);
        Model *mdl = Models_Model(subdef.modelId);
        if(!mdl) continue;

        // Load all skins.
        foreach(ModelSkin const &skin, mdl->skins())
        {
            if(Texture *tex = skin.texture)
            {
                tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl->flags().testFlag(Model::NoTextureCompression)));
            }
        }

        // Load the shiny skin too.
        if(Texture *tex = subdef.shinySkin)
        {
            tex->prepareVariant(Rend_ModelShinyTextureSpec());
        }
    }
}

int Models_CacheForMobj(thinker_t *th, void * /*context*/)
{
    if(!(useModels && precacheSkins)) return true;

    mobj_t *mo = (mobj_t *) th;

    // Check through all the model definitions.
    for(uint i = 0; i < modefs.size(); ++i)
    {
        ModelDef *modef = &modefs[i];

        if(!modef->state) continue;
        if(mo->type < 0 || mo->type >= defs.count.mobjs.num) continue; // Hmm?
        if(stateOwners[modef->state - states] != &mobjInfo[mo->type]) continue;

        Models_Cache(modef);
    }

    return false; // Used as iterator.
}

#undef Models_CacheForState
DENG_EXTERN_C void Models_CacheForState(int stateIndex)
{
    if(!useModels) return;
    if(stateIndex <= 0 || stateIndex >= defs.count.states.num) return;
    if(stateModefs[stateIndex] < 0) return;

    Models_Cache(modelDefForState(stateIndex));
}

// -----------------------------------------------------------------------------

#define NUMVERTEXNORMALS 162
static float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

/**
 * Calculate vertex normals. Only with -renorm.
 */
#if 0 // unused atm.
static void rebuildNormals(model_t &mdl)
{
    // Renormalizing?
    if(!CommandLine_Check("-renorm")) return;

    int const tris  = mdl.lodInfo[0].numTriangles;
    int const verts = mdl.info.numVertices;

    vector_t* normals = (vector_t*) Z_Malloc(sizeof(vector_t) * tris, PU_APPSTATIC, 0);
    vector_t norm;
    int cnt;

    // Calculate the normal for each vertex.
    for(int i = 0; i < mdl.info.numFrames; ++i)
    {
        model_vertex_t* list = mdl.frames[i].vertices;

        for(int k = 0; k < tris; ++k)
        {
            dmd_triangle_t const& tri = mdl.lods[0].triangles[k];

            // First calculate surface normals, combine them to vertex ones.
            V3f_PointCrossProduct(normals[k].pos,
                                  list[tri.vertexIndices[0]].vertex,
                                  list[tri.vertexIndices[2]].vertex,
                                  list[tri.vertexIndices[1]].vertex);
            V3f_Normalize(normals[k].pos);
        }

        for(int k = 0; k < verts; ++k)
        {
            memset(&norm, 0, sizeof(norm));
            cnt = 0;

            for(int j = 0; j < tris; ++j)
            {
                dmd_triangle_t const& tri = mdl.lods[0].triangles[j];

                for(int n = 0; n < 3; ++n)
                {
                    if(tri.vertexIndices[n] == k)
                    {
                        cnt++;
                        for(int n = 0; n < 3; ++n)
                        {
                            norm.pos[n] += normals[j].pos[n];
                        }
                        break;
                    }
                }
            }

            if(!cnt) continue; // Impossible...

            // Calculate the average.
            for(int n = 0; n < 3; ++n)
            {
                norm.pos[n] /= cnt;
            }

            // Normalize it.
            V3f_Normalize(norm.pos);
            memcpy(list[k].normal, norm.pos, sizeof(norm.pos));
        }
    }

    Z_Free(normals);
}
#endif

static void defineAllSkins(Model &mdl)
{
    String const &modelFilePath = findModelPath(mdl.modelId());

    int numFoundSkins = 0;
    for(int i = 0; i < mdl.skinCount(); ++i)
    {
        ModelSkin &skin = mdl.skin(i);

        if(skin.name.isEmpty())
            continue;

        try
        {
            de::Uri foundResourceUri(Path(findSkinPath(skin.name, modelFilePath)));

            skin.texture = App_ResourceSystem().defineTexture("ModelSkins", foundResourceUri);

            // We have found one more skin for this model.
            numFoundSkins += 1;
        }
        catch(FS1::NotFoundError const&)
        {
            LOG_WARNING("Failed to locate \"%s\" (#%i) for model \"%s\", ignoring.")
                << skin.name << i << NativePath(modelFilePath).pretty();
        }
    }

    if(!numFoundSkins)
    {
        // Lastly try a skin named similarly to the model in the same directory.
        de::Uri searchPath(modelFilePath.fileNamePath() / modelFilePath.fileNameWithoutExtension(), RC_GRAPHIC);

        try
        {
            String foundPath = App_FileSystem().findPath(searchPath, RLF_DEFAULT,
                                                         App_ResourceClass(RC_GRAPHIC));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            defineSkinAndAddToModelIndex(mdl, foundPath);
            // We have found one more skin for this model.
            numFoundSkins = 1;

            LOG_INFO("Assigned fallback skin \"%s\" to index #0 for model \"%s\".")
                << NativePath(foundPath).pretty()
                << NativePath(modelFilePath).pretty();
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if(!numFoundSkins)
    {
        LOG_WARNING("Failed to locate a skin for model \"%s\". This model will be rendered without a skin.")
            << NativePath(modelFilePath).pretty();
    }
}

static Model *interpretDmd(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseDmd(hndl))
    {
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\" as a DMD model.");
        Model *mdl = modelForId(modelId, true/*create*/);
        loadDmd(hndl, *mdl);
        return mdl;
    }
    return 0;
}

static Model *interpretMd2(de::FileHandle &hndl, String path, modelid_t modelId)
{
    if(recogniseMd2(hndl))
    {
        LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\" as a MD2 model.");
        Model *mdl = modelForId(modelId, true/*create*/);
        loadMd2(hndl, *mdl);
        return mdl;
    }
    return 0;
}

struct ModelFileType
{
    /// Symbolic name of the resource type.
    String const name;

    /// Known file extension.
    String const ext;

    Model *(*interpretFunc)(de::FileHandle &hndl, String path, modelid_t modelId);
};

// Model resource types.
static ModelFileType const modelTypes[] = {
    { "DMD",    ".dmd",     interpretDmd },
    { "MD2",    ".md2",     interpretMd2 },
    { "",       "",         0 } // Terminate.
};

static ModelFileType const *guessModelFileTypeFromFileName(String filePath)
{
    // An extension is required for this.
    String ext = filePath.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
        {
            ModelFileType const &type = modelTypes[i];
            if(!type.ext.compareWithoutCase(ext))
            {
                return &type;
            }
        }
    }
    return 0; // Unknown.
}

static Model *interpretModel(de::FileHandle &hndl, String path, modelid_t modelId)
{
    // Firstly try the interpreter for the guessed resource types.
    ModelFileType const *rtypeGuess = guessModelFileTypeFromFileName(path);
    if(rtypeGuess)
    {
        if(Model *mdl = rtypeGuess->interpretFunc(hndl, path, modelId))
            return mdl;
    }

    // Not yet interpreted - try each recognisable format in order.
    // Try each recognisable format instead.
    for(int i = 0; !modelTypes[i].name.isEmpty(); ++i)
    {
        ModelFileType const &modelType = modelTypes[i];

        // Already tried this?
        if(&modelType == rtypeGuess) continue;

        if(Model *mdl = modelType.interpretFunc(hndl, path, modelId))
            return mdl;
    }

    return 0;
}

/**
 * Finds the existing model or loads in a new one.
 */
static Model *loadModel(String path)
{
    // Have we already loaded this?
    modelid_t modelId = modelRepository->intern(path);
    Model *mdl = Models_Model(modelId);
    if(mdl) return mdl; // Yes.

    try
    {
        // Attempt to interpret and load this model file.
        QScopedPointer<de::FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));

        mdl = interpretModel(*hndl, path, modelId);

        // We're done with the file.
        App_FileSystem().releaseFile(hndl->file());

        // Loaded?
        if(mdl)
        {
            defineAllSkins(*mdl);

            // Enlarge the vertex buffers in preparation for drawing of this model.
            if(!Rend_ModelExpandVertexBuffers(mdl->vertexCount()))
            {
                LOG_WARNING("Model \"%s\" contains more than %u max vertices (%i), it will not be rendered.")
                    << NativePath(path).pretty()
                    << uint(RENDER_MAX_MODEL_VERTS) << mdl->vertexCount();
            }

            return mdl;
        }
    }
    catch(FS1::NotFoundError const &er)
    {
        LOG_WARNING(er.asText() + ", ignoring.");
    }

    return 0;
}

static void *allocAndLoad(de::FileHandle &file, int offset, int len)
{
    uint8_t *ptr = (uint8_t *) M_Malloc(len);
    file.seek(offset, SeekSet);
    file.read(ptr, len);
    return ptr;
}

//
#define MD2_MAGIC 0x32504449

#pragma pack(1)
struct md2_header_t
{
    int magic;
    int version;
    int skinWidth;
    int skinHeight;
    int frameSize;
    int numSkins;
    int numVertices;
    int numTexCoords;
    int numTriangles;
    int numGlCommands;
    int numFrames;
    int offsetSkins;
    int offsetTexCoords;
    int offsetTriangles;
    int offsetFrames;
    int offsetGlCommands;
    int offsetEnd;
};
#pragma pack()

static bool readMd2Header(de::FileHandle &file, md2_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(md2_header_t));
    if(readBytes < sizeof(md2_header_t)) return false;

    hdr.magic            = littleEndianByteOrder.toNative(hdr.magic);
    hdr.version          = littleEndianByteOrder.toNative(hdr.version);
    hdr.skinWidth        = littleEndianByteOrder.toNative(hdr.skinWidth);
    hdr.skinHeight       = littleEndianByteOrder.toNative(hdr.skinHeight);
    hdr.frameSize        = littleEndianByteOrder.toNative(hdr.frameSize);
    hdr.numSkins         = littleEndianByteOrder.toNative(hdr.numSkins);
    hdr.numVertices      = littleEndianByteOrder.toNative(hdr.numVertices);
    hdr.numTexCoords     = littleEndianByteOrder.toNative(hdr.numTexCoords);
    hdr.numTriangles     = littleEndianByteOrder.toNative(hdr.numTriangles);
    hdr.numGlCommands    = littleEndianByteOrder.toNative(hdr.numGlCommands);
    hdr.numFrames        = littleEndianByteOrder.toNative(hdr.numFrames);
    hdr.offsetSkins      = littleEndianByteOrder.toNative(hdr.offsetSkins);
    hdr.offsetTexCoords  = littleEndianByteOrder.toNative(hdr.offsetTexCoords);
    hdr.offsetTriangles  = littleEndianByteOrder.toNative(hdr.offsetTriangles);
    hdr.offsetFrames     = littleEndianByteOrder.toNative(hdr.offsetFrames);
    hdr.offsetGlCommands = littleEndianByteOrder.toNative(hdr.offsetGlCommands);
    hdr.offsetEnd        = littleEndianByteOrder.toNative(hdr.offsetEnd);
    return true;
}

/// @todo We only really need to read the magic bytes and the version here.
static bool recogniseMd2(de::FileHandle &file)
{
    md2_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readMd2Header(file, hdr) && LONG(hdr.magic) == MD2_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

#pragma pack(1)
struct md2_triangleVertex_t
{
    byte vertex[3];
    byte normalIndex;
};

struct md2_packedFrame_t
{
    float scale[3];
    float translate[3];
    char name[16];
    md2_triangleVertex_t vertices[1];
};

struct md2_commandElement_t {
    float s, t;
    int index;
};
#pragma pack()

/**
 * Note vertex Z/Y are swapped here (ordered XYZ in the serialized data).
 */
static void loadMd2(de::FileHandle &file, Model &mdl)
{
    // Read the header.
    md2_header_t hdr;
    bool readHeaderOk = readMd2Header(file, hdr);
    DENG2_ASSERT(readHeaderOk);
    DENG2_UNUSED(readHeaderOk); // should this be checked?

    mdl._numVertices = hdr.numVertices;

    mdl.clearAllFrames();

    // Load and convert to DMD.
    uint8_t *frameData = (uint8_t *) allocAndLoad(file, hdr.offsetFrames, hdr.frameSize * hdr.numFrames);
    for(int i = 0; i < hdr.numFrames; ++i)
    {
        md2_packedFrame_t const *pfr = (md2_packedFrame_t const *) (frameData + hdr.frameSize * i);
        Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
        Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
        String const frameName = pfr->name;

        ModelFrame *frame = new ModelFrame(mdl, frameName);
        frame->vertices.reserve(hdr.numVertices);

        // Scale and translate each vertex.
        md2_triangleVertex_t const *pVtx = pfr->vertices;
        for(int k = 0; k < hdr.numVertices; ++k, pVtx++)
        {
            frame->vertices.append(ModelFrame::Vertex());
            ModelFrame::Vertex &vtx = frame->vertices.last();

            vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                          * scale + translation;
            vtx.pos.y *= rModelAspectMod; // Aspect undoing.

            vtx.norm = Vector3f(avertexnormals[pVtx->normalIndex]);

            if(!k)
            {
                frame->min = frame->max = vtx.pos;
            }
            else
            {
                frame->min = vtx.pos.min(frame->min);
                frame->max = vtx.pos.max(frame->max);
            }
        }

        mdl.addFrame(frame); // takes owernship
    }
    M_Free(frameData);

    mdl._lods.append(new ModelDetailLevel(mdl, 0));
    ModelDetailLevel &lod0 = *mdl._lods.last();

    uint8_t *commandData = (uint8_t *) allocAndLoad(file, hdr.offsetGlCommands, 4 * hdr.numGlCommands);
    for(uint8_t const *pos = commandData; *pos;)
    {
        int count = LONG( *(int *) pos ); pos += 4;

        lod0.primitives.append(Model::Primitive());
        Model::Primitive &prim = lod0.primitives.last();

        // The type of primitive depends on the sign.
        prim.triFan = (count < 0);

        if(count < 0)
        {
            count = -count;
        }

        while(count--)
        {
            md2_commandElement_t const *v = (md2_commandElement_t *) pos; pos += 12;

            prim.elements.append(Model::Primitive::Element());
            Model::Primitive::Element &elem = prim.elements.last();
            elem.texCoord = Vector2f(FLOAT(v->s), FLOAT(v->t));
            elem.index    = LONG(v->index);
        }
    }
    M_Free(commandData);

    mdl.clearAllSkins();

    // Load skins. (Note: numSkins may be zero.)
    file.seek(hdr.offsetSkins, SeekSet);
    for(int i = 0; i < hdr.numSkins; ++i)
    {
        char name[64]; file.read((uint8_t *)name, 64);
        mdl.newSkin(name);
    }
}

//
#define DMD_MAGIC 0x4D444D44 ///< "DMDM" = Doomsday/Detailed MoDel Magic

#pragma pack(1)
struct dmd_header_t
{
    int magic;
    int version;
    int flags;
};
#pragma pack()

static bool readHeaderDmd(de::FileHandle &file, dmd_header_t &hdr)
{
    size_t readBytes = file.read((uint8_t *)&hdr, sizeof(dmd_header_t));
    if(readBytes < sizeof(dmd_header_t)) return false;

    hdr.magic   = littleEndianByteOrder.toNative(hdr.magic);
    hdr.version = littleEndianByteOrder.toNative(hdr.version);
    hdr.flags   = littleEndianByteOrder.toNative(hdr.flags);
    return true;
}

static bool recogniseDmd(de::FileHandle &file)
{
    dmd_header_t hdr;
    size_t initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    bool result = (readHeaderDmd(file, hdr) && LONG(hdr.magic) == DMD_MAGIC);
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    return result;
}

// DMD chunk types.
enum {
    DMC_END, /// Must be the last chunk.
    DMC_INFO /// Required; will be expected to exist.
};

#pragma pack(1)
typedef struct {
    int type;
    int length; /// Next chunk follows...
} dmd_chunk_t;

typedef struct {
    int skinWidth;
    int skinHeight;
    int frameSize;
    int numSkins;
    int numVertices;
    int numTexCoords;
    int numFrames;
    int numLODs;
    int offsetSkins;
    int offsetTexCoords;
    int offsetFrames;
    int offsetLODs;
    int offsetEnd;
} dmd_info_t;

typedef struct {
    int numTriangles;
    int numGlCommands;
    int offsetTriangles;
    int offsetGlCommands;
} dmd_levelOfDetail_t;

typedef struct {
    byte vertex[3];
    unsigned short normal; /// Yaw and pitch.
} dmd_packedVertex_t;

typedef struct {
    float scale[3];
    float translate[3];
    char name[16];
    dmd_packedVertex_t vertices[1]; // dmd_info_t::numVertices size
} dmd_packedFrame_t;

typedef struct {
    short vertexIndices[3];
    short textureIndices[3];
} dmd_triangle_t;
#pragma pack()

/**
 * Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
 */
static Vector3f unpackVector(ushort packed)
{
    float const yaw   = (packed & 511) / 512.0f * 2 * PI;
    float const pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
    float const cosp  = float(cos(pitch));
    return Vector3f(cos(yaw) * cosp, sin(yaw) * cosp, sin(pitch));
}

/**
 * Note vertex Z/Y are swapped here (ordered XYZ in the serialized data).
 */
static void loadDmd(de::FileHandle &file, Model &mdl)
{
    // Read the header.
    dmd_header_t hdr;
    bool readHeaderOk = readHeaderDmd(file, hdr);
    DENG2_ASSERT(readHeaderOk);
    DENG2_UNUSED(readHeaderOk); // should this be checked?

    // Read the chunks.
    dmd_chunk_t chunk;
    file.read((uint8_t *)&chunk, sizeof(chunk));

    dmd_info_t info; zap(info);
    while(LONG(chunk.type) != DMC_END)
    {
        switch(LONG(chunk.type))
        {
        case DMC_INFO: // Standard DMD information chunk.
            file.read((uint8_t *)&info, LONG(chunk.length));

            info.skinWidth       = LONG(info.skinWidth);
            info.skinHeight      = LONG(info.skinHeight);
            info.frameSize       = LONG(info.frameSize);
            info.numSkins        = LONG(info.numSkins);
            info.numVertices     = LONG(info.numVertices);
            info.numTexCoords    = LONG(info.numTexCoords);
            info.numFrames       = LONG(info.numFrames);
            info.numLODs         = LONG(info.numLODs);
            info.offsetSkins     = LONG(info.offsetSkins);
            info.offsetTexCoords = LONG(info.offsetTexCoords);
            info.offsetFrames    = LONG(info.offsetFrames);
            info.offsetLODs      = LONG(info.offsetLODs);
            info.offsetEnd       = LONG(info.offsetEnd);
            break;

        default:
            // Skip unknown chunks.
            file.seek(LONG(chunk.length), SeekCur);
            break;
        }
        // Read the next chunk header.
        file.read((uint8_t *)&chunk, sizeof(chunk));
    }
    mdl._numVertices = info.numVertices;

    mdl.clearAllSkins();

    // Allocate and load in the data. (Note: numSkins may be zero.)
    file.seek(info.offsetSkins, SeekSet);
    for(int i = 0; i < info.numSkins; ++i)
    {
        char name[64]; file.read((uint8_t *)name, 64);
        mdl.newSkin(name);
    }

    mdl.clearAllFrames();

    uint8_t *frameData = (uint8_t *) allocAndLoad(file, info.offsetFrames, info.frameSize * info.numFrames);
    for(int i = 0; i < info.numFrames; ++i)
    {
        dmd_packedFrame_t const *pfr = (dmd_packedFrame_t *) (frameData + info.frameSize * i);
        Vector3f const scale(FLOAT(pfr->scale[0]), FLOAT(pfr->scale[2]), FLOAT(pfr->scale[1]));
        Vector3f const translation(FLOAT(pfr->translate[0]), FLOAT(pfr->translate[2]), FLOAT(pfr->translate[1]));
        String const frameName = pfr->name;

        ModelFrame *frame = new ModelFrame(mdl, frameName);
        frame->vertices.reserve(info.numVertices);

        // Scale and translate each vertex.
        dmd_packedVertex_t const *pVtx = pfr->vertices;
        for(int k = 0; k < info.numVertices; ++k, ++pVtx)
        {
            frame->vertices.append(ModelFrame::Vertex());
            ModelFrame::Vertex &vtx = frame->vertices.last();

            vtx.pos = Vector3f(pVtx->vertex[0], pVtx->vertex[2], pVtx->vertex[1])
                          * scale + translation;
            vtx.pos.y *= rModelAspectMod; // Aspect undo.

            vtx.norm = unpackVector(USHORT(pVtx->normal));

            if(!k)
            {
                frame->min = frame->max = vtx.pos;
            }
            else
            {
                frame->min = vtx.pos.min(frame->min);
                frame->max = vtx.pos.max(frame->max);
            }
        }

        mdl.addFrame(frame);
    }
    M_Free(frameData);

    file.seek(info.offsetLODs, SeekSet);
    dmd_levelOfDetail_t *lodInfo = new dmd_levelOfDetail_t[info.numLODs];

    for(int i = 0; i < info.numLODs; ++i)
    {
        file.read((uint8_t *)&lodInfo[i], sizeof(dmd_levelOfDetail_t));

        lodInfo[i].numTriangles     = LONG(lodInfo[i].numTriangles);
        lodInfo[i].numGlCommands    = LONG(lodInfo[i].numGlCommands);
        lodInfo[i].offsetTriangles  = LONG(lodInfo[i].offsetTriangles);
        lodInfo[i].offsetGlCommands = LONG(lodInfo[i].offsetGlCommands);
    }

    dmd_triangle_t **triangles = new dmd_triangle_t*[info.numLODs];

    for(int i = 0; i < info.numLODs; ++i)
    {
        mdl._lods.append(new ModelDetailLevel(mdl, i));
        ModelDetailLevel &lod = *mdl._lods.last();

        triangles[i] = (dmd_triangle_t *) allocAndLoad(file, lodInfo[i].offsetTriangles,
                                                       sizeof(dmd_triangle_t) * lodInfo[i].numTriangles);

        uint8_t *commandData = (uint8_t *) allocAndLoad(file, lodInfo[i].offsetGlCommands,
                                                        4 * lodInfo[i].numGlCommands);
        for(uint8_t const *pos = commandData; *pos;)
        {
            int count = LONG( *(int *) pos ); pos += 4;

            lod.primitives.append(Model::Primitive());
            Model::Primitive &prim = lod.primitives.last();

            // The type of primitive depends on the sign of the element count.
            prim.triFan = (count < 0);

            if(count < 0)
            {
                count = -count;
            }

            while(count--)
            {
                md2_commandElement_t const *v = (md2_commandElement_t *) pos; pos += 12;

                prim.elements.append(Model::Primitive::Element());
                Model::Primitive::Element &elem = prim.elements.last();

                elem.texCoord = Vector2f(FLOAT(v->s), FLOAT(v->t));
                elem.index    = LONG(v->index);
            }
        }
        M_Free(commandData);
    }

    // Determine vertex usage at each LOD level.
    mdl._vertexUsage.resize(info.numVertices * info.numLODs);
    mdl._vertexUsage.fill(false);

    for(int i = 0; i < info.numLODs; ++i)
    for(int k = 0; k < lodInfo[i].numTriangles; ++k)
    for(int m = 0; m < 3; ++m)
    {
        int vertexIndex = SHORT(triangles[i][k].vertexIndices[m]);
        mdl._vertexUsage.setBit(vertexIndex * info.numLODs + i);
    }

    delete [] lodInfo;
    for(int i = 0; i < info.numLODs; ++i)
    {
        M_Free(triangles[i]);
    }
    delete [] triangles;
}
