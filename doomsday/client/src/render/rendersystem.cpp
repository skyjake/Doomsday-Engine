/** @file rendersystem.cpp  Renderer subsystem.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "clientapp.h"
#include "render/rendersystem.h"
#include "DrawLists"
#include "render/rend_main.h"
#include "render/rend_halo.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
//#include <de/memory.h>
#include <doomsday/console/var.h>

using namespace de;

static byte printVBufPoolDevInfo; // cvar

void WorldVBuf::setVertices(Index vertCount, Index const *indices,
    Vector3f const *posCoords, Vector4f const *colorCoords,
    Vector2f const *texCoords, Vector2f const *interTexCoords,
    Vector2f const *modTexCoords)
{
    for(int i = 0; i < vertCount; ++i)
    {
        Type &vertex = (*this)[indices[i]];

        vertex.pos = posCoords[i];

        // Sky masked polys need nothing more.
        //if(d->spec.group == SkyMaskGeom) continue;

        // Primary texture coordinates.
        //if(d->spec.unit(TU_PRIMARY).hasTexture())
        if(texCoords)
        {
            DENG2_ASSERT(texCoords != 0);
            vertex.texCoord[TCA_MAIN] = texCoords[i];
        }

        // Secondary texture coordinates.
        //if(d->spec.unit(TU_INTER).hasTexture())
        if(interTexCoords)
        {
            DENG2_ASSERT(interTexCoords != 0);
            vertex.texCoord[TCA_BLEND] = interTexCoords[i];
        }

        // First light texture coordinates.
        //if((elem->data.flags & Instance::Element::Data::HasLights) && IS_MTEX_LIGHTS)
        if(modTexCoords)
        {
            DENG2_ASSERT(modTexCoords != 0);
            vertex.texCoord[TCA_LIGHT] = modTexCoords[i];
        }

        // Color.
        if(colorCoords)
        {
            Vector4f const &srcColor = colorCoords[i];

            // We should not be relying on clamping at this late stage...
            DENG2_ASSERT(INRANGE_OF(srcColor.x, 0.f, 1.f));
            DENG2_ASSERT(INRANGE_OF(srcColor.y, 0.f, 1.f));
            DENG2_ASSERT(INRANGE_OF(srcColor.z, 0.f, 1.f));
            DENG2_ASSERT(INRANGE_OF(srcColor.w, 0.f, 1.f));

            vertex.rgba = srcColor.max(Vector4f(0, 0, 0, 0)).min(Vector4f(1, 1, 1, 1));
        }
        else
        {
            vertex.rgba = Vector4f(1, 1, 1, 1);
        }
    }
}

DENG2_PIMPL(RenderSystem)
{
    SettingsRegister settings;
    SettingsRegister appearanceSettings;
    ImageBank images;
    WorldVBuf vbuf;
    IndicePool indicePool;
    PosCoordPool posPool;
    ColorCoordPool colorPool;
    TexCoordPool texPool;
    DrawLists drawLists;

    Instance(Public *i) : Base(i)
    {
        LOG_AS("RenderSystem");

        loadAllShaders();
        loadImages();

        typedef SettingsRegister SReg;

        // Initialize settings.
        settings.define(SReg::FloatCVar, "rend-camera-fov", 95.f)
                .define(SReg::IntCVar,   "rend-model-mirror-hud", 0)
                .define(SReg::IntCVar,   "rend-model-precache", 1)
                .define(SReg::IntCVar,   "rend-sprite-precache", 1)
                .define(SReg::IntCVar,   "rend-light-multitex", 1)
                .define(SReg::IntCVar,   "rend-model-shiny-multitex", 1)
                .define(SReg::IntCVar,   "rend-tex-detail-multitex", 1)
                .define(SReg::IntCVar,   "rend-tex", 1)
                .define(SReg::IntCVar,   "rend-dev-wireframe", 0)
                .define(SReg::IntCVar,   "rend-dev-thinker-ids", 0)
                .define(SReg::IntCVar,   "rend-dev-mobj-bbox", 0)
                .define(SReg::IntCVar,   "rend-dev-polyobj-bbox", 0)
                .define(SReg::IntCVar,   "rend-dev-sector-show-indices", 0)
                .define(SReg::IntCVar,   "rend-dev-vertex-show-indices", 0)
                .define(SReg::IntCVar,   "rend-dev-generator-show-indices", 0);

        appearanceSettings.setPersistentName("renderer");
        appearanceSettings
                .define(SReg::IntCVar,   "rend-light", 1)
                .define(SReg::IntCVar,   "rend-light-decor", 1)
                .define(SReg::IntCVar,   "rend-light-blend", 0)
                .define(SReg::IntCVar,   "rend-light-num", 0)
                .define(SReg::FloatCVar, "rend-light-bright", .5f)
                .define(SReg::FloatCVar, "rend-light-fog-bright", .15f)
                .define(SReg::FloatCVar, "rend-light-radius-scale", 4.24f)
                .define(SReg::IntCVar,   "rend-light-radius-max", 256)
                .define(SReg::IntCVar,   "rend-light-ambient", 0)
                .define(SReg::FloatCVar, "rend-light-compression", 0)
                .define(SReg::IntCVar,   "rend-light-attenuation", 924)
                .define(SReg::IntCVar,   "rend-light-sky-auto", 1)
                .define(SReg::FloatCVar, "rend-light-sky", .273f)
                .define(SReg::IntCVar,   "rend-light-wall-angle-smooth", 1)
                .define(SReg::FloatCVar, "rend-light-wall-angle", 1.2f)

                .define(SReg::IntCVar,   "rend-vignette", 1)
                .define(SReg::FloatCVar, "rend-vignette-darkness", 1)
                .define(SReg::FloatCVar, "rend-vignette-width", 1)

                .define(SReg::IntCVar,   "rend-halo-realistic", 1)
                .define(SReg::IntCVar,   "rend-halo", 5)
                .define(SReg::IntCVar,   "rend-halo-bright", 45)
                .define(SReg::IntCVar,   "rend-halo-size", 80)
                .define(SReg::IntCVar,   "rend-halo-occlusion", 48)
                .define(SReg::FloatCVar, "rend-halo-radius-min", 20)
                .define(SReg::FloatCVar, "rend-halo-secondary-limit", 1)
                .define(SReg::FloatCVar, "rend-halo-dim-near", 10)
                .define(SReg::FloatCVar, "rend-halo-dim-far", 100)
                .define(SReg::FloatCVar, "rend-halo-zmag-div", 62)

                .define(SReg::FloatCVar, "rend-glow", .8f)
                .define(SReg::IntCVar,   "rend-glow-height", 100)
                .define(SReg::FloatCVar, "rend-glow-scale", 3)
                .define(SReg::IntCVar,   "rend-glow-wall", 1)

                .define(SReg::IntCVar,   "rend-bloom", 1)
                .define(SReg::FloatCVar, "rend-bloom-intensity", .65f)
                .define(SReg::FloatCVar, "rend-bloom-threshold", .35f)
                .define(SReg::FloatCVar, "rend-bloom-dispersion", 1)

                .define(SReg::IntCVar,   "rend-fakeradio", 1)
                .define(SReg::FloatCVar, "rend-fakeradio-darkness", 1.2f)
                .define(SReg::IntCVar,   "rend-shadow", 1)
                .define(SReg::FloatCVar, "rend-shadow-darkness", 1.2f)
                .define(SReg::IntCVar,   "rend-shadow-far", 1000)
                .define(SReg::IntCVar,   "rend-shadow-radius-max", 80)

                .define(SReg::IntCVar,   "rend-tex-shiny", 1)
                .define(SReg::IntCVar,   "rend-tex-mipmap", 5)
                .define(SReg::IntCVar,   "rend-tex-quality", TEXQ_BEST)
                .define(SReg::IntCVar,   "rend-tex-anim-smooth", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-smart", 0)
                .define(SReg::IntCVar,   "rend-tex-filter-sprite", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-mag", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-ui", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-anisotropic", -1)
                .define(SReg::IntCVar,   "rend-tex-detail", 1)
                .define(SReg::FloatCVar, "rend-tex-detail-scale", 4)
                .define(SReg::FloatCVar, "rend-tex-detail-strength", .5f)

                .define(SReg::IntCVar,   "rend-mobj-smooth-move", 2)
                .define(SReg::IntCVar,   "rend-mobj-smooth-turn", 1)

                .define(SReg::IntCVar,   "rend-model", 1)
                .define(SReg::IntCVar,   "rend-model-inter", 1)
                .define(SReg::IntCVar,   "rend-model-distance", 1500)
                .define(SReg::FloatCVar, "rend-model-lod", 256)
                .define(SReg::FloatCVar, "rend-model-lights", 4)

                .define(SReg::IntCVar,   "rend-sprite-mode", 0)
                .define(SReg::IntCVar,   "rend-sprite-blend", 1)
                .define(SReg::IntCVar,   "rend-sprite-lights", 4)
                .define(SReg::IntCVar,   "rend-sprite-align", 0)
                .define(SReg::IntCVar,   "rend-sprite-noz", 0)

                .define(SReg::IntCVar,   "rend-particle", 1)
                .define(SReg::IntCVar,   "rend-particle-max", 0)
                .define(SReg::FloatCVar, "rend-particle-rate", 1)
                .define(SReg::FloatCVar, "rend-particle-diffuse", 4)
                .define(SReg::IntCVar,   "rend-particle-visible-near", 0)

                .define(SReg::FloatCVar, "rend-sky-distance", 1600);
    }

    /**
     * Reads all shader definitions and sets up a Bank where the actual
     * compiled shaders are stored once they're needed.
     */
    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        App::fileSystem().findAll("shaders.dei", found);
        DENG2_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            ClientApp::shaders().addFromInfo(**i);
        }
    }

    /**
     * Reads the renderer's image definitions and sets up a Bank for caching them
     * when they're needed.
     */
    void loadImages()
    {
        //Folder const &renderPack = App::fileSystem().find<Folder>("renderer.pack");
        //images.addFromInfo(renderPack.locate<File>("images.dei"));
    }
};

