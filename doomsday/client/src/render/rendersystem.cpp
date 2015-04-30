/** @file rendersystem.cpp  Renderer subsystem.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/ScriptedInfo>
#include "clientapp.h"
#include "render/rend_main.h"
#include "render/rend_halo.h"
#include "render/angleclipper.h"
#include "render/modelrenderer.h"
#include "render/skydrawable.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "ConvexSubspace"
#include "SectorCluster"
#include "Surface"

#include "Contact"
#include "r_util.h"

using namespace de;

Store::Store() : posCoords(0), colorCoords(0), vertCount(0), vertMax(0)
{
    zap(texCoords);
}

Store::~Store()
{
    clear();
}

void Store::rewind()
{
    vertCount = 0;
}

void Store::clear()
{
    vertCount = vertMax = 0;

    M_Free(posCoords); posCoords = 0;
    M_Free(colorCoords); colorCoords = 0;

    for(dint i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
    {
        M_Free(texCoords[i]); texCoords[i] = 0;
    }
}

duint Store::allocateVertices(duint count)
{
    duint const base = vertCount;

    // Do we need to allocate more memory?
    vertCount += count;
    while(vertCount > vertMax)
    {
        if(vertMax == 0)
        {
            vertMax = 16;
        }
        else
        {
            vertMax *= 2;
        }

        posCoords = (Vector4f *) M_Realloc(posCoords, sizeof(*posCoords) * vertMax);
        colorCoords = (Vector4ub *) M_Realloc(colorCoords, sizeof(*colorCoords) * vertMax);
        for(dint i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
        {
            texCoords[i] = (Vector2f *) M_Realloc(texCoords[i], sizeof(Vector2f) * vertMax);
        }
    }

    return base;
}

ProjectionList::Node* ProjectionList::firstNode = nullptr;
ProjectionList::Node* ProjectionList::cursorNode = nullptr;

void ProjectionList::init()  // static
{
    static bool firstTime = true;
    if(firstTime)
    {
        firstNode  = 0;
        cursorNode = 0;
        firstTime  = false;
    }
}

void ProjectionList::reset()  // static
{
    // Start reusing nodes.
    cursorNode = firstNode;
}

ProjectionList &ProjectionList::add(ProjectedTextureData &texp)
{
    Node *node = newNode();
    node->projection = texp;

    if(head && sortByLuma)
    {
        dfloat luma = luminosity(node->projection);
        Node *iter = head;
        Node *last = iter;
        do
        {
            // Is this brighter than that being added?
            if(luminosity(iter->projection) > luma)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {
                // Insert it here.
                node->next = last->next;
                last->next = node;
                return *this;
            }
        } while(iter);
    }

    node->next = head;
    head = node;

    return *this;
}

ProjectionList::Node *ProjectionList::newNode()  // static
{
    Node *node;

    // Do we need to allocate mode nodes?
    if(!cursorNode)
    {
        node = (Node *) Z_Malloc(sizeof(*node), PU_APPSTATIC, nullptr);

        // Link the new node to the list.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursorNode;
        cursorNode = cursorNode->nextUsed;
    }

    node->next = nullptr;
    return node;
}

/// Average color * alpha.
dfloat ProjectionList::luminosity(ProjectedTextureData const &texp)  // static
{
    return (texp.color.x + texp.color.y + texp.color.z) / 3 * texp.color.w;
}

DENG2_PIMPL(RenderSystem)
{
    ModelRenderer models;
    SkyDrawable sky;
    SettingsRegister settings;
    SettingsRegister appearanceSettings;
    ImageBank images;

    AngleClipper clipper;

    Store buffer;
    DrawLists drawLists;

    // Texture => world surface projection lists.
    struct ProjectionLists
    {
        duint listCount = 0;
        duint cursorList = 0;
        ProjectionList *lists = nullptr;

        void initForMap(Map & /*map*/)
        {
            ProjectionList::init();

            // All memory for the lists is allocated from Zone so we can "forget" it.
            lists = nullptr;
            listCount = 0;
            cursorList = 0;
        }

        void reset()
        {
            ProjectionList::reset();

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
            throw Error("ProjectionLists::findList", "Invalid index #" + String::number(listIdx));
        }

        ProjectionList &findOrCreateList(duint *listIdx, bool sortByLuma)
        {
            DENG2_ASSERT(listIdx);

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

    Instance(Public *i) : Base(i)
    {
        LOG_AS("RenderSystem");

        // Load the required packages.
        App::packageLoader().load("net.dengine.client.renderer");
        App::packageLoader().load("net.dengine.client.renderer.lensflares");

        // -=- DEVEL -=-
        //App::packageLoader().load("net.dengine.client.testmodel");

        /*Package::Asset asset = App::asset("model.thing.possessed");
        qDebug() << asset.accessedRecord().asText();
        qDebug() << asset.getPath("path");*/

        loadAllShaders();
        loadImages();

        typedef SettingsRegister SReg;

        // Initialize settings.
        settings.define(SReg::FloatCVar,      "rend-camera-fov", 95.f)
                .define(SReg::ConfigVariable, "render.pixelDensity")
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

    /**
     * Reads all shader definitions and sets up a Bank where the actual
     * compiled shaders are stored once they're needed.
     *
     * @todo This should be reworked to support unloading packages, and
     * loading of new shaders from any newly loaded packages. -jk
     */
    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        App::findInPackages("shaders.dei", found);
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

void RenderSystem::glInit()
{
    d->models.glInit();
}

void RenderSystem::glDeinit()
{
    d->models.glDeinit();
}

GLShaderBank &RenderSystem::shaders()
{
    return BaseGuiApp::shaders();
}

ImageBank &RenderSystem::images()
{
    return d->images;
}

ModelRenderer &RenderSystem::modelRenderer()
{
    return d->models;
}

SkyDrawable &RenderSystem::sky()
{
    return d->sky;
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

void RenderSystem::resetDrawLists()
{
    d->drawLists.reset();

    // Start reallocating storage from the global vertex buffer, also.
    d->buffer.rewind();
}

DrawLists &RenderSystem::drawLists()
{
    return d->drawLists;
}

void RenderSystem::projectorInitForMap(Map &map)
{
    d->projector.initForMap(map);
}

void RenderSystem::projectorReset()
{
    d->projector.reset();
}

ProjectionList &RenderSystem::findSurfaceProjectionList(duint *listIdx, bool sortByLuma)
{
    return d->projector.findOrCreateList(listIdx, sortByLuma);
}

LoopResult RenderSystem::forAllSurfaceProjections(duint listIdx, std::function<LoopResult (ProjectedTextureData const &)> func) const
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

void RenderSystem::consoleRegister()
{
    Viewports_Register();
    Rend_Register();
    H_Register();
}
