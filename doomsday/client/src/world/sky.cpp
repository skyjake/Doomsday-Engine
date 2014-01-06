/** @file sky.cpp  Sky sphere and 3D models.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "world/sky.h"

#include "con_main.h"
#include "def_data.h"
#include "client/cl_def.h"

#include "Texture"

#ifdef __CLIENT__
#  include "gl/gl_tex.h"

#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"

#  include "render/rend_main.h"
#  include "render/rend_model.h"
#endif

#include "world/map.h"
#include "Sky"

#include <de/Log>
#ifdef __CLIENT__
#  include <de/Shared>
#endif
#include <cmath>

using namespace de;

enum SphereComponentFlag {
    UpperHemisphere = 0x1,
    LowerHemisphere = 0x2
};
Q_DECLARE_FLAGS(SphereComponentFlags, SphereComponentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(SphereComponentFlags)

static int skySphereColumns = 4*6;

/// Console variables:
static float skyDistance   = 1600; ///< Map units.
static int skySphereDetail = 6;
static int skySphereRows   = 3;

#ifdef __CLIENT__

static MaterialVariantSpec const &sphereMaterialSpec(bool masked)
{
    return App_ResourceSystem()
                .materialSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                              0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE,
                              0, -1, -1, false, true, false, false);
}

#endif // __CLIENT__

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

#ifdef __CLIENT__

#ifdef WIN32
#undef DrawState
#endif

// Hemisphere geometry used with the sky sphere.
struct HemisphereData
{
    QVector<Vector3f> vertices; // Crest is up.
    bool needMakeVertices;

    struct DrawState
    {
        bool fadeout, texXFlip;
        Vector2i texSize;
        float texOffset;
        Vector3f capColor;
    };
    DrawState ds;

    HemisphereData()
        : needMakeVertices(true)
        , vertices(0)
    {}

    // Look up the precalculated vertex.
    inline Vector3f &vertex(int r, int c)
    {
        return vertices[r * skySphereColumns + c % skySphereColumns];
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
        float const maxSideAngle = float(de::PI / 2 * height);
        horizonOffset = float(de::PI / 2 * horizonOffset);

        if(skySphereDetail < 1) skySphereDetail = 1;
        if(skySphereRows < 1) skySphereRows = 1;

        skySphereColumns = 4 * skySphereDetail;

        vertices.resize(skySphereColumns * (skySphereRows + 1));

        // Calculate the vertices.
        for(int r = 0; r < skySphereRows + 1; ++r)
        for(int c = 0; c < skySphereColumns; ++c)
        {
            Vector3f &vtx = vertex(r, c);

            float topAngle = ((c / float(skySphereColumns)) *2) * PI;
            float sideAngle = horizonOffset + maxSideAngle * (skySphereRows - r) / float(skySphereRows);
            float realRadius = cos(sideAngle);

            vtx = Vector3f(realRadius * cos(topAngle),
                           sin(sideAngle), // The height.
                           realRadius * sin(topAngle));
        }
    }

    void rebuildIfNeeded(float height, float horizonOffset)
    {
        static bool firstBuild = true;
        static float oldHorizonOffset;
        static float oldHeight;

        // Rebuild our model if any paramaters have changed.
        if(firstBuild || horizonOffset != oldHorizonOffset)
        {
            oldHorizonOffset = horizonOffset;
            needMakeVertices = true;
        }
        if(firstBuild || height != oldHeight)
        {
            oldHeight = height;
            needMakeVertices = true;
        }
        firstBuild = false;

        if(!needMakeVertices) return;

        // We have work to do...
        needMakeVertices = false;
        makeVertices(height, horizonOffset);
    }

    enum hemispherecap_t
    {
        HC_NONE = 0,
        HC_TOP,
        HC_BOTTOM
    };

    void configureDrawState(Sky::Layer const &skyLayer, hemispherecap_t setupCap)
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
                mat = App_ResourceSystem().materialPtr(de::Uri("System", Path("gray")));
            }
            else
            {
                mat = skyLayer.material();
                if(!mat)
                {
                    mat = App_ResourceSystem().materialPtr(de::Uri("System", Path("missing")));
                    ds.texXFlip = false;
                }
            }
            DENG2_ASSERT(mat != 0);

            MaterialSnapshot const &ms =
                mat->prepare(sphereMaterialSpec(skyLayer.isMasked()));

            ds.texSize = ms.texture(MTU_PRIMARY).generalCase().dimensions();
            if(ds.texSize != Vector2i(0, 0))
            {
                ds.texOffset = skyLayer.offset();
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
                float const fadeoutLimit = skyLayer.fadeoutLimit();
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

    void drawCap()
    {
        // Use the appropriate color.
        glColor3f(ds.capColor.x, ds.capColor.y, ds.capColor.z);

        // Draw the cap.
        glBegin(GL_TRIANGLE_FAN);
        for(int c = 0; c < skySphereColumns; ++c)
        {
            Vector3f const &vtx = vertex(0, c);
            glVertex3f(vtx.x, vtx.y, vtx.z);
        }
        glEnd();

        // Are we doing a colored fadeout?
        if(!ds.fadeout) return;

        // We must fill the background for the top row since it'll be translucent.
        glBegin(GL_TRIANGLE_STRIP);
        Vector3f const *vtx = &vertex(0, 0);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        int c = 0;
        for(; c < skySphereColumns; ++c)
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

    /// @param flags  @ref skySphereRenderFlags
    void draw(Sky const &sky, SphereComponentFlags flags)
    {
        int const firstLayer = sky.firstActiveLayer();

        bool const yflip    = flags.testFlag(LowerHemisphere);
        hemispherecap_t cap = flags.testFlag(LowerHemisphere)? HC_BOTTOM : HC_TOP;

        // Rebuild the hemisphere model if necessary.
        rebuildIfNeeded(sky.height(), sky.horizonOffset());

        if(yflip)
        {
            // The lower hemisphere must be flipped.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glScalef(1.0f, -1.0f, 1.0f);
        }

        configureDrawState(sky.layer(firstLayer), cap);

        // First render the cap and the background for fadeouts, if needed.
        drawCap();

        if(flags.testFlag(UpperHemisphere) || flags.testFlag(LowerHemisphere))
        {
            for(int i = firstLayer; i <= MAX_SKY_LAYERS; ++i)
            {
                SkyLayer const &skyLayer = sky.layer(i);

                if(!skyLayer.isActive()) continue;

                if(i != firstLayer)
                {
                    configureDrawState(skyLayer, HC_NONE);
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
    vtx = &vertex(r_, c_); \
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
    glVertex3f(vtx->x, vtx->y, vtx->z); \
}

                Vector3f const *vtx;
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
};

DENG2_SHARED_INSTANCE(HemisphereData)

#endif // __CLIENT__

DENG2_PIMPL(Sky)
, DENG2_OBSERVES(Layer, MaterialChange)
, DENG2_OBSERVES(Layer, ActiveChange)
, DENG2_OBSERVES(Layer, MaskedChange)
{
    Layer layers[MAX_SKY_LAYERS];
    int firstActiveLayer;
    bool needUpdateFirstActiveLayer;

    float horizonOffset;
    float height;
    bool ambientColorDefined;    /// @c true= pre-defined in a MapInfo def.
    bool needUpdateAmbientColor; /// @c true= update if not pre-defined.
    Vector3f ambientColor;

#ifdef __CLIENT__
    typedef Shared<HemisphereData> SharedHemisphereData;
    SharedHemisphereData *res;
    bool alwaysDrawSphere;

    struct ModelInfo
    {
        ded_skymodel_t const *def;
        ModelDef *model;
        int frame;
        int timer;
        int maxTimer;
        float yaw;
    };
    ModelInfo models[MAX_SKY_MODELS];
    bool haveModels;
#endif

    Instance(Public *i)
        : Base(i)
        , firstActiveLayer(-1) /// @c -1 denotes 'no active layers'.
        , needUpdateFirstActiveLayer(true)
        , horizonOffset(0)
        , height(0)
        , ambientColorDefined(false)
        , needUpdateAmbientColor(true)
#ifdef __CLIENT__
        , res(0)
        , alwaysDrawSphere(false)
        , haveModels(false)
#endif
    {
        for(int i = 0; i < MAX_SKY_LAYERS; ++i)
        {
            layers[i].audienceForMaterialChange += this;
            layers[i].audienceForActiveChange   += this;
            layers[i].audienceForMaskedChange   += this;
        }

#ifdef __CLIENT__
        zap(models);
#endif
    }

#ifdef __CLIENT__
    ~Instance()
    {
        DENG2_ASSERT(res == 0); // should have been deinited
        releaseRef(res);
    }
#endif

    inline ResourceSystem &resSys()
    {
        return App_ResourceSystem();
    }

    void updateFirstActiveLayer()
    {
        needUpdateFirstActiveLayer = false;

        // -1 denotes 'no active layers'.
        firstActiveLayer = -1;

        for(int i = 0; i < MAX_SKY_LAYERS; ++i)
        {
            if(layers[i].isActive())
            {
                firstActiveLayer = i;
            }
        }
    }

    /**
     * @todo Reimplement by rendering the sky to a low-quality cubemap and using
     * this to obtain the lighting characteristics. Doing it this way we can sample
     * the lighting of a sky using 3D models, also.
     */
    void calculateAmbientColor()
    {
        needUpdateAmbientColor = false;

        ambientColor = Vector3f(1, 1, 1);

#ifdef __CLIENT__
        // Only the client can automatically calculate this color.
        if(haveModels && !alwaysDrawSphere) return;

        Vector3f avgMaterialColor;
        Vector3f bottomCapColor;
        Vector3f topCapColor;

        int avgCount = 0;
        for(int i = 0; i < MAX_SKY_LAYERS; ++i)
        {
            Layer &layer = layers[firstActiveLayer + i];

            if(!layer.isActive()) continue;
            if(!layer.material()) continue;

            MaterialSnapshot const &ms = layer.material()
                    ->prepare(sphereMaterialSpec(layer.isMasked()));

            if(ms.hasTexture(MTU_PRIMARY))
            {
                Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
                averagecolor_analysis_t const *avgColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageColorAnalysis));
                if(!avgColor) throw Error("sky::calculateAmbientColor", QString("Texture \"%1\" has no AverageColorAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

                if(i == firstActiveLayer)
                {
                    averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageTopColorAnalysis));
                    if(!avgLineColor) throw Error("sky::calculateAmbientColor", QString("Texture \"%1\" has no AverageTopColorAnalysis").arg(tex.manifest().composeUri()));

                    topCapColor = Vector3f(avgLineColor->color.rgb);

                    avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>(tex.analysisDataPointer(Texture::AverageBottomColorAnalysis));
                    if(!avgLineColor) throw Error("sky::calculateAmbientColor", QString("Texture \"%1\" has no AverageBottomColorAnalysis").arg(tex.manifest().composeUri()));

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
            ambientColor =
                (avgMaterialColor + topCapColor + bottomCapColor) / (avgCount + 2);
        }

#endif // __CLIENT__
    }

