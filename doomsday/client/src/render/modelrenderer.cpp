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
#include "gl/gl_main.h"
#include "render/rend_main.h"
#include "world/p_players.h"
#include "clientapp.h"

#include <de/filesys/AssetObserver>
#include <de/App>
#include <de/ModelBank>
#include <de/ScriptedInfo>

using namespace de;

static String const DEF_ANIMATION("animation");
static String const DEF_UP_VECTOR("up");
static String const DEF_FRONT_VECTOR("front");

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(Bank, Load)
{
#define MAX_LIGHTS  4

    filesys::AssetObserver observer;
    ModelBank bank;
    std::unique_ptr<AtlasTexture> atlas;
    GLProgram program; /// @todo Specific models may want to use a custom program.
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uLightDirs;
    GLUniform uLightIntensities;
    Matrix4f inverseLocal;
    int lightCount;

    Instance(Public *i)
        : Base(i)
        , observer("model\\..*") // all model assets
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uTex      ("uTex",       GLUniform::Sampler2D)
        , uLightDirs("uLightDirs", GLUniform::Vec4Array, MAX_LIGHTS)
        , uLightIntensities("uLightIntensities", GLUniform::Vec4Array, MAX_LIGHTS)
        , lightCount(0)
    {
        observer.audienceForAvailability() += this;
        bank.audienceForLoad() += this;
    }

    void init()
    {
        ClientApp::shaders().build(program, "model.skeletal.normal_emission")
                << uMvpMatrix
                << uTex
                << uLightDirs
                << uLightIntensities;

        atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                    Atlas::DefaultFlags,
                    GLTexture::maximumSize().min(GLTexture::Size(4096, 4096))));

        uTex = *atlas;

        // All loaded items should use this atlas.
        bank.iterate([this] (DotPath const &path)
        {
            if(bank.isLoaded(path))
            {
                setupModel(bank.model(path));
            }
        });
    }

    void deinit()
    {
        // GL resources must be accessed from the main thread only.
        bank.unloadAll(Bank::ImmediatelyInCurrentThread);

        atlas.reset();
        program.clear();
    }

    void assetAvailabilityChanged(String const &identifier, filesys::AssetObserver::Event event)
    {
        //qDebug() << "loading model:" << identifier << event;

        if(event == filesys::AssetObserver::Added)
        {
            bank.add(identifier, App::asset(identifier).absolutePath("path"));

            // Begin loading the model right away.
            bank.load(identifier);
        }
        else
        {
            bank.remove(identifier);
        }
    }

    /**
     * Configures a ModelDrawable with the appropriate atlas and GL program.
     *
     * @param model  Model to configure.
     */
    void setupModel(ModelDrawable &model)
    {
        if(atlas)
        {
            model.setAtlas(*atlas);
        }
        else
        {
            model.unsetAtlas();
        }
        model.setProgram(program);
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
        setupModel(bank.model(path));

        auto const asset = App::asset(path);

        // Prepare the animations for the model.
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
        aux->transformation = Matrix4f::frame(front, up);

        // Set up the animation sequences for states.
        if(asset.has(DEF_ANIMATION))
        {
            auto states = ScriptedInfo::subrecordsOfType("state", asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, state, states)
            {
                // Note that the sequences are added in alphabetical order.
                auto seqs = ScriptedInfo::subrecordsOfType("sequence", *state.value());
                DENG2_FOR_EACH_CONST(Record::Subrecords, seq, seqs)
                {
                    aux->animations[state.key()] << AnimSequence(seq.key(), *seq.value());
                }
            }

            // TODO: Check for a possible timeline and calculate time factors accordingly.
        }

        // Store the animation sequence lookup in the bank.
        bank.setUserData(path, aux.release());
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

ModelRenderer::StateAnims const *ModelRenderer::animations(DotPath const &modelId) const
{
    if(auto const *aux = d->bank.userData(modelId)->maybeAs<AuxiliaryData>())
    {
        if(!aux->animations.isEmpty())
        {
            return &aux->animations;
        }
    }
    return 0;
}

void ModelRenderer::setTransformation(Matrix4f const &modelToLocal, Matrix4f const &localToView)
{
    d->uMvpMatrix   = localToView * modelToLocal;
    d->inverseLocal = modelToLocal.inverse();
}

void ModelRenderer::clearLights()
{
    d->lightCount = 0;

    for(int i = 0; i < MAX_LIGHTS; ++i)
    {
        d->uLightDirs       .set(i, Vector4f());
        d->uLightIntensities.set(i, Vector4f());
    }
}

void ModelRenderer::addLight(Vector3f const &direction, Vector3f const &intensity)
{
    if(d->lightCount == MAX_LIGHTS) return;

    int idx = d->lightCount;
    d->uLightDirs       .set(idx, Vector4f((d->inverseLocal * direction).normalize(), 1));
    d->uLightIntensities.set(idx, Vector4f(intensity, 0));

    d->lightCount++;
}

/*GLUniform &ModelRenderer::uMvpMatrix()
{
    return d->uMvpMatrix;
}*/
