/** @file lensflares.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "world/p_players.h"
#include "world/clientworld.h"
#include "gl/gl_main.h"
#include "clientapp.h"

#include <de/legacy/concurrency.h>
#include <doomsday/console/cmd.h>
#include <de/drawable.h>
#include <de/filesystem.h>
#include <de/kdtreeatlasallocator.h>
#include <de/logbuffer.h>
#include <de/range.h>
#include <de/shared.h>
#include <de/hash.h>

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
            DE_ASSERT_IN_RENDER_THREAD();
            DE_ASSERT_GL_CONTEXT_ACTIVE();

            images.addFromInfo(App::rootFolder().locate<File>("/packs/feature.lensflares/images.dei"));

            atlas.setAllocator(new KdTreeAtlasAllocator);

            flare[Exponent] = atlas.alloc(flareImage("exponent"));
            flare[Star]     = atlas.alloc(flareImage("star"));
            flare[Halo]     = atlas.alloc(flareImage("halo"));
            flare[Circle]   = atlas.alloc(flareImage("circle"));
            flare[Ring]     = atlas.alloc(flareImage("ring"));
            flare[Burst]    = atlas.alloc(flareImage("burst"));
        }
        catch (const Error &er)
        {
            LOG_GL_ERROR("Failed to initialize shared lens flare resources: %s")
                    << er.asText();
        }
    }

    ~FlareData()
    {
        DE_ASSERT_IN_MAIN_THREAD();
        DE_ASSERT_GL_CONTEXT_ACTIVE();

        LOGDEV_GL_XVERBOSE("Releasing shared data", "");
    }

    const Image &flareImage(const String &name)
    {
        return images.image("fx.lensflares." + name);
    }

    Rectanglef uvRect(FlareId id) const
    {
        return atlas.imageRectf(flare[id]);
    }

    Vec2f flareCorner(FlareId id, Corner corner) const
    {
        Vec2f p;
        switch (corner)
        {
        case TopLeft:     p = Vec2f(-1, -1); break;
        case TopRight:    p = Vec2f( 1, -1); break;
        case BottomRight: p = Vec2f( 1,  1); break;
        case BottomLeft:  p = Vec2f(-1,  1); break;
        }

        if (id == Burst)
        {
            // Non-square.
            p *= Vec2f(4, .25f);
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
    dfloat lightSourceIntensity(const de::Vec3d &) const {
        return intensity;
    }
};

static TestLight testLight;

D_CMD(TestLight)
{
    String prop = argv[1];
    float value = String(argv[2]).toFloat();

    if (!prop.compareWithoutCase("rd"))
    {
        fx::testLight.radius = value;
    }
    else if (!prop.compareWithoutCase("in"))
    {
        fx::testLight.intensity = value;
    }
    else if (!prop.compareWithoutCase("cl") && argc >= 5)
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

static float linearRangeFactor(float value, const Rangef &low, const Rangef &high)
{
    if (low.size() > 0)
    {
        if (value < low.start)
        {
            return 0;
        }
        if (low.contains(value))
        {
            return (value - low.start) / low.size();
        }
    }

    if (high.size() > 0)
    {
        if (value > high.end)
        {
            return 0;
        }
        if (high.contains(value))
        {
            return 1 - (value - high.start) / high.size();
        }
    }

    return 1;
}

DE_PIMPL(LensFlares)
{
    typedef Shared<FlareData> SharedFlareData;
    SharedFlareData *res;

    /**
     * Current state of a potentially visible light.
     */
    struct PVLight
    {
        const IPointLightSource *light;
        int seenFrame; // R_FrameCount()

        PVLight() : light(0), seenFrame(0)
        {}
    };

    typedef Hash<IPointLightSource::LightId, PVLight *> PVSet;
    PVSet pvs;

    Vec3f eyeFront;

    typedef GLBufferT<Vertex3Tex3Rgba> VBuf;
    VBuf *buffer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uViewUnit;
    GLUniform uPixelAsUv;
    GLUniform uActiveRect;
    GLUniform uAtlas;
    GLUniform uDepthBuf;

    Impl(Public *i)
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

    ~Impl()
    {
        DE_ASSERT(res == 0); // should have been deinited
        releaseRef(res);
        clearPvs();
    }

    void glInit()
    {
        // Acquire a reference to the shared flare data.
        res = SharedFlareData::hold();

        buffer = new VBuf;
        drawable.addBuffer(buffer);
        self().shaders().build(drawable.program(), "fx.lensflares")
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
        pvs.deleteAll();
        pvs.clear();
    }

    void addToPvs(const IPointLightSource *light)
    {
        PVSet::iterator found = pvs.find(light->lightSourceId());
        if (found == pvs.end())
        {
            found = pvs.insert(light->lightSourceId(), new PVLight);
        }

        PVLight *pvl = found->second;
        pvl->light = light;
        pvl->seenFrame = R_FrameCount();
    }

    void makeFlare(VBuf::Vertices &   verts,
                   VBuf::Indices &    idx,
                   FlareData::FlareId id,
                   float              axisPos,
                   float              radius,
                   Vec4f           color,
                   const PVLight *    pvl)
    {
        const Rectanglef uvRect = res->uvRect(id);
        const int firstIdx = verts.size();

        VBuf::Type vtx;
        vtx.pos  = pvl->light->lightSourceOrigin().xzy();
        vtx.rgba = Vec4f(pvl->light->lightSourceColorf(), 1.f) * color;
        vtx.texCoord[2] = Vec2f(axisPos, 0);

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
        const int thisFrame = R_FrameCount();

        // The vertex buffer will contain a number of quads.
        VBuf::Vertices verts;
        VBuf::Indices idx;
//        VBuf::Type vtx;

        for (PVSet::const_iterator i = pvs.begin(); i != pvs.end(); ++i)
        {
            const PVLight *pvl = i->second;

            // Skip lights that are not visible right now.
            /// @todo If so, it might be time to purge it from the PVS.
            if (pvl->seenFrame != thisFrame) continue;

            const coord_t distanceSquared = (Rend_EyeOrigin() - pvl->light->lightSourceOrigin().xzy()).lengthSquared();
            const coord_t distance = std::sqrt(distanceSquared);

            // Light intensity is always quadratic per distance.
            float intensity = pvl->light->lightSourceIntensity(Rend_EyeOrigin()) / distanceSquared;

            // Projected radius of the light.
            const float RADIUS_FACTOR = 128; // Light radius of 1 at this distance produces a visible radius of 1.
            /// @todo The factor should be FOV-dependent.
            float radius = pvl->light->lightSourceRadius() / distance * RADIUS_FACTOR;

            const float dot = (pvl->light->lightSourceOrigin().xzy() - Rend_EyeOrigin()).normalize().dot(eyeFront);
            const float angle = radianToDegree(std::acos(dot));

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
                Vec4f color;
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
                {   1,      FlareData::Burst,    Vec4f(1),          1,      Rgf(1.0e-8f, 1.0e-6f), Rgf(),  Rgf(), Rgf(.5f, .8f),   Rgf(), Rgf() },
                {   1,      FlareData::Star,     Vec4f(1),          1,      Rgf(1.0e-6f, 1.0e-5f), Rgf(),  Rgf(.5f, .7f), Rgf(),   Rgf(), Rgf() },
                {   1,      FlareData::Exponent, Vec4f(1),          2.5f,   Rgf(1.0e-6f, 1.0e-5f), Rgf(),  Rgf(.1f, .2f), Rgf(),   Rgf(), Rgf() },

                {  .8f,     FlareData::Halo,     Vec4f(1, 1, 1, .5f),        1,      Rgf(5.0e-6f, 5.0e-5f), Rgf(),  Rgf(.5f, .7f), Rgf(),   Rgf(), Rgf(30, 60) },

                {  -.8f,    FlareData::Ring,     Vec4f(.4f, 1, .4f, .26f),   .4f,    Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(5, 20), Rgf(40, 50) },
                {  -1,      FlareData::Circle,   Vec4f(.4f, .4f, 1, .30f),   .5f,    Rgf(4.0e-6f, 4.0e-5f), Rgf(),  Rgf(.08f, .45f), Rgf(), Rgf(0, 23), Rgf(30, 60) },
                {  -1.2f ,  FlareData::Ring,     Vec4f(1, .4f, .4f, .26f),   .56f,   Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(35, 50) },

                {  1.333f,  FlareData::Ring,     Vec4f(.5f, .5f, 1, .1f),    1.2f,   Rgf(1.0e-8f, 1.0e-7f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(25, 45) },
                {  1.45f,   FlareData::Ring,     Vec4f(1, .5f, .5f, .15f),   1.15f,  Rgf(1.0e-8f, 1.0e-7f), Rgf(),  Rgf(.1f, .5f), Rgf(),   Rgf(10, 25), Rgf(25, 45) },

                {  -1.45f,  FlareData::Ring,     Vec4f(1, 1, .9f, .25f),     .2f,    Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .4f), Rgf(),   Rgf(5, 10), Rgf(15, 30) },
                {  -.2f,    FlareData::Circle,   Vec4f(1, 1, .9f, .2f),      .23f,   Rgf(1.0e-5f, 1.0e-4f), Rgf(),  Rgf(.1f, .4f), Rgf(),   Rgf(5, 10), Rgf(15, 30) },
            };

            for (uint i = 0; i < sizeof(specs)/sizeof(specs[0]); ++i)
            {
                const Spec &spec = specs[i];

                float size = radius * spec.size;
                Vec4f color = spec.color;

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
            const Vec3f eyeToFlare = pvl->lightSourceOrigin() - eyePos;

            // Calculate the 'mirror' vector.
            const float scale = viewToCenter.dot(viewData->frontVec)
                                / Vec3f(viewData->frontVec).dot(viewData->frontVec);
            const Vec3f mirror =
                (Vec3f(viewData->frontVec) * scale - viewToCenter) * 2;
            */
        }

        buffer->setVertices(verts, gfx::Dynamic);
        buffer->setIndices(gfx::Triangles, idx, gfx::Dynamic);
    }
};

LensFlares::LensFlares(int console) : ConsoleEffect(console), d(new Impl(this))
{}

void LensFlares::clearLights()
{
    d->clearPvs();
}

void LensFlares::markLightPotentiallyVisibleForCurrentFrame(const IPointLightSource *lightSource)
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
    if (!ClientApp::world().hasMap())
    {
        // Flares are not visbile unless a map is loaded.
        return;
    }

    if (!viewPlayer) return; /// @todo How'd we get here? -ds

    const viewdata_t *viewData = &DD_Player(console())->viewport();
    d->eyeFront = Vec3f(viewData->frontVec);

    const Rectanglef rect = viewRect();
    const float aspect = rect.height() / rect.width();

    GLWindow &window = ClientWindow::main();

    d->uViewUnit  = Vec2f(aspect, 1.f);
    d->uPixelAsUv = Vec2f(1.f / window.pixelWidth(), 1.f / window.pixelHeight());
    d->uMvpMatrix = Viewer_Matrix(); //Rend_GetProjectionMatrix() * Rend_GetModelViewMatrix(console());

    DE_ASSERT(console() == displayPlayer);
    //DE_ASSERT(viewPlayer - ddPlayers == displayPlayer);
    if (DoomsdayApp::players().indexOf(viewPlayer) != displayPlayer)
    {
        debug("[LensFrames::draw] viewPlayer != displayPlayer");
        return;
    }

    // Depth information is required for occlusion.
    GLFramebuffer &target = GLState::current().target();
    GLTexture *depthTex = target.attachedTexture(GLFramebuffer::Depth);
    /**
     * @todo Handle the situation when depth information is not available in the target.
     */
    d->uDepthBuf = depthTex;

    // The active rectangle is specified with top/left coordinates, but the shader
    // works with bottom/left ones.
    Vec4f active(target.activeRectScale(), target.activeRectNormalizedOffset());
    active.w = 1 - (active.w + active.y); // flip y
    d->uActiveRect = active;

    GLState::push()
            .setCull(gfx::None)
            .setDepthTest(false)
            .setDepthWrite(false)
            .setBlend(true)
            .setBlendFunc(gfx::SrcAlpha, gfx::One);

    d->drawable.draw();

    GLState::pop();
}

void LensFlares::consoleRegister()
{
#ifdef FX_TEST_LIGHT
    C_CMD("testlight", "sf*", TestLight)
#endif
}

} // namespace fx

DE_SHARED_INSTANCE(fx::FlareData)
