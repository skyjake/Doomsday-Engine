/** @file textures.cpp Texture Resource Collection.
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

#include "de_base.h"
#include "de_console.h"
#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"
#endif
#include "resource/compositetexture.h"
#include <de/Log>
#include <de/math.h>
#include <de/mathutil.h> // for M_NumDigits
#include <QtAlgorithms>

#include "resource/textures.h"

D_CMD(ListTextures);
D_CMD(InspectTexture);

#ifdef DENG_DEBUG
D_CMD(PrintTextureStats);
#endif

namespace de {

Texture *Textures::ResourceClass::interpret(TextureManifest &manifest, void *userData)
{
    LOG_AS("Textures::ResourceClass::interpret");
    Texture *tex = new Texture(manifest);
    tex->setFlags(manifest.flags());
    tex->setDimensions(manifest.logicalDimensions());
    tex->setOrigin(manifest.origin());
    tex->setUserDataPointer(userData);
    return tex;
}

DENG2_PIMPL(Textures)
{
    /// System subspace schemes containing the textures.
    Textures::Schemes schemes;
    QList<Textures::Scheme *> schemeCreationOrder;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        clearManifests();
    }

    void clearManifests()
    {
        qDeleteAll(schemes);
        schemes.clear();
        schemeCreationOrder.clear();
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

Textures::~Textures()
{
    clearAllSchemes();
    delete d;
}

Textures::Scheme &Textures::scheme(String name) const
{
    LOG_AS("Textures::scheme");
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Textures::UnknownSchemeError("Textures::scheme", "No scheme found matching '" + name + "'");
}

Textures::Scheme& Textures::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name);
    d->schemes.insert(name.toLower(), newScheme);
    d->schemeCreationOrder.push_back(newScheme);
    return *newScheme;
}

bool Textures::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return true;
    }
    return false;
}

Textures::Schemes const& Textures::allSchemes() const
{
    return d->schemes;
}

void Textures::validateUri(Uri const &uri, UriValidationFlags flags) const
{
    if(uri.isEmpty())
    {
        /// @throw UriMissingPathError The URI is missing the required path component.
        throw UriMissingPathError("Textures::validateUri", "Missing path in URI \"" + uri.asText() + "\"");
    }

    // If this is a URN we extract the scheme from the path.
    String schemeString;
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        if(flags.testFlag(NotUrn))
        {
            /// @throw UriIsUrnError The URI uses URN notation.
            throw UriIsUrnError("Textures::validateUri", "URI \"" + uri.asText() + "\" uses URN notation");
        }

        String const &pathStr = uri.path().toStringRef();
        int const uIdPos      = pathStr.indexOf(':');
        if(uIdPos > 0)
        {
            schemeString = pathStr.left(uIdPos);
        }
    }
    else
    {
        schemeString = uri.scheme();
    }

    if(schemeString.isEmpty())
    {
        if(!flags.testFlag(AnyScheme))
        {
            /// @throw UriMissingSchemeError The URI is missing the required scheme component.
            throw UriMissingSchemeError("Textures::validateUri", "Missing scheme in URI \"" + uri.asText() + "\"");
        }
    }
    else if(!knownScheme(schemeString))
    {
        /// @throw UriUnknownSchemeError The URI specifies an unknown scheme.
        throw UriUnknownSchemeError("Textures::validateUri", "Unknown scheme in URI \"" + uri.asText() + "\"");
    }
}

Textures::Manifest &Textures::find(Uri const &uri) const
{
    LOG_AS("Textures::find");

    try
    {
        validateUri(uri, AnyScheme);

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
    }
    catch(UriValidationError const &er)
    {
        /// @throw NotFoundError Failed to locate a matching manifest.
        throw NotFoundError("Textures::find", er.asText());
    }

    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("Textures::find", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
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

TextureManifest *Textures::declare(Uri const &uri, de::Texture::Flags flags,
    Vector2i const &dimensions, Vector2i const &origin, int uniqueId, de::Uri const *resourceUri)
{
    LOG_AS("Textures::declare");

    if(uri.isEmpty()) return 0;

    // Have we already created a manifest for this?
    TextureManifest *manifest = 0;
    try
    {
        // We require a properly formed uri (but not a urn - this is a path).
        validateUri(uri, NotUrn);

        manifest = &find(uri);
    }
    catch(NotFoundError const &)
    {
        manifest = &scheme(uri.scheme()).insertManifest(uri.path());
    }
    catch(UriValidationError const &er)
    {
        throw Error("Textures::declare", er.asText() + ". Failed declaring texture \"" + uri + "\"");
    }

    /*
     * (Re)configure the manifest.
     */
    bool mustRelease = false;

    manifest->flags() = flags;
    manifest->setOrigin(origin);

    if(manifest->setLogicalDimensions(dimensions))
    {
        mustRelease = true;
    }

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release textures when they change.
    if(manifest->setUniqueId(uniqueId))
    {
        mustRelease = true;
    }

    if(resourceUri && manifest->setResourceUri(*resourceUri))
    {
        // The mapped resource is being replaced, so release any existing Texture.
        /// @todo Only release if this Texture is bound to only this binding.
        mustRelease = true;
    }

    if(mustRelease && manifest->hasTexture())
    {
#ifdef __CLIENT__
        /// @todo Update any Materials (and thus Surfaces) which reference this.
        GL_ReleaseGLTexturesByTexture(manifest->texture());
#endif
    }

    return manifest;
}

