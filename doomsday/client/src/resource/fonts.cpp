/** @file fonts.cpp Font resource collection.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "resource/fonts.h"

#include "de_console.h"
#include "dd_main.h" // App_ResourceSystem(), verbose
#ifdef __CLIENT__
#  include "BitmapFont"
#  include "CompositeBitmapFont"
#endif
#include <de/Error>
#include <de/memory.h>
#include <QList>
#include <QtAlgorithms>
#include <cstdlib>

D_CMD(ListFonts);
#ifdef DENG_DEBUG
D_CMD(PrintFontStats);
#endif

namespace de {

DENG2_PIMPL(Fonts)
{
    /// System subspace schemes containing the textures.
    Schemes schemes;
    QList<Scheme *> schemeCreationOrder;

    /// All fonts instances in the system (from all schemes).
    All fonts;

    /// Total number of URI font manifests (in all schemes).
    uint manifestCount;

    /// LUT which translates fontid_t => FontManifest*.
    /// Index with fontid_t-1
    uint manifestIdMapSize;
    FontManifest **manifestIdMap;

    Instance(Public *i)
        : Base(i)
        , manifestCount(0)
        , manifestIdMapSize(0)
        , manifestIdMap(0)
    {}

    ~Instance()
    {
        clearFonts();
        clearManifests();
    }

    void clearFonts()
    {
        qDeleteAll(fonts);
        fonts.clear();
    }

    void clearManifests()
    {
        qDeleteAll(schemes);
        schemes.clear();
        schemeCreationOrder.clear();

        // Clear the manifest index/map.
        if(manifestIdMap)
        {
            M_Free(manifestIdMap); manifestIdMap = 0;
            manifestIdMapSize = 0;
        }
        manifestCount = 0;
    }
};

void Fonts::consoleRegister() // static
{
    C_CMD("listfonts",  NULL, ListFonts)
#ifdef DENG_DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

Fonts::Fonts() : d(new Instance(this))
{}

Fonts::Scheme &Fonts::scheme(String name) const
{
    LOG_AS("Fonts::scheme");
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("Fonts::scheme", "No scheme found matching '" + name + "'");
}

Fonts::Scheme &Fonts::createScheme(String name)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name);
    d->schemes.insert(name.toLower(), newScheme);
    d->schemeCreationOrder.push_back(newScheme);

    // We want notification when a new manifest is defined in this scheme.
    newScheme->audienceForManifestDefined += this;

    return *newScheme;
}

bool Fonts::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return true;
    }
    return false;
}

Fonts::Schemes const &Fonts::allSchemes() const
{
    return d->schemes;
}

bool Fonts::has(Uri const &path) const
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

Fonts::Manifest &Fonts::find(Uri const &uri) const
{
    LOG_AS("Fonts::find");

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
    throw NotFoundError("Fonts::find", "Failed to locate a manifest matching \"" + uri.asText() + "\"");
}

Fonts::Manifest &Fonts::toManifest(fontid_t id) const
{
    if(id > 0 && id <= d->manifestCount)
    {
        duint32 idx = id - 1; // 1-based index.
        if(d->manifestIdMap[idx])
        {
            return *d->manifestIdMap[idx];
        }

        // Internal bookeeping error.
        DENG_ASSERT(0);
    }

    /// @throw UnknownIdError The specified material id is invalid.
    throw UnknownIdError("Fonts::toManifest", QString("Invalid font ID %1, valid range [1..%2)").arg(id).arg(d->manifestCount + 1));
}

#if 0
fontschemeid_t Fonts::parseScheme(char const *str)
{
    static const struct scheme_s {
        const char *name;
        size_t nameLen;
        fontschemeid_t id;
    } schemes[FONTSCHEME_COUNT+1] = {
        // Ordered according to a best guess of occurance frequency.
        { "Game",     sizeof("Game")   - 1, FS_GAME   },
        { "System",   sizeof("System") - 1, FS_SYSTEM },
        { NULL,       0,                    FS_ANY    }
    };
    size_t len, n;

    // Special case: zero-length string means "any scheme".
    if(!str || 0 == (len = strlen(str))) return FS_ANY;

    // Stop comparing characters at the first occurance of ':'
    char const *end = strchr(str, ':');
    if(end) len = end - str;

    for(n = 0; schemes[n].name; ++n)
    {
        if(len < schemes[n].nameLen) continue;
        if(strnicmp(str, schemes[n].name, len)) continue;
        return schemes[n].id;
    }

    return FS_INVALID; // Unknown.
}

void Fonts::clear()
{
    if(!size()) return;

    foreach(Scheme *scheme, d->schemes)
    {
        scheme->clear();
    }
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

void Fonts::clearRuntime()
{
    if(!size()) return;

    scheme("Game").clear();
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

void Fonts::clearSystem()
{
    if(!size()) return;

    scheme("System").clear();
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

AbstractFont *Fonts::toFont(fontid_t id)
{
    LOG_AS("Fonts::toFont");
    if(!d->validFontId(id))
    {
#ifdef DENG_DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Failed to locate font for id #%i, returning NULL.") << id;
        }
#endif
        return NULL;
    }

    FontScheme::Index::Node *node = d->findDirectoryNodeForBindId(id);
    if(!node) return 0;
    return &static_cast<FontManifest *>(node->userPointer())->font();
}

fontid_t Fonts::resolveUri(Uri const &uri, boolean quiet)
{
    LOG_AS("Fonts::resolveUri");

    if(!size()) return NOFONTID;

    if(!d->validateUri(uri, VFUF_ALLOW_ANY_SCHEME, true /*quiet please*/))
    {
#ifdef DENG_DEBUG
        LOG_WARNING("Uri \"%s\" failed validation, returning NOFONTID.") << uri;
#endif
        return NOFONTID;
    }

    // Perform the search.
    FontScheme::Index::Node *node = d->findDirectoryNodeForUri(uri);
    if(node)
    {
        // If we have bound a font - it can provide the id.
        FontManifest *manifest = (FontManifest *) node->userPointer();
        DENG2_ASSERT(manifest != 0);

        if(manifest->hasFont())
        {
            fontid_t id = manifest->font().primaryBind();
            if(d->validFontId(id)) return id;
        }

        // Oh well, look it up then.
        return d->findBindIdForDirectoryNode(*node);
    }

    // Not found.
    if(!quiet)
    {
        LOG_DEBUG("\"%s\" not found, returning NOFONTID.") << uri;
    }
    return NOFONTID;
}

