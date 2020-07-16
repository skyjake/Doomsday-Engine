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
#include "world/clientmobjthinkerdata.h"
#include "world/clientworld.h"
#include "world/convexsubspace.h"
#include "clientapp.h"

#include <doomsday/world/bspleaf.h>
#include <de/App>
#include <de/ModelBank>
#include <de/NativePointerValue>
#include <de/TextValue>

using namespace de;

static int constexpr MAX_LIGHTS = 4;

float weaponFixedFOV = 95.f;

DE_PIMPL(ModelRenderer)
, DE_OBSERVES(render::ModelLoader, NewProgram)
{
    render::ModelLoader loader;

    GLUniform uMvpMatrix        { "uMvpMatrix",        GLUniform::Mat4 };
    GLUniform uWorldMatrix      { "uWorldMatrix",      GLUniform::Mat4 }; // included in uMvpMatrix
    GLUniform uReflectionMatrix { "uReflectionMatrix", GLUniform::Mat4 };
    GLUniform uTex              { "uTex",              GLUniform::Sampler2D };
    GLUniform uReflectionTex    { "uReflectionTex",    GLUniform::SamplerCube };
    GLUniform uEyePos           { "uEyePos",           GLUniform::Vec3 };
    GLUniform uAmbientLight     { "uAmbientLight",     GLUniform::Vec4 };
    GLUniform uLightDirs        { "uLightDirs",        GLUniform::Vec3Array, MAX_LIGHTS };
    GLUniform uLightIntensities { "uLightIntensities", GLUniform::Vec4Array, MAX_LIGHTS };
    GLUniform uFogRange         { "uFogRange",         GLUniform::Vec4 };
    GLUniform uFogColor         { "uFogColor",         GLUniform::Vec4 };

    Mat4f inverseLocal; ///< Translation ignored, this is used for light vectors.
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
                << uWorldMatrix
                << uTex
                << uReflectionTex
                << uEyePos
                << uAmbientLight
                << uLightDirs
                << uLightIntensities
                << uFogRange
                << uFogColor;
    }

    void setupLighting(const VisEntityLighting &lighting,
                       const mobj_t *excludeSourceMobj,
                       float alpha = 1.0f)
    {
        // Ambient color and lighting vectors.
        setAmbientLight(lighting.ambientColor * .6f, alpha);
        clearLights();
        ClientApp::render().forAllVectorLights(lighting.vLightListIdx,
                                                     [this, excludeSourceMobj]
                                                     (const VectorLightData &vlight)
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

        // Modify the ambient light according to model configuration.
        /*
        {
            Vector4f ambient = uAmbientLight;
            if (model.flags & render::Model::ThingAlphaAsAmbientLightAlpha)
            {
                ambient.w = p.alpha;
    }
            if (model.flags & render::Model::ThingFullBrightAsAmbientLight)
            {
                ambient = {1.0f, 1.0f, 1.0f, ambient.w};
            }
            uAmbientLight = ambient;
        }
*/
    }

    void setAmbientLight(const Vec3f &ambientIntensity, float alpha)
    {
        uAmbientLight = Vec4f(ambientIntensity, alpha);
    }

    void clearLights()
    {
        lightCount = 0;

        for (int i = 0; i < MAX_LIGHTS; ++i)
        {
            uLightDirs       .set(i, Vec3f());
            uLightIntensities.set(i, Vec4f());
        }
    }

    void addLight(const Vec3f &direction, const Vec3f &intensity)
    {
        if (lightCount == MAX_LIGHTS) return;

        int idx = lightCount;
        uLightDirs       .set(idx, (inverseLocal * direction).normalize());
        uLightIntensities.set(idx, Vec4f(intensity, intensity.max()) * lightIntensityFactor);

        lightCount++;
    }

    void setupPose(const Vec3d &modelWorldOrigin,
                   const Vec3f &modelOffset,
                   float yawAngle,
                   float pitchAngle,
                   float fixedFOVAngle, /* zero to use default FOV */
                   bool usePSpriteClipPlane,
                   const Mat4f *preModelToLocal = nullptr)
    {
        Vec3f const aspectCorrect(1.0f, 1.0f/1.2f, 1.0f);
        Vec3d origin = modelWorldOrigin + modelOffset * aspectCorrect;

        // "local" == world space but with origin at model origin

        Mat4f modelToLocal =
                Mat4f::rotate(-90 + yawAngle, Vec3f(0, 1, 0) /* vertical axis for yaw */) *
                Mat4f::rotate(pitchAngle,     Vec3f(1, 0, 0));

        Vec3f relativePos = Rend_EyeOrigin() - origin;

        uReflectionMatrix = Mat4f::rotate(-yawAngle,  Vec3f(0, 1, 0)) *
                            Mat4f::rotate(pitchAngle, Vec3f(0, 0, 1));

        if (preModelToLocal)
        {
            modelToLocal = modelToLocal * (*preModelToLocal);
        }

        const Mat4f localToWorld =
                Mat4f::translate(origin) *
                Mat4f::scale(aspectCorrect); // Inverse aspect correction.

        const Mat4f viewProj =
            Rend_GetProjectionMatrix(
                fixedFOVAngle,
                usePSpriteClipPlane ? 0.1f /* near plane distance: IssueID #2373 */ : 1.0f) *
            ClientApp::render().uViewMatrix().toMat4f();

        const Mat4f localToScreen = viewProj * localToWorld;

        uWorldMatrix = localToWorld * modelToLocal;

        // Set up a suitable matrix for the pose.
        setTransformation(relativePos, modelToLocal, localToScreen);
    }

    /**
     * Sets up the transformation matrices.
     *
     * "Local space" is the same as world space but relative to the object's origin.
     * That is, (0,0,0) being the object's origin.
     *
     * @param relativeEyePos  Position of the eye in relation to object (in world space).
     * @param modelToLocal    Transformation from model space to the object's local space
     *                        (object's local frame in world space).
     * @param localToScreen   Transformation from local space to screen (projected 2D) space.
     */
    void setTransformation(const Vec3f &relativeEyePos,
                           const Mat4f &modelToLocal,
                           const Mat4f &localToScreen)
    {
        uMvpMatrix   = localToScreen * modelToLocal;
        inverseLocal = modelToLocal.inverse();
        uEyePos      = inverseLocal * relativeEyePos;
    }

    void setReflectionForSubsector(const world::Subsector *subsec)
    {
        uReflectionTex = ClientApp::render()
                            .environment().reflectionInSubsector(subsec);
    }

    void setReflectionForObject(const mobj_t *object)
    {
        if (object && Mobj_HasSubsector(*object))
        {
            setReflectionForSubsector(&Mobj_Subsector(*object));
        }
        else
        {
            uReflectionTex = ClientApp::render()
                                 .environment().defaultReflection();
        }
    }

    template <typename Params> // generic to accommodate psprites and vispsprites
    void draw(const Params &p)
    {
        DGL_FogParams(uFogRange, uFogColor);
        uTex = static_cast<const AtlasTexture *>(p.model->textures->atlas());

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

const render::ModelLoader &ModelRenderer::loader() const
{
    return d->loader;
}

ModelBank &ModelRenderer::bank()
{
    return d->loader.bank();
}

const render::Model::StateAnims *ModelRenderer::animations(const DotPath &modelId) const
{
    const auto &model = d->loader.bank().model<render::Model const>(modelId);
    if (!model.animations.isEmpty())
    {
        return &model.animations;
    }
    return nullptr;
}

void ModelRenderer::render(const vissprite_t &spr)
{
    /*
     * Work in progress:
     *
     * Here is the contact point between the old renderer and the new GL2 model renderer.
     * In the future, vissprites should form a class hierarchy, and the entire drawing
     * operation should be encapsulated within. This will allow drawing a model (or a
     * sprite, etc.) by creating a VisSprite instance and telling it to draw itself.
     */

    const drawmodel2params_t &p = spr.data.model2;

    const auto *mobjData = (p.object? &THINKER_DATA(p.object->thinker, ClientMobjThinkerData) :
                                      nullptr);

    // Use the reflection cube map appropriate for the object's location.
    d->setReflectionForObject(p.object);

    d->setupPose((spr.pose.origin + spr.pose.srvo).xzy(),
                 p.model->offset,
                 spr.pose.yaw + spr.pose.yawAngleOffset,
                 spr.pose.pitch + spr.pose.pitchAngleOffset,
                 0.0f, /* regular FOV */
                 false, /* regular clip planes */
                 mobjData ? &mobjData->modelTransformation() : nullptr);

    // Ambient color and lighting vectors.
    d->setupLighting(spr.light, nullptr);

    // Draw the model using the current animation state.
    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop();
}

void ModelRenderer::render(const vispsprite_t &pspr, const mobj_t *playerMobj)
{
    const auto &p = pspr.data.model2;
    const world::ConvexSubspace *sub = pspr.bspLeaf ? pspr.bspLeaf->subspacePtr() : nullptr;

    d->setReflectionForSubsector(sub ? sub->subsectorPtr() : nullptr);

    // Walk bobbing is specified using angle offsets.
    dfloat yaw   = vang   + p.yawAngleOffset;
    dfloat pitch = vpitch + p.pitchAngleOffset;

    Mat4f eyeSpace = Mat4f::rotate(180 - yaw, Vec3f(0, 1, 0))
                      * Mat4f::rotate(pitch    , Vec3f(1, 0, 0));

    Mat4f xform = p.model->transformation;

    d->setupPose(Rend_EyeOrigin(),
                 eyeSpace * p.model->offset,
                 -90 - yaw,
                 pitch,
                 p.model->pspriteFOV > 0 ? p.model->pspriteFOV : weaponFixedFOV,
                 true, /* nearby clip planes */
                 &xform);

    float opacity = 1.0f;
    if (p.model->flags & render::Model::ThingOpacityAsAmbientLightAlpha)
    {
        opacity = pspr.alpha;
    }

    d->setupLighting(pspr.light, playerMobj /* player holding weapon is excluded */, opacity);

    if (p.model->flags & render::Model::ThingFullBrightAsAmbientLight)
    {
        d->setAmbientLight({1.0f, 1.0f, 1.0f}, opacity);
    }

    GLState::push().setCull(p.model->cull);
    d->draw(p);
    GLState::pop();
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

static Value *Function_StateAnimator_Thing(Context &ctx, const Function::ArgumentValues &)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    if (anim.ownerNamespaceName() == DE_STR("__thing__"))
    {
        return anim[anim.ownerNamespaceName()].value().duplicate();
    }
    return nullptr;
}

static Value *Function_StateAnimator_PlayingSequences(Context &ctx, const Function::ArgumentValues &)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    std::unique_ptr<ArrayValue> playing(new ArrayValue);
    for (int i = 0; i < anim.count(); ++i)
    {
        playing->add(new NumberValue(anim.at(i).animId));
    }
    return playing.release();
}

static Value *Function_StateAnimator_StartSequence(Context &ctx, const Function::ArgumentValues &args)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    int animId = anim.animationId(args.at(0)->asText());
    if (animId >= 0)
    {
        int priority = args.at(1)->asInt();
        bool looping = args.at(2)->isTrue();
        String node  = args.at(3)->asText();

        anim.startAnimation(animId, priority, looping, node);
    }
    else
    {
        LOG_SCR_ERROR("%s has no animation \"%s\"")
                << anim.objectNamespace().gets("ID")
                << args.at(0)->asText();
    }
    return nullptr;
}

