/** @file textures.cpp Texture Resource Collection.
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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

#include <cstdlib>

#include "de_base.h"
#include "de_console.h"
#include "gl/gl_texmanager.h"
#include "m_misc.h" // for M_NumDigits

#include <QtAlgorithms>
#include <QList>
#include <de/Error>
#include <de/Log>
#include <de/PathTree>

#include "resource/compositetexture.h"
#include "resource/texturemanifest.h"
#include "resource/textures.h"

char const *TexSource_Name(TexSource source)
{
    if(source == TEXS_ORIGINAL) return "original";
    if(source == TEXS_EXTERNAL) return "external";
    return "none";
}

D_CMD(ListTextures);
D_CMD(InspectTexture);
#if _DEBUG
D_CMD(PrintTextureStats);
#endif

namespace de {

static Uri emptyUri;

Texture *Textures::ResourceClass::interpret(TextureManifest &manifest, Size2Raw const &dimensions,
    Texture::Flags flags, void *userData)
{
    LOG_AS("Textures::ResourceClass::interpret");
    return new Texture(manifest, dimensions, flags, userData);
}

Texture *Textures::ResourceClass::interpret(TextureManifest &manifest, Texture::Flags flags,
    void *userData)
{
    return interpret(manifest, Size2Raw(0, 0), flags, userData);
}

struct Textures::Instance
{
    /// System subspace schemes containing the textures.
    Textures::Schemes schemes;

    ~Instance()
    {
        DENG2_FOR_EACH(Textures::Schemes, i, schemes)
        {
            delete *i;
        }
        schemes.clear();
    }
};

void Textures::consoleRegister()
{
    C_CMD("inspecttexture", "ss",   InspectTexture)
    C_CMD("inspecttexture", "s",    InspectTexture)
    C_CMD("listtextures",   "ss",   ListTextures)
    C_CMD("listtextures",   "s",    ListTextures)
    C_CMD("listtextures",   "",     ListTextures)
#if _DEBUG
    C_CMD("texturestats",   NULL,   PrintTextureStats)
#endif
}

Textures::Textures()
{
    d = new Instance();
}

Textures::~Textures()
{
    clearAllSchemes();
    delete d;
}

Textures::Scheme& Textures::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme* newScheme = new Scheme(name);
    d->schemes.push_back(newScheme);
    return *newScheme;
}

static inline void releaseTexture(Texture *tex)
{
    /// Stub.
    GL_ReleaseGLTexturesByTexture(reinterpret_cast<texture_s *>(tex));
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

bool Textures::validateUri(Uri const &uri, UriValidationFlags flags, bool quiet) const
{
    LOG_AS("Textures::validateUri");
    bool const isUrn = !uri.scheme().compareWithoutCase("urn");

    if(uri.isEmpty())
    {
        if(!quiet) LOG_MSG("Empty path in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
        return false;
    }

    // If this is a URN we extract the scheme from the path.
    String schemeString;
    if(isUrn)
    {
        if(flags.testFlag(NotUrn))
        {
            if(!quiet) LOG_MSG("Texture URN \"%s\" supplied, URI required.") << uri;
            return false;
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
            if(!quiet) LOG_MSG("Missing scheme in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
            return false;
        }
    }
    else if(!knownScheme(schemeString))
    {
        if(!quiet) LOG_MSG("Unknown scheme in texture %s \"%s\".") << uri << (isUrn? "URN" : "URI");
        return false;
    }

    return true;
}

TextureManifest &Textures::find(Uri const &uri) const
{
    LOG_AS("Textures::find");

    if(!validateUri(uri, AnyScheme, true /*quiet please*/))
        throw NotFoundError("Textures::find", "URI \"" + uri.asText() + "\" failed validation");

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
            // No, check in each of these schemes (in priority order).
            /// @todo This priorty order should be defined by the user.
            static String const order[] = {
                "Sprites",
                "Textures",
                "Flats",
                "Patches",
                "System",
                "Details",
                "Reflections",
                "Masks",
                "ModelSkins",
                "ModelReflectionSkins",
                "Lightmaps",
                "Flaremaps",
                ""
            };
            for(int i = 0; !order[i].isEmpty(); ++i)
            {
                try
                {
                    return scheme(order[i]).find(path);
                }
                catch(Scheme::NotFoundError const &)
                {} // Ignore this error.
            }
        }
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

