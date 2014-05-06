/** @file lensflares.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/fx/lensflares.h"
#include "render/ilightsource.h"
#include "render/viewports.h"
#include "render/rend_main.h"
#include "gl/gl_main.h"
#include "clientapp.h"

#include <de/concurrency.h>
#include <de/Drawable>
#include <de/KdTreeAtlasAllocator>
#include <de/Log>
#include <de/Range>
#include <de/Shared>

#include <QHash>
#include <cmath>

//#define FX_TEST_LIGHT // draw a test light (positioned for Doom E1M1)

using namespace de;

namespace fx {

/**
 * Shared GL resources for rendering lens flares.
 */
struct FlareData
{
    ImageBank images;
    AtlasTexture atlas;
    enum FlareId {
        Burst,
        Circle,
        Exponent,
        Halo,
        Ring,
        Star,
        MAX_FLARES
    };
    enum Corner {
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft
    };
    NoneId flare[MAX_FLARES];

    FlareData()
        : atlas(Atlas::BackingStore, Atlas::Size(1024, 1024))
    {
        try
        {
            DENG_ASSERT_IN_MAIN_THREAD();
            DENG_ASSERT_GL_CONTEXT_ACTIVE();

            Folder const &pack = App::fileSystem().find<Folder>("lensflares.pack");
            images.addFromInfo(pack.locate<File>("images.dei"));

            atlas.setAllocator(new KdTreeAtlasAllocator);

            flare[Exponent] = atlas.alloc(flareImage("exponent"));
            flare[Star]     = atlas.alloc(flareImage("star"));
            flare[Halo]     = atlas.alloc(flareImage("halo"));
            flare[Circle]   = atlas.alloc(flareImage("circle"));
            flare[Ring]     = atlas.alloc(flareImage("ring"));
            flare[Burst]    = atlas.alloc(flareImage("burst"));
        }
        catch(Error const &er)
        {
            LOG_GL_ERROR("Failed to initialize shared lens flare resources: %s")
                    << er.asText();
        }
    }

    ~FlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        LOGDEV_GL_XVERBOSE("Releasing shared data");
    }

    Image const &flareImage(String const &name)
    {
        return images.image("fx.lensflares." + name);
    }

    Rectanglef uvRect(FlareId id) const
    {
        return atlas.imageRectf(flare[id]);
    }

    Vector2f flareCorner(FlareId id, Corner corner) const
    {
        Vector2f p;
        switch(corner)
        {
        case TopLeft:     p = Vector2f(-1, -1); break;
        case TopRight:    p = Vector2f( 1, -1); break;
        case BottomRight: p = Vector2f( 1,  1); break;
        case BottomLeft:  p = Vector2f(-1,  1); break;
        }

        if(id == Burst)
        {
            // Non-square.
            p *= Vector2f(4, .25f);
        }

        return p;
    }
};

#ifdef FX_TEST_LIGHT
struct TestLight : public IPointLightSource
{
public:
    float radius;
    Colorf color;
    float intensity;

    TestLight() : radius(1), color(1, 1, 1), intensity(1)
    {}

    LightId lightSourceId() const {
        return 1;
    }
    Origin lightSourceOrigin() const {
        //return Origin(0, 0, 0);
        return Origin(782, -3227, 30);
    }
    dfloat lightSourceRadius() const {
        return radius;
    }
    Colorf lightSourceColorf() const {
        return color;
    }
    dfloat lightSourceIntensity(de::Vector3d const &) const {
        return intensity;
    }
};

static TestLight testLight;

D_CMD(TestLight)
{
    String prop = argv[1];
    float value = String(argv[2]).toFloat();

    if(!prop.compareWithoutCase("rd"))
    {
        fx::testLight.radius = value;
    }
    else if(!prop.compareWithoutCase("in"))
    {
        fx::testLight.intensity = value;
    }
    else if(!prop.compareWithoutCase("cl") && argc >= 5)
    {
        fx::testLight.color = ILightSource::Colorf(value, String(argv[3]).toFloat(), String(argv[4]).toFloat());
    }
    else
    {
        return false;
    }

    return true;
}
#endif

