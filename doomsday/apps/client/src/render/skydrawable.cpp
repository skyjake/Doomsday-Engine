/** @file skydrawable.cpp  Drawable specialized for the sky.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "render/skydrawable.h"

#include "clientapp.h"
#include "client/cl_def.h" // clientPaused
#include "gl/gl_main.h"
#include "gl/gl_tex.h"
#include "MaterialVariantSpec"
#include "resource/clientresources.h"
#include "resource/framemodeldef.h"
#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vissprite.h"
#include "world/sky.h"
#include "ClientTexture"

#include <doomsday/console/var.h>
#include <doomsday/console/exec.h>
#include <doomsday/world/Materials>
#include <de/concurrency.h>
#include <de/timer.h>
#include <de/Log>
#include <de/GLInfo>
#include <cmath>

#define MAX_LAYERS  2
#define MAX_MODELS  32

using namespace de;
using namespace world;

namespace internal {

/// Console variables:
static int sphereDetail     = 6;
static float sphereDistance = 1600; ///< Map units.
static int sphereRows       = 3;    ///< Per hemisphere.

/**
 * Effective layer configuration used with both sphere and model drawing.
 */
struct LayerData
{
    bool active;   ///< @c true= the layer is active/enabled (will be drawn).
    float offset;  ///< Layer material offset (along the horizon).
};

/**
 * Geometry used with the sky sphere. The crest of the hemisphere is up (i.e., y+).
 *
 * @todo Should be a subcomponent of SkyDrawable (cleaner interface). -ds
 */
struct Hemisphere
{
    enum SphereComponent {
        UpperHemisphere,
        LowerHemisphere
    };

    int rows            = 3;
    int columns         = 4 * 6;
    float height        = 0.49f;
    float horizonOffset = -0.105f;

    typedef QVector<Vector3f> VBuf;
    VBuf verts;
    bool needRebuild = true;

    // Look up the precalculated vertex.
    inline Vector3f const &vertex(int r, int c) const {
        return verts[r * columns + c % columns];
    }

    /**
     * Determine the material to use for the given sky @a layer.
     */
    static world::Material *chooseMaterialForSkyLayer(SkyLayer const &layer)
    {
        if(renderTextures == 0)
        {
            return nullptr;
        }
        if(renderTextures == 2)
        {
            return world::Materials::get().materialPtr(de::Uri("System", Path("gray")));
        }
        if(world::Material *mat = layer.material())
        {
            return mat;
        }
        return world::Materials::get().materialPtr(de::Uri("System", Path("missing")));
    }

    /**
     * Determine the cap/fadeout color to use for the given sky @a layer.
     */
    static Vector3f chooseCapColor(SphereComponent hemisphere, SkyLayer const &layer,
                                   bool *needFadeOut = nullptr)
    {
        if (world::Material *mat = chooseMaterialForSkyLayer(layer))
        {
            MaterialAnimator &matAnimator =
                mat->as<ClientMaterial>()
                    .getAnimator(SkyDrawable::layerMaterialSpec(layer.isMasked()));

            // Ensure we've up to date info about the material.
            matAnimator.prepare();

            ClientTexture &pTex = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture->base();
            averagecolor_analysis_t const *avgColor = reinterpret_cast<averagecolor_analysis_t const *>
                    (pTex.analysisDataPointer((hemisphere == UpperHemisphere ? res::Texture::AverageTopColorAnalysis
                                                                             : res::Texture::AverageBottomColorAnalysis)));
            if (!avgColor)
            {
                de::Uri const pTexUri = pTex.manifest().composeUri();
                throw Error("Hemisphere::capColor", String("Texture \"%1\" has no Average%2ColorAnalysis")
                                                        .arg(pTexUri)
                                                        .arg(hemisphere == UpperHemisphere? "Top" : "Bottom"));
            }

            // Is the colored fadeout in use?
            Vector3f color(avgColor->color.rgb);
            dfloat const fadeOutLimit = layer.fadeOutLimit();
            if (color.x >= fadeOutLimit || color.y >= fadeOutLimit || color.z >= fadeOutLimit)
            {
                if (needFadeOut) *needFadeOut = true;
                return color;
            }
        }
        return Vector3f(); // Default color is black.
    }

