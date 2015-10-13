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
#include "render/mobjanimator.h"
#include "render/rend_main.h"
#include "render/vissprite.h"
#include "gl/gl_main.h"
#include "world/p_players.h"
#include "world/clientmobjthinkerdata.h"
#include "clientapp.h"

#include <de/filesys/AssetObserver>
#include <de/App>
#include <de/ModelBank>
#include <de/ScriptedInfo>

#include <QHash>

using namespace de;

static String const DEF_ANIMATION   ("animation");
static String const DEF_MATERIAL    ("material");
static String const DEF_UP_VECTOR   ("up");
static String const DEF_FRONT_VECTOR("front");
static String const DEF_AUTOSCALE   ("autoscale");
static String const DEF_MIRROR      ("mirror");
static String const DEF_STATE       ("state");
static String const DEF_SEQUENCE    ("sequence");
static String const DEF_RENDER      ("render");
static String const DEF_TEXTURE_MAPPING("textureMapping");
static String const DEF_SHADER      ("shader");
static String const DEF_PASS        ("pass");
static String const DEF_ENABLED     ("enabled");
static String const DEF_MESHES      ("meshes");
static String const DEF_BLENDFUNC   ("blendFunc");
static String const DEF_BLENDOP     ("blendOp");
static String const DEF_TIMELINE    ("timeline");