static Value *Function_StateAnimator_StartTimeline(Context &ctx, const Function::ArgumentValues &args)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    const render::Model &model = anim.model();
    const String timelineName = args.first()->asText();
    if (model.timelines.contains(timelineName))
    {
        anim.scheduler().start(*model.timelines[timelineName],
                               &anim.objectNamespace(),
                               timelineName);
        return new TextValue(timelineName);
    }
    else
    {
        LOG_SCR_ERROR("%s has no timeline \"%s\"")
            << anim.objectNamespace().gets("ID") << timelineName;
    }
    return nullptr;
}

static Value *Function_StateAnimator_StopTimeline(Context &ctx, const Function::ArgumentValues &args)
{
    render::StateAnimator &anim = animatorInstance(ctx);
    anim.scheduler().stop(args.first()->asText());
    return nullptr;
}

void ModelRenderer::initBindings(Binder &binder, Record &module) // static
{
    // StateAnimator
    {
        Record &anim = module.addSubrecord("StateAnimator");
        binder.init(anim)
                << DE_FUNC_NOARG(StateAnimator_Thing,            "thing")
                << DE_FUNC_NOARG(StateAnimator_PlayingSequences, "playingSequences")
                << DE_FUNC_DEFS (StateAnimator_StartSequence,    "startSequence",
                                    "sequence" << "priority" << "looping" << "node",
                                    Function::Defaults({ std::make_pair("priority", new NumberValue(0)),
                                                         std::make_pair("looping",  new NumberValue(0)),
                                                         std::make_pair("node",     new TextValue) }))
                << DE_FUNC      (StateAnimator_StartTimeline,    "startTimeline", "name")
                << DE_FUNC      (StateAnimator_StopTimeline,     "stopTimeline", "name");
    }
}
