/** @file modelrenderer.cpp  Model renderer.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/modelrenderer.h"
#include "render/stateanimator.h"
#include "render/rend_main.h"
#include "render/vissprite.h"
#include "render/environ.h"
#include "gl/gl_main.h"
#include "world/p_players.h"
#include "world/clientmobjthinkerdata.h"
#include "clientapp.h"

#include <de/filesys/AssetObserver>
#include <de/App>
#include <de/ModelBank>
#include <de/ScriptedInfo>
#include <de/NativeValue>
#include <de/MultiAtlas>

#include <QHash>

using namespace de;

static String const DEF_ANIMATION   ("animation");
static String const DEF_MATERIAL    ("material");
static String const DEF_VARIANT     ("variant");
static String const DEF_UP_VECTOR   ("up");
static String const DEF_FRONT_VECTOR("front");
static String const DEF_AUTOSCALE   ("autoscale");
static String const DEF_MIRROR      ("mirror");
static String const DEF_OFFSET      ("offset");
static String const DEF_STATE       ("state");
static String const DEF_SEQUENCE    ("sequence");
static String const DEF_RENDER      ("render");
static String const DEF_TEXTURE_MAPPING("textureMapping");
static String const DEF_SHADER      ("shader");
static String const DEF_PASS        ("pass");
static String const DEF_MESHES      ("meshes");
static String const DEF_BLENDFUNC   ("blendFunc");
static String const DEF_BLENDOP     ("blendOp");
static String const DEF_DEPTHFUNC   ("depthFunc");
static String const DEF_DEPTHWRITE  ("depthWrite");
static String const DEF_TIMELINE    ("timeline");

static String const SHADER_DEFAULT  ("model.skeletal.generic");
static String const MATERIAL_DEFAULT("default");

static String const VAR_U_MAP_TIME  ("uMapTime");

static Atlas::Size const MAX_ATLAS_SIZE(8192, 8192);

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(Bank, Load)
, DENG2_OBSERVES(ModelDrawable, AboutToGLInit)
, public MultiAtlas::IAtlasFactory
{
#define MAX_LIGHTS  4

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
            for(auto i = constBegin(); i != constEnd(); ++i)
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

    MultiAtlas atlasPool { *this };

    filesys::AssetObserver observer { "model\\..*" };
    ModelBank bank { [] () -> ModelDrawable * { return new render::Model; } };

    GLUniform uMvpMatrix        { "uMvpMatrix",        GLUniform::Mat4 };
    GLUniform uReflectionMatrix { "uReflectionMatrix", GLUniform::Mat4 };
    GLUniform uTex              { "uTex",              GLUniform::Sampler2D };
    GLUniform uReflectionTex    { "uReflectionTex",    GLUniform::SamplerCube };
    GLUniform uEyePos           { "uEyePos",           GLUniform::Vec3 };
    GLUniform uAmbientLight     { "uAmbientLight",     GLUniform::Vec4 };
    GLUniform uLightDirs        { "uLightDirs",        GLUniform::Vec3Array, MAX_LIGHTS };
    GLUniform uLightIntensities { "uLightIntensities", GLUniform::Vec4Array, MAX_LIGHTS };
    GLUniform uFogRange         { "uFogRange",         GLUniform::Vec4 };
    GLUniform uFogColor         { "uFogColor",         GLUniform::Vec4 };

    Matrix4f inverseLocal; ///< Translation ignored, this is used for light vectors.
    int lightCount = 0;

    Variable lightIntensityFactor;

    Instance(Public *i) : Base(i)
    {
        lightIntensityFactor.set(NumberValue(1.75));

        observer.audienceForAvailability() += this;
        bank.audienceForLoad() += this;
    }

    void init()
    {
        // The default shader is used whenever a model does not specifically
        // request another one.
        loadProgram(SHADER_DEFAULT);
    }

    void deinit()
    {
        // GL resources must be accessed from the main thread only.
        bank.unloadAll(Bank::ImmediatelyInCurrentThread);

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

        return atlas;
    }

    void assetAvailabilityChanged(String const &identifier, filesys::AssetObserver::Event event)
    {
        LOG_RES_MSG("Model asset \"%s\" is now %s")
                << identifier
                << (event == filesys::AssetObserver::Added? "available" :
                                                            "unavailable");

        if(event == filesys::AssetObserver::Added)
        {
            bank.add(identifier, App::asset(identifier).absolutePath("path"));

            // Begin loading the model right away.
            bank.load(identifier);
        }
        else
        {
            auto const &model = bank.model<render::Model const>(identifier);

            // Unload programs used by the various rendering passes.
            for(auto const &pass : model.passes)
            {
                DENG2_ASSERT(pass.program);
                unloadProgram(*static_cast<Program *>(pass.program));
            }

            // Alternatively, the entire model may be using a single program.
            if(model.passes.isEmpty())
            {
                DENG2_ASSERT(model.program());
                unloadProgram(*static_cast<Program *>(model.program()));
            }
            else
            {
                DENG2_ASSERT(!model.program());
            }

            bank.remove(identifier);
        }
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
        if(programs.contains(name))
        {
            programs[name]->useCount++;
            return programs[name];
        }

        std::unique_ptr<Program> prog(new Program);
        prog->shaderName = name;
        prog->def = &ClientApp::shaders()[name].valueAsRecord(); // for lookups later

        LOG_RES_VERBOSE("Loading model shader \"%s\"") << name;

        // Bind the mandatory common state.
        ClientApp::shaders().build(*prog, name)
                << uMvpMatrix
                << uReflectionMatrix
                << uTex
                << uReflectionTex
                << uEyePos
                << uAmbientLight
                << uLightDirs
                << uLightIntensities
                << uFogRange
                << uFogColor;

        // Built-in special uniforms.
        if(prog->def->hasMember(VAR_U_MAP_TIME))
        {
            *prog << ClientApp::renderSystem().uMapTime();
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
        if(--program.useCount == 0)
        {
            String name = program.shaderName;
            LOG_RES_VERBOSE("Model shader \"%s\" unloaded (no more users)") << name;
            delete &program;
            DENG2_ASSERT(programs.contains(name));
            programs.remove(name);
        }
    }

    void modelAboutToGLInit(ModelDrawable &drawable)
    {
        auto &model = static_cast<render::Model &>(drawable);

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
        for(auto const &p : cs)
        {
            if(text == p.txt)
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
        for(auto const &p : bs)
        {
            if(text == p.txt)
            {
                return p.blend;
            }
        }
        throw DefinitionError("ModelRenderer::textToBlendFunc",
                              QString("Invalid blending function \"%1\"").arg(text));
    }

    static gl::BlendOp textToBlendOp(String const &text)
    {
        if(text == "Add") return gl::Add;
        if(text == "Subtract") return gl::Subtract;
        if(text == "ReverseSubtract") return gl::ReverseSubtract;
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
        if(shaderDef.has(DEF_TEXTURE_MAPPING))
        {
            ArrayValue const &array = shaderDef.geta(DEF_TEXTURE_MAPPING);
            for(int i = 0; i < int(array.size()); ++i)
            {
                ModelDrawable::TextureMap map = ModelDrawable::textToTextureMap(array.element(i).asText());
                if(i == mapping.size())
                {
                    mapping << map;
                }
                else if(mapping.at(i) != map)
                {
                    // Must match what the shader expects to receive.
                    QStringList list;
                    for(auto map : mapping) list << ModelDrawable::textureMapToText(map);
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
    void bankLoaded(DotPath const &path)
    {
        // Models use the shared atlas.
        render::Model &model = bank.model<render::Model>(path);
        model.audienceForAboutToGLInit() += this;

        auto const asset = App::asset(path);

        // Determine the coordinate system of the model.
        Vector3f front(0, 0, 1);
        Vector3f up   (0, 1, 0);
        if(asset.has(DEF_FRONT_VECTOR))
        {
            front = Vector3f(asset.geta(DEF_FRONT_VECTOR));
        }
        if(asset.has(DEF_UP_VECTOR))
        {
            up = Vector3f(asset.geta(DEF_UP_VECTOR));
        }
        bool mirror = ScriptedInfo::isTrue(asset, DEF_MIRROR);
        model.cull = mirror? gl::Back : gl::Front;
        // Assimp's coordinate system uses different handedness than Doomsday,
        // so mirroring is needed.
        model.transformation = Matrix4f::frame(front, up, !mirror);
        if(asset.has(DEF_OFFSET))
        {
            model.offset = vectorFromValue<Vector3f>(asset.get(DEF_OFFSET));
        }
        model.autoscaleToThingHeight = ScriptedInfo::isTrue(asset, DEF_AUTOSCALE);

        // Custom texture maps and additional materials.
        model.materialIndexForName.insert(MATERIAL_DEFAULT, 0);
        if(asset.has(DEF_MATERIAL))
        {
            asset.subrecord(DEF_MATERIAL).forSubrecords(
                [this, &model] (String const &blockName, Record const &block)
            {
                if(ScriptedInfo::blockType(block) == DEF_VARIANT)
                {
                    String const materialName = blockName;
                    if(!model.materialIndexForName.contains(materialName))
                    {
                        // Add a new material.
                        model.materialIndexForName.insert(materialName, model.addMaterial());
                    }
                    block.forSubrecords([this, &model, &materialName]
                                        (String const &matName, Record const &matDef)
                    {
                        setupMaterial(model, matName, model.materialIndexForName[materialName], matDef);
                        return LoopContinue;
                    });
                }
                else
                {
                    // The default material.
                    setupMaterial(model, blockName, 0, block);
                }
                return LoopContinue;
            });
        }

        // Set up the animation sequences for states.
        if(asset.has(DEF_ANIMATION))
        {
            auto states = ScriptedInfo::subrecordsOfType(DEF_STATE, asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, state, states)
            {
                // Sequences are added in source order.
                auto seqs = ScriptedInfo::subrecordsOfType(DEF_SEQUENCE, *state.value());
                for(String key : ScriptedInfo::sortRecordsBySource(seqs))
                {
                    model.animations[state.key()] << render::Model::AnimSequence(key, *seqs[key]);
                }
            }

            // Timelines.
            auto timelines = ScriptedInfo::subrecordsOfType(DEF_TIMELINE, asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, timeline, timelines)
            {
                Scheduler *scheduler = new Scheduler;
                scheduler->addFromInfo(*timeline.value());
                model.timelines[timeline.key()] = scheduler;
            }
        }

        ModelDrawable::Mapping textureMapping;
        String modelShader = SHADER_DEFAULT;

        // Rendering passes.
        if(asset.has(DEF_RENDER))
        {
            Record const &renderBlock = asset.subrecord(DEF_RENDER);
            modelShader = renderBlock.gets(DEF_SHADER, modelShader);

            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
            for(String key : ScriptedInfo::sortRecordsBySource(passes))
            {
                try
                {
                    auto const &def = *passes[key];

                    ModelDrawable::Pass pass;
                    pass.name = key;

                    pass.meshes.resize(model.meshCount());
                    for(Value const *value : def.geta(DEF_MESHES).elements())
                    {
                        int meshId = identifierFromText(value->asText(), [&model] (String const &text) {
                            return model.meshId(text);
                        });
                        pass.meshes.setBit(meshId, true);
                    }

                    // GL state parameters.
                    if(def.has(DEF_BLENDFUNC))
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
                catch(Error const &er)
                {
                    LOG_RES_ERROR("Rendering pass \"%s\" in asset \"%s\" is invalid: %s")
                            << key << path << er.asText();
                }
            }
        }

        // Rendering passes will always have programs associated with them.
        // However, if there are no passes, we need to set up the default
        // shader for the entire model.
        if(model.passes.isEmpty())
        {
            try
            {
                model.setProgram(loadProgram(modelShader));
                composeTextureMappings(textureMapping,
                                       ClientApp::shaders()[modelShader]);
            }
            catch(Error const &er)
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
    }

    void setupMaterial(ModelDrawable &model,
                       String const &meshName,
                       duint materialIndex,
                       Record const &matDef)
    {
        ModelDrawable::MeshId const mesh {
            (duint) identifierFromText(meshName, [&model] (String const &text) {
                return model.meshId(text); }),
            materialIndex
        };

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
        if(matDef.has(textureName))
        {
            String const path = ScriptedInfo::absolutePathInContext(matDef, matDef.gets(textureName));
            model.setTexturePath(mesh, map, path);
        }
    }

    void setupLighting(VisEntityLighting const &lighting)
    {
        // Ambient color and lighting vectors.
        setAmbientLight(lighting.ambientColor * .6f);
        clearLights();
        ClientApp::renderSystem().forAllVectorLights(lighting.vLightListIdx,
                                                     [this] (VectorLightData const &vlight)
        {
            // Use this when drawing the model.
            addLight(vlight.direction.xzy(), vlight.color);
            return LoopContinue;
        });
    }

    void setAmbientLight(Vector3f const &ambientIntensity)
    {
        uAmbientLight = Vector4f(ambientIntensity, 1.f);
    }

    void clearLights()
    {
        lightCount = 0;

        for(int i = 0; i < MAX_LIGHTS; ++i)
        {
            uLightDirs       .set(i, Vector3f());
            uLightIntensities.set(i, Vector4f());
        }
    }

    void addLight(Vector3f const &direction, Vector3f const &intensity)
    {
        if(lightCount == MAX_LIGHTS) return;

        int idx = lightCount;
        uLightDirs       .set(idx, (inverseLocal * direction).normalize());
        uLightIntensities.set(idx, Vector4f(intensity, intensity.max()) * lightIntensityFactor);

        lightCount++;
    }

    void setupFog()
    {
        if(fogParams.usingFog)
        {
            uFogColor = Vector4f(fogParams.fogColor[0],
                                 fogParams.fogColor[1],
                                 fogParams.fogColor[2],
                                 1.f);

            Rangef const depthPlanes = GL_DepthClipRange();
            float const fogDepth = fogParams.fogEnd - fogParams.fogStart;
            uFogRange = Vector4f(fogParams.fogStart,
                                 fogDepth,
                                 depthPlanes.start,
                                 depthPlanes.end);
        }
        else
        {
            uFogColor = Vector4f();
        }
    }

    void setupPose(Vector3d const &modelWorldOrigin,
                   Vector3f const &modelOffset,
                   float yawAngle,
                   float pitchAngle,
                   Matrix4f const *preModelToLocal = nullptr)
    {
        Vector3f const aspectCorrect(1.0f, 1.0f/1.2f, 1.0f);
        Vector3d origin = modelWorldOrigin + modelOffset * aspectCorrect;

        Matrix4f modelToLocal =
                Matrix4f::rotate(-90 + yawAngle, Vector3f(0, 1, 0) /* vertical axis for yaw */) *
                Matrix4f::rotate(pitchAngle,     Vector3f(1, 0, 0));

        Vector3f relativePos = Rend_EyeOrigin() - origin;

        uReflectionMatrix = Matrix4f::rotate(-yawAngle,  Vector3f(0, 1, 0)) *
                            Matrix4f::rotate(pitchAngle, Vector3f(0, 0, 1));

        if(preModelToLocal)
        {
            modelToLocal = modelToLocal * (*preModelToLocal);
        }

        Matrix4f localToView =
                Viewer_Matrix() *
                Matrix4f::translate(origin) *
                Matrix4f::scale(aspectCorrect); // Inverse aspect correction.

        // Set up a suitable matrix for the pose.
        setTransformation(relativePos, modelToLocal, localToView);
    }

    /**
     * Sets up the transformation matrices.
     *
     * @param relativeEyePos  Position of the eye in relation to object (in world space).
     * @param modelToLocal    Transformation from model space to the object's local space
     *                        (object's local frame in world space).
     * @param localToView     Transformation from local space to projected view space.
     */
    void setTransformation(Vector3f const &relativeEyePos,
                           Matrix4f const &modelToLocal,
                           Matrix4f const &localToView)
    {
        uMvpMatrix   = localToView * modelToLocal;
        inverseLocal = modelToLocal.inverse();
        uEyePos      = inverseLocal * relativeEyePos;
    }

    void setReflectionForBspLeaf(BspLeaf const *bspLeaf)
    {
        uReflectionTex = ClientApp::renderSystem().environment().
                         reflectionInBspLeaf(bspLeaf);
    }

    void setReflectionForObject(mobj_t const *object)
    {
        if(object)
        {
            setReflectionForBspLeaf(&Mobj_BspLeafAtOrigin(*object));
        }
        else
        {
            uReflectionTex = ClientApp::renderSystem().environment().defaultReflection();
        }
    }

    template <typename Params> // generic to accommodate psprites and vispsprites
    void draw(Params const &p)
    {
        setupFog();
        uTex = static_cast<AtlasTexture const *>(p.model->textures->atlas());

        p.model->draw(&p.animator->appearance(), p.animator);
    }
};

