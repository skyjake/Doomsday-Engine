/** @file rendersystem.cpp  Renderer subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "render/rendersystem.h"

#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/gluniform.h>
#include <de/packageloader.h>
#include <de/dscript.h>
#include "clientapp.h"
#include "render/environ.h"
#include "render/rend_main.h"
#include "render/rend_halo.h"
#include "render/angleclipper.h"
#include "render/iworldrenderer.h"
#include "render/modelrenderer.h"
#include "render/skydrawable.h"
#include "render/store.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "world/clientworld.h"
#include "world/convexsubspace.h"
#include "world/subsector.h"
#include "world/surface.h"

#include "world/contact.h"
#include "misc/r_util.h"

using namespace de;

DE_PIMPL(RenderSystem)
, DE_OBSERVES(PackageLoader, Load)
, DE_OBSERVES(PackageLoader, Unload)
{
    Binder binder;
    Record renderModule;

    render::Environment environment;
    std::unique_ptr<IWorldRenderer> worldRenderer;
    Time prevWorldUpdateAt = Time::invalidTime();
    ModelRenderer models;
    SkyDrawable sky;

    ConfigProfiles settings;
    ConfigProfiles appearanceSettings;
//    ImageBank images;

    AngleClipper clipper;

    Store buffer;
    DrawLists drawLists;

    GLUniform uMapTime          { "uMapTime",          GLUniform::Float };
    GLUniform uViewMatrix       { "uViewMatrix",       GLUniform::Mat4  };
    GLUniform uProjectionMatrix { "uProjectionMatrix", GLUniform::Mat4  };

    // Texture => world surface projection lists.
    struct ProjectionLists
    {
        duint listCount = 0;
        duint cursorList = 0;
        ProjectionList *lists = nullptr;

        void init()
        {
            ProjectionList::init();

            // All memory for the lists is allocated from Zone so we can "forget" it.
            lists = nullptr;
            listCount = 0;
            cursorList = 0;
        }

        void reset()
        {
            ProjectionList::rewind();  // start reusing list nodes.

            // Clear the lists.
            cursorList = 0;
            if(listCount)
            {
                std::memset(lists, 0, listCount * sizeof *lists);
            }
        }

        ProjectionList *tryFindList(duint listIdx) const
        {
            if(listIdx > 0 && listIdx <= listCount)
            {
                return &lists[listIdx - 1];
            }
            return nullptr;  // not found.
        }

        ProjectionList &findList(duint listIdx) const
        {
            if(ProjectionList *found = tryFindList(listIdx)) return *found;
            /// @throw MissingListError  Invalid index specified.
            throw Error("RenderSystem::projector::findList", "Invalid index #" + String::asText(listIdx));
        }

        ProjectionList &findOrCreateList(duint *listIdx, bool sortByLuma)
        {
            DE_ASSERT(listIdx);

            // Do we need to allocate a list?
            if(!(*listIdx))
            {
                // Do we need to allocate more lists?
                if(++cursorList >= listCount)
                {
                    listCount *= 2;
                    if(!listCount) listCount = 2;

                    lists = (ProjectionList *) Z_Realloc(lists, listCount * sizeof(*lists), PU_MAP);
                }

                ProjectionList *list = &lists[cursorList - 1];
                list->head       = nullptr;
                list->sortByLuma = sortByLuma;

                *listIdx = cursorList;
            }

            return lists[(*listIdx) - 1];  // 1-based index.
        }
    } projector;

    /// VectorLight => object affection lists.
    struct VectorLights
    {
        duint listCount = 0;
        duint cursorList = 0;
        VectorLightList *lists = nullptr;

        void init()
        {
            VectorLightList::init();

            // All memory for the lists is allocated from Zone so we can "forget" it.
            lists = nullptr;
            listCount = 0;
            cursorList = 0;
        }

        void reset()
        {
            VectorLightList::rewind();  // start reusing list nodes.

            // Clear the lists.
            cursorList = 0;
            if(listCount)
            {
                std::memset(lists, 0, listCount * sizeof *lists);
            }
        }

        VectorLightList *tryFindList(duint listIdx) const
        {
            if(listIdx > 0 && listIdx <= listCount)
            {
                return &lists[listIdx - 1];
            }
            return nullptr;  // not found.
        }

        VectorLightList &findList(duint listIdx) const
        {
            if(VectorLightList *found = tryFindList(listIdx)) return *found;
            /// @throw MissingListError  Invalid index specified.
            throw Error("RenderSystem::vlights::findList", "Invalid index #" + String::asText(listIdx));
        }

        VectorLightList &findOrCreateList(duint *listIdx)
        {
            DE_ASSERT(listIdx);

            // Do we need to allocate a list?
            if(!(*listIdx))
            {
                // Do we need to allocate more lists?
                if(++cursorList >= listCount)
                {
                    listCount *= 2;
                    if(!listCount) listCount = 2;

                    lists = (VectorLightList *) Z_Realloc(lists, listCount * sizeof(*lists), PU_MAP);
                }

                VectorLightList *list = &lists[cursorList - 1];
                list->head = nullptr;

                *listIdx = cursorList;
            }

            return lists[(*listIdx) - 1];  // 1-based index.
        }
    } vlights;

    Impl(Public *i) : Base(i)
    {
        LOG_AS("RenderSystem");

        auto &pkgLoader = App::packageLoader();

        ModelRenderer::initBindings(binder, renderModule);
        ClientApp::scriptSystem().addNativeModule("Render", renderModule);                

        // General-purpose libgui shaders.
        loadShaders(FS::locate<const File>("/packs/net.dengine.stdlib.gui/shaders.dei"));

        // Packages are checked for shaders when (un)loaded.
        pkgLoader.audienceForLoad() += this;
        pkgLoader.audienceForUnload() += this;

        // Load the required packages.
        pkgLoader.load("net.dengine.client.renderer");
//        pkgLoader.load("net.dengine.client.renderer.lensflares");
        pkgLoader.load("net.dengine.gloom");

        loadImages();

        // Create a world renderer.
        worldRenderer.reset(ClientApp::app().makeWorldRenderer());

        typedef ConfigProfiles SReg;

        // Initialize settings.
        settings.define(SReg::FloatCVar, "rend-camera-fov", 95.f)
                .define(SReg::ConfigVariable, "render.pixelDensity")
                .define(SReg::IntCVar,   "rend-model-mirror-hud", 0)
                .define(SReg::IntCVar,   "rend-model-precache", 1)
                .define(SReg::IntCVar,   "rend-sprite-precache", 1)
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
                .define(SReg::FloatCVar, "rend-light-bright", .7f)
                .define(SReg::FloatCVar, "rend-light-fog-bright", .15f)
                .define(SReg::FloatCVar, "rend-light-radius-scale", 5.2f)
                .define(SReg::IntCVar,   "rend-light-radius-max", 320)
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

                .define(SReg::ConfigVariable, "render.fx.resize.factor")

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

    void packageLoaded(const String &packageId)
    {
        FS::FoundFiles found;
        App::packageLoader().package(packageId).findPartialPath("shaders.dei", found);
        DE_FOR_EACH(FS::FoundFiles, i, found)
        {
            // Load new shaders.
            loadShaders(**i);
        }
    }

    void aboutToUnloadPackage(const String &packageId)
    {
        ClientApp::shaders().removeAllFromPackage(packageId);
    }

    /*
     * Reads all shader definitions and sets up a Bank where the actual
     * compiled shaders are stored once they're needed.
     *
     * @todo This should be reworked to support unloading packages, and
     * loading of new shaders from any newly loaded packages. -jk
     */
