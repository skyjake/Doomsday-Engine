/**
 * @file fonts.cpp
 *
 * Font resource repositories.
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <de/NativePath>
#include <de/PathTree>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"
#include "gl/gl_texmanager.h"
#include "resource/font.h"
#include "resource/fonts.h"
#include "resource/bitmapfont.h"

typedef de::UserDataPathTree FontRepository;

/**
 * FontRecord. Stores metadata for a unique Font in the collection.
 */
struct FontRecord
{
    /// Scheme-unique identifier chosen by the owner of the collection.
    int uniqueId;

    /// The defined font instance (if any).
    font_t* font;
};

struct FontScheme
{
    /// FontRepository contains mappings between names and unique font records.
    FontRepository* repository;

    /// LUT which translates scheme-unique-ids to their associated fontid_t (if any).
    /// Index with uniqueId - uniqueIdBase.
    int uniqueIdBase;
    bool uniqueIdMapDirty;
    uint uniqueIdMapSize;
    fontid_t* uniqueIdMap;
};

D_CMD(ListFonts);
#if _DEBUG
D_CMD(PrintFontStats);
#endif

static int iterateDirectory(fontschemeid_t schemeId,
    int (*callback)(FontRepository::Node& node, void* parameters), void* parameters);

static de::Uri* emptyUri;

// LUT which translates fontid_t to PathTreeNode*. Index with fontid_t-1
static uint fontIdMapSize;
static FontRepository::Node** fontIdMap;

// Font scheme set.
static FontScheme schemes[FONTSCHEME_COUNT];

void Fonts_Register(void)
{
    C_CMD("listfonts",  NULL, ListFonts)
#if _DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

static inline FontRepository* repositoryBySchemeId(fontschemeid_t id)
{
    DENG_ASSERT(VALID_FONTSCHEMEID(id));
    return schemes[id - FONTSCHEME_FIRST].repository;
}

static fontschemeid_t schemeIdForRepository(de::PathTree const &pt)
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

static inline bool validFontId(fontid_t id)
{
    return (id != NOFONTID && id <= fontIdMapSize);
}

static FontRepository::Node* findDirectoryNodeForBindId(fontid_t id)
{
    if(!validFontId(id)) return NULL;
    return fontIdMap[id-1/*1-based index*/];
}

static fontid_t findBindIdForDirectoryNode(FontRepository::Node const& node)
{
    /// @todo Optimize (Low priority): Do not use a linear search.
    for(uint i = 0; i < fontIdMapSize; ++i)
    {
        if(fontIdMap[i] == &node)
            return fontid_t(i + 1); // 1-based index.
    }
    return NOFONTID; // Not linked.
}

static inline fontschemeid_t schemeIdForDirectoryNode(FontRepository::Node const& node)
{
    return schemeIdForRepository(node.tree());
}

/// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
static de::Uri composeUriForDirectoryNode(FontRepository::Node const& node)
{
    Str const* schemeName = Fonts_SchemeName(schemeIdForDirectoryNode(node));
    return de::Uri(Str_Text(schemeName), node.path());
}

/// @pre fontIdMap has been initialized and is large enough!
static void unlinkDirectoryNodeFromBindIdMap(FontRepository::Node const& node)
{
    fontid_t id = findBindIdForDirectoryNode(node);
    if(!validFontId(id)) return; // Not linked.
    fontIdMap[id - 1/*1-based index*/] = NULL;
}

/// @pre uniqueIdMap has been initialized and is large enough!
static int linkRecordInUniqueIdMap(FontRepository::Node& node, void* parameters)
{
    DENG_UNUSED(parameters);

    FontRecord const* record = (FontRecord*) node.userPointer();
    fontschemeid_t const schemeId = schemeIdForRepository(node.tree());
    FontScheme& fn = schemes[schemeId - FONTSCHEME_FIRST];
    fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = findBindIdForDirectoryNode(node);
    return 0; // Continue iteration.
}

/// @pre uniqueIdMap is large enough if initialized!
static int unlinkRecordInUniqueIdMap(FontRepository::Node& node, void* parameters)
{
    DENG_UNUSED(parameters);

    FontRecord const* record = (FontRecord*) node.userPointer();
    fontschemeid_t const schemeId = schemeIdForRepository(node.tree());
    FontScheme& fn = schemes[schemeId - FONTSCHEME_FIRST];
    if(fn.uniqueIdMap)
    {
        fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = NOFONTID;
    }
    return 0; // Continue iteration.
}

/**
 * @defgroup validateFontUriFlags  Validate Font Uri Flags
 * @ingroup flags
 */
///@{
#define VFUF_ALLOW_ANY_SCHEME       0x1 ///< The scheme of the uri may be of zero-length; signifying "any scheme".
#define VFUF_NO_URN                 0x2 ///< Do not accept a URN.
///@}

/**
 * @param uri       Uri to be validated.
 * @param flags     @ref validateFontUriFlags
 * @param quiet     @c true= Do not output validation remarks to the log.
 *
 * @return  @c true if @a Uri passes validation.
 */
static bool validateUri(de::Uri const& uri, int flags, bool quiet = false)
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
    de::String schemeString;
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        if(flags & VFUF_NO_URN) return false;
        schemeString = uri.path();
    }
    else
    {
        schemeString = uri.scheme();
    }

    fontschemeid_t schemeId = Fonts_ParseScheme(schemeString.toUtf8().constData());
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
static FontRepository::Node* findDirectoryNodeForPath(FontRepository& directory, de::Path const& path)
{
    try
    {
        FontRepository::Node &node = directory.find(path, de::PathTree::NoBranch | de::PathTree::MatchFull);
        return &node;
    }
    catch(FontRepository::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

/// @pre @a uri has already been validated and is well-formed.
static FontRepository::Node* findDirectoryNodeForUri(de::Uri const& uri)
{
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        // This is a URN of the form; urn:schemename:uniqueid
        fontschemeid_t schemeId = Fonts_ParseScheme(uri.pathCStr());
        int uidPos = uri.path().toStringRef().indexOf(':');
        if(uidPos >= 0)
        {
            int uid = uri.path().toStringRef().mid(uidPos + 1/*skip scheme delimiter*/).toInt();
            fontid_t id = Fonts_FontForUniqueId(schemeId, uid);
            if(id != NOFONTID)
            {
                return findDirectoryNodeForBindId(id);
            }
        }
        return 0;
    }

    // This is a URI.
    fontschemeid_t schemeId = Fonts_ParseScheme(uri.schemeCStr());
    de::Path const& path = uri.path();
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
        FontRepository::Node* node = findDirectoryNodeForPath(repository, path);
        if(node)
        {
            return node;
        }
    }

    return 0;
}