int Fonts::uniqueId(fontid_t id)
{
    FontScheme::Index::Node *node = d->findDirectoryNodeForBindId(id);
    if(node)
    {
        return static_cast<FontManifest *>(node->userPointer())->uniqueId();
    }
    throw Error("Fonts::UniqueId", QString("Passed invalid fontId #%1.").arg(id));
}

Uri Fonts::composeUri(fontid_t id)
{
    LOG_AS("Fonts::composeUri");
    FontScheme::Index::Node *node = d->findDirectoryNodeForBindId(id);
    if(!node)
    {
#ifdef DENG_DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning null-object.") << id;
        }
#endif
        return Uri();
    }
    return static_cast<FontManifest *>(node->userPointer())->composeUri();
}

Uri Fonts::composeUrn(fontid_t id)
{
    LOG_AS("Fonts::composeUrn");

    FontScheme::Index::Node *node = d->findDirectoryNodeForBindId(id);
    if(!node)
    {
#ifdef DENG_DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning null-object.") << id;
        }
#endif
        return Uri();
    }

    return static_cast<FontManifest *>(node->userPointer())->composeUrn();
}
#endif

void Fonts::clearDefinitionLinks()
{
    foreach(AbstractFont *font, d->fonts)
    {
        if(CompositeBitmapFont *compFont = font->maybeAs<CompositeBitmapFont>())
        {
            compFont->setDefinition(0);
        }
    }
}

