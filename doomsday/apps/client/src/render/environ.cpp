/** @file environ.cpp  Environment rendering.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/environ.h"

#include "world/map.h"
#include "world/subsector.h"
#include "clientapp.h"

#include <de/filesystem.h>
#include <de/imagefile.h>
#include <de/filesys/assetobserver.h>
#include <doomsday/world/world.h>

using namespace de;

static const char *ID_DEFAULT        = "default";
static const char *DEF_PATH          = "path";
static const char *DEF_INTERIOR_PATH = "interior.path";
static const char *DEF_EXTERIOR_PATH = "exterior.path";

namespace render {

DE_PIMPL(Environment)
, DE_OBSERVES(filesys::AssetObserver, Availability)
, DE_OBSERVES(world::World, MapChange)
{
    enum { Interior, Exterior };

    filesys::AssetObserver observer { "texture\\.reflect\\..*" };
    Dispatch dispatch;

    struct EnvMaps
    {
        Path interior;
        Path exterior;
    };
    Hash<String, EnvMaps> maps;

    /// Currently loaded reflection textures.
    GLTexture reflectionTextures[2];

    Impl(Public *i) : Base(i)
    {
        observer.audienceForAvailability() += this;
        world::World::get().audienceForMapChange() += this;

        // Reflection cube maps use mipmapping for blurred reflections.
        for (auto &tex : reflectionTextures)
        {
            tex.setMinFilter(gfx::Linear, gfx::MipLinear);
            tex.setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
        }
    }

    ~Impl()
    {
        release();
    }

    void assetAvailabilityChanged(const String &identifier,
                                  filesys::AssetObserver::Event event)
    {
        LOG_RES_MSG("Texture asset \"%s\" is now %s")
            << identifier
            << (event == filesys::AssetObserver::Added ? "available" : "unavailable");

        // Register available reflection maps.
        const String mapId = identifier.substr(BytePos(16)).lower();
        switch (event)
        {
        case filesys::AssetObserver::Added:
            addMapsFromAsset(mapId, App::asset(identifier));
            break;

        case filesys::AssetObserver::Removed:
            maps.remove(mapId);
            break;
        }
    }

    void addMapsFromAsset(const String &mapId, const Package::Asset &asset)
    {
        EnvMaps env;

        if (asset.has(DEF_PATH))
        {
            env.interior = env.exterior = asset.absolutePath(DEF_PATH);
        }
        if (asset.has(DEF_INTERIOR_PATH))
        {
            env.interior = asset.absolutePath(DEF_INTERIOR_PATH);
        }
        if (asset.has(DEF_EXTERIOR_PATH))
        {
            env.exterior = asset.absolutePath(DEF_EXTERIOR_PATH);
        }

        // All done.
        maps.insert(mapId, env);
    }

    void release()
    {
        for (auto &tex : reflectionTextures)
        {
            tex.clear();
        }
    }

    void loadCubeMap(GLTexture &tex, const String &path)
    {
        try
        {
            const ImageFile &imgFile = App::rootFolder().locate<ImageFile const>(path);

            LOG_GL_MSG("Loading reflection cube map %s") << imgFile.description();

            Image img = imgFile.image();
            Image::Size const size(img.width() / 6, img.height());
            tex.setImage(gfx::NegativeX, img.subImage(Rectanglei(0*size.x, 0, size.x, size.y)));
            tex.setImage(gfx::PositiveZ, img.subImage(Rectanglei(1*size.x, 0, size.x, size.y)));
            tex.setImage(gfx::PositiveX, img.subImage(Rectanglei(2*size.x, 0, size.x, size.y)));
            tex.setImage(gfx::NegativeZ, img.subImage(Rectanglei(3*size.x, 0, size.x, size.y)));
            tex.setImage(gfx::NegativeY, img.subImage(Rectanglei(4*size.x, 0, size.x, size.y)));
            tex.setImage(gfx::PositiveY, img.subImage(Rectanglei(5*size.x, 0, size.x, size.y)));
            tex.generateMipmap();
        }
        catch (const Error &er)
        {
            LOG_GL_WARNING("Failed to load reflection cube map from path \"%s\": %s")
                << path << er.asText();
        }
    }

    void worldMapChanged()
    {
        dispatch += [this]() { loadTexturesForCurrentMap(); };
    }

    void loadTexturesForCurrentMap()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        release();

        const String mapId = ClientApp::world().map().id().lower();

        // Check which reflection maps are available for the new map.
        auto found = maps.find(mapId);
        if (found != maps.end())
        {
            loadEnvMaps(found->second);
        }
        else
        {
            // Maybe the default maps, then?
            found = maps.find(ID_DEFAULT);
            if (found != maps.end())
            {
                loadEnvMaps(found->second);
            }
        }
    }

    void loadEnvMaps(const EnvMaps &env)
    {
        DE_ASSERT(!(env.interior.isEmpty() && env.exterior.isEmpty()));
        if (!env.exterior.isEmpty())
        {
            loadCubeMap(reflectionTextures[Exterior], env.exterior);
        }
        loadCubeMap(reflectionTextures[Interior], env.interior);
    }
};

Environment::Environment() : d(new Impl(this))
{}

void Environment::glDeinit()
{
    d->release();
}

const GLTexture &Environment::defaultReflection() const
{
    return d->reflectionTextures[Impl::Interior];
}

const GLTexture &Environment::reflectionInSubsector(const world::Subsector *subsec) const
{
    if (!subsec)
    {
        return defaultReflection();
    }
    if (subsec->as<Subsector>().hasSkyPlane())
    {
        return d->reflectionTextures[Impl::Exterior];
    }
    return d->reflectionTextures[Impl::Interior];
}

}  // namespace render
