/** @file textures.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/textures.h"
#include "doomsday/resource/resources.h"
#include "doomsday/res/TextureScheme"

#include <de/mathutil.h>
#include <de/types.h>

using namespace de;

namespace res {

DENG2_PIMPL_NOREF(Textures)
, DENG2_OBSERVES(TextureScheme,   ManifestDefined)
, DENG2_OBSERVES(TextureManifest, TextureDerived)
, DENG2_OBSERVES(Texture,         Deletion)
{
    TextureSchemes textureSchemes;
    QList<TextureScheme *> textureSchemeCreationOrder;

    /// All texture instances in the system (from all schemes).
    AllTextures textures;

    Impl()
    {
        /// @note Order here defines the ambigious-URI search order.
        createTextureScheme("Sprites");
        createTextureScheme("Textures");
        createTextureScheme("Flats");
        createTextureScheme("Patches");
        createTextureScheme("System");
        createTextureScheme("Details");
        createTextureScheme("Reflections");
        createTextureScheme("Masks");
        createTextureScheme("ModelSkins");
        createTextureScheme("ModelReflectionSkins");
        createTextureScheme("Lightmaps");
        createTextureScheme("Flaremaps");
    }

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        clearTextureManifests();
        clearAllTextureSchemes();
    }

    void createTextureScheme(String name)
    {
        DENG2_ASSERT(name.length() >= TextureScheme::min_name_length);

        // Create a new scheme.
        TextureScheme *newScheme = new TextureScheme(name);
        textureSchemes.insert(name.toLower(), newScheme);
        textureSchemeCreationOrder << newScheme;

        // We want notification when a new manifest is defined in this scheme.
        newScheme->audienceForManifestDefined += this;
    }

    void clearAllTextureSchemes()
    {
        foreach (TextureScheme *scheme, textureSchemes)
        {
            scheme->clear();
        }
    }

    void clearTextureManifests()
    {
        qDeleteAll(textureSchemes);
        textureSchemes.clear();
        textureSchemeCreationOrder.clear();
    }

    void textureSchemeManifestDefined(TextureScheme & /*scheme*/, TextureManifest &manifest) override
    {
        // We want notification when the manifest is derived to produce a texture.
        manifest.audienceForTextureDerived += this;
    }

    void textureManifestTextureDerived(TextureManifest & /*manifest*/, Texture &texture) override
    {
        // Include this new texture in the scheme-agnostic list of instances.
        textures << &texture;

        // We want notification when the texture is about to be deleted.
        texture.audienceForDeletion += this;
    }

    void textureBeingDeleted(Texture const &texture) override
    {
        textures.removeOne(const_cast<Texture *>(&texture));
    }
};

Textures::Textures()
    : d(new Impl)
{}

void Textures::clear()
{
    d->clear();
}

Textures &Textures::get() // static
{
    return Resources::get().textures();
}

TextureScheme &Textures::textureScheme(String name) const
{
    LOG_AS("ResourceSystem::textureScheme");
    if (!name.isEmpty())
    {
        TextureSchemes::iterator found = d->textureSchemes.find(name.toLower());
        if (found != d->textureSchemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Resources::UnknownSchemeError("ResourceSystem::textureScheme", "No scheme found matching '" + name + "'");
}

bool Textures::isKnownTextureScheme(String name) const
{
    if (!name.isEmpty())
    {
        return d->textureSchemes.contains(name.toLower());
    }
    return false;
}

Textures::TextureSchemes const& Textures::allTextureSchemes() const
{
    return d->textureSchemes;
}

void Textures::clearAllTextureSchemes()
{
    d->clearAllTextureSchemes();
}

bool Textures::hasTextureManifest(de::Uri const &path) const
{
    try
    {
        textureManifest(path);
        return true;
    }
    catch (Resources::MissingResourceManifestError const &)
    {}  // Ignore this error.
    return false;
}

TextureManifest &Textures::textureManifest(de::Uri const &uri) const
{
    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if (!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        dint uIdPos = pathStr.indexOf(':');
        if (uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            dint uniqueId     = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return textureScheme(schemeName).findByUniqueId(uniqueId);
            }
            catch (TextureScheme::NotFoundError const &)
            {}  // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if (!uri.scheme().isEmpty())
        {
            try
            {
                return textureScheme(uri.scheme()).find(path);
            }
            catch (TextureScheme::NotFoundError const &)
            {}  // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            for (TextureScheme *scheme : d->textureSchemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch (TextureScheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw MissingResourceManifestError Failed to locate a matching manifest.
    throw Resources::MissingResourceManifestError("ResourceSystem::findTexture", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

Textures::AllTextures const &Textures::allTextures() const
{
    return d->textures;
}

Texture *Textures::texture(String schemeName, de::Uri const &resourceUri)
{
    if (!resourceUri.isEmpty())
    {
        if (!resourceUri.path().toStringRef().compareWithoutCase("-"))
        {
            return nullptr;
        }

        try
        {
            return &textureScheme(schemeName).findByResourceUri(resourceUri).texture();
        }
        catch (TextureManifest::MissingTextureError const &)
        {}  // Ignore this error.
        catch (TextureScheme::NotFoundError const &)
        {}  // Ignore this error.
    }
    return nullptr;
}

Texture *Textures::defineTexture(String schemeName, de::Uri const &resourceUri,
                                 Vector2ui const &dimensions)
{
    LOG_AS("ResourceSystem::defineTexture");

    if (resourceUri.isEmpty()) return nullptr;

    // Have we already created one for this?
    TextureScheme &scheme = textureScheme(schemeName);
    try
    {
        return &scheme.findByResourceUri(resourceUri).texture();
    }
    catch (TextureManifest::MissingTextureError const &)
    {}  // Ignore this error.
    catch (TextureScheme::NotFoundError const &)
    {}  // Ignore this error.

    dint uniqueId = scheme.count() + 1; // 1-based index.
    if (M_NumDigits(uniqueId) > 8)
    {
        LOG_RES_WARNING("Failed declaring texture manifest in scheme %s (max:%i)")
            << schemeName << DDMAXINT;
        return nullptr;
    }

    de::Uri uri(scheme.name(), Path(String("%1").arg(uniqueId, 8, 10, QChar('0'))));
    try
    {
        TextureManifest &manifest = declareTexture(uri, Texture::Custom, dimensions,
                                                   Vector2i(), uniqueId, &resourceUri);

        /// @todo Defer until necessary (manifest texture is first referenced).
        return deriveTexture(manifest);
    }
    catch (TextureScheme::InvalidPathError const &er)
    {
        LOG_RES_WARNING("Failed declaring texture \"%s\": %s") << uri << er.asText();
    }
    return nullptr;
}

Texture *Textures::deriveTexture(TextureManifest &manifest)
{
    LOG_AS("Textures");
    Texture *tex = manifest.derive();
    if (!tex)
    {
        LOGDEV_RES_WARNING("Failed to derive a Texture for \"%s\", ignoring")
                << manifest.composeUri();
    }
    return tex;
}

void Textures::deriveAllTexturesInScheme(String schemeName)
{
    TextureScheme &scheme = textureScheme(schemeName);

    PathTreeIterator<res::TextureScheme::Index> iter(scheme.index().leafNodes());
    while (iter.hasNext())
    {
        res::TextureManifest &manifest = iter.next();
        deriveTexture(manifest);
    }
}

} // namespace res