    void drawCap(Vector3f const &color, bool drawFadeOut) const
    {
        GL_SetNoTexture();

        DGL_Color3f(color.x, color.y, color.z);

        // Draw the cap.
        DGL_Begin(DGL_TRIANGLE_FAN);
        for(int c = 0; c < columns; ++c)
        {
            Vector3f const &vtx = vertex(0, c);
            DGL_Vertex3f(vtx.x, vtx.y, vtx.z);
        }
        DGL_End();

        // Are we doing a colored fadeout?
        if(!drawFadeOut) return;

        // We must fill the background for the top row since it'll be translucent.
        DGL_Begin(DGL_TRIANGLE_STRIP);
        Vector3f const *vtx = &vertex(0, 0);
        DGL_Vertex3f(vtx->x, vtx->y, vtx->z);
        int c = 0;
        for(; c < columns; ++c)
        {
            // One step down.
            vtx = &vertex(1, c);
            DGL_Vertex3f(vtx->x, vtx->y, vtx->z);
            // And one step right.
            vtx = &vertex(0, c + 1);
            DGL_Vertex3f(vtx->x, vtx->y, vtx->z);
        }
        vtx = &vertex(1, c);
        DGL_Vertex3f(vtx->x, vtx->y, vtx->z);
        DGL_End();
    }

    void draw(SphereComponent hemisphere, Sky const &sky, dint firstActiveLayer,
              LayerData const *layerData) const
    {
        DENG2_ASSERT(layerData);

        if (verts.isEmpty()) return;
        if (firstActiveLayer < 0) return;

        bool const yflip = (hemisphere == LowerHemisphere);
        if (yflip)
        {
            // The lower hemisphere must be flipped.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();
            DGL_Scalef(1.0f, -1.0f, 1.0f);
        }

        // First draw the cap and the background for fadeouts, if needed.
        bool drawFadeOut = true;
        drawCap(chooseCapColor(hemisphere, sky.layer(firstActiveLayer), &drawFadeOut), drawFadeOut);

        for (dint i = firstActiveLayer; i < MAX_LAYERS; ++i)
        {
            SkyLayer const &skyLayer = sky.layer(i);
            LayerData const &ldata   = layerData[i];

            if (!ldata.active) continue;

            TextureVariant *layerTex = nullptr;
            if (world::Material *mat = chooseMaterialForSkyLayer(skyLayer))
            {
                MaterialAnimator &matAnimator = mat->as<ClientMaterial>()
                        .getAnimator(SkyDrawable::layerMaterialSpec(skyLayer.isMasked()));

                // Ensure we've up to date info about the material.
                matAnimator.prepare();

                layerTex = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
                GL_BindTexture(layerTex);

                DGL_Enable(DGL_TEXTURE_2D);
                DGL_MatrixMode(DGL_TEXTURE);
                DGL_PushMatrix();
                DGL_LoadIdentity();
                Vector2ui const &texSize = layerTex->base().dimensions();
                if (texSize.x > 0)
                {
                    DGL_Translatef(ldata.offset / texSize.x, 0, 0);
                    DGL_Scalef(1024.f / texSize.x, 1, 1);
                }
                if (yflip)
                {
                    DGL_Scalef(1, -1, 1);
                    DGL_Translatef(0, -1, 0);
                }
            }
            else
            {
                GL_SetNoTexture();
            }

#define WRITESKYVERTEX(r_, c_) { \
    svtx = &vertex(r_, c_); \
    if (layerTex) \
    { \
       DGL_TexCoord2f(0, (c_) / float(columns), (r_) / float(rows)); \
    } \
    if (drawFadeOut) \
    { \
        if ((r_) == 0) DGL_Color4f(1, 1, 1, 0); \
        else           DGL_Color3f(1, 1, 1); \
    } \
    else \
    { \
        if ((r_) == 0) DGL_Color3f(0, 0, 0); \
        else           DGL_Color3f(1, 1, 1); \
    } \
    DGL_Vertex3f(svtx->x, svtx->y, svtx->z); \
}

            Vector3f const *svtx;
            for (dint r = 0; r < rows; ++r)
            {
                DGL_Begin(DGL_TRIANGLE_STRIP);
                WRITESKYVERTEX(r, 0);
                WRITESKYVERTEX(r + 1, 0);
                for (dint c = 1; c <= columns; ++c)
                {
                    WRITESKYVERTEX(r, c);
                    WRITESKYVERTEX(r + 1, c);
                }
                DGL_End();
            }

            if (layerTex)
            {
                DGL_MatrixMode(DGL_TEXTURE);
                DGL_PopMatrix();
                DGL_Disable(DGL_TEXTURE_2D);
            }

#undef WRITESKYVERTEX
        }