void Fonts::schemeManifestDefined(Scheme & /*scheme*/, Manifest &manifest)
{
    // We want notification when the manifest is derived to produce a material.
    //manifest.audienceForFontDerived += this;

    // We want notification when the manifest is about to be deleted.
    manifest.audienceForDeletion += this;

    // Acquire a new unique identifier for the manifest.
    fontid_t const id = ++d->manifestCount; // 1-based.
    manifest.setUniqueId(id);

    // Add the new manifest to the id index/map.
    if(d->manifestCount > d->manifestIdMapSize)
    {
        // Allocate more memory.
        d->manifestIdMapSize += 32;
        d->manifestIdMap = (Manifest **) M_Realloc(d->manifestIdMap, sizeof(*d->manifestIdMap) * d->manifestIdMapSize);
    }
    d->manifestIdMap[d->manifestCount - 1] = &manifest;
}

#if 0
void Fonts::manifestFontDerived(Manifest & /*manifest*/, AbstractFont &font)
{
    // Include this new font in the scheme-agnostic list of instances.
    d->fonts.push_back(&font);

    // We want notification when the font is about to be deleted.
    font.audienceForDeletion += this;
}
#endif

void Fonts::manifestBeingDeleted(Manifest const &manifest)
{
    d->manifestIdMap[manifest.uniqueId() - 1 /*1-based*/] = 0;

    // There will soon be one fewer manifest in the system.
    d->manifestCount -= 1;
}

void Fonts::fontBeingDeleted(AbstractFont const &font)
{
    d->fonts.removeOne(const_cast<AbstractFont *>(&font));
}

Fonts::All const &Fonts::all() const
{
    return d->fonts;
}