#ifdef __CLIENT__
    /**
     * Models are set up using the data in the definition.
     */
    void setupModels(ded_sky_t const *def)
    {
        zap(models);

        // Normally the sky sphere is not drawn if models are in use.
        alwaysDrawSphere = (def->flags & SIF_DRAW_SPHERE) != 0;

        // The normal sphere is used if no models will be set up.
        haveModels = false;

        ded_skymodel_t const *modef = def->models;
        ModelInfo *minfo = models;
        for(int i = 0; i < MAX_SKY_MODELS; ++i, modef++, minfo++)
        {
            // Is the model ID set?
            try
            {
                minfo->model = &resSys().modelDef(modef->id);
                if(!minfo->model->subCount())
                {
                    continue;
                }

                // There is a model here.
                haveModels = true;

                minfo->def      = modef;
                minfo->maxTimer = (int) (TICSPERSEC * modef->frameInterval);
                minfo->yaw      = modef->yaw;
                minfo->frame    = minfo->model->subModelDef(0).frame;
            }
            catch(ResourceSystem::MissingModelDefError const &)
            {} // Ignore this error.
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

        for(int i = 0; i < NUM_SKY_MODELS; ++i)
        {
            ModelInfo &minfo = models[i];
            if(!minfo.def) continue;

            if(!self.layer(minfo.def->layer + 1).isActive())
            {
                continue;
            }

            float inter = (minfo.maxTimer > 0 ? minfo.timer / float(minfo.maxTimer) : 0);

            drawmodelparams_t parms; zap(parms);

            // Calculate the coordinates for the model.
            parms.origin[VX]        = vOrigin.x * -minfo.def->coordFactor[VX];
            parms.origin[VY]        = vOrigin.z * -minfo.def->coordFactor[VZ];
            parms.origin[VZ]        = vOrigin.y * -minfo.def->coordFactor[VY];
            parms.gzt               = parms.origin[VZ];
            parms.distance          = 1;

            parms.extraYawAngle     = parms.yawAngleOffset   = minfo.def->rotate[0];
            parms.extraPitchAngle   = parms.pitchAngleOffset = minfo.def->rotate[1];
            parms.inter             = inter;
            parms.mf                = minfo.model;
            parms.alwaysInterpolate = true;
            App_ResourceSystem().setModelDefFrame(*minfo.model, minfo.frame);
            parms.yaw               = minfo.yaw;
            for(int c = 0; c < 4; ++c)
            {
                parms.ambientColor[c] = minfo.def->color[c];
            }
            parms.vLightListIdx     = 0;
            parms.shineTranslateWithViewerPos = true;

            Rend_DrawModel(parms);
        }

        // We don't want that anything interferes with what was drawn.
        //glClear(GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    void drawSphere()
    {
        res = SharedHemisphereData::hold();
        HemisphereData &hemi = *res;

        // Always draw both hemispheres.
        hemi.draw(self, LowerHemisphere);
        hemi.draw(self, UpperHemisphere);

        releaseRef(res);
    }

#endif // __CLIENT__

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
    return (index > 0 && index <= MAX_SKY_LAYERS);
}

Sky::Layer &Sky::layer(int index)
{
    if(hasLayer(index))
    {
        return d->layers[index - 1]; // 1-based index.
    }
    /// @throw MissingLayerError An invalid layer index was specified.
    throw MissingLayerError("Sky::Layer", "Invalid layer index #" + String::number(index) + ".");
}

Sky::Layer const &Sky::layer(int index) const
{
    return const_cast<Layer const &>(const_cast<Sky *>(this)->layer(index));
}

int Sky::firstActiveLayer() const
{
    // Do we need to redetermine the first active layer?
    if(d->needUpdateFirstActiveLayer)
    {
        d->updateFirstActiveLayer();
    }
    return d->firstActiveLayer + 1; // 1-based index.
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
            lyr.setMaterial(App_ResourceSystem().materialPtr(de::Uri(DEFAULT_SKY_SPHERE_MATERIAL, RC_NULL)));
        }
        catch(MaterialManifest::MissingMaterialError const &)
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

static bool useAutomaticAmbientColor()
{
#ifdef __CLIENT__
    return rendSkyLightAuto;
#else
    return false;
#endif
}

Vector3f const &Sky::ambientColor() const
{
    static Vector3f const white(1, 1, 1);

    if(d->ambientColorDefined || useAutomaticAmbientColor())
    {
        if(!d->ambientColorDefined)
        {
            // Do we need to recalculate the ambient color?
            if(d->needUpdateAmbientColor)
            {
                d->calculateAmbientColor();
            }
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

void Sky::configure(ded_sky_t *def)
{
    LOG_AS("Sky");

    // The default configuration is used as a starting point.
    configureDefault();

    if(!def) return; // Go with the defaults, then.

    setHeight(def->height);
    setHorizonOffset(def->horizonOffset);

    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        ded_skylayer_t const &lyrDef = def->layers[i];
        Layer &lyr = d->layers[i];

        if(!(lyrDef.flags & Layer::Active))
        {
            lyr.disable();
            continue;
        }

        lyr.setMasked((lyrDef.flags & Layer::Masked) != 0)
           .setOffset(lyrDef.offset)
           .setFadeoutLimit(lyrDef.colorLimit)
           .enable();

        if(de::Uri *matUri = reinterpret_cast<de::Uri *>(lyrDef.material))
        {
            try
            {
                lyr.setMaterial(App_ResourceSystem().materialPtr(*matUri));
            }
            catch(ResourceSystem::MissingManifestError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_WARNING(er.asText() + ". Unknown material \"%s\" in definition layer %i, using default.")
                    << *matUri << i;
            }
        }
    }

    if(def->color[CR] > 0 || def->color[CG] > 0 || def->color[CB] > 0)
    {
        setAmbientColor(def->color);
    }

#ifdef __CLIENT__
    // Any sky models to setup? Models will override the normal sphere by default.
    d->setupModels(def);
#endif
}

void Sky::runTick(timespan_t /*elapsed*/)
{
#ifdef __CLIENT__
    // Animate sky models.
    if(clientPaused) return;
    if(!DD_IsSharpTick()) return;
    if(!d->haveModels) return;

    for(int i = 0; i < MAX_SKY_MODELS; ++i)
    {
        Instance::ModelInfo &minfo = d->models[i];
        if(!minfo.def) continue;

        // Rotate the model.
        minfo.yaw += minfo.def->yawSpeed / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(minfo.maxTimer > 0 && ++minfo.timer >= minfo.maxTimer)
        {
            minfo.frame++;
            minfo.timer = 0;

            // Execute a console command?
            if(minfo.def->execute)
            {
                Con_Execute(CMDS_SCRIPT, minfo.def->execute, true, false);
            }
        }
    }
#endif
}

#ifdef __CLIENT__

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
            Instance::ModelInfo &minfo = d->models[i];
            if(!minfo.def) continue;

            d->resSys().cache(minfo.model);
        }
    }
}