TextureManifest *Textures::declare(Uri const &uri, int uniqueId, Uri const *resourceUri)
{
    LOG_AS("Textures::declare");

    if(uri.isEmpty()) return 0;

    // We require a properly formed uri (but not a urn - this is a path).
    if(!validateUri(uri, NotUrn, (verbose >= 1)))
    {
        LOG_WARNING("Failed declaring texture \"%s\" (invalid URI), ignoring.") << uri;
        return 0;
    }

    // Have we already created a manifest for this?
    TextureManifest *manifest = 0;
    try
    {
        manifest = &find(uri);
    }
    catch(NotFoundError const &)
    {
        manifest = &scheme(uri.scheme()).insertManifest(uri.path());
        manifest->setUniqueId(uniqueId);
    }

    /*
     * (Re)configure the manifest.
     */

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release textures when they change.
    bool release = false;

    if(resourceUri && manifest->setResourceUri(*resourceUri))
    {
        release = true;
    }

    if(manifest->setUniqueId(uniqueId))
    {
        release = true;
    }

    if(release && manifest->texture())
    {
        // The mapped resource is being replaced, so release any existing Texture.
        /// @todo Only release if this Texture is bound to only this binding.
        releaseTexture(manifest->texture());
    }

    return manifest;
}

bool Textures::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH(Schemes, i, d->schemes)
        {
            if(!(*i)->name().compareWithoutCase(name))
                return true;
        }
        //Schemes::iterator found = d->schemes.find(name.toLower());
        //if(found != d->schemes.end()) return true;
    }
    return false;
}

Textures::Scheme &Textures::scheme(String name) const
{
    LOG_AS("Textures::scheme");
    DENG2_FOR_EACH(Schemes, i, d->schemes)
    {
        if(!(*i)->name().compareWithoutCase(name))
            return **i;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw Textures::UnknownSchemeError("Textures::scheme", "No scheme found matching '" + name + "'");
}

Textures::Schemes const& Textures::allSchemes() const
{
    return d->schemes;
}

int Textures::iterate(String nameOfScheme,
    int (*callback)(Texture &tex, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    if(!nameOfScheme.isEmpty())
    {
        PathTreeIterator<PathTree> iter(scheme(nameOfScheme).index().leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            if(!manifest.texture()) continue;

            int result = callback(*manifest.texture(), parameters);
            if(result) return result;
        }
    }
    else
    {
        DENG2_FOR_EACH_CONST(Schemes, i, d->schemes)
        {
            PathTreeIterator<PathTree> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
                if(!manifest.texture()) continue;

                int result = callback(*manifest.texture(), parameters);
                if(result) return result;
            }
        }
    }
    return 0;
}

int Textures::iterateDeclared(String nameOfScheme,
    int (*callback)(TextureManifest &manifest, void *parameters), void *parameters) const
{
    if(!callback) return 0;

    if(!nameOfScheme.isEmpty())
    {
        PathTreeIterator<PathTree> iter(scheme(nameOfScheme).index().leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            int result = callback(manifest, parameters);
            if(result) return result;
        }
    }
    else
    {
        DENG2_FOR_EACH_CONST(Schemes, i, d->schemes)
        {
            PathTreeIterator<PathTree> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
                int result = callback(manifest, parameters);
                if(result) return result;
            }
        }
    }

    return 0;
}

static void printVariantInfo(TextureVariant &variant)
{
    float s, t;
    variant.coords(&s, &t);
    Con_Printf("  Source:%s Masked:%s Prepared:%s Uploaded:%s\n  Coords:(s:%g t:%g)\n",
               TexSource_Name(variant.source()),
               variant.isMasked()  ? "yes":"no",
               variant.isPrepared()? "yes":"no",
               variant.isUploaded()? "yes":"no", s, t);

    Con_Printf("  Specification: ");
    GL_PrintTextureVariantSpecification(variant.spec());
}