static String const DEFAULT_SHADER  ("model.skeletal.normal_specular_emission");

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(Bank, Load)
, DENG2_OBSERVES(ModelDrawable, AboutToGLInit)
{
#define MAX_LIGHTS  4

    struct Program : public GLProgram
    {
        String shaderName;
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
            // Everything should have been unloaded.
            DENG2_ASSERT(empty());
        }
    };
    Programs programs;

    filesys::AssetObserver observer { "model\\..*" };
    ModelBank bank;
    std::unique_ptr<AtlasTexture> atlas;

    GLUniform uMvpMatrix        { "uMvpMatrix",        GLUniform::Mat4 };
    GLUniform uTex              { "uTex",              GLUniform::Sampler2D };
    GLUniform uEyePos           { "uEyePos",           GLUniform::Vec3 };
    GLUniform uAmbientLight     { "uAmbientLight",     GLUniform::Vec4 };
    GLUniform uLightDirs        { "uLightDirs",        GLUniform::Vec3Array, MAX_LIGHTS };
    GLUniform uLightIntensities { "uLightIntensities", GLUniform::Vec4Array, MAX_LIGHTS };
    Matrix4f inverseLocal;
    int lightCount = 0;
    Id defaultDiffuse  { Id::None };
    Id defaultNormals  { Id::None };
    Id defaultEmission { Id::None };
    Id defaultSpecular { Id::None };

    Instance(Public *i) : Base(i)
    {
        observer.audienceForAvailability() += this;
        bank.audienceForLoad() += this;
    }

    void init()
    {
        // The default shader is used whenever a model does not specifically
        // request another one.
        loadProgram(DEFAULT_SHADER);

        atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                    Atlas::DefaultFlags,
                    GLTexture::maximumSize().min(GLTexture::Size(4096, 4096))));
        atlas->setBorderSize(1);
        atlas->setMarginSize(0);

        // Fallback diffuse map (solid black).
        QImage img(QSize(1, 1), QImage::Format_ARGB32);
        img.fill(qRgba(0, 0, 0, 255));
        defaultDiffuse = atlas->alloc(img);

        // Fallback normal map for models who don't provide one.
        img.fill(qRgba(127, 127, 255, 255)); // z+
        defaultNormals = atlas->alloc(img);

        // Fallback emission map for models who don't have one.
        img.fill(qRgba(0, 0, 0, 0));
        defaultEmission = atlas->alloc(img);

        // Fallback specular map (no specular reflections).
        img.fill(qRgba(0, 0, 0, 0));
        defaultSpecular = atlas->alloc(img);

        uTex = *atlas;
    }

    void deinit()
    {
        // GL resources must be accessed from the main thread only.
        bank.unloadAll(Bank::ImmediatelyInCurrentThread);

        atlas.reset();
        unloadProgram(*programs[DEFAULT_SHADER]);
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
            // Unload additional resources associated with the model.
            auto entry = bank.modelAndData<AuxiliaryData const>(identifier);
            for(auto const &pass : entry.second->passes)
            {
                DENG2_ASSERT(pass.program);
                unloadProgram(*static_cast<Program *>(pass.program));
            }

            bank.remove(identifier);
        }
    }

    /**
     * Configures a ModelDrawable with the common texture atlas.
     *
     * @param model  Model to configure.
     */
    void setupModel(ModelDrawable &model)
    {
        if(atlas)
        {
            model.setAtlas(*atlas);

            model.setDefaultTexture(ModelDrawable::Diffuse,  defaultDiffuse);
            model.setDefaultTexture(ModelDrawable::Normals,  defaultNormals);
            model.setDefaultTexture(ModelDrawable::Emissive, defaultEmission);
            model.setDefaultTexture(ModelDrawable::Specular, defaultSpecular);
        }
        else
        {
            model.unsetAtlas();
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

        LOG_RES_VERBOSE("Loading model shader \"%s\"") << name;

        // Bind the mandatory common state.
        ClientApp::shaders().build(*prog, name)
                << uMvpMatrix
                << uTex
                << uEyePos
                << uAmbientLight
                << uLightDirs
                << uLightIntensities;

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
            programs.remove(name);
        }
    }

    void modelAboutToGLInit(ModelDrawable &model)
    {
        setupModel(model);
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
                                                  .arg(shaderDef.gets(ScriptedInfo::VAR_SOURCE)));
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
        ModelDrawable &model = bank.model(path);
        model.audienceForAboutToGLInit() += this;

        auto const asset = App::asset(path);

        std::unique_ptr<AuxiliaryData> aux(new AuxiliaryData);

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
        aux->cull = mirror? gl::Back : gl::Front;
        // Assimp's coordinate system uses different handedness than Doomsday,
        // so mirroring is needed.
        aux->transformation = Matrix4f::unnormalizedFrame(front, up, !mirror);
        aux->autoscaleToThingHeight = !ScriptedInfo::isFalse(asset, DEF_AUTOSCALE, false);

        // Custom texture maps.
        if(asset.has(DEF_MATERIAL))
        {
            auto mats = asset.subrecord(DEF_MATERIAL).subrecords();
            DENG2_FOR_EACH_CONST(Record::Subrecords, mat, mats)
            {
                Record const &matDef = *mat.value();
                handleMaterialTexture(model, mat.key(), matDef, "diffuseMap",  ModelDrawable::Diffuse);
                handleMaterialTexture(model, mat.key(), matDef, "normalMap",   ModelDrawable::Normals);
                handleMaterialTexture(model, mat.key(), matDef, "heightMap",   ModelDrawable::Height);
                handleMaterialTexture(model, mat.key(), matDef, "specularMap", ModelDrawable::Specular);
                handleMaterialTexture(model, mat.key(), matDef, "emissiveMap", ModelDrawable::Emissive);
            }
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
                    aux->animations[state.key()] << AnimSequence(key, *seqs[key]);
                }
            }

            // Timelines.
            auto timelines = ScriptedInfo::subrecordsOfType(DEF_TIMELINE, asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, timeline, timelines)
            {
                Scheduler *scheduler = new Scheduler;
                scheduler->addFromInfo(*timeline.value());
                aux->timelines[timeline.key()] = scheduler;
            }
        }

        ModelDrawable::Mapping textureMapping;

        // Rendering passes.
        if(asset.has(DEF_RENDER))
        {
            Record const &renderBlock = asset.subrecord(DEF_RENDER);
            String const modelShader = renderBlock.gets(DEF_SHADER, DEFAULT_SHADER);

            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
            for(String key : ScriptedInfo::sortRecordsBySource(passes))
            {
                try
                {
                    auto const &def = *passes[key];

                    ModelDrawable::Pass pass;
                    pass.meshes.resize(model.meshCount());
                    pass.name = key;
                    applyFlagOperation(pass.flags,
                                       ModelDrawable::Pass::Enabled,
                                       ScriptedInfo::isFalse(def, DEF_ENABLED, false)?
                                           UnsetFlags : SetFlags);

                    for(Value const *value : def.geta(DEF_MESHES).elements())
                    {
                        int meshId = identifierFromText(value->asText(), [&model] (String const &text) {
                            return model.meshId(text);
                        });
                        pass.meshes.setBit(meshId, true);
                    }

                    if(def.has(DEF_BLENDFUNC))
                    {
                        ArrayValue const &blendDef = def.geta(DEF_BLENDFUNC);
                        pass.blendFunc.first  = textToBlendFunc(blendDef.at(0).asText());
                        pass.blendFunc.second = textToBlendFunc(blendDef.at(1).asText());
                    }
                    pass.blendOp = textToBlendOp(def.gets(DEF_BLENDOP, "Add"));

                    String const passShader = def.gets(DEF_SHADER, modelShader);
                    pass.program = loadProgram(passShader);
                    composeTextureMappings(textureMapping,
                                           ClientApp::shaders()[passShader]);

                    aux->passes.append(pass);
                }
                catch(DefinitionError const &er)
                {
                    LOG_RES_ERROR("Error in rendering pass definition of asset \"%s\": %s")
                            << path << er.asText();
                }
            }
        }

        // Rendering passes will always have programs associated with them.
        // However, if there are no passes, we need to set up the default
        // shader for the entire model.
        if(aux->passes.isEmpty())
        {
            // Use the default shader (use count not incremented).
            model.setProgram(programs[DEFAULT_SHADER]);
            composeTextureMappings(textureMapping,
                                   ClientApp::shaders()[DEFAULT_SHADER]);
        }

        // Configure the texture mapping. Shaders used with the model must
        // be compatible with this mapping order (enforced above). The mapping
        // affects the order in which texture coordinates are packed into vertex
        // arrays.
        model.setTextureMapping(textureMapping);

        // Store the additional information in the bank.
        bank.setUserData(path, aux.release());
    }

    void handleMaterialTexture(ModelDrawable &model,
                               String const &matName,
                               Record const &matDef,
                               String const &textureName,
                               ModelDrawable::TextureMap map)
    {
        if(matDef.has(textureName))
        {
            String path = ScriptedInfo::absolutePathInContext(matDef, matDef.gets(textureName));

            int matId = identifierFromText(matName, [&model] (String const &text) {
                return model.materialId(text);
            });

            model.setTexturePath(matId, map, path);
        }
    }

    void setupLighting(VisEntityLighting const &lighting)
    {
        // Ambient color and lighting vectors.
        setAmbientLight(lighting.ambientColor * .8f);
        clearLights();
        ClientApp::renderSystem().forAllVectorLights(lighting.vLightListIdx,
                                                     [this] (VectorLightData const &vlight)
        {
            // Use this when drawing the model.
            addLight(vlight.direction.xzy(), vlight.color);
            return LoopContinue;
        });
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

    /**
     * Sets up the transformation matrices for an eye-space view. The eye position is
     * at (0, 0, 0).
     *
     * @param modelToLocal  Transformation from model space to the object's local space
     *                      (object's local frame in world space).
     * @param inverseLocal  Transformation from local space to model space, taking
     *                      the object's rotation in world space into account.
     * @param localToView   Transformation from local space to projected view space.
     */
    void setEyeSpaceTransformation(Matrix4f const &modelToLocal,
                                   Matrix4f const &inverseLocalMat,
                                   Matrix4f const &localToView)
    {
        uMvpMatrix   = localToView * modelToLocal;
        inverseLocal = inverseLocalMat;
        uEyePos      = inverseLocal * Vector3f();
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
        uLightIntensities.set(idx, Vector4f(intensity, intensity.max()));

        lightCount++;
    }

    template <typename Params> // generic to accommodate psprites and vispsprites
    void draw(Params const &p)
    {
        DENG2_ASSERT(p.auxData != nullptr);

        p.model->draw(
            p.animator,
            p.auxData->passes.isEmpty()? nullptr : &p.auxData->passes,

            // Callback for when the program changes:
            [&p] (GLProgram &program, ModelDrawable::ProgramBinding binding)
            {
                p.animator->bindUniforms(program,
                    binding == ModelDrawable::AboutToBind? MobjAnimator::Bind :
                                                           MobjAnimator::Unbind);
            },

            // Callback for each rendering pass:
            [&p] (ModelDrawable::Pass const &pass, ModelDrawable::PassState state)
            {
                p.animator->bindPassUniforms(*p.model->currentProgram(),
                    pass.name,
                    state == ModelDrawable::PassBegun? MobjAnimator::Bind :
                                                       MobjAnimator::Unbind);
            }
        );
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

ModelRenderer::AuxiliaryData const *ModelRenderer::auxiliaryData(DotPath const &modelId) const
{
    return d->bank.userData(modelId)->maybeAs<AuxiliaryData>();
}

ModelRenderer::StateAnims const *ModelRenderer::animations(DotPath const &modelId) const
{
    if(auto const *aux = auxiliaryData(modelId))
    {
        if(!aux->animations.isEmpty())
        {
            return &aux->animations;
        }
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
    gl::Cull culling = gl::Back;

    Vector3d const modelWorldOrigin = (spr.pose.origin + spr.pose.srvo).xzy();

    Matrix4f modelToLocal =
            Matrix4f::rotate(-90 + (spr.pose.viewAligned? spr.pose.yawAngleOffset :
                                                          spr.pose.yaw),
                             Vector3f(0, 1, 0) /* vertical axis for yaw */);
    Matrix4f localToView =
            Viewer_Matrix() *
            Matrix4f::translate(modelWorldOrigin) *
            Matrix4f::scale(Vector3f(1.0f, 1.0f/1.2f, 1.0f)); // Inverse aspect correction.

    if(p.object)
    {
        auto const &mobjData = THINKER_DATA(p.object->thinker, ClientMobjThinkerData);
        modelToLocal = modelToLocal * mobjData.modelTransformation();
        culling = mobjData.auxiliaryModelData().cull;
    }

    GLState::push().setCull(culling);

    // Set up a suitable matrix for the pose.
    d->setTransformation(Rend_EyeOrigin() - modelWorldOrigin, modelToLocal, localToView);

    // Ambient color and lighting vectors.
    d->setupLighting(spr.light);

    // Draw the model using the current animation state.
    d->draw(p);
    GLState::pop();

    /// @todo Something is interfering with the cull setting elsewhere (remove this).
    GLState::current().setCull(gl::Back).apply();
}

void ModelRenderer::render(vispsprite_t const &pspr)
{
    auto const &p = pspr.data.model2;

    Matrix4f modelToLocal =
            Matrix4f::rotate(180, Vector3f(0, 1, 0)) *
            p.auxData->transformation;

    Matrix4f localToView = GL_GetProjectionMatrix() * Matrix4f::translate(Vector3f(0, -10, 11));
    d->setEyeSpaceTransformation(modelToLocal,
                                 modelToLocal.inverse() *
                                 Matrix4f::rotate(vpitch, Vector3f(1, 0, 0)) *
                                 Matrix4f::rotate(vang,   Vector3f(0, 1, 0)),
                                 localToView);

    d->setupLighting(pspr.light);

    GLState::push().setCull(p.auxData->cull);
    d->draw(p);
    GLState::pop();

    /// @todo Something is interfering with the cull setting elsewhere (remove this).
    GLState::current().setCull(gl::Back).apply();
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

ModelRenderer::AnimSequence::AnimSequence(String const &name, Record const &def)
    : name(name)
    , def(&def)
{
    // Parse timeline events.
    if(def.hasSubrecord(DEF_TIMELINE))
    {
        timeline = new Scheduler;
        timeline->addFromInfo(def.subrecord(DEF_TIMELINE));
    }
    else if(def.hasMember(DEF_TIMELINE))
    {
        // Uses a shared timeline in the definition. This will be looked up when
        // the animation starts.
        sharedTimeline = def.gets(DEF_TIMELINE);
    }
}

ModelRenderer::AuxiliaryData::~AuxiliaryData()
{
    qDeleteAll(timelines.values());
}