//    void loadAllShaders()
//    {
//        // Load all the shader program definitions.
//        FS::FoundFiles found;
//        App::findInPackages("shaders.dei", found);
//        DE_FOR_EACH(FS::FoundFiles, i, found)
//        {
//            loadShaders(**i);
//        }
//    }

    void loadShaders(const File &defs)
    {
        LOG_MSG("Loading shader definitions from %s") << defs.description();
        ClientApp::shaders().addFromInfo(defs);
    }

    /**
     * Reads the renderer's image definitions and sets up a Bank for caching them
     * when they're needed.
     */
    void loadImages()
    {
        //const Folder &renderPack = App::fileSystem().find<Folder>("renderer.pack");
        //images.addFromInfo(renderPack.locate<File>("images.dei"));
    }
};

RenderSystem::RenderSystem() : d(new Impl(this))
{}

void RenderSystem::glInit()
{
    // Shader defines.
    {
        DictionaryValue defines;
        defines.add(new TextValue("DGL_BATCH_MAX"),
                    new NumberValue(DGL_BatchMaxSize()));
        shaders().setPreprocessorDefines(defines);
    }

    d->models.glInit();
    d->worldRenderer->glInit();
}

void RenderSystem::glDeinit()
{
    d->worldRenderer->glDeinit();
    d->models.glDeinit();
    d->environment.glDeinit();
}

IWorldRenderer &RenderSystem::world()
{
    return *d->worldRenderer;
}

GLShaderBank &RenderSystem::shaders()
{
    return BaseGuiApp::shaders();
}

//ImageBank &RenderSystem::images()
//{
//    return d->images;
//}

const GLUniform &RenderSystem::uMapTime() const
{
    return d->uMapTime;
}

GLUniform &RenderSystem::uProjectionMatrix() const
{
    return d->uProjectionMatrix;
}

GLUniform &RenderSystem::uViewMatrix() const
{
    return d->uViewMatrix;
}

render::Environment &RenderSystem::environment()
{
    return d->environment;
}

ModelRenderer &RenderSystem::modelRenderer()
{
    return d->models;
}

SkyDrawable &RenderSystem::sky()
{
    return d->sky;
}

void RenderSystem::timeChanged(const Clock &)
{
    // Update the current map time for shaders.
    d->uMapTime = ClientApp::world().time();

    // Update the world rendering time.
    TimeSpan elapsed = 0.0;
    const Time now = Time::currentHighPerformanceTime();
    if (d->prevWorldUpdateAt.isValid())
    {
        elapsed = now - d->prevWorldUpdateAt;
    }
    d->prevWorldUpdateAt = now;

    if (d->worldRenderer)
    {
        d->worldRenderer->advanceTime(elapsed);
    }
}

ConfigProfiles &RenderSystem::settings()
{
    return d->settings;
}

ConfigProfiles &RenderSystem::appearanceSettings()
{
    return d->appearanceSettings;
}

AngleClipper &RenderSystem::angleClipper() const
{
    return d->clipper;
}

Store &RenderSystem::buffer()
{
    return d->buffer;
}

void RenderSystem::clearDrawLists()
{
    d->drawLists.clear();

    // Clear the global vertex buffer, also.
    d->buffer.clear();
}

DrawLists &RenderSystem::drawLists()
{
    return d->drawLists;
}

void RenderSystem::worldSystemMapChanged(world::Map &)
{
    d->projector.init();
    d->vlights.init();
}

void RenderSystem::beginFrame()
{
    // Clear the draw lists ready for new geometry.
    d->drawLists.reset();
    d->buffer.rewind();  // Start reallocating storage from the global vertex buffer.

    // Clear the clipper - we're drawing from a new point of view.
    d->clipper.clearRanges();

    // Recycle view dependent list data.
    d->projector.reset();
    d->vlights.reset();

    R_BeginFrame();
}

ProjectionList &RenderSystem::findSurfaceProjectionList(duint *listIdx, bool sortByLuma)
{
    return d->projector.findOrCreateList(listIdx, sortByLuma);
}

LoopResult RenderSystem::forAllSurfaceProjections(duint listIdx, std::function<LoopResult (const ProjectedTextureData &)> func) const
{
    if(ProjectionList *list = d->projector.tryFindList(listIdx))
    {
        for(ProjectionList::Node *node = list->head; node; node = node->next)
        {
            if(auto result = func(node->projection))
                return result;
        }
    }
    return LoopContinue;
}

VectorLightList &RenderSystem::findVectorLightList(duint *listIdx)
{
    return d->vlights.findOrCreateList(listIdx);
}

LoopResult RenderSystem::forAllVectorLights(duint listIdx, std::function<LoopResult (const VectorLightData &)> func)
{
    if(VectorLightList *list = d->vlights.tryFindList(listIdx))
    {
        for(VectorLightList::Node *node = list->head; node; node = node->next)
        {
            if(auto result = func(node->vlight))
                return result;
        }
    }
    return LoopContinue;
}

void RenderSystem::consoleRegister()
{
    Viewports_Register();
    Rend_Register();
    H_Register();
}
