/** @file modelloader.cpp  Model loader.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/modelloader.h"
#include "render/model.h"
#include "render/rendersystem.h"
#include "clientapp.h"
#include "busyrunner.h"

#include <de/Hash>
#include <de/MultiAtlas>
#include <de/ScriptedInfo>
#include <de/Set>
#include <de/filesys/AssetObserver>

namespace render {

using namespace de;

const char *ModelLoader::DEF_ANIMATION = "animation";
const char *ModelLoader::DEF_MATERIAL  = "material";
const char *ModelLoader::DEF_PASS      = "pass";
const char *ModelLoader::DEF_RENDER    = "render";

DE_STATIC_STRING(DEF_ALIGNMENT_PITCH    , "alignment.pitch");
DE_STATIC_STRING(DEF_ALIGNMENT_YAW      , "alignment.yaw");
DE_STATIC_STRING(DEF_AUTOSCALE          , "autoscale");
DE_STATIC_STRING(DEF_BLENDFUNC          , "blendFunc");
DE_STATIC_STRING(DEF_BLENDOP            , "blendOp");
DE_STATIC_STRING(DEF_DEPTHFUNC          , "depthFunc");
DE_STATIC_STRING(DEF_DEPTHWRITE         , "depthWrite");
static String const DEF_FOV             ("fov");
DE_STATIC_STRING(DEF_FRONT_VECTOR       , "front");
DE_STATIC_STRING(DEF_MESHES             , "meshes");
DE_STATIC_STRING(DEF_MIRROR             , "mirror");
DE_STATIC_STRING(DEF_OFFSET             , "offset");
DE_STATIC_STRING(DEF_SEQUENCE           , "sequence");
DE_STATIC_STRING(DEF_SHADER             , "shader");
DE_STATIC_STRING(DEF_STATE              , "state");
DE_STATIC_STRING(DEF_TEXTURE_MAPPING    , "textureMapping");
DE_STATIC_STRING(DEF_TIMELINE           , "timeline");
DE_STATIC_STRING(DEF_UP_VECTOR          , "up");
DE_STATIC_STRING(DEF_WEAPON_OPACITY     , "opacityFromWeapon");
DE_STATIC_STRING(DEF_WEAPON_FULLBRIGHT  , "fullbrightFromWeapon");
DE_STATIC_STRING(DEF_VARIANT            , "variant");

DE_STATIC_STRING(SHADER_DEFAULT         , "model.skeletal.generic");
DE_STATIC_STRING(MATERIAL_DEFAULT       , "default");

DE_STATIC_STRING(VAR_U_MAP_TIME         , "uMapTime");
DE_STATIC_STRING(VAR_U_PROJECTION_MATRIX, "uProjectionMatrix");
DE_STATIC_STRING(VAR_U_VIEW_MATRIX      , "uViewMatrix");

static constexpr Atlas::Size MAX_ATLAS_SIZE(8192, 8192);

DE_PIMPL(ModelLoader)
, DE_OBSERVES(filesys::AssetObserver, Availability)
, DE_OBSERVES(Bank, Load)
, DE_OBSERVES(ModelDrawable, AboutToGLInit)
, DE_OBSERVES(BusyRunner, DeferredGLTask)
, public MultiAtlas::IAtlasFactory
{
    MultiAtlas atlasPool{*this};

    filesys::AssetObserver observer{"model\\..*"};
    ModelBank bank {
        // Using render::Model instances.
        []() -> ModelDrawable * { return new render::Model; }
    };
    LockableT<Set<String>> pendingModels;

    struct Program : public GLProgram
    {
        String shaderName;
        const Record *def = nullptr;
        int useCount = 1; ///< Number of models using the program.
    };

    /**
     * All shader programs needed for loaded models. All programs are ready for drawing,
     * and the common uniforms are bound.
     */
    struct Programs : public Hash<String, Program *>
    {
        ~Programs()
        {
#ifdef DE_DEBUG
            for (auto i = begin(); i != end(); ++i)
            {
                debug("%s %p", i->first.c_str(), i->second);
                DE_ASSERT(i->second);
                warning(
                    "ModelRenderer: Program %p still has %i users", i->second, i->second->useCount);
            }
#endif
            // Everything should have been unloaded, because all models
            // have been destroyed at this point.
            // (Does not apply during abnormal shutdown.)
            //DE_ASSERT(empty());
        }
    };
    Programs programs;

    Impl(Public *i) : Base(i)
    {
        observer.audienceForAvailability() += this;
        bank.audienceForLoad() += this;
    }

    void init()
    {
        // The default shader is used whenever a model does not specifically
        // request another one.
        loadProgram(SHADER_DEFAULT());

#if defined (DE_HAVE_BUSYRUNNER)
        ClientApp::busyRunner().audienceForDeferredGLTask() += this;
#endif
    }

    void deinit()
    {
#if defined (DE_HAVE_BUSYRUNNER)
        ClientApp::busyRunner().audienceForDeferredGLTask() -= this;
#endif

        // GL resources must be accessed from the main thread only.
        bank.unloadAll(Bank::ImmediatelyInCurrentThread);
        pendingModels.value.clear();

        atlasPool.clear();
        unloadProgram(*programs[SHADER_DEFAULT()]);
    }

    Atlas *makeAtlas(MultiAtlas &) override
    {
        // Construct a new atlas to be used in the MultiAtlas.
        Atlas *atlas = AtlasTexture::newWithKdTreeAllocator(
                    Atlas::DeferredAllocations,
                    GLTexture::maximumSize().min(MAX_ATLAS_SIZE));
        atlas->setBorderSize(1);
        atlas->setMarginSize(0);

        LOG_GL_MSG("New %ix%i model texture atlas allocated")
                << atlas->totalSize().x << atlas->totalSize().y;

        return atlas;
    }

    void assetAvailabilityChanged(const String &identifier, filesys::AssetObserver::Event event) override
    {
        LOG_RES_MSG("Model asset \"%s\" is now %s")
                << identifier
                << (event == filesys::AssetObserver::Added? "available" :
                                                            "unavailable");
        if (event == filesys::AssetObserver::Added)
        {
            bank.add(identifier, App::asset(identifier).absolutePath("path"));

            // Begin loading the model right away.
            bank.load(identifier);
        }
        else
        {
            const auto &model = bank.model<render::Model const>(identifier);

            // Unload programs used by the various rendering passes.
            for (const auto &pass : model.passes)
            {
                DE_ASSERT(pass.program);
                unloadProgram(*static_cast<Program *>(pass.program));
            }

            // Alternatively, the entire model may be using a single program.
            if (model.passes.isEmpty())
            {
                if (model.program())
                {
                    unloadProgram(*static_cast<Program *>(model.program()));
                }
            }
            else
            {
                DE_ASSERT(!model.program());
            }

            bank.remove(identifier);
            {
                DE_GUARD(pendingModels);
                pendingModels.value.remove(identifier);
            }
        }
    }

    /**
     * Initializes one or more uninitialized models for rendering.
     * Must be called from the main thread.
     *
     * @param maxCount  Maximum number of models to initialize.
     */
    void initPendingModels(int maxCount)
    {
        DE_GUARD(pendingModels);

        while (!pendingModels.value.isEmpty() && maxCount > 0)
        {
            const String identifier = *pendingModels.value.begin();
            pendingModels.value.remove(identifier);

            if (!bank.has(identifier))
            {
                continue;
            }

            LOG_GL_MSG("Initializing \"%s\"") << identifier;

            auto &model = bank.model<render::Model>(identifier);
            model.glInit();

            --maxCount;
        }
    }

    BusyRunner::DeferredResult performDeferredGLTask() override
    {
        initPendingModels(1);

        DE_GUARD(pendingModels);
        return pendingModels.value.isEmpty()? BusyRunner::AllTasksCompleted
                                            : BusyRunner::TasksPending;
    }

    /**
     * Initializes a program and stores it in the programs hash. If the program already
     * has been loaded before, the program use count is incremented.
     *
     * @param name  Name of the shader program.
     *
     * @return Shader program.
     */
    Program *loadProgram(const String &name)
    {
        if (programs.contains(name))
        {
            programs[name]->useCount++;
            return programs[name];
        }

        std::unique_ptr<Program> prog(new Program);
        prog->shaderName = name;
        prog->def = &ClientApp::shaders()[name].valueAsRecord(); // for lookups later

        LOG_RES_VERBOSE("Loading model shader \"%s\"") << name;

        // Bind the mandatory common state.
        ClientApp::shaders().build(*prog, name);

        DE_NOTIFY_PUBLIC(NewProgram, i)
        {
            i->newProgramCreated(*prog);
        }

        auto &render = ClientApp::render();

        // Built-in special uniforms.
        if (prog->def->hasMember(VAR_U_MAP_TIME()))
        {
            *prog << render.uMapTime();
        }
        if (prog->def->hasMember(VAR_U_PROJECTION_MATRIX()))
        {
            *prog << render.uProjectionMatrix();
        }
        if (prog->def->hasMember(VAR_U_VIEW_MATRIX()))
        {
            *prog << render.uViewMatrix();
        }

        programs[name] = prog.get();
        return prog.release();
    }

    /**
     * Decrements a program's use count, and deletes the program altogether if it has
     * no more users.
     *
     * @param name  Name of the shader program.
     */
    void unloadProgram(Program &program)
    {
        if (--program.useCount == 0)
        {
            String name = program.shaderName;
            LOG_RES_VERBOSE("Model shader \"%s\" unloaded (no more users)") << name;
            delete &program;
            DE_ASSERT(programs.contains(name));
            programs.remove(name);
        }
    }

    void modelAboutToGLInit(ModelDrawable &drawable) override
    {
        auto &model = static_cast<render::Model &>(drawable);
        {
            DE_GUARD(pendingModels);
            pendingModels.value.remove(model.identifier);
        }
        model.setAtlas(*model.textures);
    }

    static gfx::Comparison textToComparison(const String &text)
    {
        static struct { const char *txt; gfx::Comparison comp; } const cs[] = {
            { "Never",          gfx::Never },
            { "Always",         gfx::Always },
            { "Equal",          gfx::Equal },
            { "NotEqual",       gfx::NotEqual },
            { "Less",           gfx::Less },
            { "Greater",        gfx::Greater },
            { "LessOrEqual",    gfx::LessOrEqual },
            { "GreaterOrEqual", gfx::GreaterOrEqual }
        };
        for (const auto &p : cs)
        {
            if (text == p.txt)
            {
                return p.comp;
            }
        }
        throw DefinitionError("ModelRenderer::textToComparison",
                              stringf("Invalid comparison function \"%s\"", text.c_str()));
    }

    static gfx::Blend textToBlendFunc(const String &text)
    {
        static struct { const char *txt; gfx::Blend blend; } const bs[] = {
            { "Zero",              gfx::Zero },
            { "One",               gfx::One },
            { "SrcColor",          gfx::SrcColor },
            { "OneMinusSrcColor",  gfx::OneMinusSrcColor },
            { "SrcAlpha",          gfx::SrcAlpha },
            { "OneMinusSrcAlpha",  gfx::OneMinusSrcAlpha },
            { "DestColor",         gfx::DestColor },
            { "OneMinusDestColor", gfx::OneMinusDestColor },
            { "DestAlpha",         gfx::DestAlpha },
            { "OneMinusDestAlpha", gfx::OneMinusDestAlpha }
        };
        for (const auto &p : bs)
        {
            if (text == p.txt)
            {
                return p.blend;
            }
        }
        throw DefinitionError("ModelRenderer::textToBlendFunc",
                              stringf("Invalid blending function \"%s\"", text.c_str()));
    }

    static gfx::BlendOp textToBlendOp(const String &text)
    {
        if (text == "Add") return gfx::Add;
        if (text == "Subtract") return gfx::Subtract;
        if (text == "ReverseSubtract") return gfx::ReverseSubtract;
        throw DefinitionError("ModelRenderer::textToBlendOp",
                              stringf("Invalid blending operation \"%s\"", text.c_str()));
    }

    /**
     * Compose texture mappings of one or more shaders.
     *
     * All shaders used on one model must have the same mappings. Otherwise
     * the vertex data in the static model VBO is not compatible with all of
     * them (redoing the VBOs during drawing is inadvisable).
     *
     * @param mapping    Model texture mapping.
     * @param shaderDef  Shader definition.
     */
    void composeTextureMappings(ModelDrawable::Mapping &mapping,
                                const Record &shaderDef)
    {
        if (shaderDef.has(DEF_TEXTURE_MAPPING()))
        {
            const ArrayValue &array = shaderDef.geta(DEF_TEXTURE_MAPPING());
            for (uint i = 0; i < array.size(); ++i)
            {
                ModelDrawable::TextureMap map = ModelDrawable::textToTextureMap(array.element(i).asText());
                if (i == mapping.size())
                {
                    mapping << map;
                }
                else if (mapping.at(i) != map)
                {
                    // Must match what the shader expects to receive.
                    StringList list;
                    for (auto map : mapping) list << ModelDrawable::textureMapToText(map);
                    throw TextureMappingError(
                        "ModelRenderer::composeTextureMappings",
                        stringf("Texture mapping <%s> is incompatible with shader %s",
                                String::join(list, ", ").c_str(),
                                ScriptedInfo::sourceLocation(shaderDef).c_str()));
                }
            }
        }
    }

    /**
     * When model assets have been loaded, we can parse their metadata to see if there
     * are any animation sequences defined. If so, we'll set up a shared lookup table
     * that determines which sequences to start in which mobj states.
     *
     * @param path  Model asset id.
     */
    void bankLoaded(const DotPath &path) override
    {
        // Models use the shared atlas.
        render::Model &model = bank.model<render::Model>(path);
        model.audienceForAboutToGLInit() += this;

        const auto asset = App::asset(path);

        // Determine the coordinate system of the model.
        Vec3f front(0, 0, 1);
        Vec3f up   (0, 1, 0);
        if (asset.has(DEF_FRONT_VECTOR()))
        {
            front = Vec3f(asset.geta(DEF_FRONT_VECTOR()));
        }
        if (asset.has(DEF_UP_VECTOR()))
        {
            up = Vec3f(asset.geta(DEF_UP_VECTOR()));
        }
        bool mirror = ScriptedInfo::isTrue(asset, DEF_MIRROR());
        model.cull = mirror? gfx::Back : gfx::Front;
        // Assimp's coordinate system uses different handedness than Doomsday,
        // so mirroring is needed.
        model.transformation = Mat4f::frame(front, up, !mirror);
        if (asset.has(DEF_OFFSET()))
        {
            model.offset = vectorFromValue<Vec3f>(asset.get(DEF_OFFSET()));
        }
        applyFlagOperation(model.flags, render::Model::AutoscaleToThingHeight,
                           ScriptedInfo::isTrue(asset, DEF_AUTOSCALE()) ? SetFlags : UnsetFlags);
        applyFlagOperation(model.flags, render::Model::ThingOpacityAsAmbientLightAlpha,
                           ScriptedInfo::isTrue(asset, DEF_WEAPON_OPACITY(), true) ? SetFlags : UnsetFlags);
        applyFlagOperation(model.flags, render::Model::ThingFullBrightAsAmbientLight,
                           ScriptedInfo::isTrue(asset, DEF_WEAPON_FULLBRIGHT()) ? SetFlags : UnsetFlags);

        // Alignment modes.
        model.alignYaw   = parseAlignment(asset, DEF_ALIGNMENT_YAW());
        model.alignPitch = parseAlignment(asset, DEF_ALIGNMENT_PITCH());

        // Custom field of view (psprite only).
        model.pspriteFOV = asset.getf(DEF_FOV, 0.0f);

        // Custom texture maps and additional materials.
        model.materialIndexForName.insert(MATERIAL_DEFAULT(), 0);
        if (asset.has(DEF_MATERIAL))
        {
            asset.subrecord(DEF_MATERIAL)
                .forSubrecords([this, &model](const String &blockName, const Record &block) {
                    try
                    {
                if (ScriptedInfo::blockType(block) == DEF_VARIANT())
                        {
                            const String materialName = blockName;
                            if (!model.materialIndexForName.contains(materialName))
                            {
                                // Add a new material.
                                model.materialIndexForName.insert(materialName,
                                                                  model.addMaterial());
                            }
                            block.forSubrecords([this, &model, &materialName](
                                                    const String &matName, const Record &matDef) {
                                setupMaterial(model,
                                              matName,
                                              model.materialIndexForName[materialName],
                                              matDef);
                                return LoopContinue;
                            });
                        }
                        else
                        {
                            // The default material.
                            setupMaterial(model, blockName, 0, block);
                        }
                    }
                    catch (const Error &er)
                    {
                        LOG_GL_ERROR("Material variant \"%s\" is invalid: %s")
                            << blockName << er.asText();
                    }
                    return LoopContinue;
                });
        }

        // Set up the animation sequences for states.
        if (asset.has(DEF_ANIMATION))
        {
            auto states = ScriptedInfo::subrecordsOfType(DEF_STATE(), asset.subrecord(DEF_ANIMATION));
            for (auto &state : states)
            {
                // Sequences are added in source order.
                const auto seqs = ScriptedInfo::subrecordsOfType(DEF_SEQUENCE(), *state.second);
                if (!seqs.isEmpty())
                {
                    if (model.animationCount() > 0)
                    {
                        for (const String &key : ScriptedInfo::sortRecordsBySource(seqs))
                        {
                            model.animations[state.first] << render::Model::AnimSequence(key, *seqs[key]);
                        }
                    }
                    else
                    {
                        LOG_GL_WARNING("Model \"%s\" has no animation sequences in the model file "
                                       "but animation sequence definition found at %s")
                                << path
                                << ScriptedInfo::sourceLocation
                                   (*seqs[ScriptedInfo::sortRecordsBySource(seqs).first()]);
                    }
                }
            }

            // Timelines.
            for (const auto &timeline :
                 ScriptedInfo::subrecordsOfType(DEF_TIMELINE(), asset.subrecord(DEF_ANIMATION)))
            {
                Timeline *scheduler = new Timeline;
                scheduler->addFromInfo(*timeline.second);
                model.timelines[timeline.first] = scheduler;
            }
        }

        ModelDrawable::Mapping textureMapping;
        String modelShader = SHADER_DEFAULT();

        // Rendering passes.
        if (asset.has(DEF_RENDER))
        {
            const Record &renderBlock = asset.subrecord(DEF_RENDER);
            modelShader = renderBlock.gets(DEF_SHADER(), modelShader);

            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
            for (const String &key : ScriptedInfo::sortRecordsBySource(passes))
            {
                try
                {
                    const auto &def = *passes[key];

                    ModelDrawable::Pass pass;
                    pass.name = key;

                    pass.meshes.resize(model.meshCount());
                    for (const Value *value : def.geta(DEF_MESHES()).elements())
                    {
                        int meshId = identifierFromText(value->asText(), [&model] (const String &text) {
                            return model.meshId(text);
                        });
                        if (meshId < 0 || meshId >= model.meshCount())
                        {
                            throw DefinitionError("ModelLoader::bankLoaded",
                                                  "Unknown mesh \"" + value->asText() + "\" in " +
                                                      ScriptedInfo::sourceLocation(def));
                        }
                        pass.meshes.setBit(meshId, true);
                    }

                    // GL state parameters.
                    if (def.has(DEF_BLENDFUNC()))
                    {
                        const ArrayValue &blendDef = def.geta(DEF_BLENDFUNC());
                        pass.blendFunc.first  = textToBlendFunc(blendDef.at(0).asText());
                        pass.blendFunc.second = textToBlendFunc(blendDef.at(1).asText());
                    }
                    pass.blendOp = textToBlendOp(def.gets(DEF_BLENDOP(), "Add"));
                    pass.depthFunc = textToComparison(def.gets(DEF_DEPTHFUNC(), "Less"));
                    pass.depthWrite = ScriptedInfo::isTrue(def, DEF_DEPTHWRITE(), true);

                    const String passShader = def.gets(DEF_SHADER(), modelShader);
                    pass.program = loadProgram(passShader);
                    composeTextureMappings(textureMapping,
                                           ClientApp::shaders()[passShader]);

                    model.passes.append(pass);
                }
                catch (const Error &er)
                {
                    LOG_RES_ERROR("Rendering pass \"%s\" in asset \"%s\" is invalid: %s")
                            << key << path << er.asText();
                }
            }
        }

        // Rendering passes will always have programs associated with them.
        // However, if there are no passes, we need to set up the default
        // shader for the entire model.
        if (model.passes.isEmpty())
        {
            try
            {
                model.setProgram(loadProgram(modelShader));
                composeTextureMappings(textureMapping,
                                       ClientApp::shaders()[modelShader]);
            }
            catch (const Error &er)
            {
                LOG_RES_ERROR("Asset \"%s\" cannot use shader \"%s\": %s")
                        << path << modelShader << er.asText();
            }
        }

        // Configure the texture mapping. Shaders used with the model must
        // be compatible with this mapping order (enforced above). The mapping
        // affects the order in which texture coordinates are packed into vertex
        // arrays.
        model.setTextureMapping(textureMapping);

        // Textures of the model will be kept here.
        model.textures.reset(new MultiAtlas::AllocGroup(atlasPool));

        // Initialize for rendering at a later time.
        {
            DE_GUARD(pendingModels);
            pendingModels.value.insert(path);
        }
    }


    render::Model::Alignment parseAlignment(const RecordAccessor &def, const String &key)
    {
        if (!def.has(key)) return render::Model::NotAligned;

        const String value = def.gets(key);
        if (!value.compareWithoutCase("movement"))
        {
            return render::Model::AlignToMomentum;
        }
        else if (!value.compareWithoutCase("view"))
        {
            return render::Model::AlignToView;
        }
        else if (!value.compareWithoutCase("random"))
        {
            return render::Model::AlignRandomly;
        }
        else if (ScriptedInfo::isFalse(def, key, false))
        {
            return render::Model::NotAligned;
        }
        else
        {
            throw DefinitionError("ModelRenderer::parseAlignment",
                                  stringf("Unknown alignment value \"%s\" for %s in %s",
                                          value.c_str(),
                                          key.c_str(),
                                          ScriptedInfo::sourceLocation(def).c_str()));
        }
    }

    void setupMaterial(ModelDrawable &model,
                       const String &meshName,
                       duint materialIndex,
                       const Record &matDef)
    {
        int mid = identifierFromText(meshName,
                                     [&model](const String &text) { return model.meshId(text); });
        if (mid < 0 || mid >= model.meshCount())
        {
            throw DefinitionError("ModelLoader::setupMaterial",
                                  "Mesh \"" + meshName + "\" not found in " +
                                      ScriptedInfo::sourceLocation(matDef));
        }

        const ModelDrawable::MeshId mesh{duint(mid), materialIndex};

        setupMaterialTexture(model, mesh, matDef, DE_STR("diffuseMap"),  ModelDrawable::Diffuse);
        setupMaterialTexture(model, mesh, matDef, DE_STR("normalMap"),   ModelDrawable::Normals);
        setupMaterialTexture(model, mesh, matDef, DE_STR("heightMap"),   ModelDrawable::Height);
        setupMaterialTexture(model, mesh, matDef, DE_STR("specularMap"), ModelDrawable::Specular);
        setupMaterialTexture(model, mesh, matDef, DE_STR("emissiveMap"), ModelDrawable::Emissive);
    }

    void setupMaterialTexture(ModelDrawable &model,
                              const ModelDrawable::MeshId &mesh,
                              const Record &matDef,
                              const String &textureName,
                              ModelDrawable::TextureMap map)
    {
        if (matDef.has(textureName))
        {
            const String path = ScriptedInfo::absolutePathInContext(matDef, matDef.gets(textureName));
            model.setTexturePath(mesh, map, path);
        }
    }

    DE_PIMPL_AUDIENCE(NewProgram)
};

DE_AUDIENCE_METHOD(ModelLoader, NewProgram)

ModelLoader::ModelLoader()
    : d(new Impl(this))
{}

ModelBank &ModelLoader::bank()
{
    return d->bank;
}

const ModelBank &ModelLoader::bank() const
{
    return d->bank;
}

void ModelLoader::glInit()
{
    d->init();
}

void ModelLoader::glDeinit()
{
    d->deinit();
}

String ModelLoader::shaderName(const GLProgram &program) const
{
    return static_cast<const Impl::Program &>(program).shaderName;
}

const Record &ModelLoader::shaderDefinition(const GLProgram &program) const
{
    return *static_cast<const Impl::Program &>(program).def;
}

int ModelLoader::identifierFromText(const String &text,
                                    const std::function<int (const String &)>& resolver) // static
{
    int id = 0;
    if (text.beginsWith('@'))
    {
        id = text.substr(BytePos(1)).toInt();
    }
    else
    {
        id = resolver(text);
    }
    return id;
}

} // namespace render

