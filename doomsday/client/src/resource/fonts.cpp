/** @file fonts.cpp  Font resource collection.
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
#include "dd_main.h" // App_Fonts(), verbose
#include <de/Log>
#include <de/Observers>
#include <de/memory.h>
#include <QList>
#include <QtAlgorithms>

namespace de {

DENG2_PIMPL(Fonts),
DENG2_OBSERVES(FontScheme, ManifestDefined),
DENG2_OBSERVES(FontManifest, Deletion),
DENG2_OBSERVES(AbstractFont, Deletion)
{
    /// System subspace schemes containing the manifests/resources.
    Schemes schemes;
    QList<Scheme *> schemeCreationOrder;

    All fonts; ///< From all schemes.
    uint manifestCount; ///< Total number of font manifests (in all schemes).

    uint manifestIdMapSize;
    FontManifest **manifestIdMap; ///< Index with fontid_t-1

    Instance(Public *i)
        : Base(i)
        , manifestCount(0)
        , manifestIdMapSize(0)
        , manifestIdMap(0)
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

        // Clear the manifest index/map.
        if(manifestIdMap)
        {
            M_Free(manifestIdMap); manifestIdMap = 0;
            manifestIdMapSize = 0;
        }
        manifestCount = 0;
    }

    /// Observes Scheme ManifestDefined.
    void schemeManifestDefined(Scheme & /*scheme*/, Manifest &manifest)
    {
        // We want notification when the manifest is derived to produce a resource.
        //manifest.audienceForFontDerived += this;

        // We want notification when the manifest is about to be deleted.
        manifest.audienceForDeletion += this;

        // Acquire a new unique identifier for the manifest.
        fontid_t const id = ++manifestCount; // 1-based.
        manifest.setUniqueId(id);

        // Add the new manifest to the id index/map.
        if(manifestCount > manifestIdMapSize)
        {
            // Allocate more memory.
            manifestIdMapSize += 32;
            manifestIdMap = (Manifest **) M_Realloc(manifestIdMap, sizeof(*manifestIdMap) * manifestIdMapSize);
        }
        manifestIdMap[manifestCount - 1] = &manifest;
    }

#if 0
    /// Observes Manifest FontDerived.
    void manifestFontDerived(Manifest & /*manifest*/, AbstractFont &font)
    {
        // Include this new font in the scheme-agnostic list of instances.
        fonts.push_back(&font);

        // We want notification when the font is about to be deleted.
        font.audienceForDeletion += this;
    }
#endif

    /// Observes Manifest Deletion.
    void manifestBeingDeleted(Manifest const &manifest)
    {
        manifestIdMap[manifest.uniqueId() - 1 /*1-based*/] = 0;

        // There will soon be one fewer manifest in the system.
        manifestCount -= 1;
    }

    /// Observes AbstractFont Deletion.
    void fontBeingDeleted(AbstractFont const &font)
    {
        fonts.removeOne(const_cast<AbstractFont *>(&font));
    }
};

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

bool Fonts::knownScheme(String name) const
{
    if(!name.isEmpty())
    {
        return d->schemes.contains(name.toLower());
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
        DENG2_ASSERT(false);
    }

    /// @throw UnknownIdError The specified manifest id is invalid.
    throw UnknownIdError("Fonts::toManifest", QString("Invalid font ID %1, valid range [1..%2)").arg(id).arg(d->manifestCount + 1));
}

Fonts::All const &Fonts::all() const
{
    return d->fonts;
}

static bool pathBeginsWithComparator(FontManifest const &manifest, void *context)
{
    Path const *path = reinterpret_cast<Path *>(context);
    /// @todo Use PathTree::Node::compare()
    return manifest.path().toStringRef().beginsWith(*path, Qt::CaseInsensitive);
}

/**
 * @todo This logic should be implemented in de::PathTree -ds
 */
static int collectManifestsInScheme(FontScheme const &scheme,
    bool (*predicate)(FontManifest const &manifest, void *context), void *context,
    QList<FontManifest *> *storage = 0)
{
    int count = 0;
    PathTreeIterator<FontScheme::Index> iter(scheme.index().leafNodes());
    while(iter.hasNext())
    {
        FontManifest &manifest = iter.next();
        if(predicate(manifest, context))
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
        // Consider resources in the specified scheme only.
        count += collectManifestsInScheme(*scheme, pathBeginsWithComparator, (void *)&path, storage);
    }
    else
    {
        // Consider resources in any scheme.
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
 * @param scheme    Resource subspace scheme being printed. Can be @c NULL in
 *                  which case resources are printed from all schemes.
 * @param like      Resource path search term.
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
        heading += " like \"" _E(b) + like.toStringRef() + _E(.) "\"";
    LOG_MSG(_E(D) "%s:" _E(.)) << heading;

    // Print the result index.
    qSort(found.begin(), found.end(), compareManifestPathsAssending);
    int numFoundDigits = de::max(3/*idx*/, M_NumDigits(found.count()));
    int idx = 0;
    foreach(FontManifest *manifest, found)
    {
        String info = String("%1: %2%3" _E(.))
                        .arg(idx, numFoundDigits)
                        .arg(manifest->hasResource()? _E(1) : _E(2))
                        .arg(manifest->description(composeUriFlags));

        LOG_MSG("  " _E(>)) << info;
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
        LOG_MSG(_E(R));
    }
    // Print results within only the one scheme?
    else if(fonts.knownScheme(search.scheme()))
    {
        printTotal = printIndex2(&fonts.scheme(search.scheme()), search.path(), flags | de::Uri::OmitScheme);
        LOG_MSG(_E(R));
    }
    else
    {
        // Collect and sort results in each scheme separately.
        foreach(FontScheme *scheme, fonts.allSchemes())
        {
            int numPrinted = printIndex2(scheme, search.path(), flags | de::Uri::OmitScheme);
            if(numPrinted)
            {
                LOG_MSG(_E(R));
                printTotal += numPrinted;
            }
        }
    }
    LOG_MSG("Found " _E(b) "%i" _E(.) " %s.") << printTotal << (printTotal == 1? "font" : "fonts in total");
}

} // namespace de

static bool isKnownSchemeCallback(de::String name)
{
    return App_Fonts().knownScheme(name);
}

D_CMD(ListFonts)
{
    DENG2_UNUSED(src);

    de::Fonts &fonts = App_Fonts();
    de::Uri search = de::Uri::fromUserInput(&argv[1], argc - 1, &isKnownSchemeCallback);
    if(!search.scheme().isEmpty() && !fonts.knownScheme(search.scheme()))
    {
        LOG_WARNING("Unknown scheme %s") << search.scheme();
        return false;
    }

    de::printIndex(search);
    return true;
}

#ifdef DENG_DEBUG
D_CMD(PrintFontStats)
{
    DENG2_UNUSED3(src, argc, argv);

    de::Fonts &fonts = App_Fonts();

    LOG_MSG(_E(1) "Font Statistics:");
    foreach(de::FontScheme *scheme, fonts.allSchemes())
    {
        de::FontScheme::Index const &index = scheme->index();

        uint const count = index.count();
        LOG_MSG("Scheme: %s (%u %s)")
            << scheme->name() << count << (count == 1? "font" : "fonts");
        index.debugPrintHashDistribution();
        index.debugPrint();
    }
    return true;
}
#endif

void de::Fonts::consoleRegister() // static
{
    C_CMD("listfonts",  NULL, ListFonts)
#ifdef DENG_DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}