static void destroyFont(font_t* font)
{
    DENG_ASSERT(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Delete(font); return;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Delete(font); return;
    default:
        Con_Error("Fonts::destroyFont: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

static void destroyBoundFont(FontRepository::Node& node)
{
    FontRecord* record = (FontRecord*) node.userPointer();
    if(record && record->font)
    {
        destroyFont(record->font); record->font = 0;
    }
}

static void destroyRecord(FontRepository::Node& node)
{
    LOG_AS("Fonts::destroyRecord");
    FontRecord* record = (FontRecord*) node.userPointer();
    if(record)
    {
        if(record->font)
        {
#if _DEBUG
            de::Uri uri = composeUriForDirectoryNode(node);
            LOG_WARNING("destroyRecord: Record for \"%s\" still has Font data!") << uri;
#endif
            destroyFont(record->font);
        }

        unlinkDirectoryNodeFromBindIdMap(node);
        unlinkRecordInUniqueIdMap(node, NULL/*no parameters*/);

        // Detach our user data from this node.
        node.setUserPointer(0);

        M_Free(record);
    }
}

static void destroyFontAndRecord(FontRepository::Node& node)
{
    destroyBoundFont(node);
    destroyRecord(node);
}

boolean Fonts_IsInitialized(void)
{
    return emptyUri != 0;
}

void Fonts_Init(void)
{
    LOG_VERBOSE("Initializing Fonts collection...");

    emptyUri = new de::Uri();

    fontIdMap = 0;
    fontIdMapSize = 0;

    for(int i = 0; i < FONTSCHEME_COUNT; ++i)
    {
        FontScheme& fn = schemes[i];
        fn.repository = new FontRepository();
        fn.uniqueIdBase = 0;
        fn.uniqueIdMapSize = 0;
        fn.uniqueIdMap = NULL;
        fn.uniqueIdMapDirty = false;
    }
}

static int destroyRecordWorker(FontRepository::Node& node, void* parameters)
{
    DENG_UNUSED(parameters);
    destroyRecord(node);
    return 0; // Continue iteration.
}

void Fonts_Shutdown(void)
{
    Fonts_Clear();

    for(int i = 0; i < FONTSCHEME_COUNT; ++i)
    {
        FontScheme& fn = schemes[i];

        if(fn.repository)
        {
            fn.repository->traverse(de::PathTree::NoBranch, NULL, FontRepository::no_hash, destroyRecordWorker);
            delete fn.repository; fn.repository = 0;
        }

        if(!fn.uniqueIdMap) continue;
        M_Free(fn.uniqueIdMap); fn.uniqueIdMap = 0;

        fn.uniqueIdBase = 0;
        fn.uniqueIdMapSize = 0;
        fn.uniqueIdMapDirty = false;
    }

    // Clear the bindId to PathTreeNode LUT.
    if(fontIdMap)
    {
        M_Free(fontIdMap); fontIdMap = 0;
    }
    fontIdMapSize = 0;

    if(emptyUri)
    {
        delete emptyUri; emptyUri = 0;
    }
}

fontschemeid_t Fonts_ParseScheme(char const* str)
{
    static const struct scheme_s {
        const char* name;
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
    char const* end = strchr(str, ':');
    if(end) len = end - str;

    for(n = 0; schemes[n].name; ++n)
    {
        if(len < schemes[n].nameLen) continue;
        if(strnicmp(str, schemes[n].name, len)) continue;
        return schemes[n].id;
    }

    return FS_INVALID; // Unknown.
}

ddstring_t const* Fonts_SchemeName(fontschemeid_t id)
{
    static de::Str const schemes[1 + FONTSCHEME_COUNT] = {
        /* No scheme name */ "",
        /* FS_SYSTEM */      "System",
        /* FS_GAME */        "Game"
    };
    if(VALID_FONTSCHEMEID(id))
        return schemes[1 + (id - FONTSCHEME_FIRST)];
    return schemes[0];
}

uint Fonts_Size(void)
{
    return fontIdMapSize;
}

uint Fonts_Count(fontschemeid_t schemeId)
{
    if(!VALID_FONTSCHEMEID(schemeId) || !Fonts_Size()) return 0;

    FontRepository* directory = repositoryBySchemeId(schemeId);
    if(!directory) return 0;
    return directory->size();
}

void Fonts_Clear(void)
{
    if(!Fonts_Size()) return;

    Fonts_ClearScheme(FS_ANY);
    GL_PruneTextureVariantSpecifications();
}

void Fonts_ClearRuntime(void)
{
    if(!Fonts_Size()) return;

    Fonts_ClearScheme(FS_GAME);
    GL_PruneTextureVariantSpecifications();
}

void Fonts_ClearSystem(void)
{
    if(!Textures_Size()) return;

    Fonts_ClearScheme(FS_SYSTEM);
    GL_PruneTextureVariantSpecifications();
}

static int destroyFontAndRecordWorker(FontRepository::Node& node, void* /*parameters*/)
{
    destroyFontAndRecord(node);
    return 0; // Continue iteration.
}

void Fonts_ClearScheme(fontschemeid_t schemeId)
{
    if(!Fonts_Size()) return;

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
        FontScheme& fn = schemes[i - FONTSCHEME_FIRST];

        if(fn.repository)
        {
            fn.repository->traverse(de::PathTree::NoBranch, NULL, FontRepository::no_hash, destroyFontAndRecordWorker);
            fn.repository->clear();
        }

        fn.uniqueIdMapDirty = true;
    }
}

void Fonts_Release(font_t* font)
{
    /// Stub.
    switch(Font_Type(font))
    {
    case FT_BITMAP:
        BitmapFont_DeleteGLTexture(font);
        break;
    case FT_BITMAPCOMPOSITE:
        BitmapCompositeFont_ReleaseTextures(font);
        break;
    default:
        Con_Error("Fonts::Release: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

static void Fonts_Prepare(font_t* font)
{
    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Prepare(font); break;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Prepare(font); break;
    default:
        Con_Error("Fonts::Prepare: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

int Fonts_Ascent(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Ascent(font);
}

int Fonts_Descent(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Descent(font);
}

int Fonts_Leading(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Leading(font);
}

int Fonts_CharWidth(font_t* font, unsigned char ch)
{
    Fonts_Prepare(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             return BitmapFont_CharWidth(font, ch);
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_CharWidth(font, ch);
    default:
        Con_Error("Fonts::CharWidth: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

int Fonts_CharHeight(font_t* font, unsigned char ch)
{
    Fonts_Prepare(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             return BitmapFont_CharHeight(font, ch);
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_CharHeight(font, ch);
    default:
        Con_Error("Fonts::CharHeight: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

void Fonts_CharSize(font_t* font, Size2Raw* size, unsigned char ch)
{
    if(!size) return;
    size->width  = Fonts_CharWidth(font, ch);
    size->height = Fonts_CharHeight(font, ch);
}

font_t* Fonts_ToFont(fontid_t id)
{
    LOG_AS("Fonts::toFont");
    if(!validFontId(id))
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Failed to locate font for id #%i, returning NULL.") << id;
        }
#endif
        return NULL;
    }

    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node) return NULL;
    return ((FontRecord*) node->userPointer())->font;
}

typedef struct {
    int minId, maxId;
} finduniqueidbounds_params_t;

static int findUniqueIdBounds(FontRepository::Node& node, void* parameters)
{
    FontRecord const* record = (FontRecord*) node.userPointer();
    finduniqueidbounds_params_t* p = (finduniqueidbounds_params_t*)parameters;
    if(record->uniqueId < p->minId) p->minId = record->uniqueId;
    if(record->uniqueId > p->maxId) p->maxId = record->uniqueId;
    return 0; // Continue iteration.
}

static void rebuildUniqueIdMap(fontschemeid_t schemeId)
{
    FontScheme fn = schemes[schemeId - FONTSCHEME_FIRST];
    fontschemeid_t localSchemeId = schemeId;
    finduniqueidbounds_params_t p;
    DENG_ASSERT(VALID_FONTSCHEMEID(schemeId));

    if(!fn.uniqueIdMapDirty) return;

    // Determine the size of the LUT.
    p.minId = DDMAXINT;
    p.maxId = DDMININT;
    iterateDirectory(schemeId, findUniqueIdBounds, (void*)&p);

    if(p.minId > p.maxId)
    {
        // None found.
        fn.uniqueIdBase = 0;
        fn.uniqueIdMapSize = 0;
    }
    else
    {
        fn.uniqueIdBase = p.minId;
        fn.uniqueIdMapSize = p.maxId - p.minId + 1;
    }

    // Construct and (re)populate the LUT.
    fn.uniqueIdMap = (fontid_t*)M_Realloc(fn.uniqueIdMap, sizeof *fn.uniqueIdMap * fn.uniqueIdMapSize);
    if(!fn.uniqueIdMap && fn.uniqueIdMapSize)
        Con_Error("Fonts::rebuildUniqueIdMap: Failed on (re)allocation of %lu bytes resizing the map.", (unsigned long) sizeof *fn.uniqueIdMap * fn.uniqueIdMapSize);

    if(fn.uniqueIdMapSize)
    {
        memset(fn.uniqueIdMap, NOFONTID, sizeof *fn.uniqueIdMap * fn.uniqueIdMapSize);
        iterateDirectory(schemeId, linkRecordInUniqueIdMap, (void*)&localSchemeId);
    }
    fn.uniqueIdMapDirty = false;
}

fontid_t Fonts_FontForUniqueId(fontschemeid_t schemeId, int uniqueId)
{
    if(VALID_FONTSCHEMEID(schemeId))
    {
        FontScheme fn = schemes[schemeId - FONTSCHEME_FIRST];

        rebuildUniqueIdMap(schemeId);
        if(fn.uniqueIdMap && uniqueId >= fn.uniqueIdBase &&
           (unsigned)(uniqueId - fn.uniqueIdBase) <= fn.uniqueIdMapSize)
        {
            return fn.uniqueIdMap[uniqueId - fn.uniqueIdBase];
        }
    }
    return NOFONTID; // Not found.
}

fontid_t Fonts_ResolveUri2(Uri const* _uri, boolean quiet)
{
    LOG_AS("Fonts::resolveUri");

    if(!_uri || !Fonts_Size()) return NOFONTID;

    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);

    if(!validateUri(uri, VFUF_ALLOW_ANY_SCHEME, true /*quiet please*/))
    {
#if _DEBUG
        LOG_WARNING("Uri \"%s\" failed validation, returning NOFONTID.") << uri;
#endif
        return NOFONTID;
    }

    // Perform the search.
    FontRepository::Node* node = findDirectoryNodeForUri(uri);
    if(node)
    {
        // If we have bound a font - it can provide the id.
        FontRecord* record = (FontRecord*) node->userPointer();
        DENG_ASSERT(record);
        if(record->font)
        {
            fontid_t id = Font_PrimaryBind(record->font);
            if(validFontId(id)) return id;
        }
        // Oh well, look it up then.
        return findBindIdForDirectoryNode(*node);
    }

    // Not found.
    if(!quiet)
    {
        LOG_DEBUG("\"%s\" not found, returning NOFONTID.") << uri;
    }
    return NOFONTID;
}

fontid_t Fonts_ResolveUri(Uri const* uri)
{
    return Fonts_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

fontid_t Fonts_ResolveUriCString2(char const* path, boolean quiet)
{
    if(path && path[0])
    {
        de::Uri uri = de::Uri(path, RC_NULL);
        return Fonts_ResolveUri2(reinterpret_cast<uri_s*>(&uri), quiet);
    }
    return NOFONTID;
}

fontid_t Fonts_ResolveUriCString(char const* path)
{
    return Fonts_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

fontid_t Fonts_Declare(Uri* _uri, int uniqueId)//, const Uri* resourcePath)
{
    LOG_AS("Fonts::declare");

    if(!_uri) return NOFONTID;
    de::Uri& uri = reinterpret_cast<de::Uri&>(*_uri);

    // We require a properly formed URI (but not a URN - this is a path).
    if(!validateUri(uri, VFUF_NO_URN, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Font \"%s\", ignoring.")
            << de::NativePath(uri.asText()).pretty();
        return NOFONTID;
    }

    // Have we already created a binding for this?
    fontid_t id;
    FontRecord* record;
    FontRepository::Node* node = findDirectoryNodeForUri(uri);
    if(node)
    {
        record = (FontRecord*) node->userPointer();
        DENG_ASSERT(record);
        id = findBindIdForDirectoryNode(*node);
    }
    else
    {
        /*
         * A new binding.
         */

        record = (FontRecord*) M_Malloc(sizeof *record);
        if(!record) Con_Error("Fonts::Declare: Failed on allocation of %lu bytes for new FontRecord.", (unsigned long) sizeof *record);
        record->font = 0;
        record->uniqueId = uniqueId;

        fontschemeid_t schemeId = Fonts_ParseScheme(uri.schemeCStr());
        FontScheme& fn = schemes[schemeId - FONTSCHEME_FIRST];

        node = &fn.repository->insert(uri.path());
        node->setUserPointer(record);

        // We'll need to rebuild the unique id map too.
        fn.uniqueIdMapDirty = true;

        id = fontIdMapSize + 1; // 1-based identfier
        // Link it into the id map.
        fontIdMap = (FontRepository::Node**) M_Realloc(fontIdMap, sizeof *fontIdMap * ++fontIdMapSize);
        if(!fontIdMap) Con_Error("Fonts::Declare: Failed on (re)allocation of %lu bytes enlarging bindId to PathTreeNode LUT.", (unsigned long) sizeof *fontIdMap * fontIdMapSize);
        fontIdMap[id - 1] = node;
    }

    /**
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
        fontschemeid_t schemeId = schemeIdForRepository(node->tree());
        FontScheme& fn = schemes[schemeId - FONTSCHEME_FIRST];
        fn.uniqueIdMapDirty = true;
    }

    if(releaseFont && record->font)
    {
        // The mapped resource is being replaced, so release any existing Font.
        /// @todo Only release if this Font is bound to only this binding.
        Fonts_Release(record->font);
    }

    return id;
}

static font_t* createFont(fonttype_t type, fontid_t bindId)
{
    switch(type)
    {
    case FT_BITMAP:             return BitmapFont_New(bindId);
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_New(bindId);
    default:
        Con_Error("Fonts::ConstructFont: Unknown font type %i.", (int)type);
        exit(1); // Unreachable.
    }
}

static font_t* createFontFromDef(fontid_t bindId, ded_compositefont_t* def)
{
    DENG_ASSERT(def);

    LOG_AS("Fonts::createFontFromDef");

    font_t* font = createFont(FT_BITMAPCOMPOSITE, bindId);
    BitmapCompositeFont_SetDefinition(font, def);

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;
        try
        {
            QByteArray path = reinterpret_cast<de::Uri&>(*def->charMap[i].path).resolved().toUtf8();
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }

    // Lets try to prepare it right away.
    BitmapCompositeFont_Prepare(font);
    return font;
}

static font_t* createFontFromFile(fontid_t bindId, char const* resourcePath)
{
    DENG_ASSERT(resourcePath);

    font_t* font = createFont(FT_BITMAP, bindId);
    BitmapFont_SetFilePath(font, resourcePath);

    // Lets try and prepare it right away.
    BitmapFont_Prepare(font);
    return font;
}

void Fonts_RebuildFromDef(font_t* font, ded_compositefont_t* def)
{
    LOG_AS("Fonts::rebuildFromDef");

    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildFromDef: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    BitmapCompositeFont_SetDefinition(font, def);
    if(!def) return;

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;

        try
        {
            QByteArray path = reinterpret_cast<de::Uri&>(*def->charMap[i].path).resolved().toUtf8();
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }
}

font_t* Fonts_CreateFromDef(fontid_t id, ded_compositefont_t* def)
{
    LOG_AS("Fonts::createFromDef");

    FontRepository::Node* node = findDirectoryNodeForBindId(id);
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

    FontRecord* record = (FontRecord*) node->userPointer();
    DENG_ASSERT(record);
    if(record->font)
    {
        /// @todo Do not update fonts here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
        font_t* font = record->font;
#if _DEBUG
        de::Uri* uri = reinterpret_cast<de::Uri*>(Fonts_ComposeUri(id));
        LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.") << uri;
        delete uri;
#endif
        Fonts_RebuildFromDef(font, def);
        return font;
    }

    // A new font.
    record->font = createFontFromDef(id, def);
    if(record->font && verbose >= 1)
    {
        de::Uri* uri = reinterpret_cast<de::Uri*>(Fonts_ComposeUri(id));
        LOG_VERBOSE("New font \"%s\"") << uri;
        delete uri;
    }

    return record->font;
}

void Fonts_RebuildFromFile(font_t* font, char const* resourcePath)
{
    if(Font_Type(font) != FT_BITMAP)
    {
        Con_Error("Fonts::RebuildFromFile: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
    BitmapFont_SetFilePath(font, resourcePath);
}

font_t* Fonts_CreateFromFile(fontid_t id, char const* resourcePath)
{
    LOG_AS("Fonts::createFromFile");

    FontRepository::Node* node = findDirectoryNodeForBindId(id);
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

    FontRecord* record = (FontRecord*) node->userPointer();
    DENG_ASSERT(record);
    if(record->font)
    {
        /// @todo Do not update fonts here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
        font_t* font = record->font;
#if _DEBUG
        de::Uri* uri = reinterpret_cast<de::Uri*>(Fonts_ComposeUri(id));
        LOG_DEBUG("A Font with uri \"%s\" already exists, returning existing.") << uri;
        delete uri;
#endif
        Fonts_RebuildFromFile(font, resourcePath);
        return font;
    }

    // A new font.
    record->font = createFontFromFile(id, resourcePath);
    if(record->font && verbose >= 1)
    {
        de::Uri* uri = reinterpret_cast<de::Uri*>(Fonts_ComposeUri(id));
        LOG_VERBOSE("New font \"%s\"") << *uri;
        delete uri;
    }

    return record->font;
}

int Fonts_UniqueId(fontid_t id)
{
    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node) Con_Error("Fonts::UniqueId: Passed invalid fontId #%u.", id);
    return ((FontRecord*) node->userPointer())->uniqueId;
}

fontid_t Fonts_Id(font_t* font)
{
    LOG_AS("Fonts::id");
    if(!font)
    {
#if _DEBUG
        LOG_WARNING("Attempted with invalid reference [%p], returning NOFONTID.") << de::dintptr(font);
#endif
        return NOFONTID;
    }
    return Font_PrimaryBind(font);
}

fontschemeid_t Fonts_Scheme(fontid_t id)
{
    LOG_AS("Fonts::scheme");
    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning FS_ANY.") << id;
        }
#endif
        return FS_ANY;
    }
    return schemeIdForDirectoryNode(*node);
}

AutoStr* Fonts_ComposePath(fontid_t id)
{
    LOG_AS("Fonts::composePath");
    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        LOG_WARNING("Attempted with unbound fontId #%u, returning null-object.") << id;
#endif
        return AutoStr_NewStd();
    }
    QByteArray path = node->path().toUtf8();
    return AutoStr_FromTextStd(path.constData());
}

Uri* Fonts_ComposeUri(fontid_t id)
{
    LOG_AS("Fonts::composeUri");
    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning null-object.") << id;
        }
#endif
        return Uri_New();
    }
    return reinterpret_cast<Uri*>(new de::Uri(composeUriForDirectoryNode(*node)));
}

Uri* Fonts_ComposeUrn(fontid_t id)
{
    LOG_AS("Fonts::composeUrn");

    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning null-object.") << id;
        }
#endif
        return Uri_New();
    }

    FontRecord const* record = (FontRecord*) node->userPointer();
    DENG_ASSERT(record);

    ddstring_t const* schemeName = Fonts_SchemeName(schemeIdForDirectoryNode(*node));

    ddstring_t path; Str_Init(&path);
    Str_Reserve(&path, Str_Length(schemeName) +1/*delimiter*/ + M_NumDigits(DDMAXINT));

    Str_Appendf(&path, "%s:%i", Str_Text(schemeName), record->uniqueId);

    de::Uri* uri = new de::Uri();
    uri->setScheme("urn").setPath(Str_Text(&path));

    Str_Free(&path);
    return reinterpret_cast<Uri*>(uri);
}

typedef struct {
    int (*definedCallback)(font_t* font, void* parameters);
    int (*declaredCallback)(fontid_t id, void* parameters);
    void* parameters;
} iteratedirectoryworker_params_t;

static int iterateDirectoryWorker(FontRepository::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);
    iteratedirectoryworker_params_t* p = (iteratedirectoryworker_params_t*)parameters;

    FontRecord* record = (FontRecord*) node.userPointer();
    DENG_ASSERT(record);
    if(p->definedCallback)
    {
        if(record->font)
        {
            return p->definedCallback(record->font, p->parameters);
        }
    }
    else
    {
        fontid_t id = NOFONTID;

        // If we have bound a font it can provide the id.
        if(record->font)
        {
            id = Font_PrimaryBind(record->font);
        }

        // Otherwise look it up.
        if(!validFontId(id))
        {
            id = findBindIdForDirectoryNode(node);
        }

        // Sanity check.
        DENG_ASSERT(validFontId(id));

        return p->declaredCallback(id, p->parameters);
    }
    return 0; // Continue iteration.
}

static int iterateDirectory(fontschemeid_t schemeId,
    int (*callback)(FontRepository::Node& node, void* parameters), void* parameters)
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
        FontRepository* directory = repositoryBySchemeId(fontschemeid_t(i));
        if(!directory) continue;

        result = directory->traverse(de::PathTree::NoBranch, NULL, FontRepository::no_hash, callback, parameters);
        if(result) break;
    }
    return result;
}

int Fonts_Iterate2(fontschemeid_t schemeId,
    int (*callback)(font_t* font, void* parameters), void* parameters)
{
    if(!callback) return 0;

    iteratedirectoryworker_params_t p;
    p.definedCallback = callback;
    p.declaredCallback = NULL;
    p.parameters = parameters;
    return iterateDirectory(schemeId, iterateDirectoryWorker, &p);
}

int Fonts_Iterate(fontschemeid_t schemeId,
    int (*callback)(font_t* font, void* parameters))
{
    return Fonts_Iterate2(schemeId, callback, NULL/*no parameters*/);
}

int Fonts_IterateDeclared2(fontschemeid_t schemeId,
    int (*callback)(fontid_t fontId, void* parameters), void* parameters)
{
    if(!callback) return 0;

    iteratedirectoryworker_params_t p;
    p.declaredCallback = callback;
    p.definedCallback = NULL;
    p.parameters = parameters;
    return iterateDirectory(schemeId, iterateDirectoryWorker, &p);
}

int Fonts_IterateDeclared(fontschemeid_t schemeId,
    int (*callback)(fontid_t fontId, void* parameters))
{
    return Fonts_IterateDeclared2(schemeId, callback, NULL/*no parameters*/);
}

font_t* R_CreateFontFromFile(Uri* uri, char const* resourcePath)
{
    LOG_AS("R_CreateFontFromFile");

    if(!uri || !resourcePath || !resourcePath[0] || !F_Access(resourcePath))
    {
        LOG_WARNING("Invalid Uri or ResourcePath reference, ignoring.");
        return 0;
    }

    fontschemeid_t schemeId = Fonts_ParseScheme(Str_Text(Uri_Scheme(uri)));
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        AutoStr* path = Uri_ToString(uri);
        LOG_WARNING("Invalid font scheme in Font Uri \"%s\", ignoring.") << Str_Text(path);
        return 0;
    }

    int uniqueId = Fonts_Count(schemeId) + 1; // 1-based index.
    fontid_t fontId = Fonts_Declare(uri, uniqueId/*, resourcePath*/);
    if(fontId == NOFONTID) return 0; // Invalid URI?

    // Have we already encountered this name?
    font_t* font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromFile(font, resourcePath);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromFile(fontId, resourcePath);
        if(!font)
        {
            AutoStr* path = Uri_ToString(uri);
            LOG_WARNING("Failed defining new Font for \"%s\", ignoring.") << Str_Text(path);
        }
    }
    return font;
}

font_t* R_CreateFontFromDef(ded_compositefont_t* def)
{
    LOG_AS("R_CreateFontFromDef");

    if(!def || !def->uri)
    {
        LOG_WARNING("Invalid Definition or Uri reference, ignoring.");
        return 0;
    }
    de::Uri const &uri = reinterpret_cast<de::Uri const &>(*def->uri);

    fontschemeid_t schemeId = Fonts_ParseScheme(uri.schemeCStr());
    if(!VALID_FONTSCHEMEID(schemeId))
    {
        LOG_WARNING("Invalid URI scheme in font definition \"%s\", ignoring.")
            << de::NativePath(uri.asText()).pretty();
        return NULL;
    }

    int uniqueId = Fonts_Count(schemeId) + 1; // 1-based index.
    fontid_t fontId = Fonts_Declare(def->uri, uniqueId);
    if(fontId == NOFONTID) return 0; // Invalid URI?

    // Have we already encountered this name?
    font_t* font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromDef(font, def);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromDef(fontId, def);
        if(!font)
        {
            LOG_WARNING("Failed defining new Font for \"%s\", ignoring.")
                << de::NativePath(uri.asText()).pretty();
        }
    }
    return font;
}

static void printFontOverview(FontRepository::Node& node, bool printSchemeName)
{
    FontRecord* record = (FontRecord*) node.userPointer();
    fontid_t fontId = findBindIdForDirectoryNode(node);
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Fonts_Size()));
    Uri* uri = record->font? Fonts_ComposeUri(fontId) : Uri_New();
    Str const* path = (printSchemeName? Uri_ToString(uri) : Uri_Path(uri));
    font_t* font = record->font;

    Con_FPrintf(!font? CPF_LIGHT : CPF_WHITE,
        "%-*s %*u %s", printSchemeName? 22 : 14, F_PrettyPath(Str_Text(path)),
        numUidDigits, fontId, !font? "unknown" : Font_Type(font) == FT_BITMAP? "bitmap" : "bitmap_composite");

    if(font && Font_IsPrepared(font))
    {
        Con_Printf(" (ascent:%i, descent:%i, leading:%i", Fonts_Ascent(font), Fonts_Descent(font), Fonts_Leading(font));
        if(Font_Type(font) == FT_BITMAP && BitmapFont_GLTextureName(font))
        {
            Con_Printf(", texWidth:%i, texHeight:%i", BitmapFont_TextureWidth(font), BitmapFont_TextureHeight(font));
        }
        Con_Printf(")\n");
    }
    else
    {
        Con_Printf("\n");
    }

    Uri_Delete(uri);
}

/**
 * @todo A horridly inefficent algorithm. This should be implemented in PathTree
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the font search/listing console commands so
 * is not hugely important right now.
 */
typedef struct {
    de::String like;
    int idx;
    FontRepository::Node** storage;
} collectdirectorynodeworker_params_t;

static int collectDirectoryNodeWorker(FontRepository::Node& node, void* parameters)
{
    collectdirectorynodeworker_params_t* p = (collectdirectorynodeworker_params_t*)parameters;

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

static FontRepository::Node** collectDirectoryNodes(fontschemeid_t schemeId,
    de::String like, uint* count, FontRepository::Node** storage)
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
        FontRepository* fontDirectory = repositoryBySchemeId(fontschemeid_t(i));
        if(!fontDirectory) continue;

        fontDirectory->traverse(de::PathTree::NoBranch | de::PathTree::MatchFull,
                                NULL, FontRepository::no_hash, collectDirectoryNodeWorker, (void*)&p);
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

    storage = (FontRepository::Node**) M_Malloc(sizeof *storage * (p.idx+1));
    if(!storage)
        Con_Error("collectFonts: Failed on allocation of %lu bytes for new collection.", (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectDirectoryNodes(schemeId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(void const* a, void const* b)
{
    // Decode paths before determining a lexicographical delta.
    FontRepository::Node const& nodeA = **(FontRepository::Node const**)a;
    FontRepository::Node const& nodeB = **(FontRepository::Node const**)b;
    QByteArray pathAUtf8 = nodeA.path().toUtf8();
    QByteArray pathBUtf8 = nodeB.path().toUtf8();
    AutoStr* pathA = Str_PercentDecode(AutoStr_FromTextStd(pathAUtf8.constData()));
    AutoStr* pathB = Str_PercentDecode(AutoStr_FromTextStd(pathBUtf8.constData()));
    return qstricmp(Str_Text(pathA), Str_Text(pathB));
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
static size_t printFonts3(fontschemeid_t schemeId, char const* like, int flags)
{
    bool const printScheme = !(flags & PFF_TRANSFORM_PATH_NO_SCHEME);
    uint idx, count = 0;
    FontRepository::Node** foundFonts = collectDirectoryNodes(schemeId, like, &count, NULL);
    FontRepository::Node** iter;
    int numFoundDigits, numUidDigits;

    if(!foundFonts) return 0;

    if(!printScheme)
        Con_FPrintf(CPF_YELLOW, "Known fonts in scheme '%s'", Str_Text(Fonts_SchemeName(schemeId)));
    else // Any scheme.
        Con_FPrintf(CPF_YELLOW, "Known fonts");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits((int)count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits((int)Fonts_Size()));
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
    qsort(foundFonts, (size_t)count, sizeof *foundFonts, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundFonts; *iter; ++iter)
    {
        FontRepository::Node* node = *iter;
        Con_Printf(" %*u: ", numFoundDigits, idx++);
        printFontOverview(*node, printScheme);
    }

    M_Free(foundFonts);
    return count;
}

static void printFonts2(fontschemeid_t schemeId, const char* like, int flags)
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
        int i;
        for(i = FONTSCHEME_FIRST; i <= FONTSCHEME_LAST; ++i)
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

static void printFonts(fontschemeid_t schemeId, const char* like)
{
    printFonts2(schemeId, like, DEFAULT_PRINTFONTFLAGS);
}

static int clearDefinitionLinks(font_t* font, void* parameters)
{
    DENG_UNUSED(parameters);
    if(Font_Type(font) == FT_BITMAPCOMPOSITE)
    {
        BitmapCompositeFont_SetDefinition(font, NULL);
    }
    return 0; // Continue iteration.
}

void Fonts_ClearDefinitionLinks(void)
{
    if(!Fonts_Size()) return;
    Fonts_Iterate(FS_ANY, clearDefinitionLinks);
}

static int releaseFontTextures(font_t* font, void* parameters)
{
    DENG_UNUSED(parameters);
    switch(Font_Type(font))
    {
    case FT_BITMAP:
        BitmapFont_DeleteGLTexture(font);
        break;
    case FT_BITMAPCOMPOSITE:
        BitmapCompositeFont_ReleaseTextures(font);
        break;
    default:
        Con_Error("Fonts::ReleaseFontTextures: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
    return 0; // Continue iteration.
}

void Fonts_ReleaseTexturesByScheme(fontschemeid_t schemeId)
{
    if(novideo || isDedicated) return;
    if(!Fonts_Size()) return;
    Fonts_Iterate(schemeId, releaseFontTextures);
}

void Fonts_ReleaseTextures(void)
{
    Fonts_ReleaseTexturesByScheme(FS_ANY);
}

void Fonts_ReleaseRuntimeTextures(void)
{
    Fonts_ReleaseTexturesByScheme(FS_GAME);
}

void Fonts_ReleaseSystemTextures(void)
{
    Fonts_ReleaseTexturesByScheme(FS_SYSTEM);
}

ddstring_t** Fonts_CollectNames(int* rCount)
{
    uint count = 0;
    ddstring_t** list = NULL;

    FontRepository::Node** foundFonts = collectDirectoryNodes(FS_ANY, "", &count, 0);
    if(foundFonts)
    {
        qsort(foundFonts, (size_t)count, sizeof *foundFonts, composeAndCompareDirectoryNodePaths);

        list = (ddstring_t**) M_Malloc(sizeof *list * (count+1));
        if(!list)
            Con_Error("Fonts::CollectNames: Failed on allocation of %lu bytes for name list.", sizeof *list * (count+1));

        int idx = 0;
        for(FontRepository::Node** iter = foundFonts; *iter; ++iter)
        {
            FontRepository::Node& node = **iter;
            QByteArray path = node.path().toUtf8();
            list[idx++] = Str_Set(Str_NewStd(), path.constData());
        }
        list[idx] = NULL; // Terminate.
    }

    if(rCount) *rCount = count;
    return list;
}

D_CMD(ListFonts)
{
    DENG_UNUSED(src);

    fontschemeid_t schemeId = FS_ANY;
    char const* like = NULL;
    de::Uri uri;

    if(!Fonts_Size())
    {
        Con_Message("There are currently no fonts defined/loaded.\n");
        return true;
    }

    // "listfonts [scheme] [name]"
    if(argc > 2)
    {
        uri.setScheme(argv[1]).setPath(argv[2]);

        schemeId = Fonts_ParseScheme(uri.schemeCStr());
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
            schemeId = Fonts_ParseScheme(uri.schemeCStr());
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
            schemeId = Fonts_ParseScheme(uri.pathCStr());

            if(!VALID_FONTSCHEMEID(schemeId))
            {
                schemeId = FS_ANY;
                like = argv[1];
            }
        }
    }

    printFonts(schemeId, like);

    return true;
}

#if _DEBUG
D_CMD(PrintFontStats)
{
    DENG_UNUSED(src); DENG_UNUSED(argv); DENG_UNUSED(argc);

    if(!Fonts_Size())
    {
        Con_Message("There are currently no fonts defined/loaded.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Font Statistics:\n");
    for(uint i = uint(FONTSCHEME_FIRST); i <= uint(FONTSCHEME_LAST); ++i)
    {
        FontRepository* fontDirectory = repositoryBySchemeId(fontschemeid_t(i));
        uint size;

        if(!fontDirectory) continue;

        size = fontDirectory->size();
        Con_Printf("Scheme: %s (%u %s)\n", Str_Text(Fonts_SchemeName(fontschemeid_t(i))), size, size==1? "font":"fonts");
        fontDirectory->debugPrintHashDistribution();
        fontDirectory->debugPrint();
    }
    return true;
}
#endif
