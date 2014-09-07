/** @file skydrawble.cpp  Drawable specialized for the sky.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "render/skydrawable.h"

#include <cmath>
#include <de/timer.h>
#include <de/concurrency.h>
#include <de/Log>
#include <doomsday/console/var.h>
#include <doomsday/console/exec.h>

#include "clientapp.h"
#include "client/cl_def.h"

#include "gl/gl_main.h"
#include "gl/gl_tex.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"

#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vissprite.h"

#include "Texture"
#include "world/sky.h"

#define MAX_LAYERS  2
#define MAX_MODELS  32

using namespace de;

namespace internal {

/// Console variables:
static int sphereDetail     = 6;
static float sphereDistance = 1600; ///< Map units.
static int sphereRows       = 3;

/**
 * Geometry used with the sky sphere. The crest of the hemisphere is up (i.e., y+)
 */
struct Hemisphere
{
    enum SphereComponent {
        UpperHemisphere,
        LowerHemisphere
    };

    float height        = 0.49f;
    float horizonOffset = -0.105f;

    int rows    = 3;
    int columns = 4 * 6;

    typedef QVector<Vector3f> VBuf;
    VBuf verts;
    bool needRebuild = true;

    static inline ResourceSystem &resSys() {
        return ClientApp::resourceSystem();
    }

    // Look up the precalculated vertex.
    inline Vector3f const &vertex(int r, int c) const {
        return verts[r * columns + c % columns];
    }

    /**
     * Determine the material to use for the given sky @a layer.
     */
    static Material *chooseMaterialForSkyLayer(SkyDrawable::LayerData const &layer)
    {
        if(renderTextures == 0)
        {
            return 0;
        }
        if(renderTextures == 2)
        {
            return resSys().materialPtr(de::Uri("System", Path("gray")));
        }
        if(Material *mat = layer.material)
        {
            return mat;
        }
        return resSys().materialPtr(de::Uri("System", Path("missing")));
    }

    /**
     * Determine the cap/fadeout color to use for the given sky @a layer.
     */
    static Vector3f chooseCapColor(SphereComponent hemisphere, SkyDrawable::LayerData const &layer,
                                   bool *needFadeOut = 0)
    {
        if(Material *mat = chooseMaterialForSkyLayer(layer))
        {
            MaterialSnapshot const &ms = mat->prepare(SkyDrawable::layerMaterialSpec(layer.masked));

            Texture &pTex = ms.texture(MTU_PRIMARY).generalCase();
            averagecolor_analysis_t const *avgColor = reinterpret_cast<averagecolor_analysis_t const *>
                    (pTex.analysisDataPointer((hemisphere == UpperHemisphere? Texture::AverageTopColorAnalysis
                                                                            : Texture::AverageBottomColorAnalysis)));
            if(!avgColor)
            {
                de::Uri const pTexUri = pTex.manifest().composeUri();
                throw Error("Hemisphere::capColor", QString("Texture \"%1\" has no Average%2ColorAnalysis")
                                                        .arg(pTexUri)
                                                        .arg(hemisphere == UpperHemisphere? "Top" : "Bottom"));
            }

            // Is the colored fadeout in use?
            Vector3f color(avgColor->color.rgb);
            float const fadeOutLimit = layer.fadeOutLimit;
            if(color >= Vector3f(fadeOutLimit, fadeOutLimit, fadeOutLimit))
            {
                if(needFadeOut) *needFadeOut = true;
                return color;
            }
        }

        return Vector3f(); // Default color is black.
    }

    void drawCap(Vector3f const &color, bool drawFadeOut) const
    {
        GL_SetNoTexture();

        glColor3f(color.x, color.y, color.z);

        // Draw the cap.
        glBegin(GL_TRIANGLE_FAN);
        for(int c = 0; c < columns; ++c)
        {
            Vector3f const &vtx = vertex(0, c);
            glVertex3f(vtx.x, vtx.y, vtx.z);
        }
        glEnd();

        // Are we doing a colored fadeout?
        if(!drawFadeOut) return;

        // We must fill the background for the top row since it'll be translucent.
        glBegin(GL_TRIANGLE_STRIP);
        Vector3f const *vtx = &vertex(0, 0);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        int c = 0;
        for(; c < columns; ++c)
        {
            // One step down.
            vtx = &vertex(1, c);
            glVertex3f(vtx->x, vtx->y, vtx->z);
            // And one step right.
            vtx = &vertex(0, c + 1);
            glVertex3f(vtx->x, vtx->y, vtx->z);
        }
        vtx = &vertex(1, c);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        glEnd();
    }

    void draw(SphereComponent hemisphere, SkyDrawable const &sky) const
    {
        if(verts.isEmpty()) return;

        int const firstActiveLayer = sky.firstActiveLayer();
        if(firstActiveLayer < 0) return;

        bool const yflip = (hemisphere == LowerHemisphere);
        if(yflip)
        {
            // The lower hemisphere must be flipped.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glScalef(1.0f, -1.0f, 1.0f);
        }

        // First draw the cap and the background for fadeouts, if needed.
        bool drawFadeOut = true;
        drawCap(chooseCapColor(hemisphere, sky.layer(firstActiveLayer), &drawFadeOut), drawFadeOut);

        for(int i = firstActiveLayer; i < MAX_LAYERS; ++i)
        {
            SkyDrawable::LayerData const &layer = sky.layer(i);

            if(!layer.active) continue;

            // The fade out is only drawn for the first layer.
            drawFadeOut = (i == firstActiveLayer);

            TextureVariant *layerTex = 0;
            if(Material *mat = chooseMaterialForSkyLayer(layer))
            {
                MaterialSnapshot const &ms = mat->prepare(SkyDrawable::layerMaterialSpec(layer.masked));

                layerTex = &ms.texture(MTU_PRIMARY);
                GL_BindTexture(layerTex);

                glEnable(GL_TEXTURE_2D);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                Vector2i const &texSize = layerTex->generalCase().dimensions();
                if(texSize.x > 0)
                {
                    glTranslatef(layer.offset / texSize.x, 0, 0);
                    glScalef(1024.f / texSize.x, 1, 1);
                }
                if(yflip)
                {
                    glScalef(1, -1, 1);
                    glTranslatef(0, -1, 0);
                }
            }
            else
            {
                GL_SetNoTexture();
            }

#define WRITESKYVERTEX(r_, c_) { \
    svtx = &vertex(r_, c_); \
    if(layerTex) \
    { \
       glTexCoord2f((c_) / float(columns), (r_) / float(rows)); \
    } \
    if(drawFadeOut) \
    { \
        if((r_) == 0) glColor4f(1, 1, 1, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    else \
    { \
        if((r_) == 0) glColor3f(0, 0, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    glVertex3f(svtx->x, svtx->y, svtx->z); \
}

            Vector3f const *svtx;
            for(int r = 0; r < rows; ++r)
            {
                glBegin(GL_TRIANGLE_STRIP);
                WRITESKYVERTEX(r, 0);
                WRITESKYVERTEX(r + 1, 0);
                for(int c = 1; c <= columns; ++c)
                {
                    WRITESKYVERTEX(r, c);
                    WRITESKYVERTEX(r + 1, c);
                }
                glEnd();
            }

            if(layerTex)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
            }

#undef WRITESKYVERTEX
        }

        if(yflip)
        {
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
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
    void makeVertices()
    {
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

    void rebuildIfNeeded(float newHeight, float newHorizonOffset)
    {
        // Rebuild our model if any parameters have changed.
        if(verts.isEmpty() || horizonOffset != newHorizonOffset)
        {
            horizonOffset = newHorizonOffset;
            needRebuild   = true;
        }
        if(verts.isEmpty() || height != newHeight)
        {
            height      = newHeight;
            needRebuild = true;
        }

        // Any work to do?
        if(needRebuild)
        {
            needRebuild = false;
            makeVertices();
        }
    }
};

static Hemisphere hemisphere;

} // namespace internal
using namespace ::internal;

DENG2_PIMPL(SkyDrawable)
{
    LayerData layers[MAX_LAYERS];
    int firstActiveLayer = -1; ///< Denotes no active layers.

    ModelData models[MAX_MODELS];
    bool haveModels       = false;
    bool alwaysDrawSphere = false;

    Instance(Public *i) : Base(i)
    {
        de::zap(layers);
        de::zap(models);
    }

    static inline ResourceSystem &resSys() {
        return ClientApp::resourceSystem();
    }

    void drawSphere(float height, float horizonOffset) const
    {
        if(haveModels && !alwaysDrawSphere) return;

        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
        glScalef(sphereDistance, sphereDistance, sphereDistance);

        hemisphere.rebuildIfNeeded(height, horizonOffset);

        // Always draw both hemispheres.
        hemisphere.draw(Hemisphere::LowerHemisphere, self);
        hemisphere.draw(Hemisphere::UpperHemisphere, self);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void drawModels() const
    {
        if(!haveModels) return;

        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Setup basic translation.
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);

        for(int i = 0; i < MAX_MODELS; ++i)
        {
            ModelData const &mdata    = self.model(i);
            Record const *skyModelDef = mdata.def;

            if(!skyModelDef) continue;

            // If the associated sky layer is not active then the model won't be drawn.
            if(!self.layer(skyModelDef->geti("layer")).active)
            {
                continue;
            }

            vissprite_t vis; de::zap(vis);

            vis.pose.origin          = vOrigin.xzy() * -Vector3f(skyModelDef->get("originOffset")).xzy();
            vis.pose.topZ            = vis.pose.origin.z;
            vis.pose.distance        = 1;

            Vector2f rotate(skyModelDef->get("rotate"));
            vis.pose.yaw             = mdata.yaw;
            vis.pose.extraYawAngle   = vis.pose.yawAngleOffset   = rotate.x;
            vis.pose.extraPitchAngle = vis.pose.pitchAngleOffset = rotate.y;

            drawmodelparams_t &visModel = *VS_MODEL(&vis);
            visModel.inter                       = (mdata.maxTimer > 0 ? mdata.timer / float(mdata.maxTimer) : 0);
            visModel.mf                          = mdata.model;
            visModel.alwaysInterpolate           = true;
            visModel.shineTranslateWithViewerPos = true;
            resSys().setModelDefFrame(*mdata.model, mdata.frame);

            vis.light.ambientColor = Vector4f(skyModelDef->get("color"), 1);

            Rend_DrawModel(vis);
        }

        // We don't want that anything interferes with what was drawn.
        //glClear(GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
};

SkyDrawable::SkyDrawable() : d(new Instance(this))
{}

void SkyDrawable::setupModels(defn::Sky const *def)
{
    // Normally the sky sphere is not drawn if models are in use.
    d->alwaysDrawSphere = (def && (def->geti("flags") & SIF_DRAW_SPHERE) != 0);

    // The normal sphere is used if no models will be set up.
    d->haveModels = false;
    de::zap(d->models);

    if(!def) return;

    for(int i = 0; i < def->modelCount(); ++i)
    {
        Record const &modef = def->model(i);
        ModelData &mdata    = model(i);

        // Is the model ID set?
        try
        {
            mdata.model = &d->resSys().modelDef(modef.gets("id"));
            if(!mdata.model->subCount())
            {
                continue;
            }

            // There is a model here.
            d->haveModels = true;

            mdata.def      = modef.accessedRecordPtr();
            mdata.maxTimer = int(TICSPERSEC * modef.getf("frameInterval"));
            mdata.yaw      = modef.getf("yaw");
            mdata.frame    = mdata.model->subModelDef(0).frame;
        }
        catch(ResourceSystem::MissingModelDefError const &)
        {} // Ignore this error.
    }
}

bool SkyDrawable::hasLayer(int index) const
{
    return (index >= 0 && index < MAX_LAYERS);
}

SkyDrawable::LayerData &SkyDrawable::layer(int index)
{
    if(hasLayer(index))
    {
        return d->layers[index];
    }
    /// @throw MissingLayerError An invalid layer index was specified.
    throw MissingModelError("SkyDrawable::layer", "Invalid layer index #" + String::number(index) + ".");
}

SkyDrawable::LayerData const &SkyDrawable::layer(int index) const
{
    return const_cast<LayerData const &>(const_cast<SkyDrawable *>(this)->layer(index));
}

int SkyDrawable::firstActiveLayer() const
{
    return d->firstActiveLayer; // Updated at draw time.
}

bool SkyDrawable::hasModel(int index) const
{
    return (index >= 0 && index < MAX_MODELS);
}

SkyDrawable::ModelData &SkyDrawable::model(int index)
{
    if(hasModel(index))
    {
        return d->models[index];
    }
    /// @throw MissingModelError An invalid model index was specified.
    throw MissingModelError("SkyDrawable::model", "Invalid model index #" + String::number(index) + ".");
}

SkyDrawable::ModelData const &SkyDrawable::model(int index) const
{
    return const_cast<ModelData const &>(const_cast<SkyDrawable *>(this)->model(index));
}

void SkyDrawable::cacheDrawableAssets(Sky const *sky)
{
    DENG2_ASSERT(sky);

    for(SkyLayer const *layer : sky->layers())
    {
        if(Material *mat = layer->material())
        {
            d->resSys().cache(*mat, layerMaterialSpec(layer->isMasked()));
        }
    }

    if(d->haveModels)
    {
        for(int i = 0; i < MAX_MODELS; ++i)
        {
            ModelData &mdata = model(i);
            if(!mdata.def) continue;

            d->resSys().cache(mdata.model);
        }
    }
}

void SkyDrawable::draw(Sky const *sky) const
{
    DENG2_ASSERT(sky);
    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Determine the layer configuration for drawing.
    // Note that this is used for sky models even if the sphere is not being drawn.
    d->firstActiveLayer = -1;
    for(int i = 0; i < MAX_LAYERS; ++i)
    {
        SkyLayer const *skyLayer = sky->layer(i);
        LayerData &ldata         = const_cast<SkyDrawable *>(this)->layer(i);

        ldata.active       = skyLayer->isActive();
        ldata.masked       = skyLayer->isMasked();
        ldata.offset       = skyLayer->offset();
        ldata.material     = skyLayer->material();
        ldata.fadeOutLimit = skyLayer->fadeoutLimit();

        if(d->firstActiveLayer == -1 && ldata.active)
        {
            d->firstActiveLayer = i;
        }
    }

    // Only drawn when at least one layer is active.
    if(d->firstActiveLayer < 0) return;

    if(usingFog) glEnable(GL_FOG);

    d->drawSphere(sky->height(), sky->horizonOffset());
    d->drawModels();

    if(usingFog) glDisable(GL_FOG);
}

MaterialVariantSpec const &SkyDrawable::layerMaterialSpec(bool masked) // static
{
    return Instance::resSys()
                .materialSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                              0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE,
                              0, -1, -1, false, true, false, false);
}

namespace {
void markSphereForRebuild()
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    hemisphere.needRebuild = true;
}
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
    SkyDrawable *sky;
    Instance(SkyDrawable *sky = 0) : sky(sky) {}
};

SkyDrawable::Animator::Animator() : d(new Instance)
{}

SkyDrawable::Animator::Animator(SkyDrawable &sky) : d(new Instance(&sky))
{}

SkyDrawable::Animator::~Animator()
{}

void SkyDrawable::Animator::setSky(SkyDrawable *sky)
{
    d->sky = sky;
}

SkyDrawable &SkyDrawable::Animator::sky() const
{
    DENG2_ASSERT(d->sky != 0);
    return *d->sky;
}

void SkyDrawable::Animator::advanceTime(timespan_t /*elapsed*/)
{
    LOG_AS("SkyDrawable::Animator");

    if(!d->sky) return;

    if(clientPaused) return;
    if(!DD_IsSharpTick()) return;

    // Animate layers.
    /*for(int i = 0; i < MAX_LAYERS; ++i)
    {
        LayerData &lyr = sky().layer(i);
    }*/

    // Animate models.
    for(int i = 0; i < MAX_MODELS; ++i)
    {
        ModelData &mdata = sky().model(i);
        if(!mdata.def) continue;

        // Rotate the model.
        mdata.yaw += mdata.def->getf("yawSpeed") / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(mdata.maxTimer > 0 && ++mdata.timer >= mdata.maxTimer)
        {
            mdata.frame++;
            mdata.timer = 0;

            // Execute a console command?
            String const execute = mdata.def->gets("execute");
            if(!execute.isEmpty())
            {
                Con_Execute(CMDS_SCRIPT, execute.toUtf8().constData(), true, false);
            }
        }
    }
}