        if (yflip)
        {
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }

    /**
     * The top row (row 0) is the one that's faded out.
     * There must be at least 4 columns. The preferable number is 4n, where
     * n is 1, 2, 3... There should be at least two rows because the first
     * one is always faded.
     *
     * The total number of triangles per hemisphere can be calculated thus:
     *
     * Sum: rows * columns * 2 + (hemisphere)
     *      rows * 2 + (fadeout)
     *      rows - 2 (cap)
     */
    void makeVertices(float height, float horizonOffset)
    {
        DENG2_ASSERT(height > 0);

        rows    = de::max(sphereRows,   1);
        columns = de::max(sphereDetail, 1) * 4;

        verts.resize(columns * (rows + 1));

        float const maxSideAngle = float(de::PI / 2 * height);
        float const sideOffset   = float(de::PI / 2 * horizonOffset);

        for(int r = 0; r < rows + 1; ++r)
        for(int c = 0; c < columns; ++c)
        {
            Vector3f &svtx = verts[r * columns + c % columns];

            float const topAngle  = ((c / float(columns)) * 2) * PI;
            float const sideAngle = sideOffset + maxSideAngle * (rows - r) / float(rows);
            float const radius    = cos(sideAngle);

            svtx = Vector3f(radius * cos(topAngle),
                            sin(sideAngle), // The height.
                            radius * sin(topAngle));
        }
    }
};

// All SkyDrawables use the same hemisphere model.
static Hemisphere hemisphere;

} // namespace internal
using namespace ::internal;