static int iterateTextures(TextureScheme const &scheme,
    int (*callback)(Texture &tex, void *parameters), void *parameters)
{
    PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();
        if(!manifest.hasTexture()) continue;

        if(int result = callback(manifest.texture(), parameters))
            return result;
    }
    return 0; // Continue iteration.
}

int Textures::iterate(String nameOfScheme,
    int (*callback)(Texture &tex, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    // Limit iteration to a specific scheme?
    if(!nameOfScheme.isEmpty())
    {
        return iterateTextures(scheme(nameOfScheme), callback, parameters);
    }

    foreach(Scheme *scheme, d->schemes)
    {
        if(int result = iterateTextures(*scheme, callback, parameters))
            return result;
    }
    return 0;
}

static int iterateManifests(TextureScheme const &scheme,
    int (*callback)(TextureManifest &manifest, void *parameters), void *parameters)
{
    PathTreeIterator<TextureScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        if(int result = callback(iter.next(), parameters))
            return result;
    }
    return 0; // Continue iteration.
}

int Textures::iterateDeclared(String nameOfScheme,
    int (*callback)(TextureManifest &manifest, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    // Limit iteration to a specific scheme?
    if(!nameOfScheme.isEmpty())
    {
        return iterateManifests(scheme(nameOfScheme), callback, parameters);
    }

    foreach(Scheme *scheme, d->schemes)
    {
        if(int result = iterateManifests(*scheme, callback, parameters))
            return result;
    }

    return 0;
}

#ifdef __CLIENT__
static void printVariantInfo(TextureVariant &variant)
{
    float s, t;
    variant.coords(&s, &t);
    Con_Printf("  Source:%s GLName:%u Masked:%s Prepared:%s Uploaded:%s\n  Coords:(s:%g t:%g)\n",
               TexSource_Name(variant.source()),
               variant.glName(),
               variant.isMasked()  ? "yes":"no",
               variant.isPrepared()? "yes":"no",
               variant.isUploaded()? "yes":"no", s, t);

    Con_Printf("  Specification: ");
    GL_PrintTextureVariantSpecification(variant.spec());
}
#endif

static void printTextureInfo(Texture &tex)
{
    Uri uri = tex.manifest().composeUri();
    QByteArray path = uri.asText().toUtf8();

#ifdef __CLIENT__
    Con_Printf("Texture \"%s\" [%p] x%u origin:%s\n",
               path.constData(), (void *)&tex, tex.variantCount(),
               tex.flags().testFlag(Texture::Custom)? "addon" : "game");
#else
    Con_Printf("Texture \"%s\" [%p] origin:%s\n",
               path.constData(), (void *)&tex,
               tex.flags().testFlag(Texture::Custom)? "addon" : "game");
#endif

    if(tex.width() == 0 && tex.height() == 0)
        Con_Printf("Dimensions: unknown (not yet loaded)\n");
    else
        Con_Printf("Dimensions: %d x %d\n", tex.width(), tex.height());

#ifdef __CLIENT__
    uint variantIdx = 0;
    foreach(TextureVariant *variant, tex.variants())
    {
        Con_Printf("Variant #%i:\n", variantIdx);
        printVariantInfo(*variant);

        ++variantIdx;
    }
#endif
}

static void printManifestInfo(TextureManifest &manifest,
    de::Uri::ComposeAsTextFlags uriCompositionFlags = de::Uri::DefaultComposeAsTextFlags)
{
    String sourceDescription = !manifest.hasTexture()? "unknown"
        : manifest.texture().flags().testFlag(Texture::Custom)? "addon" : "game";

    String info = String("%1 %2")
                    .arg(manifest.composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                         ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) )
                    .arg(sourceDescription, -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!manifest.hasTexture()? 0 : manifest.texture().variantCount());
#endif
    info += " " + (manifest.resourceUri().isEmpty()? "N/A" : manifest.resourceUri().asText());

    info += "\n";
    Con_FPrintf(!manifest.hasTexture()? CPF_LIGHT : CPF_WHITE, info.toUtf8().constData());
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

    if(!path.isEmpty())
    {
        if(scheme)
        {
            // Consider materials in the specified scheme only.
            count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void*)&path, storage);
        }
        else
        {
            // Consider materials in any scheme.
            foreach(TextureScheme *scheme, App_Textures().allSchemes())
            {
                count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void*)&path, storage);
            }
        }
    }

    if(storage)
    {
        return *storage;
    }

    QList<TextureManifest *> result;
    if(count == 0) return result;

