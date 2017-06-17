/** @file modelrenderer.cpp  Model renderer.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "render/modelloader.h"
#include "render/stateanimator.h"
#include "render/rend_main.h"
#include "render/vissprite.h"
#include "render/environ.h"
#include "render/rendersystem.h"
#include "gl/gl_main.h"
#include "world/p_players.h"
#include "world/p_object.h"
#include "world/bspleaf.h"
#include "world/clientmobjthinkerdata.h"
#include "world/convexsubspace.h"
#include "clientapp.h"

#include <de/App>
#include <de/ModelBank>
#include <de/NativePointerValue>

using namespace de;

static int constexpr MAX_LIGHTS = 4;

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(render::ModelLoader, NewProgram)
{
    render::ModelLoader loader;

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

    Impl(Public *i) : Base(i)
    {
        loader.audienceForNewProgram() += this;
        lightIntensityFactor.set(NumberValue(1.75));
    }

    void init()
    {
        loader.glInit();
    }

    void deinit()
    {
        loader.glDeinit();
    }

    void newProgramCreated(GLProgram &program)
    {
        program << uMvpMatrix
                << uReflectionMatrix
                << uTex
                << uReflectionTex
                << uEyePos
                << uAmbientLight
                << uLightDirs
                << uLightIntensities
                << uFogRange
                << uFogColor;
    }

    void setupLighting(VisEntityLighting const &lighting,
                       mobj_t const *excludeSourceMobj)
    {
        // Ambient color and lighting vectors.
        setAmbientLight(lighting.ambientColor * .6f);
        clearLights();
        ClientApp::renderSystem().forAllVectorLights(lighting.vLightListIdx,
                                                     [this, excludeSourceMobj]
                                                     (VectorLightData const &vlight)
        {
            if (excludeSourceMobj && vlight.sourceMobj == excludeSourceMobj)
            {
                // This source should not be included.
                return LoopContinue;
            }
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

        for (int i = 0; i < MAX_LIGHTS; ++i)
        {
            uLightDirs       .set(i, Vector3f());
            uLightIntensities.set(i, Vector4f());
        }
    }

    void addLight(Vector3f const &direction, Vector3f const &intensity)
    {
        if (lightCount == MAX_LIGHTS) return;

        int idx = lightCount;
        uLightDirs       .set(idx, (inverseLocal * direction).normalize());
        uLightIntensities.set(idx, Vector4f(intensity, intensity.max()) * lightIntensityFactor);

        lightCount++;
    }

    void setupFog()
    {
        if (fogParams.usingFog)
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

        if (preModelToLocal)
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

    void setReflectionForSubsector(world::Subsector const *subsec)
    {
        uReflectionTex = ClientApp::renderSystem()
                            .environment().reflectionInSubsector(subsec);
    }

    void setReflectionForObject(mobj_t const *object)
    {
        if (object && Mobj_HasSubsector(*object))
        {
            setReflectionForSubsector(&Mobj_Subsector(*object));
        }
        else
        {
            uReflectionTex = ClientApp::renderSystem()
                                 .environment().defaultReflection();
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

ModelRenderer::ModelRenderer() : d(new Impl(this))
{}

void ModelRenderer::glInit()
{
    d->init();
}

void ModelRenderer::glDeinit()
{
    d->deinit();
}

render::ModelLoader &ModelRenderer::loader()
{
    return d->loader;
}

render::ModelLoader const &ModelRenderer::loader() const
{
    return d->loader;
}

ModelBank &ModelRenderer::bank()
{
    return d->loader.bank();
}

render::Model::StateAnims const *ModelRenderer::animations(DotPath const &modelId) const
{
    auto const &model = d->loader.bank().model<render::Model const>(modelId);
    if (!model.animations.isEmpty())
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
                 spr.pose.yaw   + spr.pose.yawAngleOffset,
                 spr.pose.pitch + spr.pose.pitchAngleOffset,
                 mobjData? &mobjData->modelTransformation() : nullptr);

    // Ambient color and lighting vectors.
    d->setupLighting(spr.light, nullptr);

    // Draw the model using the current animation state.
    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop().apply();
}

void ModelRenderer::render(vispsprite_t const &pspr, mobj_t const *playerMobj)
{
    auto const &p = pspr.data.model2;
    world::ConvexSubspace const *sub = pspr.bspLeaf ? pspr.bspLeaf->subspacePtr() : nullptr;

    d->setReflectionForSubsector(sub ? sub->subsectorPtr() : nullptr);

    // Walk bobbing is specified using angle offsets.
    dfloat yaw   = vang   + p.yawAngleOffset;
    dfloat pitch = vpitch + p.pitchAngleOffset;

    Matrix4f eyeSpace = Matrix4f::rotate(180 - yaw, Vector3f(0, 1, 0))
                      * Matrix4f::rotate(pitch    , Vector3f(1, 0, 0));

    Matrix4f xform = p.model->transformation;

    d->setupPose(Rend_EyeOrigin(), eyeSpace * p.model->offset,
                 -90 - yaw, pitch,
                 &xform);

    d->setupLighting(pspr.light, playerMobj /* player holding weapon is excluded */);

    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop().apply();
}

//---------------------------------------------------------------------------------------

static render::StateAnimator &animatorInstance(Context &ctx)
{
    if (auto *self = maybeAs<NativePointerValue>(ctx.selfInstance().get(Record::VAR_NATIVE_SELF)))
    {
        if (auto *obj = self->nativeObject<render::StateAnimator>())
        {
            return *obj;
        }
    }
    throw Value::IllegalError("ModelRenderer::animatorInstance",
                              "Not a StateAnimator instance");
}

static Value *Function_StateAnimator_Thing(Context &ctx, Function::ArgumentValues const &)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    if (anim.ownerNamespaceName() == QStringLiteral("__thing__"))
    {
        return anim[anim.ownerNamespaceName()].value().duplicate();
    }
    return nullptr;
}

static Value *Function_StateAnimator_PlayingSequences(Context &ctx, Function::ArgumentValues const &)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    std::unique_ptr<ArrayValue> playing(new ArrayValue);
    for (int i = 0; i < anim.count(); ++i)
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
                << DENG2_FUNC_NOARG(StateAnimator_Thing,            "thing")
                << DENG2_FUNC_NOARG(StateAnimator_PlayingSequences, "playingSequences");
    }
}