DENG2_PIMPL(SkyDrawable)
, DENG2_OBSERVES(Sky, Deletion)
, DENG2_OBSERVES(Sky, HeightChange)
, DENG2_OBSERVES(Sky, HorizonOffsetChange)
{
    Sky const *sky = nullptr;

    dfloat height = 1;
    bool needHeightUpdate = false;

    dfloat horizonOffset = 0;
    bool needHorizonOffsetUpdate = false;

    LayerData layers[MAX_LAYERS];
    dint firstActiveLayer = -1;  ///< @c -1= no active layers.

    bool needBuildHemisphere = true;

    struct ModelData
    {
        FrameModelDef *modef = nullptr;
    };
    ModelData models[MAX_MODELS];
    bool haveModels       = false;
    bool alwaysDrawSphere = false;

    Impl(Public *i) : Base(i)
    {
        de::zap(models);
    }

    ~Impl()
    {
        // Stop observing Sky change notifications (if observing).
        self().configure();
    }

    /**
     * Prepare for drawing; determine layer configuration, sphere dimensions, etc..
     */
    void prepare(Animator const *animator)
    {
        // Determine the layer configuration. Note that this is also used for sky
        // models, even if the sphere is not being drawn.
        firstActiveLayer = -1;
        for (dint i = 0; i < MAX_LAYERS; ++i)
        {
            SkyLayer const &skyLayer = sky->layer(i);

            layers[i].active = skyLayer.isActive();
            layers[i].offset = skyLayer.offset() + animator->layer(i).offset;

            if(firstActiveLayer == -1 && layers[i].active)
            {
                firstActiveLayer = i;
            }
        }

        updateHeightIfNeeded();
        updateHorizonOffsetIfNeeded();

        if(needBuildHemisphere || hemisphere.needRebuild)
        {
            needBuildHemisphere = false;
            hemisphere.makeVertices(height, horizonOffset);
        }
    }

    void drawSphere() const
    {
        if(haveModels && !alwaysDrawSphere) return;

        // We don't want anything written in the depth buffer.
        //glDisable(GL_DEPTH_TEST);
        //glDepthMask(GL_FALSE);

        // Disable culling, all triangles face the viewer.
        //glDisable(GL_CULL_FACE);
        DGL_PushState();
        DGL_CullFace(DGL_NONE);
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_Disable(DGL_DEPTH_WRITE);

        // Setup a proper matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(vOrigin.x, vOrigin.y, vOrigin.z);
        DGL_Scalef(sphereDistance, sphereDistance, sphereDistance);

        // Always draw both hemispheres.
        hemisphere.draw(Hemisphere::LowerHemisphere, *sky, firstActiveLayer, layers);
        hemisphere.draw(Hemisphere::UpperHemisphere, *sky, firstActiveLayer, layers);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();

        // Restore assumed default GL state.
        DGL_PopState();
    }

    void drawModels(Animator const *animator) const
    {
        if(!haveModels) return;

        DGL_Flush();

        // Sky models use depth testing, but they won't interfere with world geometry.
        //glEnable(GL_DEPTH_TEST);
        //glDepthMask(GL_TRUE);
        DGL_Enable(DGL_DEPTH_TEST);
        DGL_Enable(DGL_DEPTH_WRITE);
        LIBGUI_GL.glClear(GL_DEPTH_BUFFER_BIT);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        // Setup basic translation.
        DGL_Translatef(vOrigin.x, vOrigin.y, vOrigin.z);

        for(int i = 0; i < MAX_MODELS; ++i)
        {
            ModelData const &mdata = models[i];

            // Is this model in use?
            FrameModelDef *modef = mdata.modef;
            if(!modef) continue;

            // If the associated layer is not active then the model won't be drawn.
            Record const &skyModelDef = defn::Sky(*sky->def()).model(i);
            int const layerNum        = skyModelDef.geti("layer");
            if(layerNum > 0 && layerNum <= MAX_LAYERS)
            {
                if(!layers[layerNum - 1].active)
                    continue;
            }

            Animator::ModelState const &mstate = animator->model(i);

            // Prepare a vissprite for ordered drawing.
            vissprite_t vis{};

            vis.pose.origin          = vOrigin.xzy() * -Vector3f(skyModelDef.get("originOffset")).xzy();
            vis.pose.topZ            = vis.pose.origin.z;
            vis.pose.distance        = 1;

            Vector2f rotate(skyModelDef.get("rotate"));
            vis.pose.yaw             = mstate.yaw;
            vis.pose.extraYawAngle   = vis.pose.yawAngleOffset   = rotate.x;
            vis.pose.extraPitchAngle = vis.pose.pitchAngleOffset = rotate.y;

            drawmodelparams_t &visModel = *VS_MODEL(&vis);
            visModel.inter                       = (mstate.maxTimer > 0 ? mstate.timer / float(mstate.maxTimer) : 0);
            visModel.mf                          = modef;
            visModel.alwaysInterpolate           = true;
            visModel.shineTranslateWithViewerPos = true;
            ClientApp::resources().setModelDefFrame(*modef, mstate.frame);

            vis.light.ambientColor = Vector4f(skyModelDef.get("color"), 1);

            Rend_DrawModel(vis);
        }

        DGL_Flush();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();

        // We don't want that anything in the world geometry interferes with what was
        // drawn in the sky.
        LIBGUI_GL.glClear(GL_DEPTH_BUFFER_BIT);
    }

    void setupModels(Record const *def)
    {
        // Normally the sky sphere is not drawn if models are in use.
        alwaysDrawSphere = (def && (def->geti("flags") & SIF_DRAW_SPHERE) != 0);

        // The normal sphere is used if no models will be set up.
        haveModels = false;
        de::zap(models);

        if(!def) return;

        defn::Sky const skyDef(*def);
        for(int i = 0; i < skyDef.modelCount(); ++i)
        {
            ModelData &mdata          = models[i];
            Record const &skyModelDef = skyDef.model(i);

            try
            {
                if(FrameModelDef *modef = &ClientApp::resources().modelDef(skyModelDef.gets("id")))
                {
                    if(modef->subCount())
                    {
                        mdata.modef = modef;
                        haveModels = true; // There is at least one model here.
                    }
                }
            }
            catch(ClientResources::MissingModelDefError const &)
            {} // Ignore this error.
        }
    }

    void updateHeightIfNeeded()
    {
        if(!needHeightUpdate) return;
        needHeightUpdate = false;

        float newHeight = (sky? sky->height() : 1);
        if(!de::fequal(height, newHeight))
        {
            height = newHeight;
            needBuildHemisphere = true;
        }
    }

    void updateHorizonOffsetIfNeeded()
    {
        if(!needHorizonOffsetUpdate) return;
        needHorizonOffsetUpdate = false;

        float newHorizonOffset = (sky? sky->horizonOffset() : 0);
        if(!de::fequal(horizonOffset, newHorizonOffset))
        {
            horizonOffset = newHorizonOffset;
            needBuildHemisphere = true;
        }
    }

    /// Observes Sky Deletion
    void skyBeingDeleted(Sky const &)
    {
        // Stop observing Sky change notifications.
        self().configure();
    }

    /// Observes Sky HeightChange
    void skyHeightChanged(Sky &)
    {
        // Defer the update (we may be part way through drawing a frame).
        needHeightUpdate = true;
    }

    /// Observes Sky HeightChange
    void skyHorizonOffsetChanged(Sky &)
    {
        // Defer the update (we may be part way through drawing a frame).
        needHorizonOffsetUpdate = true;
    }
};