#ifdef DENG2_QT_4_7_OR_NEWER
    result.reserve(count);
#endif
    return collectManifests(scheme, path, &result);
}

/**
 * Decode and then lexicographically compare the two manifest
 * paths, returning @c true if @a is less than @a b.
 */
static bool compareTextureManifestPathsAssending(TextureManifest const *a,
                                                 TextureManifest const *b)
{
    String pathA = QString(QByteArray::fromPercentEncoding(a->path().toUtf8()));
    String pathB = QString(QByteArray::fromPercentEncoding(b->path().toUtf8()));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printTextures2(TextureScheme *scheme, Path const &like,
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
        heading += " like \"" + like + "\"";
    heading += ":";
    Con_FPrintf(CPF_YELLOW, "%s\n", heading.toUtf8().constData());

    // Print the result index key.
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));

#ifdef __CLIENT__
    Con_Printf(" %*s: %-*s origin n# uri\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path");
#else
    Con_Printf(" %*s: %-*s origin uri\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path");
#endif
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareTextureManifestPathsAssending);
    int idx = 0;
    foreach(TextureManifest *manifest, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printManifestInfo(*manifest, composeUriFlags);
    }

    return found.count();
}

static void printTextures(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    Textures &textures = App_Textures();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextures2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(textures.knownScheme(search.scheme()))
    {
        printTotal = printTextures2(&textures.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(TextureScheme *scheme, textures.allSchemes())
        {
            int numPrinted = printTextures2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                Con_PrintRuler();
                printTotal += numPrinted;
            }
        }
    }
    Con_Printf("Found %i %s.\n", printTotal, printTotal == 1? "Texture" : "Textures");
}

} // namespace de

/**
 * @param argv  The arguments to be composed. All are assumed to be in non-encoded
 *              representation.
 *
 *              Supported forms (where <> denote keyword component names):
 *               - [0: "<scheme>:<path>"]
 *               - [0: "<scheme>"]              (if @a matchSchemeOnly)
 *               - [0: "<path>"]
 *               - [0: "<scheme>", 1: "<path>"]
 * @param argc  The number of elements in @a argv.
 * @param matchSchemeOnly  @c true= check if the sole argument matches a known scheme.
 */
static de::Uri composeSearchUri(char **argv, int argc, bool matchSchemeOnly = true)
{
    if(argv)
    {
        // [0: <scheme>:<path>] or [0: <scheme>] or [0: <path>].
        if(argc == 1)
        {
            // Try to extract the scheme and encode the rest of the path.
            de::String rawUri = de::String(argv[0]);
            int pos = rawUri.indexOf(':');
            if(pos >= 0)
            {
                de::String scheme = rawUri.left(pos);
                rawUri.remove(0, pos + 1);
                return de::Uri(scheme, QString(QByteArray(rawUri.toUtf8()).toPercentEncoding()));
            }

            // Just a scheme name?
            if(matchSchemeOnly && App_Textures().knownScheme(rawUri))
            {
                return de::Uri().setScheme(rawUri);
            }

            // Just a path.
            return de::Uri(de::Path(QString(QByteArray(rawUri.toUtf8()).toPercentEncoding())));
        }
        // [0: <scheme>, 1: <path>]
        if(argc == 2)
        {
            // Assign the scheme and encode the path.
            return de::Uri(argv[0], QString(QByteArray(argv[1]).toPercentEncoding()));
        }
    }
    return de::Uri();
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Textures &textures = App_Textures();
    de::Uri search = composeSearchUri(&argv[1], argc - 1);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    de::printTextures(search);
    return true;
}

D_CMD(InspectTexture)
{
    DENG2_UNUSED(src);

    de::Textures &textures = App_Textures();
    de::Uri search = composeSearchUri(&argv[1], argc - 1, false /*don't match schemes*/);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    try
    {
        de::TextureManifest &manifest = textures.find(search);
        if(manifest.hasTexture())
        {
            de::printTextureInfo(manifest.texture());
        }
        else
        {
            de::printManifestInfo(manifest);
        }
        return true;
    }
    catch(de::Textures::NotFoundError const &er)
    {
        QString msg = er.asText() + ".";
        Con_Printf("%s\n", msg.toUtf8().constData());
    }
    return false;
}

#ifdef DENG_DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED3(src, argc, argv);

    de::Textures &textures = App_Textures();

    Con_FPrintf(CPF_YELLOW, "Texture Statistics:\n");
    foreach(de::TextureScheme *scheme, textures.allSchemes())
    {
        de::TextureScheme::Index const &index = scheme->index();

        uint const count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme->name().toUtf8().constData(), count, count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
