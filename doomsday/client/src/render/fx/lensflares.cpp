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

#include <de/concurrency.h>
#include <de/Drawable>
#include <de/Shared>
#include <de/Log>

#include <QHash>

using namespace de;

namespace fx {

/**
 * Shared GL resources for rendering lens flares.
 */
struct FlareData
{
    AtlasTexture atlas;

    FlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();
    }

    ~FlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        LOG_DEBUG("Releasing shared data");
    }
};

DENG2_PIMPL(LensFlares)
{
    typedef Shared<FlareData> SharedFlareData;
    SharedFlareData *res;

    struct TestLight : public ILightSource
    {
    public:
        LightId lightSourceId() const {
            return 1;
        }
        Origin lightSourceOrigin() const {
            //return Origin(0, 0, 0);
            return Origin(1055, -3280, 30);
        }
        dfloat lightSourceRadius() const {
            return 10;
        }
        Colorf lightSourceColorf() const {
            return Colorf(1, 1, 1);
        }
        dfloat lightSourceIntensity(de::Vector3d const &) const {
            return 1;
        }
    };
    TestLight testLight;

    /**
     * Current state of a potentially visible light.
     */
    struct PVLight
    {
        ILightSource const *light;
        int seenFrame; // R_FrameCount()

        PVLight() : light(0), seenFrame(0)
        {}
    };

    typedef QHash<ILightSource::LightId, PVLight *> PVSet;
    PVSet pvs;

    typedef GLBufferT<Vertex3Tex3Rgba> VBuf;
    VBuf *buffer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uViewUnit;

    Instance(Public *i)
        : Base(i)
        , res(0)
        , buffer(0)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uViewUnit ("uViewUnit",  GLUniform::Vec2)
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
                << uMvpMatrix << uViewUnit;
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

    void addToPvs(ILightSource const *light)
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

            float radius = .1f;
            Rectanglef uvRect;

            int const firstIdx = verts.size();

            vtx.pos  = pvl->light->lightSourceOrigin().xzy();
            vtx.rgba = pvl->light->lightSourceColorf();
            vtx.rgba.w = 1.0f;

            vtx.texCoord[0] = uvRect.topLeft;
            vtx.texCoord[1] = Vector2f(-1, -1) * radius;
            verts << vtx;

            vtx.texCoord[0] = uvRect.topRight();
            vtx.texCoord[1] = Vector2f(1, -1) * radius;
            verts << vtx;

            vtx.texCoord[0] = uvRect.bottomRight;
            vtx.texCoord[1] = Vector2f(1, 1) * radius;
            verts << vtx;

            vtx.texCoord[0] = uvRect.bottomLeft();
            vtx.texCoord[1] = Vector2f(-1, 1) * radius;
            verts << vtx;

            // Make two triangles.
            idx << firstIdx << firstIdx + 2 << firstIdx + 1
                << firstIdx << firstIdx + 2 << firstIdx + 3;
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

void LensFlares::markLightPotentiallyVisibleForCurrentFrame(ILightSource const *lightSource)
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
    markLightPotentiallyVisibleForCurrentFrame(&d->testLight); // testing

    d->makeVerticesForPVS();
}

void LensFlares::draw()
{
    Rectanglef const rect = viewRect();
    float const    aspect = rect.height() / rect.width();

    d->uViewUnit  = Vector2f(aspect, 1.f);
    d->uMvpMatrix = GL_GetProjectionMatrix() * Rend_GetModelViewMatrix(console());

    GLState::push()
            .setCull(gl::None)
            .setDepthTest(false);

    d->drawable.draw();

    GLState::pop().apply();
}

} // namespace fx

DENG2_SHARED_INSTANCE(fx::FlareData)