static void printTextureInfo(Texture &tex)
{
    Uri uri = tex.manifest().composeUri();
    QByteArray path = NativePath(uri.asText()).pretty().toUtf8();

    Con_Printf("Texture \"%s\" [%p] x%u origin:%s\n",
               path.constData(), (void *)&tex, tex.variantCount(),
               tex.isCustom()? "addon" : "game");

    bool willAssumeImageDimensions = (tex.width() == 0 && tex.height() == 0);
    if(willAssumeImageDimensions)
        Con_Printf("Dimensions: unknown (not yet loaded)\n");
    else
        Con_Printf("Dimensions: %d x %d\n", tex.width(), tex.height());

    uint variantIdx = 0;
    DENG2_FOR_EACH_CONST(Texture::Variants, i, tex.variantList())
    {
        TextureVariant &variant = **i;

        Con_Printf("Variant #%i: GLName:%u\n", variantIdx, variant.glName());
        printVariantInfo(variant);

        ++variantIdx;
    }
}

static void printTextureSummary(TextureManifest &manifest, bool printSchemeName = true)
{
    Uri uri = manifest.composeUri();
    QByteArray path = printSchemeName? uri.asText().toUtf8() : QByteArray::fromPercentEncoding(uri.path().toStringRef().toUtf8());

    QByteArray resourceUri = manifest.resourceUri().asText().toUtf8();
    if(resourceUri.isEmpty()) resourceUri = "N/A";

    Con_FPrintf(!manifest.texture()? CPF_LIGHT : CPF_WHITE,
                "%-*s %-6s x%u %s\n", printSchemeName? 22 : 14, path.constData(),
                !manifest.texture()? "unknown" : manifest.texture()->isCustom()? "addon" : "game",
                manifest.texture()->variantCount(), resourceUri.constData());
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static QList<TextureManifest *> collectTextureManifests(Textures::Scheme *scheme,
    Path const &path, QList<TextureManifest *> *storage = 0)
{
    int count = 0;

    if(scheme)
    {
        // Only consider textures in this scheme.
        PathTreeIterator<Textures::Scheme::Index> iter(scheme->index().leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            if(!path.isEmpty())
            {
                /// @todo Use PathTree::Node::compare()
                if(!manifest.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
            }

            if(storage) // Store mode.
            {
                storage->push_back(&manifest);
            }
            else // Count mode.
            {
                ++count;
            }
        }
    }
    else
    {
        // Consider textures in any scheme.
        Textures::Schemes const &schemes = App_Textures()->allSchemes();
        DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
        {
            PathTreeIterator<Textures::Scheme::Index> iter((*i)->index().leafNodes());
            while(iter.hasNext())
            {
                TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
                if(!path.isEmpty())
                {
                    /// @todo Use PathTree::Node::compare()
                    if(!manifest.path().toString().beginsWith(path, Qt::CaseInsensitive)) continue;
                }

                if(storage) // Store mode.
                {
                    storage->push_back(&manifest);
                }
                else // Count mode.
                {
                    ++count;
                }
            }
        }
    }

    if(storage)
    {
        return *storage;
    }

    QList<TextureManifest*> result;
    if(count == 0) return result;

    result.reserve(count);
    return collectTextureManifests(scheme, path, &result);
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
 * @defgroup printTextureFlags  Print Texture Flags
 * @ingroup flags
 */
///@{
#define PTF_TRANSFORM_PATH_NO_SCHEME        0x1 ///< Do not print the scheme.
///@}

#define DEFAULT_PRINTTEXTUREFLAGS           0

/**
 * @param scheme    Texture subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Texture path search term.
 * @param flags     @ref printTextureFlags
 */
static int printTextures2(Textures::Scheme *scheme, Path const &like, int flags)
{
    QList<TextureManifest *> found = collectTextureManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(flags & PTF_TRANSFORM_PATH_NO_SCHEME);

    // Compose a heading.
    String heading = "Known textures";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" + NativePath(like).withSeparators('/') + "\"";
    heading += ":\n";

    // Print the result heading.
    Con_FPrintf(CPF_YELLOW, "%s", heading.toUtf8().constData());

    // Print the result index key.
    int numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(found.count()));

    Con_Printf(" %*s: %-*s origin n# uri\n", numFoundDigits, "idx",
               printSchemeName? 22 : 14, printSchemeName? "scheme:path" : "path");
    Con_PrintRuler();

    // Sort and print the index.
    qSort(found.begin(), found.end(), compareTextureManifestPathsAssending);
    int idx = 0;
    DENG2_FOR_EACH(QList<TextureManifest *>, i, found)
    {
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printTextureSummary(**i, printSchemeName);
    }

    return found.count();
}

static void printTextures(de::Uri const &search, int flags = DEFAULT_PRINTTEXTUREFLAGS)
{
    Textures &textures = *App_Textures();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printTextures2(0/*any scheme*/, search.path(), flags & ~PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(textures.knownScheme(search.scheme()))
    {
        printTotal = printTextures2(&textures.scheme(search.scheme()), search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        Textures::Schemes const &schemes = textures.allSchemes();
        DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
        {
            int numPrinted = printTextures2(*i, search.path(), flags | PTF_TRANSFORM_PATH_NO_SCHEME);
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
            if(matchSchemeOnly && App_Textures()->knownScheme(rawUri))
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

static de::Textures* textures;

de::Textures* App_Textures()
{
    if(!textures) throw de::Error("App_Textures", "Textures collection not yet initialized");
    return textures;
}

void Textures_Init(void)
{
    DENG_ASSERT(!textures);
    textures = new de::Textures();
}

void Textures_Shutdown(void)
{
    if(!textures) return;
    delete textures; textures = 0;
}

/// @note Part of the Doomsday public API.
int Textures_UniqueId2(Uri const *_uri, boolean quiet)
{
    LOG_AS("Textures_UniqueId");
    if(!_uri) return -1;
    de::Uri const &uri = reinterpret_cast<de::Uri const &>(*_uri);

    de::Textures &textures = *App_Textures();
    try
    {
        return textures.find(uri).uniqueId();
    }
    catch(de::Textures::NotFoundError const &)
    {
        // Log but otherwise ignore this error.
        if(!quiet)
        {
            LOG_WARNING("Unknown texture %s.") << uri;
        }
    }
    return -1;
}

/// @note Part of the Doomsday public API.
int Textures_UniqueId(Uri const *uri)
{
    return Textures_UniqueId2(uri, false);
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    de::Textures &textures = *App_Textures();
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

    de::Textures &textures = *App_Textures();
    de::Uri search = composeSearchUri(&argv[1], argc - 1, false /*don't match schemes*/);
    if(!search.scheme().isEmpty() && !textures.knownScheme(search.scheme()))
    {
        Con_Printf("Unknown scheme '%s'.\n", search.schemeCStr());
        return false;
    }

    try
    {
        de::TextureManifest &manifest = textures.find(search);
        if(de::Texture* tex = manifest.texture())
        {
            de::printTextureInfo(*tex);
        }
        else
        {
            de::printTextureSummary(manifest);
        }
        return true;
    }
    catch(de::Textures::NotFoundError const &er)
    {
        QString msg = er.asText() + ".";
        Con_Printf(msg.toUtf8().constData());
    }
    return false;
}

#if _DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED(src); DENG2_UNUSED(argc); DENG2_UNUSED(argv);

    de::Textures &textures = *App_Textures();

    Con_FPrintf(CPF_YELLOW, "Texture Statistics:\n");
    de::Textures::Schemes const &schemes = textures.allSchemes();
    DENG2_FOR_EACH_CONST(de::Textures::Schemes, i, schemes)
    {
        de::Textures::Scheme &scheme = **i;
        de::Textures::Scheme::Index const &index = scheme.index();

        uint const count = index.count();
        Con_Printf("Scheme: %s (%u %s)\n", scheme.name().toUtf8().constData(), count, count == 1? "texture" : "textures");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif
