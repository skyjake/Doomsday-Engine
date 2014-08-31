/** @file sky.cpp  Sky sphere and 3D models.
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
#include "render/sky.h"

#include <cmath>
#include <doomsday/console/var.h>
#include <doomsday/console/exec.h>
#include <de/Log>
#include <de/timer.h>
#include <de/concurrency.h>

#include "clientapp.h"
#include "client/cl_def.h"

#include "gl/gl_main.h"
#include "gl/gl_tex.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vissprite.h"

using namespace de;

DENG2_PIMPL_NOREF(Sky::Animator)
{
    Sky *sky;
    Instance(Sky *sky = 0) : sky(sky) {}
};

Sky::Animator::Animator() : d(new Instance)
{}

Sky::Animator::Animator(Sky &sky) : d(new Instance(&sky))
{}

Sky::Animator::~Animator()
{}

void Sky::Animator::setSky(Sky &sky)
{
    d->sky = &sky;
}

Sky &Sky::Animator::sky() const
{
    DENG2_ASSERT(d->sky != 0);
    return *d->sky;
}

void Sky::Animator::configure(defn::Sky *def)
{
    LOG_AS("Sky::Animator");

    // The default configuration is used as a starting point.
    sky().configureDefault();

    if(!def) return; // Go with the defaults, then.

    sky().setHeight(def->getf("height"));
    sky().setHorizonOffset(def->getf("horizonOffset"));

    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Record const &lyrDef = def->layer(i);
        Layer &lyr = sky().layer(i);

        lyr.setMasked((lyrDef.geti("flags") & Layer::Masked) != 0)
           .setOffset(lyrDef.getf("offset"))
           .setFadeoutLimit(lyrDef.getf("colorLimit"));

        de::Uri const matUri(lyrDef.gets("material"));
        if(!matUri.isEmpty())
        {
            try
            {
                lyr.setMaterial(ClientApp::resourceSystem().materialPtr(matUri));
            }
            catch(ResourceSystem::MissingManifestError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_RES_WARNING(er.asText() + ". Unknown material \"%s\" in definition layer %i, using default")
                    << matUri << i;
            }
        }

        lyr.setActive(lyrDef.geti("flags") & Layer::Active);
    }

    Vector3f ambientColor = Vector3f(def->get("color")).max(Vector3f(0, 0, 0));
    if(ambientColor != Vector3f(0, 0, 0))
    {
        sky().setAmbientColor(ambientColor);
    }

    // Models are set up using the data in the definition (will override the sphere by default).
    sky().setupModels(*def);
}

void Sky::Animator::advanceTime(timespan_t /*elapsed*/)
{
    LOG_AS("Sky::Animator");

    if(!d->sky) return;

    if(clientPaused) return;
    if(!DD_IsSharpTick()) return;

    // Animate layers.
    /*for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Sky::Layer &lyr = sky().layer(i);
    }*/

    // Animate models.
    for(int i = 0; i < MAX_SKY_MODELS; ++i)
    {
        Sky::ModelInfo &minfo = sky().model(i);
        if(!minfo.def) continue;

        // Rotate the model.
        minfo.yaw += minfo.def->getf("yawSpeed") / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(minfo.maxTimer > 0 && ++minfo.timer >= minfo.maxTimer)
        {
            minfo.frame++;
            minfo.timer = 0;

            // Execute a console command?
            String const execute = minfo.def->gets("execute");
            if(!execute.isEmpty())
            {
                Con_Execute(CMDS_SCRIPT, execute.toUtf8().constData(), true, false);
            }
        }
    }
}

enum SphereComponentFlag {
    UpperHemisphere = 0x1,
    LowerHemisphere = 0x2
};
Q_DECLARE_FLAGS(SphereComponentFlags, SphereComponentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(SphereComponentFlags)

enum hemispherecap_t
{
    HC_NONE = 0,
    HC_TOP,
    HC_BOTTOM
};

static int skySphereColumns = 4*6;

/// Console variables:
static float skyDistance   = 1600; ///< Map units.
static int skySphereDetail = 6;
static int skySphereRows   = 3;

// Hemisphere geometry used with the sky sphere.
static QVector<Vector3f> hemisphereVerts; // Crest is up.
static bool needMakeHemisphere = true;

// Sphere draw state paramaters. Global for performance reasons.
struct drawhemispherestate_t
{
    bool fadeout, texXFlip;
    Vector2i texSize;
    float texOffset;
    Vector3f capColor;
};
static drawhemispherestate_t ds;

static MaterialVariantSpec const &sphereMaterialSpec(bool masked)
{
    return ClientApp::resourceSystem()
                .materialSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                              0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE,
                              0, -1, -1, false, true, false, false);
}

Sky::Layer::Layer(Material *material)
    : _flags(DefaultFlags)
    , _material(material)
    , _offset(0)
    , _fadeoutLimit(0)
{}

Sky::Layer &Sky::Layer::setFlags(Flags flags, FlagOp operation)
{
    Flags const oldFlags = _flags;

    applyFlagOperation(_flags, flags, operation);

    if(_flags.testFlag(Active) != oldFlags.testFlag(Active))
    {
        DENG2_FOR_AUDIENCE(ActiveChange, i)
        {
            i->skyLayerActiveChanged(*this);
        }
    }
    if(_flags.testFlag(Masked) != oldFlags.testFlag(Masked))
    {
        DENG2_FOR_AUDIENCE(MaskedChange, i)
        {
            i->skyLayerMaskedChanged(*this);
        }
    }

    return *this;
}

bool Sky::Layer::isActive() const
{
    return _flags.testFlag(Active);
}

Sky::Layer &Sky::Layer::setActive(bool yes)
{
    return setFlags(Active, yes? SetFlags : UnsetFlags);
}

bool Sky::Layer::isMasked() const
{
    return _flags.testFlag(Masked);
}

Sky::Layer &Sky::Layer::setMasked(bool yes)
{
    return setFlags(Masked, yes? SetFlags : UnsetFlags);
}

Material *Sky::Layer::material() const
{
    return _material;
}

Sky::Layer &Sky::Layer::setMaterial(Material *newMaterial)
{
    if(_material != newMaterial)
    {
        _material = newMaterial;
        DENG2_FOR_AUDIENCE(MaterialChange, i)
        {
            i->skyLayerMaterialChanged(*this);
        }
    }
    return *this;
}

float Sky::Layer::offset() const
{
    return _offset;
}

Sky::Layer &Sky::Layer::setOffset(float newOffset)
{
    _offset = newOffset;
    return *this;
}

float Sky::Layer::fadeoutLimit() const
{
    return _fadeoutLimit;
}

Sky::Layer &Sky::Layer::setFadeoutLimit(float newLimit)
{
    _fadeoutLimit = newLimit;
    return *this;
}

DENG2_PIMPL(Sky)
, DENG2_OBSERVES(Layer, MaterialChange)
, DENG2_OBSERVES(Layer, ActiveChange)
, DENG2_OBSERVES(Layer, MaskedChange)
{
    Animator *animator = nullptr;

    Layer layers[MAX_SKY_LAYERS];
    int firstActiveLayer = -1; /// @c -1= 'no active layers'.
    bool needUpdateFirstActiveLayer = true;

    float horizonOffset = 0;
    float height = 0;
    bool ambientColorDefined = false;   /// @c true= pre-defined in a MapInfo def.
    bool needUpdateAmbientColor = true; /// @c true= update if not pre-defined.
    Vector3f ambientColor;

    bool alwaysDrawSphere = false;

    ModelInfo models[MAX_SKY_MODELS];
    bool haveModels = false;

    Instance(Public *i) : Base(i)
    {
        for(int i = 0; i < MAX_SKY_LAYERS; ++i)
        {
            layers[i].audienceForMaterialChange += this;
            layers[i].audienceForActiveChange   += this;
            layers[i].audienceForMaskedChange   += this;
        }

        de::zap(models);
    }

    inline ResourceSystem &resSys() {
        return App_ResourceSystem();
    }

    void updateFirstActiveLayerIfNeeded()
    {
        if(!needUpdateFirstActiveLayer) return;

        needUpdateFirstActiveLayer = false;

        firstActiveLayer = -1; // -1 denotes 'no active layers'.
        for(int i = 0; i < MAX_SKY_LAYERS; ++i)
        {
            if(layers[i].isActive())
            {
                firstActiveLayer = i;
                break;
            }
        }
    }

    /**
     * @todo Re-implement me by rendering the sky to a low-quality cubemap and use that
     * to obtain the lighting characteristics.
     */
    void updateAmbientColorIfNeeded()
    {
        if(!needUpdateAmbientColor) return;

        needUpdateAmbientColor = false;

        // By default the ambient color is pure white.
        ambientColor = Vector3f(1, 1, 1);

        // Presently we can only calculate this color if the sky sphere is in use.
        if(haveModels && !alwaysDrawSphere) return;

        updateFirstActiveLayerIfNeeded();
        if(firstActiveLayer < 0) return;

        Vector3f avgMaterialColor;
        Vector3f bottomCapColor;
        Vector3f topCapColor;

        int avgCount = 0;
        for(int i = firstActiveLayer; i < MAX_SKY_LAYERS; ++i)
        {
            Layer &layer = layers[i];

            // Inactive layers won't be drawn.
            if(!layer.isActive()) continue;

            // A material is required for drawing.
            if(!layer.material()) continue;
            Material *mat = layer.material();

            // Prepare and ensure the material has at least a primary texture.
            MaterialSnapshot const &ms = mat->prepare(sphereMaterialSpec(layer.isMasked()));
            if(ms.hasTexture(MTU_PRIMARY))
            {
                Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
                averagecolor_analysis_t const *avgColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageColorAnalysis));
                if(!avgColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageColorAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

                if(i == firstActiveLayer)
                {
                    averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageTopColorAnalysis));
                    if(!avgLineColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageTopColorAnalysis").arg(tex.manifest().composeUri()));

                    topCapColor = Vector3f(avgLineColor->color.rgb);

                    avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageBottomColorAnalysis));
                    if(!avgLineColor) throw Error("calculateSkyAmbientColor", QString("Texture \"%1\" has no AverageBottomColorAnalysis").arg(tex.manifest().composeUri()));

                    bottomCapColor = Vector3f(avgLineColor->color.rgb);
                }

                avgMaterialColor += Vector3f(avgColor->color.rgb);
                ++avgCount;
            }
        }

        if(avgCount)
        {
            // The caps cover a large amount of the sky sphere, so factor it in too.
            // Each cap is another unit.
            ambientColor = (avgMaterialColor + topCapColor + bottomCapColor) / (avgCount + 2);
        }
    }

    void drawModels()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Setup basic translation.
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);

        for(int i = 0; i < MAX_SKY_MODELS; ++i)
        {
            ModelInfo &minfo          = models[i];
            Record const *skyModelDef = minfo.def;

            if(!skyModelDef) continue;

            // If the associated sky layer is not active then the model won't be drawn.
            if(!self.layer(skyModelDef->geti("layer")).isActive())
            {
                continue;
            }

            vissprite_t vis; de::zap(vis);

            vis.pose.origin          = vOrigin.xzy() * -Vector3f(skyModelDef->get("originOffset")).xzy();
            vis.pose.topZ            = vis.pose.origin.z;
            vis.pose.distance        = 1;

            Vector2f rotate(skyModelDef->get("rotate"));
            vis.pose.yaw             = minfo.yaw;
            vis.pose.extraYawAngle   = vis.pose.yawAngleOffset   = rotate.x;
            vis.pose.extraPitchAngle = vis.pose.pitchAngleOffset = rotate.y;

            drawmodelparams_t &visModel = *VS_MODEL(&vis);
            visModel.inter                       = (minfo.maxTimer > 0 ? minfo.timer / float(minfo.maxTimer) : 0);
            visModel.mf                          = minfo.model;
            visModel.alwaysInterpolate           = true;
            visModel.shineTranslateWithViewerPos = true;
            App_ResourceSystem().setModelDefFrame(*minfo.model, minfo.frame);

            vis.light.ambientColor = Vector4f(skyModelDef->get("color"), 1);

            Rend_DrawModel(vis);
        }

        // We don't want that anything interferes with what was drawn.
        //glClear(GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    /// Observes Layer MaterialChange
    void skyLayerMaterialChanged(Layer &layer)
    {
        // We may need to recalculate the ambient color of the sky.
        if(!layer.isActive()) return;
        //if(ambientColorDefined) return;

        needUpdateAmbientColor = true;
    }

    /// Observes Layer ActiveChange
    void skyLayerActiveChanged(Layer & /*layer*/)
    {
        needUpdateFirstActiveLayer = true;
        needUpdateAmbientColor     = true;
    }

    /// Observes Layer MaskedChange
    void skyLayerMaskedChanged(Layer &layer)
    {
        // We may need to recalculate the ambient color of the sky.
        if(!layer.isActive()) return;
        //if(ambientColorDefined) return;

        needUpdateAmbientColor = true;
    }
};

Sky::Sky() : d(new Instance(this))
{}

bool Sky::hasLayer(int index) const
{
    return (index >= 0 && index < MAX_SKY_LAYERS);
}

Sky::Layer &Sky::layer(int index)
{
    if(hasLayer(index))
    {
        return d->layers[index];
    }
    /// @throw MissingLayerError An invalid layer index was specified.
    throw MissingLayerError("Sky::layer", "Invalid layer index #" + String::number(index) + ".");
}

Sky::Layer const &Sky::layer(int index) const
{
    return const_cast<Layer const &>(const_cast<Sky *>(this)->layer(index));
}

int Sky::firstActiveLayer() const
{
    d->updateFirstActiveLayerIfNeeded();
    return d->firstActiveLayer;
}

bool Sky::hasModel(int index) const
{
    return (index >= 0 && index < MAX_SKY_MODELS);
}

Sky::ModelInfo &Sky::model(int index)
{
    if(hasModel(index))
    {
        return d->models[index];
    }
    /// @throw MissingModelError An invalid model index was specified.
    throw MissingModelError("Sky::model", "Invalid model index #" + String::number(index) + ".");
}

Sky::ModelInfo const &Sky::model(int index) const
{
    return const_cast<ModelInfo const &>(const_cast<Sky *>(this)->model(index));
}

void Sky::configureDefault()
{
    d->height                 = DEFAULT_SKY_HEIGHT;
    d->horizonOffset          = DEFAULT_SKY_HORIZON_OFFSET;
    d->ambientColorDefined    = false;
    d->needUpdateAmbientColor = true;
    d->ambientColor           = Vector3f(1, 1, 1);

    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Layer &lyr = d->layers[i];

        lyr.setMasked(false)
           .setOffset(DEFAULT_SKY_SPHERE_XOFFSET)
           .setFadeoutLimit(DEFAULT_SKY_SPHERE_FADEOUT_LIMIT)
           .setActive(i == 0);

        lyr.setMaterial(0);
        try
        {
            lyr.setMaterial(ClientApp::resourceSystem().materialPtr(de::Uri(DEFAULT_SKY_SPHERE_MATERIAL, RC_NULL)));
        }
        catch(MaterialManifest::MissingMaterialError const &)
        {} // Ignore this error.
    }

    d->haveModels = false;
    de::zap(d->models);
}

void Sky::setupModels(defn::Sky const &def)
{
    // Normally the sky sphere is not drawn if models are in use.
    d->alwaysDrawSphere = (def.geti("flags") & SIF_DRAW_SPHERE) != 0;

    // The normal sphere is used if no models will be set up.
    d->haveModels = false;
    de::zap(d->models);

    for(int i = 0; i < def.modelCount(); ++i)
    {
        Record const &modef = def.model(i);
        ModelInfo *minfo = &d->models[i];

        // Is the model ID set?
        try
        {
            minfo->model = &App_ResourceSystem().modelDef(modef.gets("id"));
            if(!minfo->model->subCount())
            {
                continue;
            }

            // There is a model here.
            d->haveModels = true;

            minfo->def      = modef.accessedRecordPtr();
            minfo->maxTimer = int(TICSPERSEC * modef.getf("frameInterval"));
            minfo->yaw      = modef.getf("yaw");
            minfo->frame    = minfo->model->subModelDef(0).frame;
        }
        catch(ResourceSystem::MissingModelDefError const &)
        {} // Ignore this error.
    }
}

float Sky::horizonOffset() const
{
    return d->horizonOffset;
}

void Sky::setHorizonOffset(float newOffset)
{
    d->horizonOffset = newOffset;
}

float Sky::height() const
{
    return d->height;
}

void Sky::setHeight(float newHeight)
{
    d->height = de::clamp(0.f, newHeight, 1.f);
}

Vector3f const &Sky::ambientColor() const
{
    static Vector3f const white(1, 1, 1);

    if(d->ambientColorDefined || rendSkyLightAuto)
    {
        if(!d->ambientColorDefined)
        {
            d->updateAmbientColorIfNeeded();
        }
        return d->ambientColor;
    }
    return white;
}

void Sky::setAmbientColor(Vector3f const &newColor)
{
    d->ambientColor = newColor.min(Vector3f(1, 1, 1)).max(Vector3f(0, 0, 0));
    d->ambientColorDefined = true;
}

// Look up the precalculated vertex.
static inline Vector3f &skyVertex(int r, int c)
{
    return hemisphereVerts[r * skySphereColumns + c % skySphereColumns];
}

static void drawHemisphereCap()
{
    // Use the appropriate color.
    glColor3f(ds.capColor.x, ds.capColor.y, ds.capColor.z);

    // Draw the cap.
    glBegin(GL_TRIANGLE_FAN);
    for(int c = 0; c < skySphereColumns; ++c)
    {
        Vector3f const &vtx = skyVertex(0, c);
        glVertex3f(vtx.x, vtx.y, vtx.z);
    }
    glEnd();

    // Are we doing a colored fadeout?
    if(!ds.fadeout) return;

    // We must fill the background for the top row since it'll be translucent.
    glBegin(GL_TRIANGLE_STRIP);
    Vector3f const *vtx = &skyVertex(0, 0);
    glVertex3f(vtx->x, vtx->y, vtx->z);
    int c = 0;
    for(; c < skySphereColumns; ++c)
    {
        // One step down.
        vtx = &skyVertex(1, c);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        // And one step right.
        vtx = &skyVertex(0, c + 1);
        glVertex3f(vtx->x, vtx->y, vtx->z);
    }
    vtx = &skyVertex(1, c);
    glVertex3f(vtx->x, vtx->y, vtx->z);
    glEnd();
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
static void makeHemisphere(float height, float horizonOffset)
{
    float const maxSideAngle = float(de::PI / 2 * height);
    horizonOffset = float(de::PI / 2 * horizonOffset);

    if(skySphereDetail < 1) skySphereDetail = 1;
    if(skySphereRows < 1) skySphereRows = 1;

    skySphereColumns = 4 * skySphereDetail;

    hemisphereVerts.resize(skySphereColumns * (skySphereRows + 1));

    // Calculate the vertices.
    for(int r = 0; r < skySphereRows + 1; ++r)
    for(int c = 0; c < skySphereColumns; ++c)
    {
        Vector3f &svtx = skyVertex(r, c);

        float topAngle = ((c / float(skySphereColumns)) *2) * PI;
        float sideAngle = horizonOffset + maxSideAngle * (skySphereRows - r) / float(skySphereRows);
        float realRadius = cos(sideAngle);

        svtx = Vector3f(realRadius * cos(topAngle),
                        sin(sideAngle), // The height.
                        realRadius * sin(topAngle));
    }
}

static void rebuildHemisphereIfNeeded(Sky &sky)
{
    static bool firstBuild = true;
    static float oldHorizonOffset;
    static float oldHeight;

    // Rebuild our model if any paramaters have changed.
    if(firstBuild || sky.horizonOffset() != oldHorizonOffset)
    {
        oldHorizonOffset = sky.horizonOffset();
        needMakeHemisphere = true;
    }
    if(firstBuild || sky.height() != oldHeight)
    {
        oldHeight = sky.height();
        needMakeHemisphere = true;
    }
    firstBuild = false;

    if(!needMakeHemisphere) return;

    // We have work to do...
    needMakeHemisphere = false;
    makeHemisphere(sky.height(), sky.horizonOffset());
}

static void configureSphereDrawState(Sky &sky, int layerIndex, hemispherecap_t setupCap)
{
    // Default state is no texture and no fadeout.
    ds.texSize = Vector2i();
    if(setupCap != HC_NONE)
        ds.fadeout = false;
    ds.texXFlip = true;

    if(renderTextures != 0)
    {
        Material *mat;

        if(renderTextures == 2)
        {
            mat = ClientApp::resourceSystem().materialPtr(de::Uri("System", Path("gray")));
        }
        else
        {
            mat = sky.layer(layerIndex).material();
            if(!mat)
            {
                mat = ClientApp::resourceSystem().materialPtr(de::Uri("System", Path("missing")));
                ds.texXFlip = false;
            }
        }
        DENG2_ASSERT(mat != 0);

        MaterialSnapshot const &ms =
            mat->prepare(sphereMaterialSpec(sky.layer(layerIndex).isMasked()));

        ds.texSize = ms.texture(MTU_PRIMARY).generalCase().dimensions();
        if(ds.texSize != Vector2i(0, 0))
        {
            ds.texOffset = sky.layer(layerIndex).offset();
            GL_BindTexture(&ms.texture(MTU_PRIMARY));
        }
        else
        {
            // Disable texturing.
            ds.texSize = Vector2i();
            GL_SetNoTexture();
        }

        if(setupCap != HC_NONE)
        {
            averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>
                    (ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer((setupCap == HC_TOP? Texture::AverageTopColorAnalysis : Texture::AverageBottomColorAnalysis)));
            float const fadeoutLimit = sky.layer(layerIndex).fadeoutLimit();
            if(!avgLineColor) throw Error("configureDrawHemisphereState", QString("Texture \"%1\" has no %2").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()).arg(setupCap == HC_TOP? "AverageTopColorAnalysis" : "AverageBottomColorAnalysis"));

            ds.capColor = Vector3f(avgLineColor->color.rgb);
            // Is the colored fadeout in use?
            ds.fadeout = (ds.capColor.x >= fadeoutLimit ||
                          ds.capColor.y >= fadeoutLimit ||
                          ds.capColor.z >= fadeoutLimit);;
        }
    }
    else
    {
        GL_SetNoTexture();
    }

    if(setupCap != HC_NONE && !ds.fadeout)
    {
        // Default color is black.
        ds.capColor = Vector3f();
    }
}

/// @param flags  @ref skySphereRenderFlags
static void drawHemisphere(Sky &sky, SphereComponentFlags flags)
{
    int const firstLayer = sky.firstActiveLayer();
    DENG2_ASSERT(firstLayer >= 0);

    bool const yflip    = flags.testFlag(LowerHemisphere);
    hemispherecap_t cap = flags.testFlag(LowerHemisphere)? HC_BOTTOM : HC_TOP;

    // Rebuild the hemisphere model if necessary.
    rebuildHemisphereIfNeeded(sky);

    if(yflip)
    {
        // The lower hemisphere must be flipped.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glScalef(1.0f, -1.0f, 1.0f);
    }

    // First render the cap and the background for fadeouts, if needed.
    configureSphereDrawState(sky, firstLayer, cap);
    drawHemisphereCap();

    if(flags.testFlag(UpperHemisphere) || flags.testFlag(LowerHemisphere))
    {
        for(int i = firstLayer; i < MAX_SKY_LAYERS; ++i)
        {
            if(!sky.layer(i).isActive()) continue;

            if(i != firstLayer)
            {
                configureSphereDrawState(sky, i, HC_NONE);
            }

            if(ds.texSize.x != 0)
            {
                glEnable(GL_TEXTURE_2D);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                glTranslatef(ds.texOffset / ds.texSize.x, 0, 0);
                glScalef(1024.f / ds.texSize.x * (ds.texXFlip? 1 : -1), yflip? -1 : 1, 1);
                if(yflip) glTranslatef(0, -1, 0);
            }

#define WRITESKYVERTEX(r_, c_) { \
    svtx = &skyVertex(r_, c_); \
    if(ds.texSize.x != 0) \
       glTexCoord2f((c_) / float(skySphereColumns), (r_) / float(skySphereRows)); \
    if(ds.fadeout) \
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
            for(int r = 0; r < skySphereRows; ++r)
            {
                glBegin(GL_TRIANGLE_STRIP);
                WRITESKYVERTEX(r, 0);
                WRITESKYVERTEX(r + 1, 0);
                for(int c = 1; c <= skySphereColumns; ++c)
                {
                    WRITESKYVERTEX(r, c);
                    WRITESKYVERTEX(r + 1, c);
                }
                glEnd();
            }

            if(ds.texSize.x != 0)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
            }

#undef WRITESKYVERTEX
        }
    }

    if(yflip)
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

void Sky::cacheDrawableAssets()
{
    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Layer &lyr = d->layers[i];
        if(Material *mat = lyr.material())
        {
            d->resSys().cache(*mat, sphereMaterialSpec(lyr.isMasked()));
        }
    }

    if(d->haveModels)
    {
        for(int i = 0; i < MAX_SKY_MODELS; ++i)
        {
            ModelInfo &minfo = d->models[i];
            if(!minfo.def) continue;

            d->resSys().cache(minfo.model);
        }
    }
}

void Sky::draw()
{
    // Is there a sky to be rendered?
    if(firstActiveLayer() < 0) return;

    if(usingFog) glEnable(GL_FOG);

    // If sky models have been inited, they will be used.
    if(!d->haveModels || d->alwaysDrawSphere)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
        glScalef(skyDistance, skyDistance, skyDistance);

        // Always draw both hemispheres.
        drawHemisphere(*this, LowerHemisphere);
        drawHemisphere(*this, UpperHemisphere);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // How about some 3D models?
    if(d->haveModels)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        d->drawModels();

        // Restore assumed default GL state.
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    if(usingFog) glDisable(GL_FOG);
}

bool Sky::hasAnimator() const
{
    return d->animator != 0;
}

void Sky::setAnimator(Animator *newAnimator)
{
    d->animator = newAnimator;
}

Sky::Animator &Sky::animator()
{
    if(d->animator) return *d->animator;
    /// @throw MissingAnimatorError  No animator is presently configured.
    throw MissingAnimatorError("Sky::animator", "Missing animator");
}

static Sky sky;
Sky *theSky = &sky;

static void markSkySphereForRebuild()
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    needMakeHemisphere = true;
}

void Sky::consoleRegister() //static
{
    C_VAR_INT2 ("rend-sky-detail",   &skySphereDetail,   0,          3, 7, markSkySphereForRebuild);
    C_VAR_INT2 ("rend-sky-rows",     &skySphereRows,     0,          1, 8, markSkySphereForRebuild);
    C_VAR_FLOAT("rend-sky-distance", &skyDistance, CVF_NO_MAX, 1, 0);
}

#undef R_SkyParams
DENG_EXTERN_C void R_SkyParams(int layerIndex, int param, void * /*data*/)
{
    LOG_AS("R_SkyParams");
    if(sky.hasLayer(layerIndex))
    {
        Sky::Layer &layer = sky.layer(layerIndex);
        switch(param)
        {
        case DD_ENABLE:  layer.enable();  break;
        case DD_DISABLE: layer.disable(); break;
        default:
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Failed configuring layer #%i: bad parameter %i")
                    << layerIndex << param;
        }
        return;
    }
    LOG_GL_WARNING("Invalid layer #%i") << + layerIndex;
}