ModelRenderer::ModelRenderer() : d(new Instance(this))
{}

void ModelRenderer::glInit()
{
    d->init();
}

void ModelRenderer::glDeinit()
{
    d->deinit();
}

ModelBank &ModelRenderer::bank()
{
    return d->bank;
}

render::Model::StateAnims const *ModelRenderer::animations(DotPath const &modelId) const
{
    auto const &model = d->bank.model<render::Model const>(modelId);
    if(!model.animations.isEmpty())
    {
        return &model.animations;
    }
    return nullptr;
}

void ModelRenderer::render(vissprite_t const &spr)
{
    /*
     * Work in progress:
     *
     * Here is the contact point between the old renderer and the new GL2 model renderer.
     * In the future, vissprites should form a class hierarchy, and the entire drawing
     * operation should be encapsulated within. This will allow drawing a model (or a
     * sprite, etc.) by creating a VisSprite instance and telling it to draw itself.
     */

    drawmodel2params_t const &p = spr.data.model2;

    auto const *mobjData = (p.object? &THINKER_DATA(p.object->thinker, ClientMobjThinkerData) :
                                      nullptr);

    // Use the reflection cube map appropriate for the object's location.
    d->setReflectionForObject(p.object);

    d->setupPose((spr.pose.origin + spr.pose.srvo).xzy(),
                 p.model->offset,
                 /*Timer_RealSeconds()*20 +*/ (spr.pose.viewAligned? spr.pose.yawAngleOffset : spr.pose.yaw),
                 0 /*Timer_RealSeconds()*50*/ /* pitch */,
                 mobjData? &mobjData->modelTransformation() : nullptr);

    // Ambient color and lighting vectors.
    d->setupLighting(spr.light);

    // Draw the model using the current animation state.
    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop().apply();
}

