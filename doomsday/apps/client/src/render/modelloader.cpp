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

#include <de/filesys/AssetObserver>
#include <de/MultiAtlas>
#include <de/ScriptedInfo>

#include <QHash>
#include <QSet>

namespace render {

using namespace de;

String const ModelLoader::DEF_ANIMATION ("animation");
String const ModelLoader::DEF_MATERIAL  ("material");
String const ModelLoader::DEF_PASS      ("pass");
String const ModelLoader::DEF_RENDER    ("render");

static String const DEF_ALIGNMENT_PITCH ("alignment.pitch");
static String const DEF_ALIGNMENT_YAW   ("alignment.yaw");
static String const DEF_AUTOSCALE       ("autoscale");
static String const DEF_BLENDFUNC       ("blendFunc");
static String const DEF_BLENDOP         ("blendOp");
static String const DEF_DEPTHFUNC       ("depthFunc");
static String const DEF_DEPTHWRITE      ("depthWrite");
static String const DEF_FOV             ("fov");
static String const DEF_FRONT_VECTOR    ("front");
static String const DEF_MESHES          ("meshes");
static String const DEF_MIRROR          ("mirror");
static String const DEF_OFFSET          ("offset");
static String const DEF_SEQUENCE        ("sequence");
static String const DEF_SHADER          ("shader");
static String const DEF_STATE           ("state");
static String const DEF_TEXTURE_MAPPING ("textureMapping");
static String const DEF_TIMELINE        ("timeline");
static String const DEF_UP_VECTOR       ("up");
static String const DEF_WEAPON_OPACITY   ("opacityFromWeapon");
static String const DEF_WEAPON_FULLBRIGHT("fullbrightFromWeapon");
static String const DEF_VARIANT         ("variant");

static String const SHADER_DEFAULT      ("model.skeletal.generic");
static String const MATERIAL_DEFAULT    ("default");

static String const VAR_U_MAP_TIME          ("uMapTime");
static String const VAR_U_PROJECTION_MATRIX ("uProjectionMatrix");
static String const VAR_U_VIEW_MATRIX       ("uViewMatrix");

static Atlas::Size const MAX_ATLAS_SIZE (8192, 8192);

DENG2_PIMPL(ModelLoader)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(Bank, Load)
, DENG2_OBSERVES(ModelDrawable, AboutToGLInit)
, DENG2_OBSERVES(BusyRunner, DeferredGLTask)
, public MultiAtlas::IAtlasFactory
{
    MultiAtlas atlasPool { *this };

    filesys::AssetObserver observer { "model\\..*" };
    ModelBank bank {
        // Using render::Model instances.
        []() -> ModelDrawable * { return new render::Model; }
    };
    LockableT<QSet<String>> pendingModels;

    struct Program : public GLProgram
    {
        String shaderName;
        Record const *def = nullptr;
        int useCount = 1; ///< Number of models using the program.
    };

    /**
     * All shader programs needed for loaded models. All programs are ready for drawing,
     * and the common uniforms are bound.
     */
    struct Programs : public QHash<String, Program *>
    {
        ~Programs()
        {
#ifdef DENG2_DEBUG
            for (auto i = constBegin(); i != constEnd(); ++i)
            {
                qDebug() << i.key() << i.value();
                DENG2_ASSERT(i.value());
                qWarning() << "ModelRenderer: Program" << i.key()
                           << "still has" << i.value()->useCount << "users";
            }
#endif
            // Everything should have been unloaded, because all models
            // have been destroyed at this point.
            DENG2_ASSERT(empty());
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
        loadProgram(SHADER_DEFAULT);

#if defined (DENG_HAVE_BUSYRUNNER)
        ClientApp::busyRunner().audienceForDeferredGLTask() += this;
#endif
    }

    void deinit()
    {
#if defined (DENG_HAVE_BUSYRUNNER)
        ClientApp::busyRunner().audienceForDeferredGLTask() -= this;
#endif

        // GL resources must be accessed from the main thread only.
        bank.unloadAll(Bank::ImmediatelyInCurrentThread);
        pendingModels.value.clear();

        atlasPool.clear();
        unloadProgram(*programs[SHADER_DEFAULT]);
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

    void assetAvailabilityChanged(String const &identifier, filesys::AssetObserver::Event event) override
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
            auto const &model = bank.model<render::Model const>(identifier);

            // Unload programs used by the various rendering passes.
            for (auto const &pass : model.passes)
            {
                DENG2_ASSERT(pass.program);
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
                DENG2_ASSERT(!model.program());
            }

            bank.remove(identifier);
            {
                DENG2_GUARD(pendingModels);
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
        DENG2_GUARD(pendingModels);

        while (!pendingModels.value.isEmpty() && maxCount > 0)
        {
            String const identifier = *pendingModels.value.begin();
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

        DENG2_GUARD(pendingModels);
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
    Program *loadProgram(String const &name)
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

        DENG2_FOR_PUBLIC_AUDIENCE2(NewProgram, i)
        {
            i->newProgramCreated(*prog);
        }

        auto &render = ClientApp::renderSystem();

        // Built-in special uniforms.
        if (prog->def->hasMember(VAR_U_MAP_TIME))
        {
            *prog << render.uMapTime();
        }
        if (prog->def->hasMember(VAR_U_PROJECTION_MATRIX))
        {
            *prog << render.uProjectionMatrix();
        }
        if (prog->def->hasMember(VAR_U_VIEW_MATRIX))
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
            DENG2_ASSERT(programs.contains(name));
            programs.remove(name);
        }
    }

    void modelAboutToGLInit(ModelDrawable &drawable) override
    {
        auto &model = static_cast<render::Model &>(drawable);
        {
            DENG2_GUARD(pendingModels);
            pendingModels.value.remove(model.identifier);
        }
        model.setAtlas(*model.textures);
    }

    static gl::Comparison textToComparison(String const &text)
    {
        static struct { char const *txt; gl::Comparison comp; } const cs[] = {
            { "Never",          gl::Never },
            { "Always",         gl::Always },
            { "Equal",          gl::Equal },
            { "NotEqual",       gl::NotEqual },
            { "Less",           gl::Less },
            { "Greater",        gl::Greater },
            { "LessOrEqual",    gl::LessOrEqual },
            { "GreaterOrEqual", gl::GreaterOrEqual }
        };
        for (auto const &p : cs)
        {
            if (text == p.txt)
            {
                return p.comp;
            }
        }
        throw DefinitionError("ModelRenderer::textToComparison",
                              QString("Invalid comparison function \"%1\"").arg(text));
    }

    static gl::Blend textToBlendFunc(String const &text)
    {
        static struct { char const *txt; gl::Blend blend; } const bs[] = {
            { "Zero",              gl::Zero },
            { "One",               gl::One },
            { "SrcColor",          gl::SrcColor },
            { "OneMinusSrcColor",  gl::OneMinusSrcColor },
            { "SrcAlpha",          gl::SrcAlpha },
            { "OneMinusSrcAlpha",  gl::OneMinusSrcAlpha },
            { "DestColor",         gl::DestColor },
            { "OneMinusDestColor", gl::OneMinusDestColor },
            { "DestAlpha",         gl::DestAlpha },
            { "OneMinusDestAlpha", gl::OneMinusDestAlpha }
        };
        for (auto const &p : bs)
        {
            if (text == p.txt)
            {
                return p.blend;
            }
        }
        throw DefinitionError("ModelRenderer::textToBlendFunc",
                              QString("Invalid blending function \"%1\"").arg(text));
    }

    static gl::BlendOp textToBlendOp(String const &text)
    {
        if (text == "Add") return gl::Add;
        if (text == "Subtract") return gl::Subtract;
        if (text == "ReverseSubtract") return gl::ReverseSubtract;
        throw DefinitionError("ModelRenderer::textToBlendOp",
                              QString("Invalid blending operation \"%1\"").arg(text));
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
                                Record const &shaderDef)
    {
        if (shaderDef.has(DEF_TEXTURE_MAPPING))
        {
            ArrayValue const &array = shaderDef.geta(DEF_TEXTURE_MAPPING);
            for (int i = 0; i < int(array.size()); ++i)
            {
                ModelDrawable::TextureMap map = ModelDrawable::textToTextureMap(array.element(i).asText());
                if (i == mapping.size())
                {
                    mapping << map;
                }
                else if (mapping.at(i) != map)
                {
                    // Must match what the shader expects to receive.
                    QStringList list;
                    for (auto map : mapping) list << ModelDrawable::textureMapToText(map);
                    throw TextureMappingError("ModelRenderer::composeTextureMappings",
                                              QString("Texture mapping <%1> is incompatible with shader %2")
                                                  .arg(list.join(", "))
                                                  .arg(ScriptedInfo::sourceLocation(shaderDef)));
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
    void bankLoaded(DotPath const &path) override
    {
        // Models use the shared atlas.
        render::Model &model = bank.model<render::Model>(path);
        model.audienceForAboutToGLInit() += this;

        auto const asset = App::asset(path);

        // Determine the coordinate system of the model.
        Vector3f front(0, 0, 1);
        Vector3f up   (0, 1, 0);
        if (asset.has(DEF_FRONT_VECTOR))
        {
            front = Vector3f(asset.geta(DEF_FRONT_VECTOR));
        }
        if (asset.has(DEF_UP_VECTOR))
        {
            up = Vector3f(asset.geta(DEF_UP_VECTOR));
        }
        bool mirror = ScriptedInfo::isTrue(asset, DEF_MIRROR);
        model.cull = mirror? gl::Back : gl::Front;
        // Assimp's coordinate system uses different handedness than Doomsday,
        // so mirroring is needed.
        model.transformation = Matrix4f::frame(front, up, !mirror);
        if (asset.has(DEF_OFFSET))
        {
            model.offset = vectorFromValue<Vector3f>(asset.get(DEF_OFFSET));
        }
        applyFlagOperation(model.flags, render::Model::AutoscaleToThingHeight,
                           ScriptedInfo::isTrue(asset, DEF_AUTOSCALE)? SetFlags : UnsetFlags);
        applyFlagOperation(model.flags, render::Model::ThingOpacityAsAmbientLightAlpha,
                           ScriptedInfo::isTrue(asset, DEF_WEAPON_OPACITY, true)? SetFlags : UnsetFlags);
        applyFlagOperation(model.flags, render::Model::ThingFullBrightAsAmbientLight,
                           ScriptedInfo::isTrue(asset, DEF_WEAPON_FULLBRIGHT)? SetFlags : UnsetFlags);

        // Alignment modes.
        model.alignYaw   = parseAlignment(asset, DEF_ALIGNMENT_YAW);
        model.alignPitch = parseAlignment(asset, DEF_ALIGNMENT_PITCH);

        // Custom field of view (psprite only).
        model.pspriteFOV = asset.getf(DEF_FOV, 0.0f);

        // Custom texture maps and additional materials.
        model.materialIndexForName.insert(MATERIAL_DEFAULT, 0);
        if (asset.has(DEF_MATERIAL))
        {
            asset.subrecord(DEF_MATERIAL)
                .forSubrecords([this, &model](String const &blockName, Record const &block) {
                    try
                    {
                        if (ScriptedInfo::blockType(block) == DEF_VARIANT)
                        {
                            String const materialName = blockName;
                            if (!model.materialIndexForName.contains(materialName))
                            {
                                // Add a new material.
                                model.materialIndexForName.insert(materialName,
                                                                  model.addMaterial());
                            }
                            block.forSubrecords([this, &model, &materialName](
                                                    String const &matName, Record const &matDef) {
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
            auto states = ScriptedInfo::subrecordsOfType(DEF_STATE, asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, state, states)
            {
                // Sequences are added in source order.
                auto const seqs = ScriptedInfo::subrecordsOfType(DEF_SEQUENCE, *state.value());
                if (!seqs.isEmpty())
                {
                    if (model.animationCount() > 0)
                    {
                        for (String key : ScriptedInfo::sortRecordsBySource(seqs))
                        {
                            model.animations[state.key()] << render::Model::AnimSequence(key, *seqs[key]);
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
            auto timelines = ScriptedInfo::subrecordsOfType(DEF_TIMELINE, asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, timeline, timelines)
            {
                Timeline *scheduler = new Timeline;
                scheduler->addFromInfo(*timeline.value());
                model.timelines[timeline.key()] = scheduler;
            }
        }

        ModelDrawable::Mapping textureMapping;
        String modelShader = SHADER_DEFAULT;

        // Rendering passes.
        if (asset.has(DEF_RENDER))
        {
            Record const &renderBlock = asset.subrecord(DEF_RENDER);
            modelShader = renderBlock.gets(DEF_SHADER, modelShader);

            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
            for (String key : ScriptedInfo::sortRecordsBySource(passes))
            {
                try
                {
                    auto const &def = *passes[key];

                    ModelDrawable::Pass pass;
                    pass.name = key;

                    pass.meshes.resize(model.meshCount());
                    for (Value const *value : def.geta(DEF_MESHES).elements())
                    {
                        int meshId = identifierFromText(value->asText(), [&model] (String const &text) {
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
                    if (def.has(DEF_BLENDFUNC))
                    {
                        ArrayValue const &blendDef = def.geta(DEF_BLENDFUNC);
                        pass.blendFunc.first  = textToBlendFunc(blendDef.at(0).asText());
                        pass.blendFunc.second = textToBlendFunc(blendDef.at(1).asText());
                    }
                    pass.blendOp = textToBlendOp(def.gets(DEF_BLENDOP, "Add"));
                    pass.depthFunc = textToComparison(def.gets(DEF_DEPTHFUNC, "Less"));
                    pass.depthWrite = ScriptedInfo::isTrue(def, DEF_DEPTHWRITE, true);

                    String const passShader = def.gets(DEF_SHADER, modelShader);
                    pass.program = loadProgram(passShader);
                    composeTextureMappings(textureMapping,
                                           ClientApp::shaders()[passShader]);

                    model.passes.append(pass);
                }
                catch (Error const &er)
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
            catch (Error const &er)
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
            DENG2_GUARD(pendingModels);
            pendingModels.value.insert(path);
        }
    }


    render::Model::Alignment parseAlignment(RecordAccessor const &def, String const &key)
    {
        if (!def.has(key)) return render::Model::NotAligned;

        String const value = def.gets(key);
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
                                  String("Unknown alignment value \"%1\" for %2 in %3")
                                  .arg(value)
                                  .arg(key)
                                  .arg(ScriptedInfo::sourceLocation(def)));
        }
    }

    void setupMaterial(ModelDrawable &model,
                       String const &meshName,
                       duint materialIndex,
                       Record const &matDef)
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

        setupMaterialTexture(model, mesh, matDef, QStringLiteral("diffuseMap"),  ModelDrawable::Diffuse);
        setupMaterialTexture(model, mesh, matDef, QStringLiteral("normalMap"),   ModelDrawable::Normals);
        setupMaterialTexture(model, mesh, matDef, QStringLiteral("heightMap"),   ModelDrawable::Height);
        setupMaterialTexture(model, mesh, matDef, QStringLiteral("specularMap"), ModelDrawable::Specular);
        setupMaterialTexture(model, mesh, matDef, QStringLiteral("emissiveMap"), ModelDrawable::Emissive);
    }

    void setupMaterialTexture(ModelDrawable &model,
                              ModelDrawable::MeshId const &mesh,
                              Record const &matDef,
                              String const &textureName,
                              ModelDrawable::TextureMap map)
    {
        if (matDef.has(textureName))
        {
            String const path = ScriptedInfo::absolutePathInContext(matDef, matDef.gets(textureName));
            model.setTexturePath(mesh, map, path);
        }
    }

    DENG2_PIMPL_AUDIENCE(NewProgram)
};

DENG2_AUDIENCE_METHOD(ModelLoader, NewProgram)

ModelLoader::ModelLoader()
    : d(new Impl(this))
{}

ModelBank &ModelLoader::bank()
{
    return d->bank;
}

ModelBank const &ModelLoader::bank() const
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

String ModelLoader::shaderName(GLProgram const &program) const
{
    return static_cast<Impl::Program const &>(program).shaderName;
}

Record const &ModelLoader::shaderDefinition(GLProgram const &program) const
{
    return *static_cast<Impl::Program const &>(program).def;
}

int ModelLoader::identifierFromText(String const &text,
                                    std::function<int (String const &)> resolver) // static
{
    int id = 0;
    if (text.beginsWith('@'))
    {
        id = text.mid(1).toInt();
    }
    else
    {
        id = resolver(text);
    }
    return id;
}

} // namespace render

