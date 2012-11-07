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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"

#include "gl_texmanager.h"
#include "pathtree.h"

#include "font.h"
#include "bitmapfont.h"
#include "fonts.h"

typedef de::PathTree FontRepository;

/**
 * FontRecord. Stores metadata for a unique Font in the collection.
 */
struct FontRecord
{
    /// Namespace-unique identifier chosen by the owner of the collection.
    int uniqueId;

    /// The defined font instance (if any).
    font_t* font;
};

struct FontNamespace
{
    /// FontRepository contains mappings between names and unique font records.
    FontRepository* repository;

    /// LUT which translates namespace-unique-ids to their associated fontid_t (if any).
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

static int iterateDirectory(fontnamespaceid_t namespaceId,
    int (*callback)(FontRepository::Node& node, void* parameters), void* parameters);

static de::Uri* emptyUri;

// LUT which translates fontid_t to PathTreeNode*. Index with fontid_t-1
static uint fontIdMapSize;
static FontRepository::Node** fontIdMap;

// Font namespace set.
static FontNamespace namespaces[FONTNAMESPACE_COUNT];

void Fonts_Register(void)
{
    C_CMD("listfonts",  NULL, ListFonts)
#if _DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

static inline FontRepository* repositoryByNamespaceId(fontnamespaceid_t id)
{
    DENG_ASSERT(VALID_FONTNAMESPACEID(id));
    return namespaces[id - FONTNAMESPACE_FIRST].repository;
}

static fontnamespaceid_t namespaceIdForRepository(FontRepository& pt)
{
    for(uint i = uint(FONTNAMESPACE_FIRST); i <= uint(FONTNAMESPACE_LAST); ++i)
    {
        uint idx = i - FONTNAMESPACE_FIRST;
        if(namespaces[idx].repository == &pt) return fontnamespaceid_t(i);
    }
    // Only reachable if attempting to find the id for a Font that is not
    // in the collection, or the collection has not yet been initialized.
    Con_Error("Fonts::namespaceIdForRepository: Failed to determine id for directory %p.", (void*)&pt);
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

static inline fontnamespaceid_t namespaceIdForDirectoryNode(FontRepository::Node const& node)
{
    return namespaceIdForRepository(node.tree());
}

/// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
static de::Uri composeUriForDirectoryNode(FontRepository::Node const& node)
{
    Str const* namespaceName = Fonts_NamespaceName(namespaceIdForDirectoryNode(node));
    AutoStr* path = node.composePath(AutoStr_NewStd(), NULL, FONTS_PATH_DELIMITER);
    return de::Uri(Str_Text(path), RC_NULL).setScheme(Str_Text(namespaceName));
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
    fontnamespaceid_t const namespaceId = namespaceIdForRepository(node.tree());
    FontNamespace& fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];
    fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = findBindIdForDirectoryNode(node);
    return 0; // Continue iteration.
}

/// @pre uniqueIdMap is large enough if initialized!
static int unlinkRecordInUniqueIdMap(FontRepository::Node& node, void* parameters)
{
    DENG_UNUSED(parameters);

    FontRecord const* record = (FontRecord*) node.userPointer();
    fontnamespaceid_t const namespaceId = namespaceIdForRepository(node.tree());
    FontNamespace& fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];
    if(fn.uniqueIdMap)
    {
        fn.uniqueIdMap[record->uniqueId - fn.uniqueIdBase] = NOFONTID;
    }
    return 0; // Continue iteration.
}

/**
 * @defgroup validateFontUriFlags  Validate Font Uri Flags
 */
///@{
#define VFUF_ALLOW_NAMESPACE_ANY    0x1 ///< The namespace of the uri may be of zero-length; signifying "any namespace".
#define VFUF_NO_URN                 0x2 ///< Do not accept a URN.
///@}

/**
 * @param uri       Uri to be validated.
 * @param flags     @see validateFontUriFlags
 * @param quiet     @c true= Do not output validation remarks to the log.
 *
 * @return  @c true if @a Uri passes validation.
 */
static bool validateUri(de::Uri const& uri, int flags, bool quiet = false)
{
    LOG_AS("Fonts::validateUri");

    if(Str_IsEmpty(uri.path()))
    {
        if(!quiet)
        {
            LOG_MSG("Invalid path in Font uri \"%s\".") << uri;
        }
        return false;
    }

    // If this is a URN we extract the namespace from the path.
    ddstring_t const* namespaceString;
    if(!Str_CompareIgnoreCase(uri.scheme(), "urn"))
    {
        if(flags & VFUF_NO_URN) return false;
        namespaceString = uri.path();
    }
    else
    {
        namespaceString = uri.scheme();
    }

    fontnamespaceid_t namespaceId = Fonts_ParseNamespace(Str_Text(namespaceString));
    if(!((flags & VFUF_ALLOW_NAMESPACE_ANY) && namespaceId == FN_ANY) &&
       !VALID_FONTNAMESPACEID(namespaceId))
    {
        if(!quiet)
        {
            LOG_MSG("Unknown namespace in Font uri \"%s\".") << uri;
        }
        return false;
    }

    return true;
}

/**
 * Given a directory and path, search the Fonts collection for a match.
 *
 * @param directory  Namespace-specific PathTree to search in.
 * @param path  Path of the font to search for.
 * @return  Found DirectoryNode else @c NULL
 */
static FontRepository::Node* findDirectoryNodeForPath(FontRepository& directory, char const* path)
{
    try
    {
        FontRepository::Node& node = directory.find(PCF_NO_BRANCH | PCF_MATCH_FULL, path, FONTS_PATH_DELIMITER);
        return &node;
    }
    catch(FontRepository::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

/// @pre @a uri has already been validated and is well-formed.
static FontRepository::Node* findDirectoryNodeForUri(de::Uri const& uri)
{
    if(!Str_CompareIgnoreCase(uri.scheme(), "urn"))
    {
        // This is a URN of the form; urn:namespacename:uniqueid
        fontnamespaceid_t namespaceId = Fonts_ParseNamespace(Str_Text(uri.path()));

        char* uid = strchr(Str_Text(uri.path()), ':');
        if(uid)
        {
            fontid_t id = Fonts_FontForUniqueId(namespaceId, strtol(uid + 1/*skip namespace delimiter*/, 0, 0));
            if(id != NOFONTID)
            {
                return findDirectoryNodeForBindId(id);
            }
        }
        return NULL;
    }

    // This is a URI.
    fontnamespaceid_t namespaceId = Fonts_ParseNamespace(Str_Text(uri.scheme()));
    char const* path = Str_Text(uri.path());
    if(namespaceId != FN_ANY)
    {
        // Caller wants a font in a specific namespace.
        return findDirectoryNodeForPath(*repositoryByNamespaceId(namespaceId), path);
    }

    // Caller does not care which namespace.
    // Check for the font in these namespaces in priority order.
    static const fontnamespaceid_t order[] = {
        FN_GAME, FN_SYSTEM, FN_ANY
    };

    FontRepository::Node* node = NULL;
    int n = 0;
    do
    {
        node = findDirectoryNodeForPath(*repositoryByNamespaceId(order[n]), path);
    } while(!node && order[++n] != FN_ANY);
    return node;
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

    for(int i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        FontNamespace& fn = namespaces[i];
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

    for(int i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        FontNamespace& fn = namespaces[i];

        if(fn.repository)
        {
            fn.repository->iterate(PCF_NO_BRANCH, NULL, PATHTREE_NOHASH, destroyRecordWorker);
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

fontnamespaceid_t Fonts_ParseNamespace(char const* str)
{
    static const struct namespace_s {
        const char* name;
        size_t nameLen;
        fontnamespaceid_t id;
    } namespaces[FONTNAMESPACE_COUNT+1] = {
        // Ordered according to a best guess of occurance frequency.
        { FN_GAME_NAME,     sizeof(FN_GAME_NAME)   - 1, FN_GAME   },
        { FN_SYSTEM_NAME,   sizeof(FN_SYSTEM_NAME) - 1, FN_SYSTEM },
        { NULL,             0,                          FN_ANY    }
    };
    size_t len, n;

    // Special case: zero-length string means "any namespace".
    if(!str || 0 == (len = strlen(str))) return FN_ANY;

    // Stop comparing characters at the first occurance of ':'
    char const* end = strchr(str, ':');
    if(end) len = end - str;

    for(n = 0; namespaces[n].name; ++n)
    {
        if(len < namespaces[n].nameLen) continue;
        if(strnicmp(str, namespaces[n].name, len)) continue;
        return namespaces[n].id;
    }

    return FN_INVALID; // Unknown.
}

ddstring_t const* Fonts_NamespaceName(fontnamespaceid_t id)
{
    static de::Str const namespaces[1 + FONTNAMESPACE_COUNT] = {
        /* No namespace name */ "",
        /* FN_SYSTEM */         FN_SYSTEM_NAME,
        /* FN_SYSTEM */         FN_GAME_NAME
    };
    if(VALID_FONTNAMESPACEID(id))
        return namespaces[1 + (id - FONTNAMESPACE_FIRST)];
    return namespaces[0];
}

uint Fonts_Size(void)
{
    return fontIdMapSize;
}

uint Fonts_Count(fontnamespaceid_t namespaceId)
{
    if(!VALID_FONTNAMESPACEID(namespaceId) || !Fonts_Size()) return 0;

    FontRepository* directory = repositoryByNamespaceId(namespaceId);
    if(!directory) return 0;
    return directory->size();
}

void Fonts_Clear(void)
{
    if(!Fonts_Size()) return;

    Fonts_ClearNamespace(FN_ANY);
    GL_PruneTextureVariantSpecifications();
}

void Fonts_ClearRuntime(void)
{
    if(!Fonts_Size()) return;

    Fonts_ClearNamespace(FN_GAME);
    GL_PruneTextureVariantSpecifications();
}

void Fonts_ClearSystem(void)
{
    if(!Textures_Size()) return;

    Fonts_ClearNamespace(FN_SYSTEM);
    GL_PruneTextureVariantSpecifications();
}

static int destroyFontAndRecordWorker(FontRepository::Node& node, void* /*parameters*/)
{
    destroyFontAndRecord(node);
    return 0; // Continue iteration.
}

void Fonts_ClearNamespace(fontnamespaceid_t namespaceId)
{
    if(!Fonts_Size()) return;

    fontnamespaceid_t from, to;
    if(namespaceId == FN_ANY)
    {
        from = FONTNAMESPACE_FIRST;
        to   = FONTNAMESPACE_LAST;
    }
    else if(VALID_FONTNAMESPACEID(namespaceId))
    {
        from = to = namespaceId;
    }
    else
    {
        Con_Error("Fonts::ClearNamespace: Invalid font namespace %i.", (int) namespaceId);
        exit(1); // Unreachable.
    }

    for(uint i = uint(from); i <= uint(to); ++i)
    {
        FontNamespace& fn = namespaces[i - FONTNAMESPACE_FIRST];

        if(fn.repository)
        {
            fn.repository->iterate(PCF_NO_BRANCH, NULL, PATHTREE_NOHASH, destroyFontAndRecordWorker);
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

static void rebuildUniqueIdMap(fontnamespaceid_t namespaceId)
{
    FontNamespace fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];
    fontnamespaceid_t localNamespaceId = namespaceId;
    finduniqueidbounds_params_t p;
    DENG_ASSERT(VALID_FONTNAMESPACEID(namespaceId));

    if(!fn.uniqueIdMapDirty) return;

    // Determine the size of the LUT.
    p.minId = DDMAXINT;
    p.maxId = DDMININT;
    iterateDirectory(namespaceId, findUniqueIdBounds, (void*)&p);

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
        iterateDirectory(namespaceId, linkRecordInUniqueIdMap, (void*)&localNamespaceId);
    }
    fn.uniqueIdMapDirty = false;
}

fontid_t Fonts_FontForUniqueId(fontnamespaceid_t namespaceId, int uniqueId)
{
    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        FontNamespace fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];

        rebuildUniqueIdMap(namespaceId);
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

    if(!validateUri(uri, VFUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
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
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        fontid_t id = Fonts_ResolveUri2(uri, quiet);
        Uri_Delete(uri);
        return id;
    }
    return NOFONTID;
}

fontid_t Fonts_ResolveUriCString(char const* path)
{
    return Fonts_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

fontid_t Fonts_Declare(Uri const* _uri, int uniqueId)//, const Uri* resourcePath)
{
    DENG_ASSERT(_uri);

    LOG_AS("Fonts::declare");

    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);

    // We require a properly formed uri (but not a urn - this is a path).
    if(!validateUri(uri, VFUF_NO_URN, (verbose >= 1)))
    {
        LOG_WARNING("Failed creating Font \"%s\", ignoring.") << uri;
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

        // Ensure the path is lowercase.
        int pathLen = Str_Length(uri.path());
        ddstring_t path; Str_Reserve(Str_Init(&path), pathLen);
        for(int i = 0; i < pathLen; ++i)
        {
            Str_AppendChar(&path, tolower(Str_At(uri.path(), i)));
        }

        record = (FontRecord*) M_Malloc(sizeof *record);
        if(!record) Con_Error("Fonts::Declare: Failed on allocation of %lu bytes for new FontRecord.", (unsigned long) sizeof *record);
        record->font = 0;
        record->uniqueId = uniqueId;

        fontnamespaceid_t namespaceId = Fonts_ParseNamespace(Str_Text(uri.scheme()));
        FontNamespace& fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];

        node = fn.repository->insert(Str_Text(&path), FONTS_PATH_DELIMITER);
        node->setUserPointer(record);

        // We'll need to rebuild the unique id map too.
        fn.uniqueIdMapDirty = true;

        id = fontIdMapSize + 1; // 1-based identfier
        // Link it into the id map.
        fontIdMap = (FontRepository::Node**) M_Realloc(fontIdMap, sizeof *fontIdMap * ++fontIdMapSize);
        if(!fontIdMap) Con_Error("Fonts::Declare: Failed on (re)allocation of %lu bytes enlarging bindId to PathTreeNode LUT.", (unsigned long) sizeof *fontIdMap * fontIdMapSize);
        fontIdMap[id - 1] = node;

        Str_Free(&path);
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
        fontnamespaceid_t namespaceId = namespaceIdForRepository(node->tree());
        FontNamespace& fn = namespaces[namespaceId - FONTNAMESPACE_FIRST];
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

    font_t* font = createFont(FT_BITMAPCOMPOSITE, bindId);
    BitmapCompositeFont_SetDefinition(font, def);

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t const* path;
        if(!def->charMap[i].path) continue;

        path = Uri_ResolvedConst(def->charMap[i].path);
        if(path)
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
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
    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildFromDef: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    BitmapCompositeFont_SetDefinition(font, def);
    if(!def) return;

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t const* path;
        if(!def->charMap[i].path) continue;

        path = Uri_ResolvedConst(def->charMap[i].path);
        if(path)
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
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
        LOG_VERBOSE("New font \"%s\"") << uri;
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

fontnamespaceid_t Fonts_Namespace(fontid_t id)
{
    LOG_AS("Fonts::namespace");
    FontRepository::Node* node = findDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            LOG_WARNING("Attempted with unbound fontId #%u, returning FN_ANY.") << id;
        }
#endif
        return FN_ANY;
    }
    return namespaceIdForDirectoryNode(*node);
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
    return node->composePath(AutoStr_NewStd(), NULL, FONTS_PATH_DELIMITER);
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

    ddstring_t const* namespaceName = Fonts_NamespaceName(namespaceIdForDirectoryNode(*node));

    ddstring_t path; Str_Init(&path);
    Str_Reserve(&path, Str_Length(namespaceName) +1/*delimiter*/ + M_NumDigits(DDMAXINT));

    Str_Appendf(&path, "%s:%i", Str_Text(namespaceName), record->uniqueId);

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

static int iterateDirectory(fontnamespaceid_t namespaceId,
    int (*callback)(FontRepository::Node& node, void* parameters), void* parameters)
{
    fontnamespaceid_t from, to;
    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        from = to = namespaceId;
    }
    else
    {
        from = FONTNAMESPACE_FIRST;
        to   = FONTNAMESPACE_LAST;
    }

    int result = 0;
    for(uint i = uint(from); i <= uint(to); ++i)
    {
        FontRepository* directory = repositoryByNamespaceId(fontnamespaceid_t(i));
        if(!directory) continue;

        result = directory->iterate(PCF_NO_BRANCH, NULL, PATHTREE_NOHASH, callback, parameters);
        if(result) break;
    }
    return result;
}

int Fonts_Iterate2(fontnamespaceid_t namespaceId,
    int (*callback)(font_t* font, void* parameters), void* parameters)
{
    if(!callback) return 0;

    iteratedirectoryworker_params_t p;
    p.definedCallback = callback;
    p.declaredCallback = NULL;
    p.parameters = parameters;
    return iterateDirectory(namespaceId, iterateDirectoryWorker, &p);
}

int Fonts_Iterate(fontnamespaceid_t namespaceId,
    int (*callback)(font_t* font, void* parameters))
{
    return Fonts_Iterate2(namespaceId, callback, NULL/*no parameters*/);
}

int Fonts_IterateDeclared2(fontnamespaceid_t namespaceId,
    int (*callback)(fontid_t fontId, void* parameters), void* parameters)
{
    if(!callback) return 0;

    iteratedirectoryworker_params_t p;
    p.declaredCallback = callback;
    p.definedCallback = NULL;
    p.parameters = parameters;
    return iterateDirectory(namespaceId, iterateDirectoryWorker, &p);
}

int Fonts_IterateDeclared(fontnamespaceid_t namespaceId,
    int (*callback)(fontid_t fontId, void* parameters))
{
    return Fonts_IterateDeclared2(namespaceId, callback, NULL/*no parameters*/);
}

static void printFontOverview(FontRepository::Node& node, bool printNamespace)
{
    FontRecord* record = (FontRecord*) node.userPointer();
    fontid_t fontId = findBindIdForDirectoryNode(node);
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Fonts_Size()));
    Uri* uri = record->font? Fonts_ComposeUri(fontId) : Uri_New();
    Str const* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));
    font_t* font = record->font;

    Con_FPrintf(!font? CPF_LIGHT : CPF_WHITE,
        "%-*s %*u %s", printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
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
    char delimiter;
    const char* like;
    int idx;
    FontRepository::Node** storage;
} collectdirectorynodeworker_params_t;

static int collectDirectoryNodeWorker(FontRepository::Node& node, void* parameters)
{
    collectdirectorynodeworker_params_t* p = (collectdirectorynodeworker_params_t*)parameters;

    if(p->like && p->like[0])
    {
        AutoStr* path = node.composePath(AutoStr_NewStd(), NULL, p->delimiter);
        int delta = strnicmp(Str_Text(path), p->like, strlen(p->like));
        if(delta) return 0; // Continue iteration.
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

static FontRepository::Node** collectDirectoryNodes(fontnamespaceid_t namespaceId,
    char const * like, uint* count, FontRepository::Node** storage)
{
    fontnamespaceid_t fromId, toId;
    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        // Only consider fonts in this namespace.
        fromId = toId = namespaceId;
    }
    else
    {
        // Consider fonts in any namespace.
        fromId = FONTNAMESPACE_FIRST;
        toId   = FONTNAMESPACE_LAST;
    }

    collectdirectorynodeworker_params_t p;
    p.delimiter = FONTS_PATH_DELIMITER;
    p.like = like;
    p.idx = 0;
    p.storage = storage;

    for(uint i = uint(fromId); i <= uint(toId); ++i)
    {
        FontRepository* fontDirectory = repositoryByNamespaceId(fontnamespaceid_t(i));
        if(!fontDirectory) continue;

        fontDirectory->iterate(PCF_NO_BRANCH | PCF_MATCH_FULL, NULL, PATHTREE_NOHASH, collectDirectoryNodeWorker, (void*)&p);
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
    return collectDirectoryNodes(namespaceId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(void const* a, void const* b)
{
    // Decode paths before determining a lexicographical delta.
    FontRepository::Node const& nodeA = **(FontRepository::Node const**)a;
    FontRepository::Node const& nodeB = **(FontRepository::Node const**)b;
    AutoStr* pathA = Str_PercentDecode(nodeA.composePath(AutoStr_NewStd(), NULL, FONTS_PATH_DELIMITER));
    AutoStr* pathB = Str_PercentDecode(nodeB.composePath(AutoStr_NewStd(), NULL, FONTS_PATH_DELIMITER));
    return stricmp(Str_Text(pathA), Str_Text(pathB));
}

/**
 * @defgroup printFontFlags  Print Font Flags
 */
///@{
#define PFF_TRANSFORM_PATH_NO_NAMESPACE 0x1 /// Do not print the namespace.
///@}

#define DEFAULT_PRINTFONTFLAGS          0

/**
 * @param flags  @see printFontFlags
 */
static size_t printFonts3(fontnamespaceid_t namespaceId, char const* like, int flags)
{
    bool const  printNamespace = !(flags & PFF_TRANSFORM_PATH_NO_NAMESPACE);
    uint idx, count = 0;
    FontRepository::Node** foundFonts = collectDirectoryNodes(namespaceId, like, &count, NULL);
    FontRepository::Node** iter;
    int numFoundDigits, numUidDigits;

    if(!foundFonts) return 0;

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known fonts in namespace '%s'", Str_Text(Fonts_NamespaceName(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known fonts");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits((int)count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits((int)Fonts_Size()));
    Con_Printf(" %*s: %-*s %*s type", numFoundDigits, "idx",
               printNamespace? 22 : 14, printNamespace? "namespace:path" : "path",
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
        printFontOverview(*node, printNamespace);
    }

    M_Free(foundFonts);
    return count;
}

static void printFonts2(fontnamespaceid_t namespaceId, const char* like, int flags)
{
    size_t printTotal = 0;
    // Do we care which namespace?
    if(namespaceId == FN_ANY && like && like[0])
    {
        printTotal = printFonts3(namespaceId, like, flags & ~PFF_TRANSFORM_PATH_NO_NAMESPACE);
        Con_PrintRuler();
    }
    // Only one namespace to print?
    else if(VALID_FONTNAMESPACEID(namespaceId))
    {
        printTotal = printFonts3(namespaceId, like, flags | PFF_TRANSFORM_PATH_NO_NAMESPACE);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each namespace separately.
        int i;
        for(i = FONTNAMESPACE_FIRST; i <= FONTNAMESPACE_LAST; ++i)
        {
            size_t printed = printFonts3((fontnamespaceid_t)i, like, flags | PFF_TRANSFORM_PATH_NO_NAMESPACE);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Font" : "Fonts");
}

static void printFonts(fontnamespaceid_t namespaceId, const char* like)
{
    printFonts2(namespaceId, like, DEFAULT_PRINTFONTFLAGS);
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
    Fonts_Iterate(FN_ANY, clearDefinitionLinks);
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

void Fonts_ReleaseTexturesByNamespace(fontnamespaceid_t namespaceId)
{
    if(novideo || isDedicated) return;
    if(!Fonts_Size()) return;
    Fonts_Iterate(namespaceId, releaseFontTextures);
}

void Fonts_ReleaseTextures(void)
{
    Fonts_ReleaseTexturesByNamespace(FN_ANY);
}

void Fonts_ReleaseRuntimeTextures(void)
{
    Fonts_ReleaseTexturesByNamespace(FN_GAME);
}

void Fonts_ReleaseSystemTextures(void)
{
    Fonts_ReleaseTexturesByNamespace(FN_SYSTEM);
}

ddstring_t** Fonts_CollectNames(int* rCount)
{
    uint count = 0;
    ddstring_t** list = NULL;

    FontRepository::Node** foundFonts = collectDirectoryNodes(FN_ANY, NULL, &count, 0);
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
            list[idx++] = Str_FromAutoStr(node.composePath(AutoStr_NewStd(), NULL, FONTS_PATH_DELIMITER));
        }
        list[idx] = NULL; // Terminate.
    }

    if(rCount) *rCount = count;
    return list;
}

D_CMD(ListFonts)
{
    DENG_UNUSED(src);

    fontnamespaceid_t namespaceId = FN_ANY;
    const char* like = NULL;
    Uri* uri = NULL;

    if(!Fonts_Size())
    {
        Con_Message("There are currently no fonts defined/loaded.\n");
        return true;
    }

    // "listfonts [namespace] [name]"
    if(argc > 2)
    {
        uri = Uri_New();
        Uri_SetScheme(uri, argv[1]);
        Uri_SetPath(uri, argv[2]);

        namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(uri)));
        if(!VALID_FONTNAMESPACEID(namespaceId))
        {
            Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(uri)));
            Uri_Delete(uri);
            return false;
        }
        like = Str_Text(Uri_Path(uri));
    }
    // "listfonts [namespace:name]" i.e., a partial Uri
    else if(argc > 1)
    {
        uri = Uri_NewWithPath2(argv[1], RC_NULL);
        if(!Str_IsEmpty(Uri_Scheme(uri)))
        {
            namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(uri)));
            if(!VALID_FONTNAMESPACEID(namespaceId))
            {
                Con_Printf("Invalid namespace \"%s\".\n", Str_Text(Uri_Scheme(uri)));
                Uri_Delete(uri);
                return false;
            }

            if(!Str_IsEmpty(Uri_Path(uri)))
                like = Str_Text(Uri_Path(uri));
        }
        else
        {
            namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Path(uri)));

            if(!VALID_FONTNAMESPACEID(namespaceId))
            {
                namespaceId = FN_ANY;
                like = argv[1];
            }
        }
    }

    printFonts(namespaceId, like);

    if(uri) Uri_Delete(uri);
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
    for(uint i = uint(FONTNAMESPACE_FIRST); i <= uint(FONTNAMESPACE_LAST); ++i)
    {
        FontRepository* fontDirectory = repositoryByNamespaceId(fontnamespaceid_t(i));
        uint size;

        if(!fontDirectory) continue;

        size = fontDirectory->size();
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(Fonts_NamespaceName(fontnamespaceid_t(i))), size, size==1? "font":"fonts");
        FontRepository::debugPrintHashDistribution(*fontDirectory);
        FontRepository::debugPrint(*fontDirectory, FONTS_PATH_DELIMITER);
    }
    return true;
}
#endif