static float linearRangeFactor(float value, Rangef const &low, Rangef const &high)
{
    if(low.size() > 0)
    {
        if(value < low.start)
        {
            return 0;
        }
        if(low.contains(value))
        {
            return (value - low.start) / low.size();
        }
    }

    if(high.size() > 0)
    {
        if(value > high.end)
        {
            return 0;
        }
        if(high.contains(value))
        {
            return 1 - (value - high.start) / high.size();
        }
    }

    return 1;
}

DENG2_PIMPL(LensFlares)
{
    typedef Shared<FlareData> SharedFlareData;
    SharedFlareData *res;

    /**
     * Current state of a potentially visible light.
     */
    struct PVLight
    {
        IPointLightSource const *light;
        int seenFrame; // R_FrameCount()

        PVLight() : light(0), seenFrame(0)
        {}
    };

    typedef QHash<IPointLightSource::LightId, PVLight *> PVSet;
    PVSet pvs;

    Vector3f eyeFront;

    typedef GLBufferT<Vertex3Tex3Rgba> VBuf;
    VBuf *buffer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uViewUnit;
    GLUniform uPixelAsUv;
    GLUniform uActiveRect;
    GLUniform uAtlas;
    GLUniform uDepthBuf;

    Instance(Public *i)
        : Base(i)
        , res(0)
        , buffer(0)
        , uMvpMatrix ("uMvpMatrix",  GLUniform::Mat4)
        , uViewUnit  ("uViewUnit",   GLUniform::Vec2)
        , uPixelAsUv ("uPixelAsUv",  GLUniform::Vec2)
        , uActiveRect("uActiveRect", GLUniform::Vec4)
        , uAtlas     ("uTex",        GLUniform::Sampler2D)
        , uDepthBuf  ("uDepthBuf",   GLUniform::Sampler2D)
    {}

    ~Instance()
    {
        DENG2_ASSERT(res == 0); // should have been deinited
        releaseRef(res);
        clearPvs();
    }

    void glInit()
    {
        // Acquire a reference to the shared flare data.
        res = SharedFlareData::hold();

        buffer = new VBuf;
        drawable.addBuffer(buffer);
        self.shaders().build(drawable.program(), "fx.lensflares")
                << uMvpMatrix
                << uViewUnit << uPixelAsUv << uActiveRect
                << uAtlas << uDepthBuf;

        uAtlas = res->atlas;
    }

    void glDeinit()
    {
        drawable.clear();
        buffer = 0;
        clearPvs();
        releaseRef(res);
    }

    void clearPvs()
    {
        qDeleteAll(pvs);
        pvs.clear();
    }

    void addToPvs(IPointLightSource const *light)
    {
        PVSet::iterator found = pvs.find(light->lightSourceId());
        if(found == pvs.end())
        {
            found = pvs.insert(light->lightSourceId(), new PVLight);
        }

        PVLight *pvl = found.value();
        pvl->light = light;
        pvl->seenFrame = R_FrameCount();
    }

    void makeFlare(VBuf::Vertices &   verts,
                   VBuf::Indices &    idx,
                   FlareData::FlareId id,
                   float              axisPos,
                   float              radius,
                   Vector4f           color,
                   PVLight const *    pvl)
    {
        Rectanglef const uvRect = res->uvRect(id);
        int const firstIdx = verts.size();

        VBuf::Type vtx;
        vtx.pos  = pvl->light->lightSourceOrigin().xzy();
        vtx.rgba = Vector4f(pvl->light->lightSourceColorf(), 1.f) * color;
        vtx.texCoord[2] = Vector2f(axisPos, 0);

        vtx.texCoord[0] = uvRect.topLeft;
        vtx.texCoord[1] = res->flareCorner(id, FlareData::TopLeft) * radius;
        verts << vtx;

        vtx.texCoord[0] = uvRect.topRight();
        vtx.texCoord[1] = res->flareCorner(id, FlareData::TopRight) * radius;
        verts << vtx;

        vtx.texCoord[0] = uvRect.bottomRight;
        vtx.texCoord[1] = res->flareCorner(id, FlareData::BottomRight) * radius;
        verts << vtx;

        vtx.texCoord[0] = uvRect.bottomLeft();
        vtx.texCoord[1] = res->flareCorner(id, FlareData::BottomLeft) * radius;
        verts << vtx;

        // Make two triangles.
        idx << firstIdx << firstIdx + 1 << firstIdx + 2
            << firstIdx << firstIdx + 2 << firstIdx + 3;
    }

    void makeVerticesForPVS()
    {
        int const thisFrame = R_FrameCount();

        // The vertex buffer will contain a number of quads.
        VBuf::Vertices verts;
        VBuf::Indices idx;
        VBuf::Type vtx;

        for(PVSet::const_iterator i = pvs.constBegin(); i != pvs.constEnd(); ++i)
        {
            PVLight const *pvl = i.value();

            // Skip lights that are not visible right now.
            /// @todo If so, it might be time to purge it from the PVS.
            if(pvl->seenFrame != thisFrame) continue;

            coord_t const distanceSquared = (vOrigin - pvl->light->lightSourceOrigin().xzy()).lengthSquared();
            coord_t const distance = std::sqrt(distanceSquared);

            // Light intensity is always quadratic per distance.
            float intensity = pvl->light->lightSourceIntensity(vOrigin) / distanceSquared;

            // Projected radius of the light.
            float const RADIUS_FACTOR = 128; // Light radius of 1 at this distance produces a visible radius of 1.
            /// @todo The factor should be FOV-dependent.
            float radius = pvl->light->lightSourceRadius() / distance * RADIUS_FACTOR;

            float const dot = (pvl->light->lightSourceOrigin().xzy() - vOrigin).normalize().dot(eyeFront);
            float const angle = radianToDegree(std::acos(dot));

            //qDebug() << "i:" << intensity << "r:" << radius << "IR:" << radius*intensity;

            /*
             * The main flare.
             * - small + bright => burst
             * - big + bright   => star
             * - small + dim    => exponent
             * - big + dim      => exponent
             */

            struct Spec {
                float axisPos;
                FlareData::FlareId id;
                Vector4f color;
                float size;
                Rangef minIntensity;
                Rangef maxIntensity;
                Rangef minRadius;
                Rangef maxRadius;
                Rangef minAngle;
                Rangef maxAngle;
            };
            typedef Rangef Rgf;
            static Spec const specs[] = {
                //  axisPos id                   color                          size    intensity min/max              radius min/max          angle min/max
                {   1,      FlareData::Burst,    Vector4f(1, 1, 1, 1),          1,      Rgf(1.0e-8f, 1.0e-6f), Rgf(),  Rgf(), Rgf(.5f, .8f),   Rgf(), Rgf() },
                {   1,      FlareData::Star,     Vector4f(1, 1, 1, 1),          1,      Rgf(1.0e-6f, 1.0e-5f), Rgf(),  Rgf(.5f, .7f), Rgf(),   Rgf(), Rgf() },
                {   1,      FlareData::Exponent, Vector4f(1, 1, 1, 1),          2.5f,   Rgf(1.0e-6f, 1.0e-5f), Rgf(),  Rgf(.1f, .2f), Rgf(),   Rgf(), Rgf() },

                {  .8f,     FlareData::Halo,     Vector4f(1, 1, 1, .5f),        1,      Rgf(5.0e-6f, 5.0e-5f), Rgf(),  Rgf(.5f, .7f), Rgf(),   Rgf(), Rgf(30, 60) },

                {  -.8f,    FlareData::Ring,     Vector4f(.4f, 1, .4f, .26f),   .4f,    Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(5, 20), Rgf(40, 50) },
                {  -1,      FlareData::Circle,   Vector4f(.4f, .4f, 1, .30f),   .5f,    Rgf(4.0e-6f, 4.0e-5f), Rgf(),  Rgf(.08f, .45f), Rgf(), Rgf(0, 23), Rgf(30, 60) },
                {  -1.2f ,  FlareData::Ring,     Vector4f(1, .4f, .4f, .26f),   .56f,   Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(35, 50) },

                {  1.333f,  FlareData::Ring,     Vector4f(.5f, .5f, 1, .1f),    1.2f,   Rgf(1.0e-8f, 1.0e-7f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(25, 45) },
                {  1.45f,   FlareData::Ring,     Vector4f(1, .5f, .5f, .15f),   1.15f,  Rgf(1.0e-8f, 1.0e-7f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(25, 45) },

                {  -1.45f,  FlareData::Ring,     Vector4f(1, 1, .9f, .25f),     .2f,    Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .4f), Rgf(),   Rgf(5, 10), Rgf(15, 30) },
                {  -.2f,    FlareData::Circle,   Vector4f(1, 1, .9f, .2f),      .23f,   Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .4f), Rgf(),   Rgf(5, 10), Rgf(15, 30) },
            };

            for(uint i = 0; i < sizeof(specs)/sizeof(specs[0]); ++i)
            {
                Spec const &spec = specs[i];

                float size = radius * spec.size;
                Vector4f color = spec.color;

                // Apply limits.
                color.w *= linearRangeFactor(intensity, spec.minIntensity, spec.maxIntensity);
                color.w *= linearRangeFactor(radius, spec.minRadius, spec.maxRadius);
                color.w *= linearRangeFactor(angle, spec.minAngle, spec.maxAngle);

                //qDebug() << linearRangeFactor(intensity, spec.minIntensity, spec.maxIntensity);
                //qDebug() << linearRangeFactor(radius, spec.minRadius, spec.maxRadius);

                makeFlare(verts, idx, spec.id, spec.axisPos, size, color, pvl);
            }

            //makeFlare(verts, idx, FlareData::Halo, -1, size, pvl);


            /*
            // Project viewtocenter vector onto viewSideVec.
            Vector3f const eyeToFlare = pvl->lightSourceOrigin() - eyePos;

            // Calculate the 'mirror' vector.
            float const scale = viewToCenter.dot(viewData->frontVec)
                                / Vector3f(viewData->frontVec).dot(viewData->frontVec);
            Vector3f const mirror =
                (Vector3f(viewData->frontVec) * scale - viewToCenter) * 2;
            */
        }

        buffer->setVertices(verts, gl::Dynamic);
        buffer->setIndices(gl::Triangles, idx, gl::Dynamic);
    }
};

LensFlares::LensFlares(int console) : ConsoleEffect(console), d(new Instance(this))
{}

void LensFlares::clearLights()
{
    d->clearPvs();
}

void LensFlares::markLightPotentiallyVisibleForCurrentFrame(IPointLightSource const *lightSource)
{
    d->addToPvs(lightSource);
}

void LensFlares::glInit()
{
    LOG_AS("fx::LensFlares");

    ConsoleEffect::glInit();
    d->glInit();
}

void LensFlares::glDeinit()
{
    LOG_AS("fx::LensFlares");

    d->glDeinit();
    ConsoleEffect::glDeinit();
}

void fx::LensFlares::beginFrame()
{
#ifdef FX_TEST_LIGHT
    markLightPotentiallyVisibleForCurrentFrame(&testLight); // testing
#endif

    d->makeVerticesForPVS();
}

void LensFlares::draw()
{
    viewdata_t const *viewData = R_ViewData(console());
    d->eyeFront = Vector3f(viewData->frontVec);

    Rectanglef const rect = viewRect();
    float const aspect = rect.height() / rect.width();

    Canvas &canvas = ClientWindow::main().canvas();

    d->uViewUnit  = Vector2f(aspect, 1.f);
    d->uPixelAsUv = Vector2f(1.f / canvas.width(), 1.f / canvas.height());
    d->uMvpMatrix = GL_GetProjectionMatrix() * Rend_GetModelViewMatrix(console());

    // Depth information is required for occlusion.
    GLTarget &target = GLState::current().target();
    GLTexture *depthTex = target.attachedTexture(GLTarget::Depth);
    /**
     * @todo Handle the situation when depth information is not available in the target.
     */
    d->uDepthBuf = depthTex;

    // The active rectangle is specified with top/left coordinates, but the shader
    // works with bottom/left ones.
    Vector4f active(target.activeRectScale(), target.activeRectNormalizedOffset());
    active.w = 1 - (active.w + active.y); // flip y
    d->uActiveRect = active;

    GLState::push()
            .setCull(gl::None)
            .setDepthTest(false)
            .setDepthWrite(false)
            .setBlend(true)
            .setBlendFunc(gl::SrcAlpha, gl::One);

    d->drawable.draw();

    GLState::pop().apply();
}

void LensFlares::consoleRegister()
{
#ifdef FX_TEST_LIGHT
    C_CMD("testlight", "sf*", TestLight)
#endif
}

} // namespace fx

DENG2_SHARED_INSTANCE(fx::FlareData)
