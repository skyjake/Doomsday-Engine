/** @file textures.cpp  Texture Resource Collection.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "resource/textures.h"

#include "de_console.h"
#include "dd_main.h" // App_Textures(), verbose
#include <de/Log>
#include <de/Observers>
#include <QList>
#include <QtAlgorithms>

D_CMD(ListTextures);
D_CMD(InspectTexture);
#ifdef DENG_DEBUG
D_CMD(PrintTextureStats);
#endif

namespace de {

DENG2_PIMPL(Textures),
DENG2_OBSERVES(TextureScheme, ManifestDefined),
DENG2_OBSERVES(TextureManifest, TextureDerived),
DENG2_OBSERVES(Texture, Deletion)
{
    /// System subspace schemes containing the textures.
    Schemes schemes;
    QList<TextureScheme *> schemeCreationOrder;

    /// All texture instances in the system (from all schemes).
    All textures;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        self.clearAllSchemes();
        clearManifests();
    }

    void clearManifests()
    {
        qDeleteAll(schemes);
        schemes.clear();
        schemeCreationOrder.clear();
    }

    /// Observes Scheme ManifestDefined.
    void schemeManifestDefined(Scheme &scheme, Manifest &manifest)
    {
        DENG2_UNUSED(scheme);

        // We want notification when the manifest is derived to produce a texture.
        manifest.audienceForTextureDerived += this;
    }

    /// Observes Manifest TextureDerived.
    void manifestTextureDerived(Manifest &manifest, Texture &texture)
    {
        DENG2_UNUSED(manifest);

        // Include this new texture in the scheme-agnostic list of instances.
        textures.push_back(&texture);

        // We want notification when the texture is about to be deleted.
        texture.audienceForDeletion += this;
    }

    /// Observes Texture Deletion.
    void textureBeingDeleted(Texture const &texture)
    {
        textures.removeOne(const_cast<Texture *>(&texture));
    }
};

void Textures::consoleRegister()
{
    C_CMD("inspecttexture", "ss",   InspectTexture)
    C_CMD("inspecttexture", "s",    InspectTexture)
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)

#ifdef DENG_DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif
}

Textures::Textures() : d(new Instance(this))
{}

Textures::Scheme &Textures::scheme(String name) const
{
    LOG_AS("Textures::scheme");
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("Textures::scheme", "No scheme found matching '" + name + "'");
}

TextureScheme &Textures::createScheme(String name)
{
    DENG2_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name);
    d->schemes.insert(name.toLower(), newScheme);
    d->schemeCreationOrder.push_back(newScheme);

    // We want notification when a new manifest is defined in this scheme.
    newScheme->audienceForManifestDefined += d;

    return *newScheme;
}

bool Textures::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->schemes.contains(name.toLower());
    }
    return false;
}

Textures::Schemes const& Textures::allSchemes() const
{
    return d->schemes;
}

bool Textures::has(Uri const &path) const
{
    try
    {
        find(path);
        return true;
    }
    catch(NotFoundError const &)
    {} // Ignore this error.
    return false;
}

TextureManifest &Textures::find(Uri const &uri) const
{
    LOG_AS("Textures::find");

    // Perform the search.
    // Is this a URN? (of the form "urn:schemename:uniqueid")
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        String const &pathStr = uri.path().toStringRef();
        int uIdPos = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            String schemeName = pathStr.left(uIdPos);
            int uniqueId      = pathStr.mid(uIdPos + 1 /*skip delimiter*/).toInt();

            try
            {
                return scheme(schemeName).findByUniqueId(uniqueId);
            }
            catch(Scheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
    }
    else
    {
        // No, this is a URI.
        String const &path = uri.path();

        // Does the user want a manifest in a specific scheme?
        if(!uri.scheme().isEmpty())
        {
            try
            {
                return scheme(uri.scheme()).find(path);
            }
            catch(Scheme::NotFoundError const &)
            {} // Ignore, we'll throw our own...
        }
        else
        {
            // No, check each scheme in priority order.
            foreach(Scheme *scheme, d->schemeCreationOrder)
            {
                try
                {
                    return scheme->find(path);
                }
                catch(Scheme::NotFoundError const &)
                {} // Ignore, we'll throw our own...
            }
        }
    }

    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("Textures::find", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

Textures::All const &Textures::all() const
{
    return d->textures;
}

static bool pathBeginsWithComparator(TextureManifest const &manifest, void *parameters)
{
    Path const *path = reinterpret_cast<Path*>(parameters);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static int collectManifestsInScheme(TextureScheme const &scheme,
    bool (*predicate)(TextureManifest const &manifest, void *parameters), void *parameters,
    QList<TextureManifest *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();
        if(predicate(manifest, parameters))
        {
            count += 1;
            if(storage) // Store mode?
            {
                storage->push_back(&manifest);
            }
        }
    }
    return count;
}

static QList<TextureManifest *> collectManifests(TextureScheme *scheme,
    Path const &path, QList<TextureManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Consider materials in the specified scheme only.
        count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider materials in any scheme.
        foreach(TextureScheme *scheme, App_Textures().allSchemes())
        {
            count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<TextureManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectManifests(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
static bool compareManifestPathsAssending(TextureManifest const *a, TextureManifest const *b)
{
    String pathA(QString(QByteArray::fromPercentEncoding(a->path().toUtf8())));
    String pathB(QString(QByteArray::fromPercentEncoding(b->path().toUtf8())));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printIndex2(TextureScheme *scheme, Path const &like,
                       Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<TextureManifest *> found = collectManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & Uri::OmitScheme);

    // Print a heading.
    String heading = "Known textures";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index key.
    qSort(found.begin(), found.end(), compareManifestPathsAssending);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(TextureManifest *manifest, found)
    {
        String info = String("%1: %2%3")
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasTexture()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_MSG("  " _E(>)) << info;
        idx++;
    }

    return found.count();
}

static void printIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    Textures &textures = App_Textures();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(textures.knownScheme(search.scheme()))
    {
        printTotal = printIndex2(&textures.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(TextureScheme *scheme, textures.allSchemes())
        {
            int numPrinted = printIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "texture" : "textures in total");
}

} // namespace de

static bool isKnownSchemeCallback(de::String name)
{
    return App_Textures().knownScheme(name);
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Textures &textures = App_Textures();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownSchemeCallback);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    de::printIndex(search);
    return true;
}

D_CMD(InspectTexture)
{
    DENG2_UNUSED(src);

    de::Textures &textures = App_Textures();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    try
    {
        de::TextureManifest &manifest = textures.find(search);
        if(manifest.hasTexture())
        {
            de::Texture &texture = manifest.texture();
            de::String variantCountText;
#ifdef __CLIENT__
            variantCountText = de::String(" (x%1)").arg(texture.variantCount());
#endif

            LOG_MSG("Texture " _E(1) "%s" _E(.) "%s"
                    "\n" _E(l) "Dimensions: " _E(.) _E(i) "%s" _E(.)
                     " " _E(l) "Source: "     _E(.) _E(i) "%s")
                << manifest.composeUri()
                << variantCountText
                << (texture.width() == 0 &&
                    texture.height() == 0? de::String("unknown (not yet prepared)")
                                         : texture.dimensions().asText())
                << manifest.sourceDescription();

#if defined(__CLIENT__) && defined(DENG_DEBUG)
            if(texture.variantCount())
            {
                // Print variant specs.
                LOG_MSG(_E(R));

                int variantIdx = 0;
                foreach(de::TextureVariant *variant, texture.variants())
                {
                    de::Vector2f coords;
                    variant->glCoords(&coords.x, &coords.y);

                    de::String textualVariantSpec = variant->spec().asText();

                    LOG_MSG(_E(D) "Variant #%i:" _E(.)
                            " " _E(l) "Source: " _E(.) _E(i) "%s" _E(.)
                            " " _E(l) "Masked: " _E(.) _E(i) "%s" _E(.)
                            " " _E(l) "GLName: " _E(.) _E(i) "%i" _E(.)
                            " " _E(l) "Coords: " _E(.) _E(i) "%s" _E(.)
                            _E(R)
                            "\n" _E(1) "Specification:" _E(.) "%s")
                        << variantIdx
                        << variant->sourceDescription()
                        << (variant->isMasked()? "yes":"no")
                        << variant->glName()
                        << coords.asText()
                        << textualVariantSpec;

                    ++variantIdx;
                }
            }
#endif // __CLIENT__ && DENG_DEBUG
        }
        else
        {
            LOG_MSG("%s") << manifest.description();
        }
        return true;
    }
    catch(de::Textures::NotFoundError const &er)
    {
        LOG_WARNING("%s.") << er.asText();
    }
    return false;
}

#ifdef DENG_DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED3(src, argc, argv);

    de::Textures &textures = App_Textures();

    LOG_MSG(_E(1) "Texture Statistics:");
    foreach(de::TextureScheme *scheme, textures.allSchemes())
    {
        de::TextureScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
