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
#include "de_filesys.h"
#include "dd_main.h" // App_ResourceSystem(), verbose
#include "resource/font.h"
#include "resource/bitmapfont.h"
#ifdef __CLIENT__
#  include "gl/gl_main.h"
#  include "gl/gl_texmanager.h"
#endif
#include <de/NativePath>
#include <de/PathTree>
#include <de/mathutil.h>
#include <de/memory.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

D_CMD(ListFonts);
#ifdef DENG_DEBUG
D_CMD(PrintFontStats);
#endif

namespace de {

typedef UserDataPathTree FontRepository;

/**
 * FontRecord. Stores metadata for a unique Font in the collection.
 */
struct FontRecord
{
    /// Scheme-unique identifier chosen by the owner of the collection.
    int uniqueId;

    /// The defined font instance (if any).
    font_t *font;
};

struct FontScheme
{
    /// FontRepository contains mappings between names and unique font records.
    FontRepository *repository;

    /// LUT which translates scheme-unique-ids to their associated fontid_t (if any).
    /// Index with uniqueId - uniqueIdBase.
    int uniqueIdBase;
    bool uniqueIdMapDirty;
    uint uniqueIdMapSize;
    fontid_t *uniqueIdMap;
};

static void destroyBoundFont(FontRepository::Node &node)
{
    FontRecord *record = (FontRecord *) node.userPointer();
    if(!record) return;
    delete record->font; record->font = 0;
}

static int composeAndCompareDirectoryNodePaths(void const *a, void const *b)
{
    // Decode paths before determining a lexicographical delta.
    FontRepository::Node const &nodeA = **(FontRepository::Node const **)a;
    FontRepository::Node const &nodeB = **(FontRepository::Node const **)b;
    QByteArray pathAUtf8 = nodeA.path().toUtf8();
    QByteArray pathBUtf8 = nodeB.path().toUtf8();
    AutoStr *pathA = Str_PercentDecode(AutoStr_FromTextStd(pathAUtf8.constData()));
    AutoStr *pathB = Str_PercentDecode(AutoStr_FromTextStd(pathBUtf8.constData()));
    return qstricmp(Str_Text(pathA), Str_Text(pathB));
}

/**
 * @defgroup validateFontUriFlags  Validate Font Uri Flags
 * @ingroup flags
 */
///@{
#define VFUF_ALLOW_ANY_SCHEME       0x1 ///< The scheme of the uri may be of zero-length; signifying "any scheme".
#define VFUF_NO_URN                 0x2 ///< Do not accept a URN.
///@}

DENG2_PIMPL(Fonts)
{
    Uri *emptyUri;

    // LUT which translates fontid_t to PathTreeNode*. Index with fontid_t-1
    uint fontIdMapSize;
    FontRepository::Node **fontIdMap;

    // Font scheme set.
    FontScheme schemes[FONTSCHEME_COUNT];

    Instance(Public *i) : Base(i)
    {
        LOG_VERBOSE("Initializing Fonts collection...");

        emptyUri = new Uri();

        fontIdMap = 0;
        fontIdMapSize = 0;

        for(int i = 0; i < FONTSCHEME_COUNT; ++i)
        {
            FontScheme &fn = schemes[i];
            fn.repository       = new FontRepository();
            fn.uniqueIdBase     = 0;
            fn.uniqueIdMapSize  = 0;
            fn.uniqueIdMap      = 0;
            fn.uniqueIdMapDirty = false;
        }
    }

    ~Instance()
    {
        self.clear();

        for(int i = 0; i < FONTSCHEME_COUNT; ++i)
        {
            FontScheme &fn = schemes[i];

            if(fn.repository)
            {
                fn.repository->traverse(PathTree::NoBranch, NULL, FontRepository::no_hash, destroyRecordWorker, this);
                delete fn.repository; fn.repository = 0;
            }

            M_Free(fn.uniqueIdMap); fn.uniqueIdMap = 0;

            fn.uniqueIdBase     = 0;
            fn.uniqueIdMapSize  = 0;
            fn.uniqueIdMapDirty = false;
        }

        // Clear the bindId to PathTreeNode LUT.
        M_Free(fontIdMap); fontIdMap = 0;
        fontIdMapSize = 0;

        if(emptyUri)
        {
            delete emptyUri; emptyUri = 0;
        }
    }

    void destroyRecord(FontRepository::Node &node)
    {
        LOG_AS("Fonts::destroyRecord");
        FontRecord *record = (FontRecord *) node.userPointer();
        if(!record) return;

#ifdef DENG_DEBUG
        if(record->font)
        {
            Uri uri = composeUriForDirectoryNode(node);
            LOG_WARNING("destroyRecord: Record for \"%s\" still has Font data!") << uri;
        }
#endif
        delete record->font; record->font = 0;

        unlinkDirectoryNodeFromBindIdMap(node);
        unlinkRecordInUniqueIdMap(node);

        // Detach our user data from this node.
        node.setUserPointer(0);

        M_Free(record);
    }

    void destroyFontAndRecord(FontRepository::Node &node)
    {
        destroyBoundFont(node);
        destroyRecord(node);
    }

    static int destroyRecordWorker(FontRepository::Node &node, void *context)
    {
        Instance *inst = static_cast<Instance *>(context);
        inst->destroyRecord(node);
        return 0; // Continue iteration.
    }

    static int destroyFontAndRecordWorker(FontRepository::Node &node, void *context)
    {
        Instance *inst = static_cast<Instance *>(context);
        inst->destroyFontAndRecord(node);
        return 0; // Continue iteration.
    }

    inline bool validFontId(fontid_t id)
    {
        return (id != NOFONTID && id <= fontIdMapSize);
    }

    inline FontRepository *repositoryBySchemeId(fontschemeid_t id)
    {
        DENG2_ASSERT(VALID_FONTSCHEMEID(id));
        return schemes[id - FONTSCHEME_FIRST].repository;
    }

    fontschemeid_t schemeIdForRepository(PathTree const &pt)
    {
        for(uint i = uint(FONTSCHEME_FIRST); i <= uint(FONTSCHEME_LAST); ++i)
        {
            uint idx = i - FONTSCHEME_FIRST;
            if(schemes[idx].repository == &pt) return fontschemeid_t(i);
        }
        // Only reachable if attempting to find the id for a Font that is not
        // in the collection, or the collection has not yet been initialized.
        Con_Error("Fonts::schemeIdForRepository: Failed to determine id for directory %p.", (void*)&pt);
        exit(1); // Unreachable.
    }

    FontRepository::Node *findDirectoryNodeForBindId(fontid_t id)
    {
        if(!validFontId(id)) return 0;
        return fontIdMap[id-1/*1-based index*/];
    }

    fontid_t findBindIdForDirectoryNode(FontRepository::Node const &node)
    {
        /// @todo Optimize (Low priority): Do not use a linear search.
        for(uint i = 0; i < fontIdMapSize; ++i)
        {
            if(fontIdMap[i] == &node)
                return fontid_t(i + 1); // 1-based index.
        }
        return NOFONTID; // Not linked.
    }

    inline fontschemeid_t schemeIdForDirectoryNode(FontRepository::Node const &node)
    {
        return schemeIdForRepository(node.tree());
    }

    /// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
    Uri composeUriForDirectoryNode(FontRepository::Node const &node)
    {
        return Uri(self.schemeName(schemeIdForDirectoryNode(node)), node.path());
    }

    /// @pre fontIdMap has been initialized and is large enough!
    void unlinkDirectoryNodeFromBindIdMap(FontRepository::Node const &node)
    {
        fontid_t id = findBindIdForDirectoryNode(node);
        if(!validFontId(id)) return; // Not linked.
        fontIdMap[id - 1/*1-based index*/] = 0;
    }

    /// @pre uniqueIdMap has been initialized and is large enough!
    static int linkRecordInUniqueIdMap(FontRepository::Node &node, void *context)
    {
        Instance *inst = static_cast<Instance *>(context);

        FontRecord const *record = (FontRecord *) node.userPointer();
        fontschemeid_t const schemeId = inst->schemeIdForRepository(node.tree());
        FontScheme &fn = inst->schemes[schemeId - FONTSCHEME_FIRST];
        fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = inst->findBindIdForDirectoryNode(node);
        return 0; // Continue iteration.
    }

    /// @pre uniqueIdMap has been initialized and is large enough!
    static int linkRecordInUniqueIdMapWorker(FontRepository::Node &node, void *context)
    {
        Instance *inst = static_cast<Instance *>(context);

        FontRecord const *record = (FontRecord *) node.userPointer();
        fontschemeid_t const schemeId = inst->schemeIdForRepository(node.tree());
        FontScheme &fn = inst->schemes[schemeId - FONTSCHEME_FIRST];
        fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = inst->findBindIdForDirectoryNode(node);
        return 0; // Continue iteration.
    }

    /// @pre uniqueIdMap is large enough if initialized!
    void unlinkRecordInUniqueIdMap(FontRepository::Node &node)
    {
        FontRecord const *record = (FontRecord *) node.userPointer();
        fontschemeid_t const schemeId = schemeIdForRepository(node.tree());
        FontScheme &fn = schemes[schemeId - FONTSCHEME_FIRST];
        if(fn.uniqueIdMap)
        {
            fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = NOFONTID;
        }
    }

    /**
     * @param uri       Uri to be validated.
     * @param flags     @ref validateFontUriFlags
     * @param quiet     @c true= Do not output validation remarks to the log.
     *
     * @return  @c true if @a Uri passes validation.
     */
    bool validateUri(Uri const &uri, int flags, bool quiet = false)
    {
        LOG_AS("Fonts::validateUri");

        if(uri.isEmpty())
        {
            if(!quiet)
            {
                LOG_MSG("Invalid path in Font uri \"%s\".") << uri;
            }
            return false;
        }

        // If this is a URN we extract the scheme from the path.
        String schemeString;
        if(!uri.scheme().compareWithoutCase("urn"))
        {
            if(flags & VFUF_NO_URN) return false;
            schemeString = uri.path();
        }
        else
        {
            schemeString = uri.scheme();
        }

        fontschemeid_t schemeId = self.parseScheme(schemeString.toUtf8().constData());
        if(!((flags & VFUF_ALLOW_ANY_SCHEME) && schemeId == FS_ANY) &&
           !VALID_FONTSCHEMEID(schemeId))
        {
            if(!quiet)
            {
                LOG_MSG("Unknown scheme in Font uri \"%s\".") << uri;
            }
            return false;
        }

        return true;
    }

    /**
     * Given a directory and path, search the Fonts collection for a match.
     *
     * @param directory  Scheme-specific PathTree to search in.
     * @param path  Path of the font to search for.
     * @return  Found DirectoryNode else @c NULL
     */
    FontRepository::Node *findDirectoryNodeForPath(FontRepository &directory, Path const &path)
    {
        try
        {
            FontRepository::Node &node = directory.find(path, PathTree::NoBranch | PathTree::MatchFull);
            return &node;
        }
        catch(FontRepository::NotFoundError const &)
        {} // Ignore this error.
        return 0;
    }

    /// @pre @a uri has already been validated and is well-formed.
    FontRepository::Node *findDirectoryNodeForUri(Uri const &uri)
    {
        if(!uri.scheme().compareWithoutCase("urn"))
        {
            // This is a URN of the form; urn:schemename:uniqueid
            fontschemeid_t schemeId = self.parseScheme(uri.pathCStr());
            int uidPos = uri.path().toStringRef().indexOf(':');
            if(uidPos >= 0)
            {
                int uid = uri.path().toStringRef().mid(uidPos + 1/*skip scheme delimiter*/).toInt();
                fontid_t id = self.fontForUniqueId(schemeId, uid);
                if(id != NOFONTID)
                {
                    return findDirectoryNodeForBindId(id);
                }
            }
            return 0;
        }

        // This is a URI.
        fontschemeid_t schemeId = self.parseScheme(uri.schemeCStr());
        Path const &path = uri.path();
        if(schemeId != FS_ANY)
        {
            // Caller wants a font in a specific scheme.
            return findDirectoryNodeForPath(*repositoryBySchemeId(schemeId), path);
        }

        // Caller does not care which scheme.
        // Check for the font in these schemes in priority order.
        static const fontschemeid_t order[] = {
            FS_GAME, FS_SYSTEM, FS_ANY
        };

        for(int i = 0; order[i] != FS_ANY; ++i)
        {
            FontRepository &repository = *repositoryBySchemeId(order[i]);
            FontRepository::Node *node = findDirectoryNodeForPath(repository, path);
            if(node)
            {
                return node;
            }
        }

        return 0;
    }

    font_t *createFromDef(fontid_t id, ded_compositefont_t *def)
    {
        LOG_AS("Fonts::createFromDef");

        FontRepository::Node *node = findDirectoryNodeForBindId(id);
        if(!node)
        {
            LOG_WARNING("Failed creating Font #%u (invalid id), ignoring.") << id;
            return 0;
        }

        if(!def)
        {
            LOG_WARNING("Failed creating Font #%u (def = NULL), ignoring.") << id;
            return 0;
        }

        FontRecord *record = (FontRecord *) node->userPointer();
        DENG_ASSERT(record != 0);

        if(record->font)
        {
            if(bitmapcompositefont_t *compFont = record->font->maybeAs<bitmapcompositefont_t>())
            {
                /// @todo Do not update fonts here (not enough knowledge). We should
                /// instead return an invalid reference/signal and force the caller
                /// to implement the necessary update logic.
#ifdef DENG_DEBUG
                LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.")
                    << self.composeUri(id);
#endif
                compFont->rebuildFromDef(def);
                return compFont;
            }
        }

        // A new font.
        record->font = bitmapcompositefont_t::fromDef(id, def);
        if(record->font && verbose >= 1)
        {
            LOG_VERBOSE("New font \"%s\"") << self.composeUri(id);
        }

        return record->font;
    }

    font_t *createFromFile(fontid_t id, char const *resourcePath)
    {
        LOG_AS("Fonts::createFromFile");

        FontRepository::Node *node = findDirectoryNodeForBindId(id);
        if(!node)
        {
            LOG_WARNING("Failed creating Font #%u (invalid id), ignoring.") << id;
            return 0;
        }

        if(!resourcePath || !resourcePath[0])
        {
            LOG_WARNING("Failed creating Font #%u (resourcePath = NULL), ignoring.") << id;
            return 0;
        }

        FontRecord *record = (FontRecord *) node->userPointer();
        DENG2_ASSERT(record != 0);

        if(record->font)
        {
            /// @todo Do not update fonts here (not enough knowledge). We should
            /// instead return an invalid reference/signal and force the caller
            /// to implement the necessary update logic.
            font_t *font = record->font;
#ifdef DENG_DEBUG
            LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.")
                << self.composeUri(id);
#endif
            Font_RebuildFromFile(font, resourcePath);
            return font;
        }

        // A new font.
        record->font = Font_FromFile(id, resourcePath);
        if(record->font && verbose >= 1)
        {
            LOG_VERBOSE("New font \"%s\"") << self.composeUri(id);
        }

        return record->font;
    }

    int iterateDirectory(fontschemeid_t schemeId,
        int (*callback)(FontRepository::Node &node, void *context), void *context)
    {
        fontschemeid_t from, to;
        if(VALID_FONTSCHEMEID(schemeId))
        {
            from = to = schemeId;
        }
        else
        {
            from = FONTSCHEME_FIRST;
            to   = FONTSCHEME_LAST;
        }

        int result = 0;
        for(uint i = uint(from); i <= uint(to); ++i)
        {
            FontRepository *directory = repositoryBySchemeId(fontschemeid_t(i));
            if(!directory) continue;

            result = directory->traverse(PathTree::NoBranch, 0, FontRepository::no_hash, callback, context);
            if(result) break;
        }
        return result;
    }

    struct iteratedirectoryworker_params_t
    {
        Instance *inst;
        int (*definedCallback)(font_t *font, void *context);
        int (*declaredCallback)(fontid_t id, void *context);
        void *context;
    };

    static int iterateWorker(FontRepository::Node &node, void *context)
    {
        DENG2_ASSERT(context != 0);
        iteratedirectoryworker_params_t *p = (iteratedirectoryworker_params_t*)context;

        FontRecord *record = (FontRecord *) node.userPointer();
        DENG2_ASSERT(record != 0);
        if(p->definedCallback)
        {
            if(record->font)
            {
                return p->definedCallback(record->font, p->context);
            }
        }
        else
        {
            fontid_t id = NOFONTID;

            // If we have bound a font it can provide the id.
            if(record->font)
            {
                id = record->font->primaryBind();
            }

            // Otherwise look it up.
            if(!p->inst->validFontId(id))
            {
                id = p->inst->findBindIdForDirectoryNode(node);
            }

            // Sanity check.
            DENG2_ASSERT(p->inst->validFontId(id));

            return p->declaredCallback(id, p->context);
        }
        return 0; // Continue iteration.
    }

    struct finduniqueidbounds_params_t
    {
        int minId, maxId;
    };

    static int findUniqueIdBounds(FontRepository::Node &node, void *context)
    {
        FontRecord const *record = (FontRecord *) node.userPointer();
        finduniqueidbounds_params_t *p = (finduniqueidbounds_params_t *)context;
        if(record->uniqueId < p->minId) p->minId = record->uniqueId;
        if(record->uniqueId > p->maxId) p->maxId = record->uniqueId;
        return 0; // Continue iteration.
    }

    void rebuildUniqueIdMap(fontschemeid_t schemeId)
    {
        FontScheme scheme = schemes[schemeId - FONTSCHEME_FIRST];
        DENG2_ASSERT(VALID_FONTSCHEMEID(schemeId));

        if(!scheme.uniqueIdMapDirty) return;

        // Determine the size of the LUT.
        finduniqueidbounds_params_t parm; zap(parm);
        parm.minId = DDMAXINT;
        parm.maxId = DDMININT;
        iterateDirectory(schemeId, findUniqueIdBounds, &parm);

        if(parm.minId > parm.maxId)
        {
            // None found.
            scheme.uniqueIdBase = 0;
            scheme.uniqueIdMapSize = 0;
        }
        else
        {
            scheme.uniqueIdBase = parm.minId;
            scheme.uniqueIdMapSize = parm.maxId - parm.minId + 1;
        }

        // Construct and (re)populate the LUT.
        scheme.uniqueIdMap = (fontid_t *)M_Realloc(scheme.uniqueIdMap, sizeof *scheme.uniqueIdMap * scheme.uniqueIdMapSize);
        if(scheme.uniqueIdMapSize)
        {
            std::memset(scheme.uniqueIdMap, NOFONTID, sizeof *scheme.uniqueIdMap * scheme.uniqueIdMapSize);
            iterateDirectory(schemeId, linkRecordInUniqueIdMap, this);
        }
        scheme.uniqueIdMapDirty = false;
    }

    /**
     * @todo A horridly inefficent algorithm. This should be implemented in PathTree
     * itself and not force users of this class to implement this sort of thing themselves.
     * However this is only presently used for the font search/listing console commands so
     * is not hugely important right now.
     */
    struct collectdirectorynodeworker_params_t
    {
        de::String like;
        int idx;
        FontRepository::Node **storage;
    };

    static int collectDirectoryNodeWorker(FontRepository::Node &node, void *context)
    {
        collectdirectorynodeworker_params_t *p = (collectdirectorynodeworker_params_t *)context;

        if(!p->like.isEmpty())
        {
            de::String path = node.path();
            if(!path.beginsWith(p->like, Qt::CaseInsensitive)) return 0; // Continue iteration.
        }

        if(p->storage)
        {
            p->storage[p->idx++] = &node;
        }
        else
        {
            ++p->idx;
        }

        return 0; // Continue iteration.
    }

    FontRepository::Node **collectDirectoryNodes(fontschemeid_t schemeId,
        String like, uint *count, FontRepository::Node **storage)
    {
        fontschemeid_t fromId, toId;
        if(VALID_FONTSCHEMEID(schemeId))
        {
            // Only consider fonts in this scheme.
            fromId = toId = schemeId;
        }
        else
        {
            // Consider fonts in any scheme.
            fromId = FONTSCHEME_FIRST;
            toId   = FONTSCHEME_LAST;
        }

        collectdirectorynodeworker_params_t p;
        p.like = like;
        p.idx = 0;
        p.storage = storage;

        for(uint i = uint(fromId); i <= uint(toId); ++i)
        {
            FontRepository *fontDirectory = repositoryBySchemeId(fontschemeid_t(i));
            if(!fontDirectory) continue;

            fontDirectory->traverse(PathTree::NoBranch | PathTree::MatchFull, 0,
                                    FontRepository::no_hash, collectDirectoryNodeWorker, &p);
        }

        if(storage)
        {
            storage[p.idx] = NULL; // Terminate.
            if(count) *count = p.idx;
            return storage;
        }

        if(p.idx == 0)
        {
            if(count) *count = 0;
            return NULL;
        }

        storage = (FontRepository::Node **) M_Malloc(sizeof(*storage) * (p.idx+1));
        return collectDirectoryNodes(schemeId, like, count, storage);
    }
};

Fonts::Fonts() : d(new Instance(this))
{}

void Fonts::consoleRegister() // static
{
    C_CMD("listfonts",  NULL, ListFonts)
#ifdef DENG_DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

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
        { NULL,             0,                          FS_ANY    }
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

String const &Fonts::schemeName(fontschemeid_t id)
{
    static String const schemes[1 + FONTSCHEME_COUNT] = {
        /* No scheme name */ "",
        /* FS_SYSTEM */      "System",
        /* FS_GAME */        "Game"
    };
    if(VALID_FONTSCHEMEID(id))
    {
        return schemes[1 + (id - FONTSCHEME_FIRST)];
    }
    return schemes[0];
}

uint Fonts::size()
{
    return d->fontIdMapSize;
}

uint Fonts::count(fontschemeid_t schemeId)
{
    if(!VALID_FONTSCHEMEID(schemeId)) return 0;
    if(!size()) return 0;

    FontRepository *directory = d->repositoryBySchemeId(schemeId);
    if(!directory) return 0;
    return directory->size();
}

void Fonts::clear()
{
    if(!size()) return;

    clearScheme(FS_ANY);
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

void Fonts::clearRuntime()
{
    if(!size()) return;

    clearScheme(FS_GAME);
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

void Fonts::clearSystem()
{
    if(!size()) return;

    clearScheme(FS_SYSTEM);
#ifdef __CLIENT__
    GL_PruneTextureVariantSpecifications();
#endif
}

void Fonts::clearScheme(fontschemeid_t schemeId)
{
    if(!size()) return;

    fontschemeid_t from, to;
    if(schemeId == FS_ANY)
    {
        from = FONTSCHEME_FIRST;
        to   = FONTSCHEME_LAST;
    }
    else if(VALID_FONTSCHEMEID(schemeId))
    {
        from = to = schemeId;
    }
    else
    {
        Con_Error("Fonts::ClearScheme: Invalid font scheme %i.", (int) schemeId);
        exit(1); // Unreachable.
    }

    for(uint i = uint(from); i <= uint(to); ++i)
    {
        FontScheme &fn = d->schemes[i - FONTSCHEME_FIRST];

        if(fn.repository)
        {
            fn.repository->traverse(PathTree::NoBranch, 0, FontRepository::no_hash,
                                    Instance::destroyFontAndRecordWorker, d);
            fn.repository->clear();
        }

        fn.uniqueIdMapDirty = true;
    }
}

font_t *Fonts::toFont(fontid_t id)
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

    FontRepository::Node *node = d->findDirectoryNodeForBindId(id);
    if(!node) return 0;
    return ((FontRecord *) node->userPointer())->font;
}

fontid_t Fonts::fontForUniqueId(fontschemeid_t schemeId, int uniqueId)
{
    if(VALID_FONTSCHEMEID(schemeId))
    {
        FontScheme fn = d->schemes[schemeId - FONTSCHEME_FIRST];

        d->rebuildUniqueIdMap(schemeId);
        if(fn.uniqueIdMap && uniqueId >= fn.uniqueIdBase &&
           (unsigned)(uniqueId - fn.uniqueIdBase) <= fn.uniqueIdMapSize)
        {
            return fn.uniqueIdMap[uniqueId - fn.uniqueIdBase];
        }
    }
    return NOFONTID; // Not found.
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
    FontRepository::Node *node = d->findDirectoryNodeForUri(uri);
    if(node)
    {
        // If we have bound a font - it can provide the id.
        FontRecord *record = (FontRecord *) node->userPointer();
        DENG2_ASSERT(record != 0);

        if(record->font)
        {
            fontid_t id = record->font->primaryBind();
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

fontid_t Fonts::declare(Uri const &uri, int uniqueId)
{
    LOG_AS("Fonts::declare");

    // We require a properly formed URI (but not a URN - this is a path).
    if(!d->validateUri(uri, VFUF_NO_URN, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Font \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
        return NOFONTID;
    }

    // Have we already created a binding for this?
    fontid_t id;
    FontRecord *record;
    FontRepository::Node *node = d->findDirectoryNodeForUri(uri);
    if(node)
    {
        record = (FontRecord *) node->userPointer();
        DENG2_ASSERT(record != 0);
        id = d->findBindIdForDirectoryNode(*node);
    }
    else
    {
        /*
         * A new binding.
         */

        record = (FontRecord  *) M_Malloc(sizeof *record);
        record->font     = 0;
        record->uniqueId = uniqueId;

        fontschemeid_t schemeId = parseScheme(uri.schemeCStr());
        FontScheme &fn = d->schemes[schemeId - FONTSCHEME_FIRST];

        node = &fn.repository->insert(uri.path());
        node->setUserPointer(record);

        // We'll need to rebuild the unique id map too.
        fn.uniqueIdMapDirty = true;

        id = d->fontIdMapSize + 1; // 1-based identfier
        // Link it into the id map.
        d->fontIdMap = (FontRepository::Node **) M_Realloc(d->fontIdMap, sizeof(*d->fontIdMap) * ++d->fontIdMapSize);
        d->fontIdMap[id - 1] = node;
    }

    /*
     * (Re)configure this binding.
     */
    bool releaseFont = false;

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release fonts when they change.
    if(record->uniqueId != uniqueId)
    {
        record->uniqueId = uniqueId;
        releaseFont = true;

        // We'll need to rebuild the id map too.
        fontschemeid_t schemeId = d->schemeIdForRepository(node->tree());
        FontScheme &fn = d->schemes[schemeId - FONTSCHEME_FIRST];
        fn.uniqueIdMapDirty = true;
    }

    if(releaseFont && record->font)
    {
        // The mapped resource is being replaced, so release any existing Font.
        /// @todo Only release if this Font is bound to only this binding.
        record->font->glDeinit();
    }

    return id;
}

int Fonts::uniqueId(fontid_t id)
{
    FontRepository::Node *node = d->findDirectoryNodeForBindId(id);
    if(node)
    {
        return ((FontRecord*) node->userPointer())->uniqueId;
    }
    throw Error("Fonts::UniqueId", QString("Passed invalid fontId #%1.").arg(id));
}

fontid_t Fonts::id(font_t *font)
{
    LOG_AS("Fonts::id");
    if(!font)
    {
#ifdef DENG_DEBUG
        LOG_WARNING("Attempted with invalid reference [%p], returning NOFONTID.") << font;
#endif
        return NOFONTID;
    }
    return font->primaryBind();
}

/*fontschemeid_t Fonts::scheme(fontid_t id)
{
    LOG_AS("Fonts::scheme");
    FontRepository::Node *node = d->findDirectoryNodeForBindId(id);
    if(!node)
    {
#ifdef DENG_DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning FS_ANY.") << id;
        }
#endif
        return FS_ANY;
    }
    return d->schemeIdForDirectoryNode(*node);
}*/

Uri Fonts::composeUri(fontid_t id)
{
    LOG_AS("Fonts::composeUri");
    FontRepository::Node *node = d->findDirectoryNodeForBindId(id);
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
    return d->composeUriForDirectoryNode(*node);
}

Uri Fonts::composeUrn(fontid_t id)
{
    LOG_AS("Fonts::composeUrn");

    FontRepository::Node *node = d->findDirectoryNodeForBindId(id);
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

    FontRecord const *record = (FontRecord *) node->userPointer();
    DENG2_ASSERT(record != 0);

    return Uri("urn", String("%1:%2").arg(schemeName(d->schemeIdForDirectoryNode(*node))).arg(record->uniqueId, 0, 10));
}

font_t *Fonts::createFontFromFile(Uri const &uri, char const *resourcePath)
{
    LOG_AS("R_CreateFontFromFile");

    if(!resourcePath || !resourcePath[0] || !F_Access(resourcePath))
    {
        LOG_WARNING("Invalid Uri or ResourcePath reference, ignoring.");
        return 0;
    }

    fontschemeid_t schemeId = parseScheme(uri.schemeCStr());
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        LOG_WARNING("Invalid font scheme in Font Uri \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
        return 0;
    }

    int uniqueId = count(schemeId) + 1; // 1-based index.
    fontid_t fontId = declare(uri, uniqueId/*, resourcePath*/);
    if(fontId == NOFONTID) return 0; // Invalid URI?

    // Have we already encountered this name?
    font_t *font = toFont(fontId);
    if(font)
    {
        Font_RebuildFromFile(font, resourcePath);
    }
    else
    {
        // A new font.
        font = d->createFromFile(fontId, resourcePath);
        if(!font)
        {
            LOG_WARNING("Failed defining new Font for \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
        }
    }
    return font;
}

font_t *Fonts::createFontFromDef(ded_compositefont_t *def)
{
    LOG_AS("Fonts::CreateFontFromDef");

    if(!def || !def->uri)
    {
        LOG_WARNING("Invalid Definition or Uri reference, ignoring.");
        return 0;
    }
    Uri const &uri = reinterpret_cast<Uri const &>(*def->uri);

    fontschemeid_t schemeId = parseScheme(uri.schemeCStr());
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        LOG_WARNING("Invalid URI scheme in font definition \"%s\", ignoring.")
            << NativePath(uri.asText()).pretty();
        return 0;
    }

    int uniqueId = count(schemeId) + 1; // 1-based index.
    fontid_t fontId = declare(uri, uniqueId);
    if(fontId == NOFONTID) return 0; // Invalid URI?

    // Have we already encountered this name?
    font_t *font = toFont(fontId);
    if(font)
    {
        if(bitmapcompositefont_t *compFont = font->maybeAs<bitmapcompositefont_t>())
        {
            compFont->rebuildFromDef(def);
        }
    }
    else
    {
        // A new font.
        font = d->createFromDef(fontId, def);
        if(!font)
        {
            LOG_WARNING("Failed defining new Font for \"%s\", ignoring.")
                << NativePath(uri.asText()).pretty();
        }
    }
    return font;
}

int Fonts::iterate(fontschemeid_t schemeId,
    int (*callback)(font_t *font, void *context), void *context)
{
    if(!callback) return 0;

    Instance::iteratedirectoryworker_params_t parm; zap(parm);
    parm.inst            = d;
    parm.definedCallback = callback;
    parm.context         = context;
    return d->iterateDirectory(schemeId, Instance::iterateWorker, &parm);
}

int Fonts::iterateDeclared(fontschemeid_t schemeId,
    int (*callback)(fontid_t fontId, void *context), void *context)
{
    if(!callback) return 0;

    Instance::iteratedirectoryworker_params_t parm; zap(parm);
    parm.inst             = d;
    parm.declaredCallback = callback;
    parm.context          = context;
    return d->iterateDirectory(schemeId, Instance::iterateWorker, &parm);
}

static int clearDefinitionLinkWorker(font_t *font, void * /*context*/)
{
    if(bitmapcompositefont_t *compFont = font->maybeAs<bitmapcompositefont_t>())
    {
        compFont->setDefinition(0);
    }
    return 0; // Continue iteration.
}

void Fonts::clearDefinitionLinks()
{
    if(!size()) return;
    iterate(FS_ANY, clearDefinitionLinkWorker);
}

static int releaseFontTextureWorker(font_t *font, void * /*context*/)
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

ddstring_t **Fonts::collectNames(int *rCount)
{
    uint count = 0;
    ddstring_t **list = NULL;

    FontRepository::Node **foundFonts = d->collectDirectoryNodes(FS_ANY, "", &count, 0);
    if(foundFonts)
    {
        qsort(foundFonts, (size_t)count, sizeof(*foundFonts), composeAndCompareDirectoryNodePaths);

        list = (ddstring_t **) M_Malloc(sizeof(*list) * (count+1));

        int idx = 0;
        for(FontRepository::Node **iter = foundFonts; *iter; ++iter)
        {
            FontRepository::Node &node = **iter;
            QByteArray path = node.path().toUtf8();
            list[idx++] = Str_Set(Str_NewStd(), path.constData());
        }
        list[idx] = NULL; // Terminate.
    }

    if(rCount) *rCount = count;
    return list;
}

#if 0

static void printFontOverview(FontRepository::Node &node, bool printSchemeName)
{
    FontRecord *record = (FontRecord *) node.userPointer();
    fontid_t fontId = findBindIdForDirectoryNode(node);
    int numUidDigits = de::max(3/*uid*/, M_NumDigits(Fonts_Size()));
    de::Uri uri;
    if(record->font)
    {
        uri = App_Fonts().composeUri(fontId);
    }
    ddstring_t const *path = (printSchemeName? uri.asText() : uri.path());
    font_t *font = record->font;

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
    FontRepository::Node **foundFonts = collectDirectoryNodes(schemeId, like, &count, NULL);
    FontRepository::Node **iter;
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
        FontRepository::Node *node = *iter;
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
        FontRepository *fontDirectory = repositoryBySchemeId(fontschemeid_t(i));
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
