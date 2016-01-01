/** @file environ.cpp  Environment rendering.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "world/bspleaf.h"
#include "world/convexsubspace.h"
#include "clientapp.h"

#include <de/ImageFile>
#include <de/filesys/AssetObserver>
#include <doomsday/world/world.h>

using namespace de;

static String const DEF_PATH         ("path");
static String const DEF_INTERIOR_PATH("interior.path");
static String const DEF_EXTERIOR_PATH("exterior.path");

namespace render {

DENG2_PIMPL(Environment)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(World, MapChange)
{
    enum { Interior, Exterior };

    filesys::AssetObserver observer { "texture\\.reflect\\..*" };
    LoopCallback mainCall;

    struct EnvMaps
    {
        Path interior;
        Path exterior;
    };
    QHash<String, EnvMaps> maps;

    /// Currently loaded reflection textures.
    GLTexture reflectionTextures[2];

    Instance(Public *i) : Base(i)
    {
        observer.audienceForAvailability() += this;
        World::get().audienceForMapChange() += this;

        // Reflection cube maps use mipmapping for blurred reflections.
        for(auto &tex : reflectionTextures)
        {
            tex.setMinFilter(gl::Linear, gl::MipLinear);
            tex.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        }
    }

    ~Instance()
    {
        World::get().audienceForMapChange() -= this;

        release();
    }

    void assetAvailabilityChanged(String const &identifier,
                                  filesys::AssetObserver::Event event)
    {
        LOG_RES_MSG("Texture asset \"%s\" is now %s")
                << identifier
                << (event == filesys::AssetObserver::Added? "available" :
                                                            "unavailable");
        // Register available reflection maps.
        String const mapId = identifier.substr(16).toLower();
        switch(event)
        {
        case filesys::AssetObserver::Added:
            addMapsFromAsset(mapId, App::asset(identifier));
            break;

        case filesys::AssetObserver::Removed:
            maps.remove(mapId);
            break;
        }
    }

    void addMapsFromAsset(String const &mapId, Package::Asset const &asset)
    {
        EnvMaps env;

        if(asset.has(DEF_PATH))
        {
            env.interior = env.exterior = asset.absolutePath(DEF_PATH);
        }
        if(asset.has(DEF_INTERIOR_PATH))
        {
            env.interior = asset.absolutePath(DEF_INTERIOR_PATH);
        }
        if(asset.has(DEF_EXTERIOR_PATH))
        {
            env.exterior = asset.absolutePath(DEF_EXTERIOR_PATH);
        }

        // All done.
        maps.insert(mapId, env);
    }

    void release()
    {
        for(auto &tex : reflectionTextures)
        {
            tex.clear();
        }
    }

    void loadCubeMap(GLTexture &tex, String const &path)
    {
        try
        {
            ImageFile const &imgFile = App::rootFolder().locate<ImageFile const>(path);

            LOG_GL_MSG("Loading reflection cube map %s") << imgFile.description();

            Image img = imgFile.image();
            Image::Size const size(img.width() / 6, img.height());
            tex.setImage(gl::NegativeX, img.subImage(Rectanglei(0*size.x, 0, size.x, size.y)));
            tex.setImage(gl::PositiveZ, img.subImage(Rectanglei(1*size.x, 0, size.x, size.y)));
            tex.setImage(gl::PositiveX, img.subImage(Rectanglei(2*size.x, 0, size.x, size.y)));
            tex.setImage(gl::NegativeZ, img.subImage(Rectanglei(3*size.x, 0, size.x, size.y)));
            tex.setImage(gl::NegativeY, img.subImage(Rectanglei(4*size.x, 0, size.x, size.y)));
            tex.setImage(gl::PositiveY, img.subImage(Rectanglei(5*size.x, 0, size.x, size.y)));
            tex.generateMipmap();
        }
        catch(Error const &er)
        {
            LOG_GL_WARNING("Failed to load reflection cube map from path \"%s\": %s")
                    << path << er.asText();
        }
    }

    void worldMapChanged()
    {
        mainCall.enqueue([this] () { loadTexturesForCurrentMap(); });
    }

    void loadTexturesForCurrentMap()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        release();

        String const mapId = ClientApp::world().map().id().toLower();

        // Check which reflection maps are available for the new map.
        auto found = maps.constFind(mapId);
        if(found != maps.constEnd())
        {
            EnvMaps const &env = found.value();
            DENG2_ASSERT(!(env.interior.isEmpty() && env.exterior.isEmpty()));

            if(!env.exterior.isEmpty())
            {
                loadCubeMap(reflectionTextures[Exterior], env.exterior);
            }

            loadCubeMap(reflectionTextures[Interior], env.interior);
        }
    }
};

Environment::Environment()
    : d(new Instance(this))
{}

GLTexture const &Environment::defaultReflection() const
{
    return d->reflectionTextures[Instance::Interior];
}

GLTexture const &Environment::reflectionInBspLeaf(BspLeaf const *bspLeaf) const
{
    if(!bspLeaf || !bspLeaf->hasSubspace())
    {
        return defaultReflection();
    }

    Sector const &sector = bspLeaf->subspace().sector();
    if(sector.hasSkyMaskedPlane())
    {
        return d->reflectionTextures[Instance::Exterior];
    }
    return d->reflectionTextures[Instance::Interior];
}

} // namespace render