static bool pathBeginsWithComparator(FontManifest const &manifest, void *parameters)
{
    Path const *path = reinterpret_cast<Path *>(parameters);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static int collectManifestsInScheme(FontScheme const &scheme,
    bool (*predicate)(FontManifest const &manifest, void *parameters), void *parameters,
    QList<FontManifest *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<FontScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        FontManifest &manifest = iter.next();
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

static QList<FontManifest *> collectManifests(FontScheme *scheme,
    Path const &path, QList<FontManifest *> *storage = 0)
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
        foreach(FontScheme *scheme, App_Fonts().allSchemes())
        {
            count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
        }
    }

    // Are we done?
    if(storage) return *storage;

    // Collect and populate.
    QList<FontManifest *> result;
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
static bool compareManifestPathsAssending(FontManifest const *a, FontManifest const *b)
{
    String pathA(QString(QByteArray::fromPercentEncoding(a->path().toUtf8())));
    String pathB(QString(QByteArray::fromPercentEncoding(b->path().toUtf8())));
    return pathA.compareWithoutCase(pathB) < 0;
}

/**
 * @param scheme    Font subspace scheme being printed. Can be @c NULL in
 *                  which case textures are printed from all schemes.
 * @param like      Font path search term.
 * @param composeUriFlags  Flags governing how URIs should be composed.
 */
static int printIndex2(FontScheme *scheme, Path const &like,
                       Uri::ComposeAsTextFlags composeUriFlags)
{
    QList<FontManifest *> found = collectManifests(scheme, like);
    if(found.isEmpty()) return 0;

    bool const printSchemeName = !(composeUriFlags & Uri::OmitScheme);

    // Print a heading.
    String heading = "Known fonts";
    if(!printSchemeName && scheme)
        heading += " in scheme '" + scheme->name() + "'";
    if(!like.isEmpty())
        heading += " like \"" + like.toStringRef() + "\"";
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
    qSort(found.begin(), found.end(), compareManifestPathsAssending);
    int idx = 0;
    foreach(FontManifest *manifest, found)
    {
        String info = String(" %1: ").arg(idx, numFoundDigits)
                    + manifest->description(composeUriFlags);

        Con_FPrintf(!manifest->hasFont()? CPF_LIGHT : CPF_WHITE, "%s\n", info.toUtf8().constData());
        idx++;
    }

    return found.count();
}

static void printIndex(de::Uri const &search,
    de::Uri::ComposeAsTextFlags flags = de::Uri::DefaultComposeAsTextFlags)
{
    Fonts &fonts = App_Fonts();

    int printTotal = 0;

    // Collate and print results from all schemes?
    if(search.scheme().isEmpty() && !search.path().isEmpty())
    {
        printTotal = printIndex2(0/*any scheme*/, search.path(), flags & ~de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    // Print results within only the one scheme?
    else if(fonts.knownScheme(search.scheme()))
    {
        printTotal = printIndex2(&fonts.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(FontScheme *scheme, fonts.allSchemes())
        {
            int numPrinted = printIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                Con_PrintRuler();
                printTotal += numPrinted;
            }
        }
    }
    Con_Message("Found %i %s.", printTotal, printTotal == 1? "Font" : "Fonts");
}

#if 0

static int releaseFontTextureWorker(AbstractFont *font, void * /*context*/)
{
    font->glDeinit();
    return 0; // Continue iteration.
}

void Fonts::releaseTexturesByScheme(fontschemeid_t schemeId)
{
    if(novideo || isDedicated) return;
    if(!size()) return;
    iterate(schemeId, releaseFontTextureWorker);
}

void Fonts::releaseRuntimeTextures()
{
    releaseTexturesByScheme(FS_GAME);
}

void Fonts::releaseSystemTextures()
{
    releaseTexturesByScheme(FS_SYSTEM);
}

static void printFontOverview(FontScheme::Index::Node &node, bool printSchemeName)
{
    FontRecord *manifest = (FontRecord *) node.userPointer();
    fontid_t fontId = findBindIdForDirectoryNode(node);
    int numUidDigits = de::max(3/*uid*/, M_NumDigits(Fonts_Size()));
    de::Uri uri;
    if(manifest->font)
    {
        uri = App_Fonts().composeUri(fontId);
    }
    ddstring_t const *path = (printSchemeName? uri.asText() : uri.path());
    AbstractFont *font = manifest->font;

    Con_FPrintf(!font? CPF_LIGHT : CPF_WHITE,
        "%-*s %*u %s", printSchemeName? 22 : 14, F_PrettyPath(Str_Text(path)),
        numUidDigits, fontId, !font? "unknown" : font->type() == FT_BITMAP? "bitmap" : "bitmap_composite");

    if(font && Font_IsPrepared(font))
    {
        Con_Printf(" (ascent:%i, descent:%i, leading:%i", Font_Ascent(font), Font_Descent(font), Font_Leading(font));
        if(font->type() == FT_BITMAP && BitmapFont_GLTextureName(font))
        {
            Con_Printf(", texWidth:%i, texHeight:%i", BitmapFont_TextureWidth(font), BitmapFont_TextureHeight(font));
        }
        Con_Printf(")\n");
    }
    else
    {
        Con_Printf("\n");
    }
}

/**
 * @defgroup printFontFlags  Print Font Flags
 * @ingroup flags
 */
///@{
#define PFF_TRANSFORM_PATH_NO_SCHEME 0x1 /// Do not print the scheme.
///@}

#define DEFAULT_PRINTFONTFLAGS          0

/**
 * @param flags  @ref printFontFlags
 */
static size_t printFonts3(fontschemeid_t schemeId, char const *like, int flags)
{
    bool const printScheme = !(flags & PFF_TRANSFORM_PATH_NO_SCHEME);
    uint idx, count = 0;
    FontScheme::Index::Node **foundFonts = collectDirectoryNodes(schemeId, like, &count, NULL);
    FontScheme::Index::Node **iter;
    int numFoundDigits, numUidDigits;

    if(!foundFonts) return 0;

    if(!printScheme)
    {
        Con_FPrintf(CPF_YELLOW, "Known fonts in scheme '%s'", Str_Text(Fonts_SchemeName(schemeId)));
    }
    else // Any scheme.
    {
        Con_FPrintf(CPF_YELLOW, "Known fonts");
    }

    if(like && like[0])
    {
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    }
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = de::max(3/*idx*/, M_NumDigits((int)count));
    numUidDigits = de::max(3/*uid*/, M_NumDigits((int)Fonts_Size()));
    Con_Printf(" %*s: %-*s %*s type", numFoundDigits, "idx",
               printScheme ? 22 : 14, printScheme? "scheme:path" : "path",
               numUidDigits, "uid");

    // Fonts may be prepared only if GL is inited thus if we can't prepare, we can't list property values.
    if(GL_IsInited())
    {
        Con_Printf(" (<property-name>:<value>, ...)");
    }
    Con_Printf("\n");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundFonts, (size_t)count, sizeof(*foundFonts), composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundFonts; *iter; ++iter)
    {
        FontScheme::Index::Node *node = *iter;
        Con_Printf(" %*u: ", numFoundDigits, idx++);
        printFontOverview(*node, printScheme);
    }

    M_Free(foundFonts);
    return count;
}

static void printFonts2(fontschemeid_t schemeId, char const *like, int flags)
{
    size_t printTotal = 0;
    // Do we care which scheme?
    if(schemeId == FS_ANY && like && like[0])
    {
        printTotal = printFonts3(schemeId, like, flags & ~PFF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    // Only one scheme to print?
    else if(VALID_FONTSCHEMEID(schemeId))
    {
        printTotal = printFonts3(schemeId, like, flags | PFF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each scheme separately.
        for(int i = FONTSCHEME_FIRST; i <= FONTSCHEME_LAST; ++i)
        {
            size_t printed = printFonts3((fontschemeid_t)i, like, flags | PFF_TRANSFORM_PATH_NO_SCHEME);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Font" : "Fonts");
}

static void printFonts(fontschemeid_t schemeId, char const *like)
{
    printFonts2(schemeId, like, DEFAULT_PRINTFONTFLAGS);
}

#endif

} // namespace de

D_CMD(ListFonts)
{
#if 0
    DENG_UNUSED(src);

    de::Fonts &fonts = App_ResourceSystem().fonts();
    fontschemeid_t schemeId = FS_ANY;
    char const *like = NULL;
    de::Uri uri;

    if(!fonts.size())
    {
        Con_Message("There are currently no fonts defined/loaded.");
        return true;
    }

    // "listfonts [scheme] [name]"
    if(argc > 2)
    {
        uri.setScheme(argv[1]).setPath(argv[2]);

        schemeId = fonts.parseScheme(uri.schemeCStr());
        if(!VALID_FONTSCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
            return false;
        }
        like = uri.pathCStr();
    }
    // "listfonts [scheme:name]" i.e., a partial Uri
    else if(argc > 1)
    {
        uri.setUri(argv[1], RC_NULL);
        if(!uri.scheme().isEmpty())
        {
            schemeId = fonts.parseScheme(uri.schemeCStr());
            if(!VALID_FONTSCHEMEID(schemeId))
            {
                Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
                return false;
            }

            if(!uri.path().isEmpty())
                like = uri.pathCStr();
        }
        else
        {
            schemeId = fonts.parseScheme(uri.pathCStr());

            if(!VALID_FONTSCHEMEID(schemeId))
            {
                schemeId = FS_ANY;
                like = argv[1];
            }
        }
    }

    printFonts(schemeId, like);
#endif
    return true;
}

#ifdef DENG_DEBUG
D_CMD(PrintFontStats)
{
#if 0
    DENG2_UNUSED3(src, argv, argc);

    de::Fonts &fonts = App_ResourceSystem().fonts();
    if(!fonts.size())
    {
        Con_Message("There are currently no fonts defined/loaded.");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Font Statistics:\n");
    for(uint i = uint(FONTSCHEME_FIRST); i <= uint(FONTSCHEME_LAST); ++i)
    {
        FontScheme::Index *fontDirectory = repositoryBySchemeId(fontschemeid_t(i));
        uint size;

        if(!fontDirectory) continue;

        size = fontDirectory->size();
        Con_Printf("Scheme: %s (%u %s)\n", fonts.schemeName(fontschemeid_t(i)).toUtf8().constData(), size, size==1? "font":"fonts");
        fontDirectory->debugPrintHashDistribution();
        fontDirectory->debugPrint();
    }
#endif
    return true;
}
#endif