RenderSystem::RenderSystem() : d(new Instance(this))
{}

GLShaderBank &RenderSystem::shaders()
{
    return BaseGuiApp::shaders();
}

ImageBank &RenderSystem::images()
{
    return d->images;
}

void RenderSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

SettingsRegister &RenderSystem::settings()
{
    return d->settings;
}

SettingsRegister &RenderSystem::appearanceSettings()
{
    return d->appearanceSettings;
}

WorldVBuf &RenderSystem::buffer()
{
    return d->vbuf;
}

void RenderSystem::clearDrawLists()
{
    d->drawLists.clear();
    d->vbuf.clear(); // Clear the global vertex buffer, also.
}

void RenderSystem::resetDrawLists()
{
    d->drawLists.reset();
    d->vbuf.reset(); // Start reallocating storage from the global vertex buffer, also.
}

DrawLists &RenderSystem::drawLists()
{
    return d->drawLists;
}

void RenderSystem::printVBufPoolInfo()
{
    if(!printVBufPoolDevInfo) return;

    d->indicePool.devPrint();
    d->posPool.devPrint();
    d->colorPool.devPrint();
    d->texPool.devPrint();
}

void RenderSystem::resetVBufPools()
{
    d->indicePool.reset();
    d->posPool.reset();
    d->colorPool.reset();
    d->texPool.reset();
}

RenderSystem::IndicePool &RenderSystem::indicePool() const
{
    return d->indicePool;
}

RenderSystem::PosCoordPool &RenderSystem::posPool() const
{
    return d->posPool;
}

RenderSystem::ColorCoordPool &RenderSystem::colorPool() const
{
    return d->colorPool;
}

RenderSystem::TexCoordPool &RenderSystem::texPool() const
{
    return d->texPool;
}

void RenderSystem::consoleRegister()
{
    Viewports_Register();
    Rend_Register();
    H_Register();
    C_VAR_BYTE("rend-info-rendpolys", &printVBufPoolDevInfo, CVF_NO_ARCHIVE, 0, 1);
}