void Sky::draw()
{
    // Is there a sky to be rendered?
    if(!firstActiveLayer()) return;

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

        d->drawSphere();

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

static void markSkySphereForRebuild()
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    /// @todo fixme: Update the shared sphere geometry!
    //hemi.needMakeHemisphere = true;
}
#endif // __CLIENT__

void Sky::consoleRegister() //static
{
#ifdef __CLIENT__
    C_VAR_INT2 ("rend-sky-detail",   &skySphereDetail,   0,          3, 7, markSkySphereForRebuild);
    C_VAR_INT2 ("rend-sky-rows",     &skySphereRows,     0,          1, 8, markSkySphereForRebuild);
    C_VAR_FLOAT("rend-sky-distance", &skyDistance,       CVF_NO_MAX, 1, 0);
#endif
}

static void setSkyLayerParams(Sky &sky, int layerIndex, int param, void *data)
{
    try
    {
        Sky::Layer &layer = sky.layer(layerIndex);

        switch(param)
        {
        case DD_ENABLE:      layer.enable(); break;
        case DD_DISABLE:     layer.disable(); break;
        case DD_MASK:        layer.setMasked(*((int *)data) == DD_YES); break;
        case DD_MATERIAL:    layer.setMaterial((Material *)data); break;
        case DD_OFFSET:      layer.setOffset(*((float *)data)); break;
        case DD_COLOR_LIMIT: layer.setFadeoutLimit(*((float *)data)); break;

        default:
            // Log but otherwise ignore this error.
            LOG_WARNING("Bad parameter %i. Failed configuring sky layer #%i, ignoring.")
                << param << layerIndex;
        }
    }
    catch(Sky::MissingLayerError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ". Failed configuring sky layer #%i, ignoring.")
            << layerIndex;
    }
}

#undef R_SkyParams
DENG_EXTERN_C void R_SkyParams(int layerIndex, int param, void *data)
{
    LOG_AS("R_SkyParams");

    if(!App_World().hasMap()) return;

    /// @todo Do not assume the current map.
    Sky &sky = App_World().map().sky();

    // The whole sky?
    if(layerIndex == DD_SKY)
    {
        switch(param)
        {
        case DD_HEIGHT:  sky.setHeight(*((float *)data)); break;
        case DD_HORIZON: sky.setHorizonOffset(*((float *)data)); break;

        default: // Operate on all layers.
            for(int i = 1; i <= MAX_SKY_LAYERS; ++i)
            {
                setSkyLayerParams(sky, i, param, data);
            }
        }
        return;
    }

    // A specific layer?
    if(layerIndex >= 0 && layerIndex < MAX_SKY_LAYERS)
    {
        setSkyLayerParams(sky, layerIndex + 1, param, data);
    }
}