SkyDrawable::SkyDrawable(Sky const *sky) : d(new Impl(this))
{
    configure(sky);
}

SkyDrawable &SkyDrawable::configure(Sky const *sky)
{
    if(d->sky)
    {
        d->sky->audienceForDeletion()            -= d;
        d->sky->audienceForHeightChange()        -= d;
        d->sky->audienceForHorizonOffsetChange() -= d;
    }

    d->sky = sky;
    d->needHeightUpdate        = true;
    d->needHorizonOffsetUpdate = true;

    if(d->sky)
    {
        d->sky->audienceForHorizonOffsetChange() += d;
        d->sky->audienceForHeightChange()        += d;
        d->sky->audienceForDeletion()            += d;
    }

    // Models are set up using the data in the definition (will override the sphere by default).
    d->setupModels(d->sky? d->sky->def() : 0);

    return *this;
}

Sky const *SkyDrawable::sky() const
{
    return d->sky;
}

void SkyDrawable::cacheAssets()
{
    if (!d->sky) return;

    d->sky->forAllLayers([] (SkyLayer const &layer)
    {
        if (world::Material *mat = layer.material())
        {
            ClientApp::resources().cache(mat->as<ClientMaterial>(), layerMaterialSpec(layer.isMasked()));
        }
        return LoopContinue;
    });

    if(!d->haveModels) return;

    for (dint i = 0; i < MAX_MODELS; ++i)
    {
        // Is this model in use?
        if (FrameModelDef *modef = d->models[i].modef)
        {
            ClientApp::resources().cache(modef);
        }
    }
}

FrameModelDef *SkyDrawable::modelDef(int modelIndex) const
{
    if(modelIndex >= 0 && modelIndex < MAX_MODELS)
    {
        return d->models[modelIndex].modef;
    }
    return nullptr;
}

void SkyDrawable::draw(Animator const *animator) const
{
    DENG2_ASSERT(animator);
    DENG2_ASSERT(&animator->sky() == this && d->sky == animator->sky().sky());
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    d->prepare(animator);

    // Only drawn when at least one layer is active.
    if(d->firstActiveLayer < 0) return;

    if(fogParams.usingFog) DGL_Enable(DGL_FOG);

    d->drawSphere();
    d->drawModels(animator);

    if(fogParams.usingFog) DGL_Disable(DGL_FOG);
}

MaterialVariantSpec const &SkyDrawable::layerMaterialSpec(bool masked) // static
{
    return ClientApp::resources().materialSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                                 0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE,
                                 0, -1, -1, false, true, false, false);
}

static void markSphereForRebuild()
{
    // Defer this task until draw time, when we can be sure we are in the correct thread.
    hemisphere.needRebuild = true;
}