void ModelRenderer::render(vispsprite_t const &pspr)
{
    auto const &p = pspr.data.model2;

    d->setReflectionForBspLeaf(pspr.bspLeaf);

    // Walk bobbing is specified using angle offsets.
    float yaw   = vang + p.yawAngleOffset;
    float pitch = vpitch + p.pitchAngleOffset;

    Matrix4f eyeSpace =
            Matrix4f::rotate(180 - yaw, Vector3f(0, 1, 0)) *
            Matrix4f::rotate(pitch, Vector3f(1, 0, 0));

    Matrix4f xform = p.model->transformation;

    d->setupPose(Rend_EyeOrigin(), eyeSpace * p.model->offset,
                 -90 - yaw, pitch,
                 &xform);

    d->setupLighting(pspr.light);

    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop().apply();
}

String ModelRenderer::shaderName(GLProgram const &program) const
{
    return static_cast<Instance::Program const &>(program).shaderName;
}

Record const &ModelRenderer::shaderDefinition(GLProgram const &program) const
{
    return *static_cast<Instance::Program const &>(program).def;
}

int ModelRenderer::identifierFromText(String const &text,
                                      std::function<int (String const &)> resolver) // static
{
    /// @todo This might be useful on a more general level, outside ModelRenderer. -jk

    int id = 0;
    if(text.beginsWith('@'))
    {
        id = text.mid(1).toInt();
    }
    else
    {
        id = resolver(text);
    }
    return id;
}

//---------------------------------------------------------------------------------------

static render::StateAnimator &animatorInstance(Context &ctx)
{
    if(auto *self = ctx.selfInstance().get(Record::VAR_NATIVE_SELF).maybeAs<NativeValue>())
    {
        if(auto *obj = self->nativeObject<render::StateAnimator>())
        {
            return *obj;
        }
    }
    throw Value::IllegalError("ModelRenderer::animatorInstance",
                              "No StateAnimator instance available");
}

static Value *Function_StateAnimator_PlayingSequences(Context &ctx, Function::ArgumentValues const &)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    std::unique_ptr<ArrayValue> playing(new ArrayValue);
    for(int i = 0; i < anim.count(); ++i)
    {
        playing->add(new NumberValue(anim.at(i).animId));
    }
    return playing.release();
}

void ModelRenderer::initBindings(Binder &binder, Record &module) // static
{
    // StateAnimator
    {
        Record &anim = module.addSubrecord("StateAnimator");
        binder.init(anim)
                << DENG2_FUNC_NOARG(StateAnimator_PlayingSequences, "playingSequences");
    }
}