void SkyDrawable::consoleRegister() // static
{
    C_VAR_INT2 ("rend-sky-detail",   &sphereDetail,     0,          3, 7, markSphereForRebuild);
    C_VAR_INT2 ("rend-sky-rows",     &sphereRows,       0,          1, 8, markSphereForRebuild);
    C_VAR_FLOAT("rend-sky-distance", &sphereDistance,   CVF_NO_MAX, 1, 0);
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(SkyDrawable::Animator)
{
    SkyDrawable *sky = nullptr;

    LayerState layers[MAX_LAYERS];
    ModelState models[MAX_MODELS];

    Impl()
    {
        de::zap(layers);
        de::zap(models);
    }
};

SkyDrawable::Animator::Animator() : d(new Impl)
{}

SkyDrawable::Animator::Animator(SkyDrawable &sky) : d(new Impl)
{
    d->sky = &sky;
}

SkyDrawable::Animator::~Animator()
{}

void SkyDrawable::Animator::setSky(SkyDrawable *sky)
{
    d->sky = sky;

    // (Re)Initalize animation states.
    de::zap(d->layers);
    de::zap(d->models);

    if(!d->sky) return;

    defn::Sky const skyDef(*d->sky->sky()->def());
    for(int i = 0; i < MAX_MODELS; ++i)
    {
        ModelState &mstate = model(i);

        // Is this model in use?
        if(FrameModelDef const *modef = d->sky->modelDef(i))
        {
            Record const &skyModelDef = skyDef.model(i);

            mstate.frame    = modef->subModelDef(0).frame;
            mstate.maxTimer = int(TICSPERSEC * skyModelDef.getf("frameInterval"));
            mstate.yaw      = skyModelDef.getf("yaw");
        }
    }
}

SkyDrawable &SkyDrawable::Animator::sky() const
{
    DENG2_ASSERT(d->sky);
    return *d->sky;
}

bool SkyDrawable::Animator::hasLayer(int index) const
{
    return (index >= 0 && index < MAX_LAYERS);
}

SkyDrawable::Animator::LayerState &SkyDrawable::Animator::layer(int index)
{
    if(hasLayer(index)) return d->layers[index];
    /// @throw MissingLayerStateError An invalid layer state index was specified.
    throw MissingLayerStateError("SkyDrawable::Animator::layer", "Invalid layer state index #" + String::number(index) + ".");
}

SkyDrawable::Animator::LayerState const &SkyDrawable::Animator::layer(int index) const
{
    return const_cast<SkyDrawable::Animator *>(this)->layer(index);
}

bool SkyDrawable::Animator::hasModel(int index) const
{
    return (index >= 0 && index < MAX_MODELS);
}

SkyDrawable::Animator::ModelState &SkyDrawable::Animator::model(int index)
{
    if(hasModel(index)) return d->models[index];
    /// @throw MissingModelStateError An invalid model state index was specified.
    throw MissingModelStateError("SkyDrawable::Animator::model", "Invalid model state index #" + String::number(index) + ".");
}

SkyDrawable::Animator::ModelState const &SkyDrawable::Animator::model(int index) const
{
    return const_cast<SkyDrawable::Animator *>(this)->model(index);
}

void SkyDrawable::Animator::advanceTime(timespan_t /*elapsed*/)
{
    LOG_AS("SkyDrawable::Animator");

    if(!d->sky) return;
    if(!sky().sky()) return;

    if(clientPaused) return;
    if(!DD_IsSharpTick()) return;

    // Animate layers.
    defn::Sky const skyDef(*sky().sky()->def());
    for(int i = 0; i < MAX_LAYERS; ++i)
    {
        Record const &skyLayerDef = skyDef.layer(i);
        LayerState &lstate        = layer(i);

        // Translate the layer origin.
        lstate.offset += skyLayerDef.getf("offsetSpeed") / TICSPERSEC;
    }

    // Animate models.
    for(int i = 0; i < MAX_MODELS; ++i)
    {
        // Is this model in use?
        FrameModelDef const *modef = sky().modelDef(i);
        if(!modef) continue;

        Record const &skyModelDef = skyDef.model(i);
        ModelState &mstate        = model(i);

        // Rotate the model.
        mstate.yaw += skyModelDef.getf("yawSpeed") / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(mstate.maxTimer > 0 && ++mstate.timer >= mstate.maxTimer)
        {
            mstate.frame++;
            mstate.timer = 0;

            // Execute a console command?
            String const execute = skyModelDef.gets("execute");
            if(!execute.isEmpty())
            {
                Con_Execute(CMDS_SCRIPT, execute.toUtf8().constData(), true, false);
            }
        }
    }
}
