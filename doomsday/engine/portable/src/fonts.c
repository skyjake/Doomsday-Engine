/**\file fonts.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
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
#include "pathdirectory.h"

#include "font.h"
#include "bitmapfont.h"
#include "fonts.h"

/**
 * FontRecord. Stores metadata for a unique Font in the collection.
 */
typedef struct {
    /// Namespace-unique identifier chosen by the owner of the collection.
    int uniqueId;

    /// Path to the data resource which contains/wraps the loadable font data.
    //Uri* resourcePath;

    /// The defined font instance (if any).
    font_t* font;
} fontrecord_t;

typedef struct {
    /// PathDirectory containing mappings between names and unique font records.
    PathDirectory* directory;

    /// LUT which translates namespace-unique-ids to their associated fontid_t (if any).
    /// Index with uniqueId - uniqueIdBase.
    int uniqueIdBase;
    boolean uniqueIdMapDirty;
    uint uniqueIdMapSize;
    fontid_t* uniqueIdMap;
} fontnamespace_t;

D_CMD(ListFonts);
#if _DEBUG
D_CMD(PrintFontStats);
#endif

static int iterateDirectory(fontnamespaceid_t namespaceId,
    int (*callback)(PathDirectoryNode* node, void* parameters), void* parameters);

static Uri* emptyUri;

// LUT which translates fontid_t to PathDirectoryNode*. Index with fontid_t-1
static uint fontIdMapSize;
static PathDirectoryNode** fontIdMap;

// Font namespace set.
static fontnamespace_t namespaces[FONTNAMESPACE_COUNT];

void Fonts_Register(void)
{
    C_CMD("listfonts",  NULL, ListFonts)
#if _DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

static __inline PathDirectory* getDirectoryForNamespaceId(fontnamespaceid_t id)
{
    assert(VALID_FONTNAMESPACEID(id));
    return namespaces[id-FONTNAMESPACE_FIRST].directory;
}

static fontnamespaceid_t namespaceIdForDirectory(PathDirectory* pd)
{
    fontnamespaceid_t id;
    assert(pd);

    for(id = FONTNAMESPACE_FIRST; id <= FONTNAMESPACE_LAST; ++id)
    {
        if(namespaces[id-FONTNAMESPACE_FIRST].directory == pd) return id;
    }
    // Only reachable if attempting to find the id for a Font that is not
    // in the collection, or the collection has not yet been initialized.
    Con_Error("Fonts::namespaceIdForDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
}

static __inline boolean validFontId(fontid_t id)
{
    return (id != NOFONTID && id <= fontIdMapSize);
}

static PathDirectoryNode* getDirectoryNodeForBindId(fontid_t id)
{
    if(!validFontId(id)) return NULL;
    return fontIdMap[id-1/*1-based index*/];
}

static fontid_t findBindIdForDirectoryNode(const PathDirectoryNode* node)
{
    uint i;
    /// @todo Optimize (Low priority): Do not use a linear search.
    for(i = 0; i < fontIdMapSize; ++i)
    {
        if(fontIdMap[i] == node)
            return (fontid_t)(i+1); // 1-based index.
    }
    return NOFONTID; // Not linked.
}

static __inline fontnamespaceid_t namespaceIdForDirectoryNode(const PathDirectoryNode* node)
{
    return namespaceIdForDirectory(PathDirectoryNode_Directory(node));
}

/// @return  Newly composed path for @a node. Must be released with Str_Delete()
static __inline AutoStr* composePathForDirectoryNode(const PathDirectoryNode* node, char delimiter)
{
    return PathDirectoryNode_ComposePath2(node, AutoStr_NewStd(), NULL, delimiter);
}

/// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
static Uri* composeUriForDirectoryNode(const PathDirectoryNode* node)
{
    const Str* namespaceName = Fonts_NamespaceName(namespaceIdForDirectoryNode(node));
    AutoStr* path = composePathForDirectoryNode(node, FONTS_PATH_DELIMITER);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(namespaceName));
    return uri;
}

/// @pre fontIdMap has been initialized and is large enough!
static void unlinkDirectoryNodeFromBindIdMap(const PathDirectoryNode* node)
{
    fontid_t id = findBindIdForDirectoryNode(node);
    if(!validFontId(id)) return; // Not linked.
    fontIdMap[id - 1/*1-based index*/] = NULL;
}

/// @pre uniqueIdMap has been initialized and is large enough!
static int linkRecordInUniqueIdMap(PathDirectoryNode* node, void* parameters)
{
    const fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    const fontnamespaceid_t namespaceId = namespaceIdForDirectory(PathDirectoryNode_Directory(node));
    fontnamespace_t* fn = &namespaces[namespaceId-FONTNAMESPACE_FIRST];
    fn->uniqueIdMap[record->uniqueId - fn->uniqueIdBase] = findBindIdForDirectoryNode(node);
    return 0; // Continue iteration.
}

/// @pre uniqueIdMap is large enough if initialized!
static int unlinkRecordInUniqueIdMap(PathDirectoryNode* node, void* parameters)
{
    const fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    const fontnamespaceid_t namespaceId = namespaceIdForDirectory(PathDirectoryNode_Directory(node));
    fontnamespace_t* fn = &namespaces[namespaceId-FONTNAMESPACE_FIRST];
    if(fn->uniqueIdMap)
    {
        fn->uniqueIdMap[record->uniqueId - fn->uniqueIdBase] = NOFONTID;
    }
    return 0; // Continue iteration.
}

/**
 * @defgroup validateFontUriFlags  Validate Font Uri Flags
 * @{
 */
#define VFUF_ALLOW_NAMESPACE_ANY 0x1 /// The namespace of the uri may be of zero-length; signifying "any namespace".
#define VFUF_NO_URN             0x2 /// Do not accept a URN.
/**@}*/

/**
 * @param uri  Uri to be validated.
 * @param flags  @see validateFontUriFlags
 * @param quiet  @c true= Do not output validation remarks to the log.
 * @return  @c true if @a Uri passes validation.
 */
static boolean validateFontUri2(const Uri* uri, int flags, boolean quiet)
{
    fontnamespaceid_t namespaceId;
    const ddstring_t* namespaceString;

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
        if(!quiet)
        {
            AutoStr* uriStr = Uri_ToString(uri);
            Con_Message("Invalid path '%s' in Font uri \"%s\".\n", Str_Text(Uri_Path(uri)), Str_Text(uriStr));
        }
        return false;
    }

    // If this is a URN we extract the namespace from the path.
    if(!Str_CompareIgnoreCase(Uri_Scheme(uri), "urn"))
    {
        if(flags & VFUF_NO_URN) return false;
        namespaceString = Uri_Path(uri);
    }
    else
    {
        namespaceString = Uri_Scheme(uri);
    }

    namespaceId = Fonts_ParseNamespace(Str_Text(namespaceString));
    if(!((flags & VFUF_ALLOW_NAMESPACE_ANY) && namespaceId == FN_ANY) &&
       !VALID_FONTNAMESPACEID(namespaceId))
    {
        if(!quiet)
        {
            AutoStr* uriStr = Uri_ToString(uri);
            Con_Message("Unknown namespace in Font uri \"%s\".\n", Str_Text(uriStr));
        }
        return false;
    }

    return true;
}

static boolean validateFontUri(const Uri* uri, int flags)
{
    return validateFontUri2(uri, flags, false);
}

/**
 * Given a directory and path, search the Fonts collection for a match.
 *
 * @param directory  Namespace-specific PathDirectory to search in.
 * @param path  Path of the font to search for.
 * @return  Found DirectoryNode else @c NULL
 */
static PathDirectoryNode* findDirectoryNodeForPath(PathDirectory* directory, const char* path)
{
    return PathDirectory_Find2(directory, PCF_NO_BRANCH|PCF_MATCH_FULL, path, FONTS_PATH_DELIMITER);
}

/// @pre @a uri has already been validated and is well-formed.
static PathDirectoryNode* findDirectoryNodeForUri(const Uri* uri)
{
    fontnamespaceid_t namespaceId;
    PathDirectoryNode* node = NULL;
    const char* path;

    if(!Str_CompareIgnoreCase(Uri_Scheme(uri), "urn"))
    {
        // This is a URN of the form; urn:namespacename:uniqueid
        char* uid;
        namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Path(uri)));
        uid = strchr(Str_Text(Uri_Path(uri)), ':');
        if(uid)
        {
            fontid_t id = Fonts_FontForUniqueId(namespaceId,
                strtol(uid +1/*skip namespace delimiter*/, 0, 0));
            if(id != NOFONTID)
            {
                return getDirectoryNodeForBindId(id);
            }
        }
        return NULL;
    }

    // This is a URI.
    namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(uri)));
    path = Str_Text(Uri_Path(uri));
    if(namespaceId != FN_ANY)
    {
        // Caller wants a font in a specific namespace.
        node = findDirectoryNodeForPath(getDirectoryForNamespaceId(namespaceId), path);
    }
    else
    {
        // Caller does not care which namespace.
        // Check for the font in these namespaces in priority order.
        static const fontnamespaceid_t order[] = {
            FN_GAME, FN_SYSTEM, FN_ANY
        };
        int n = 0;
        do
        {
            node = findDirectoryNodeForPath(getDirectoryForNamespaceId(order[n]), path);
        } while(!node && order[++n] != FN_ANY);
    }
    return node;
}

static void destroyFont(font_t* font)
{
    assert(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Delete(font); return;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Delete(font); return;
    default:
        Con_Error("Fonts::destroyFont: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

static int destroyBoundFont(PathDirectoryNode* node, void* parameters)
{
    fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    if(record && record->font)
    {
        destroyFont(record->font), record->font = NULL;
    }
    return 0; // Continue iteration.
}

static int destroyRecord(PathDirectoryNode* node, void* parameters)
{
    fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);

    DENG_UNUSED(parameters);

    if(record)
    {
        if(record->font)
        {
#if _DEBUG
            Uri* uri = composeUriForDirectoryNode(node);
            AutoStr* path = Uri_ToString(uri);
            Con_Message("Warning:Fonts::destroyRecord: Record for \"%s\" still has Font data!\n", Str_Text(path));
            Uri_Delete(uri);
#endif
            destroyFont(record->font);
        }

        /*if(record->resourcePath)
        {
            Uri_Delete(record->resourcePath);
        }*/

        unlinkDirectoryNodeFromBindIdMap(node);
        unlinkRecordInUniqueIdMap(node, NULL/*no parameters*/);

        // Detach our user data from this node.
        PathDirectoryNode_SetUserData(node, 0);

        free(record);
    }
    return 0; // Continue iteration.
}

static int destroyFontAndRecord(PathDirectoryNode* node, void* parameters)
{
    destroyBoundFont(node, parameters);
    destroyRecord(node, parameters);
    return 0; // Continue iteration.
}

boolean Fonts_IsInitialized(void)
{
    return emptyUri != 0;
}

void Fonts_Init(void)
{
    int i;

    VERBOSE( Con_Message("Initializing Fonts collection...\n") )

    emptyUri = Uri_New();

    fontIdMap = NULL;
    fontIdMapSize = 0;

    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        fontnamespace_t* fn = &namespaces[i];
        fn->directory = PathDirectory_New();
        fn->uniqueIdBase = 0;
        fn->uniqueIdMapSize = 0;
        fn->uniqueIdMap = NULL;
        fn->uniqueIdMapDirty = false;
    }
}

void Fonts_Shutdown(void)
{
    int i;

    Fonts_Clear();

    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        fontnamespace_t* fn = &namespaces[i];

        if(fn->directory)
        {
            PathDirectory_Iterate(fn->directory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, destroyRecord);
            PathDirectory_Delete(fn->directory);
            fn->directory = 0;
        }

        if(!fn->uniqueIdMap) continue;
        free(fn->uniqueIdMap); fn->uniqueIdMap = 0;

        fn->uniqueIdBase = 0;
        fn->uniqueIdMapSize = 0;
        fn->uniqueIdMapDirty = false;
    }

    // Clear the bindId to PathDirectoryNode LUT.
    if(fontIdMap)
    {
        free(fontIdMap);
        fontIdMap = NULL;
    }
    fontIdMapSize = 0;

    if(emptyUri)
    {
        Uri_Delete(emptyUri);
        emptyUri = NULL;
    }
}

fontnamespaceid_t Fonts_ParseNamespace(const char* str)
{
    static const struct namespace_s {
        const char* name;
        size_t nameLen;
        fontnamespaceid_t id;
    } namespaces[FONTNAMESPACE_COUNT+1] = {
        // Ordered according to a best guess of occurance frequency.
        { FN_GAME_NAME, sizeof(FN_GAME_NAME)-1, FN_GAME },
        { FN_SYSTEM_NAME, sizeof(FN_SYSTEM_NAME)-1, FN_SYSTEM },
        { NULL }
    };
    const char* end;
    size_t len, n;

    // Special case: zero-length string means "any namespace".
    if(!str || 0 == (len = strlen(str))) return FN_ANY;

    // Stop comparing characters at the first occurance of ':'
    end = strchr(str, ':');
    if(end) len = end - str;

    for(n = 0; namespaces[n].name; ++n)
    {
        if(len < namespaces[n].nameLen) continue;
        if(strnicmp(str, namespaces[n].name, len)) continue;
        return namespaces[n].id;
    }

    return FN_INVALID; // Unknown.
}

const ddstring_t* Fonts_NamespaceName(fontnamespaceid_t id)
{
    static const ddstring_t namespaces[1+FONTNAMESPACE_COUNT] = {
        /* No namespace name */ { "" },
        /* FN_SYSTEM */         { FN_SYSTEM_NAME },
        /* FN_SYSTEM */         { FN_GAME_NAME }
    };
    if(VALID_FONTNAMESPACEID(id))
        return namespaces + 1 + (id - FONTNAMESPACE_FIRST);
    return namespaces + 0;
}

uint Fonts_Size(void)
{
    return fontIdMapSize;
}

uint Fonts_Count(fontnamespaceid_t namespaceId)
{
    PathDirectory* directory;
    if(!VALID_FONTNAMESPACEID(namespaceId) || !Fonts_Size()) return 0;
    directory = getDirectoryForNamespaceId(namespaceId);
    if(!directory) return 0;
    return PathDirectory_Size(directory);
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

void Fonts_ClearNamespace(fontnamespaceid_t namespaceId)
{
    fontnamespaceid_t from, to, iter;
    if(!Fonts_Size()) return;

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

    for(iter = from; iter <= to; ++iter)
    {
        fontnamespace_t* fn = &namespaces[iter - FONTNAMESPACE_FIRST];

        if(fn->directory)
        {
            PathDirectory_Iterate(fn->directory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, destroyFontAndRecord);
            PathDirectory_Clear(fn->directory);
        }

        fn->uniqueIdMapDirty = true;
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
    PathDirectoryNode* node;
    if(!validFontId(id))
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            Con_Message("Warning:Fonts::ToFont: Failed to locate font for id #%i, returning NULL.\n", id);
        }
#endif
        return NULL;
    }
    node = getDirectoryNodeForBindId(id);
    if(!node) return NULL;
    return ((fontrecord_t*)PathDirectoryNode_UserData(node))->font;
}

typedef struct {
    int minId, maxId;
} finduniqueidbounds_params_t;

static int findUniqueIdBounds(PathDirectoryNode* node, void* parameters)
{
    const fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    finduniqueidbounds_params_t* p = (finduniqueidbounds_params_t*)parameters;
    if(record->uniqueId < p->minId) p->minId = record->uniqueId;
    if(record->uniqueId > p->maxId) p->maxId = record->uniqueId;
    return 0; // Continue iteration.
}

static void rebuildUniqueIdMap(fontnamespaceid_t namespaceId)
{
    fontnamespace_t* fn = &namespaces[namespaceId - FONTNAMESPACE_FIRST];
    fontnamespaceid_t localNamespaceId = namespaceId;
    finduniqueidbounds_params_t p;
    assert(VALID_FONTNAMESPACEID(namespaceId));

    if(!fn->uniqueIdMapDirty) return;

    // Determine the size of the LUT.
    p.minId = DDMAXINT;
    p.maxId = DDMININT;
    iterateDirectory(namespaceId, findUniqueIdBounds, (void*)&p);

    if(p.minId > p.maxId)
    {
        // None found.
        fn->uniqueIdBase = 0;
        fn->uniqueIdMapSize = 0;
    }
    else
    {
        fn->uniqueIdBase = p.minId;
        fn->uniqueIdMapSize = p.maxId - p.minId + 1;
    }

    // Construct and (re)populate the LUT.
    fn->uniqueIdMap = (fontid_t*)realloc(fn->uniqueIdMap, sizeof *fn->uniqueIdMap * fn->uniqueIdMapSize);
    if(!fn->uniqueIdMap && fn->uniqueIdMapSize)
        Con_Error("Fonts::rebuildUniqueIdMap: Failed on (re)allocation of %lu bytes resizing the map.", (unsigned long) sizeof *fn->uniqueIdMap * fn->uniqueIdMapSize);

    if(fn->uniqueIdMapSize)
    {
        memset(fn->uniqueIdMap, NOFONTID, sizeof *fn->uniqueIdMap * fn->uniqueIdMapSize);
        iterateDirectory(namespaceId, linkRecordInUniqueIdMap, (void*)&localNamespaceId);
    }
    fn->uniqueIdMapDirty = false;
}

fontid_t Fonts_FontForUniqueId(fontnamespaceid_t namespaceId, int uniqueId)
{
    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        fontnamespace_t* fn = &namespaces[namespaceId - FONTNAMESPACE_FIRST];

        rebuildUniqueIdMap(namespaceId);
        if(fn->uniqueIdMap && uniqueId >= fn->uniqueIdBase &&
           (unsigned)(uniqueId - fn->uniqueIdBase) <= fn->uniqueIdMapSize)
        {
            return fn->uniqueIdMap[uniqueId - fn->uniqueIdBase];
        }
    }
    return NOFONTID; // Not found.
}

fontid_t Fonts_ResolveUri2(const Uri* uri, boolean quiet)
{
    PathDirectoryNode* node;
    if(!uri || !Fonts_Size()) return NOFONTID;

    if(!validateFontUri2(uri, VFUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        AutoStr* uriStr = Uri_ToString(uri);
        Con_Message("Warning:Fonts::ResolveUri: Uri \"%s\" failed to validate, returning NULL.\n", Str_Text(uriStr));
#endif
        return NOFONTID;
    }

    // Perform the search.
    node = findDirectoryNodeForUri(uri);
    if(node)
    {
        // If we have bound a font - it can provide the id.
        fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
        assert(record);
        if(record->font)
        {
            fontid_t id = Font_PrimaryBind(record->font);
            if(validFontId(id)) return id;
        }
        // Oh well, look it up then.
        return findBindIdForDirectoryNode(node);
    }

    // Not found.
    if(!quiet)
    {
        AutoStr* path = Uri_ToString(uri);
        Con_Message("Fonts::ResolveUri: \"%s\" not found!\n", Str_Text(path));
    }
    return NOFONTID;
}

fontid_t Fonts_ResolveUri(const Uri* uri)
{
    return Fonts_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

fontid_t Fonts_ResolveUriCString2(const char* path, boolean quiet)
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

fontid_t Fonts_ResolveUriCString(const char* path)
{
    return Fonts_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

fontid_t Fonts_Declare(const Uri* uri, int uniqueId)//, const Uri* resourcePath)
{
    PathDirectoryNode* node;
    fontrecord_t* record;
    fontid_t id;
    boolean releaseFont = false;
    assert(uri);

    // We require a properly formed uri (but not a urn - this is a path).
    if(!validateFontUri2(uri, VFUF_NO_URN, (verbose >= 1)))
    {
        AutoStr* uriStr = Uri_ToString(uri);
        Con_Message("Warning: Failed creating Font \"%s\", ignoring.\n", Str_Text(uriStr));
        return NOFONTID;
    }

    // Have we already created a binding for this?
    node = findDirectoryNodeForUri(uri);
    if(node)
    {
        record = (fontrecord_t*)PathDirectoryNode_UserData(node);
        assert(record);
        id = findBindIdForDirectoryNode(node);
    }
    else
    {
        // A new binding.
        fontnamespaceid_t namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(uri)));
        fontnamespace_t* fn = &namespaces[namespaceId - FONTNAMESPACE_FIRST];
        ddstring_t path;
        int i, pathLen;

        // Ensure the path is lowercase.
        pathLen = Str_Length(Uri_Path(uri));
        Str_Init(&path);
        Str_Reserve(&path, pathLen);
        for(i = 0; i < pathLen; ++i)
        {
            Str_AppendChar(&path, tolower(Str_At(Uri_Path(uri), i)));
        }

        record = (fontrecord_t*)malloc(sizeof *record);
        if(!record)
            Con_Error("Fonts::Declare: Failed on allocation of %lu bytes for new FontRecord.", (unsigned long) sizeof *record);
        record->font = NULL;
        //record->resourcePath = NULL;
        record->uniqueId = uniqueId;

        node = PathDirectory_Insert2(fn->directory, Str_Text(&path), FONTS_PATH_DELIMITER);
        PathDirectoryNode_SetUserData(node, record);

        // We'll need to rebuild the unique id map too.
        fn->uniqueIdMapDirty = true;

        id = fontIdMapSize + 1; // 1-based identfier
        // Link it into the id map.
        fontIdMap = (PathDirectoryNode**) realloc(fontIdMap, sizeof *fontIdMap * ++fontIdMapSize);
        if(!fontIdMap)
            Con_Error("Fonts::Declare: Failed on (re)allocation of %lu bytes enlarging bindId to PathDirectoryNode LUT.", (unsigned long) sizeof *fontIdMap * fontIdMapSize);
        fontIdMap[id - 1] = node;

        Str_Free(&path);
    }

    /**
     * (Re)configure this binding.
     */

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release fonts when they change.
    if(record->uniqueId != uniqueId)
    {
        fontnamespaceid_t namespaceId = namespaceIdForDirectory(PathDirectoryNode_Directory(node));
        fontnamespace_t* fn = &namespaces[namespaceId - FONTNAMESPACE_FIRST];

        record->uniqueId = uniqueId;
        releaseFont = true;

        // We'll need to rebuild the id map too.
        fn->uniqueIdMapDirty = true;
    }

    /*if(resourcePath)
    {
        if(!record->resourcePath)
        {
            record->resourcePath = Uri_NewCopy(resourcePath);
            releaseFont = true;
        }
        else if(!Uri_Equality(record->resourcePath, resourcePath))
        {
            Uri_Copy(record->resourcePath, resourcePath);
            releaseFont = true;
        }
    }
    else if(record->resourcePath)
    {
        Uri_Delete(record->resourcePath);
        record->resourcePath = NULL;
        releaseFont = true;
    }*/

    if(releaseFont && record->font)
    {
        // The mapped resource is being replaced, so release any existing Font.
        /// \todo Only release if this Font is bound to only this binding.
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
    font_t* font = createFont(FT_BITMAPCOMPOSITE, bindId);
    int i;
    assert(def);
    BitmapCompositeFont_SetDefinition(font, def);

    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path) continue;

        path = Uri_Resolved(def->charMap[i].path);
        if(path)
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }

    // Lets try to prepare it right away.
    BitmapCompositeFont_Prepare(font);
    return font;
}

static font_t* createFontFromFile(fontid_t bindId, const char* resourcePath)
{
    font_t* font = createFont(FT_BITMAP, bindId);
    assert(resourcePath);
    BitmapFont_SetFilePath(font, resourcePath);

    // Lets try and prepare it right away.
    BitmapFont_Prepare(font);
    return font;
}

void Fonts_RebuildFromDef(font_t* font, ded_compositefont_t* def)
{
    int i;

    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildFromDef: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    BitmapCompositeFont_SetDefinition(font, def);
    if(!def) return;

    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path) continue;

        path = Uri_Resolved(def->charMap[i].path);
        if(path)
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }
}

font_t* Fonts_CreateFromDef(fontid_t id, ded_compositefont_t* def)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    fontrecord_t* record;
    if(!node)
    {
        Con_Message("Warning: Failed creating Font #%u (invalid id), ignoring.\n", id);
        return NULL;
    }

    if(!def)
    {
        Con_Message("Warning: Failed creating Font #%u (def=NULL), ignoring.\n", id);
        return NULL;
    }

    record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    assert(record);
    if(record->font)
    {
        /// @todo Do not update fonts here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
        font_t* font = record->font;
#if _DEBUG
        Uri* uri = Fonts_ComposeUri(id);
        AutoStr* path = Uri_ToString(uri);
        Con_Message("Warning:Fonts::CreateFromDef: A Font with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Uri_Delete(uri);
#endif
        Fonts_RebuildFromDef(font, def);
        return font;
    }

    // A new font.
    record->font = createFontFromDef(id, def);
    if(record->font && verbose >= 1)
    {
        Uri* uri = Fonts_ComposeUri(id);
        AutoStr* path = Uri_ToString(uri);
        Con_Message("New font \"%s\"\n", Str_Text(path));
        Uri_Delete(uri);
    }

    return record->font;
}

void Fonts_RebuildFromFile(font_t* font, const char* resourcePath)
{
    if(Font_Type(font) != FT_BITMAP)
    {
        Con_Error("Fonts::RebuildFromFile: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
    BitmapFont_SetFilePath(font, resourcePath);
}

font_t* Fonts_CreateFromFile(fontid_t id, const char* resourcePath)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    fontrecord_t* record;
    if(!node)
    {
        Con_Message("Warning: Failed creating Font #%u (invalid id), ignoring.\n", id);
        return NULL;
    }

    if(!resourcePath || !resourcePath[0])
    {
        Con_Message("Warning: Failed creating Font #%u (resourcePath=NULL), ignoring.\n", id);
        return NULL;
    }

    record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    assert(record);
    if(record->font)
    {
        /// @todo Do not update fonts here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
        font_t* font = record->font;
#if _DEBUG
        Uri* uri = Fonts_ComposeUri(id);
        AutoStr* path = Uri_ToString(uri);
        Con_Message("Warning:Fonts::CreateFromFile: A Font with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Uri_Delete(uri);
#endif
        Fonts_RebuildFromFile(font, resourcePath);
        return font;
    }

    // A new font.
    record->font = createFontFromFile(id, resourcePath);
    if(record->font && verbose >= 1)
    {
        Uri* uri = Fonts_ComposeUri(id);
        AutoStr* path = Uri_ToString(uri);
        Con_Message("New font \"%s\"\n", Str_Text(path));
        Uri_Delete(uri);
    }

    return record->font;
}

int Fonts_UniqueId(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    if(!node)
        Con_Error("Fonts::UniqueId: Passed invalid fontId #%u.", id);
    return ((fontrecord_t*)PathDirectoryNode_UserData(node))->uniqueId;
}

/*const Uri* Fonts_ResourcePath(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    if(!node) return emptyUri;
    return ((fontrecord_t*)PathDirectoryNode_UserData(node))->resourcePath;
}*/

fontid_t Fonts_Id(font_t* font)
{
    if(!font)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::Id: Attempted with invalid reference [%p], returning invalid id.\n", (void*)font);
#endif
        return NOFONTID;
    }
    return Font_PrimaryBind(font);
}

fontnamespaceid_t Fonts_Namespace(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            Con_Message("Warning:Fonts::Namespace: Attempted with unbound fontId #%u, returning null-object.\n", id);
        }
#endif
        return FN_ANY;
    }
    return namespaceIdForDirectoryNode(node);
}

AutoStr* Fonts_ComposePath(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::ComposePath: Attempted with unbound fontId #%u, returning null-object.\n", id);
#endif
        return AutoStr_NewStd();
    }
    return composePathForDirectoryNode(node, FONTS_PATH_DELIMITER);
}

Uri* Fonts_ComposeUri(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            Con_Message("Warning:Fonts::ComposeUri: Attempted with unbound fontId #%u, returning null-object.\n", id);
        }
#endif
        return Uri_New();
    }
    return composeUriForDirectoryNode(node);
}

Uri* Fonts_ComposeUrn(fontid_t id)
{
    PathDirectoryNode* node = getDirectoryNodeForBindId(id);
    const ddstring_t* namespaceName;
    const fontrecord_t* record;
    Uri* uri = Uri_New();
    ddstring_t path;
    if(!node)
    {
#if _DEBUG
        if(id != NOFONTID)
        {
            Con_Message("Warning:Fonts::ComposeUrn: Attempted with unbound fontId #%u, returning null-object.\n", id);
        }
#endif
        return uri;
    }
    record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    assert(record);
    namespaceName = Fonts_NamespaceName(namespaceIdForDirectoryNode(node));
    Str_Init(&path);
    Str_Reserve(&path, Str_Length(namespaceName) +1/*delimiter*/ +M_NumDigits(DDMAXINT));
    Str_Appendf(&path, "%s:%i", Str_Text(namespaceName), record->uniqueId);
    Uri_SetScheme(uri, "urn");
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);
    return uri;
}

typedef struct {
    int (*definedCallback)(font_t* font, void* parameters);
    int (*declaredCallback)(fontid_t id, void* parameters);
    void* parameters;
} iteratedirectoryworker_params_t;

static int iterateDirectoryWorker(PathDirectoryNode* node, void* parameters)
{
    iteratedirectoryworker_params_t* p = (iteratedirectoryworker_params_t*)parameters;
    fontrecord_t* record = (fontrecord_t*)PathDirectoryNode_UserData(node);
    assert(node && p && record);
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
            id = Font_PrimaryBind(record->font);
        // Otherwise look it up.
        if(!validFontId(id))
            id = findBindIdForDirectoryNode(node);
        // Sanity check.
        assert(validFontId(id));

        return p->declaredCallback(id, p->parameters);
    }
    return 0; // Continue iteration.
}

static int iterateDirectory(fontnamespaceid_t namespaceId,
    int (*callback)(PathDirectoryNode* node, void* parameters), void* parameters)
{
    fontnamespaceid_t from, to, iter;
    int result = 0;

    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        from = to = namespaceId;
    }
    else
    {
        from = FONTNAMESPACE_FIRST;
        to   = FONTNAMESPACE_LAST;
    }

    for(iter = from; iter <= to; ++iter)
    {
        PathDirectory* directory = getDirectoryForNamespaceId(iter);
        if(!directory) continue;

        result = PathDirectory_Iterate2(directory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, callback, parameters);
        if(result) break;
    }
    return result;
}

int Fonts_Iterate2(fontnamespaceid_t namespaceId,
    int (*callback)(font_t* font, void* parameters), void* parameters)
{
    iteratedirectoryworker_params_t p;
    if(!callback) return 0;
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
    iteratedirectoryworker_params_t p;
    if(!callback) return 0;
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

static void printFontOverview(PathDirectoryNode* node, boolean printNamespace)
{
    fontrecord_t* record = (fontrecord_t*) PathDirectoryNode_UserData(node);
    fontid_t fontId = findBindIdForDirectoryNode(node);
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Fonts_Size()));
    Uri* uri = record->font? Fonts_ComposeUri(fontId) : Uri_New();
    const Str* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));
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
 * \todo A horridly inefficent algorithm. This should be implemented in PathDirectory
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the font search/listing console commands so
 * is not hugely important right now.
 */
typedef struct {
    char delimiter;
    const char* like;
    int idx;
    PathDirectoryNode** storage;
} collectdirectorynodeworker_params_t;

static int collectDirectoryNodeWorker(PathDirectoryNode* node, void* parameters)
{
    collectdirectorynodeworker_params_t* p = (collectdirectorynodeworker_params_t*)parameters;

    if(p->like && p->like[0])
    {
        AutoStr* path = composePathForDirectoryNode(node, p->delimiter);
        int delta = strnicmp(Str_Text(path), p->like, strlen(p->like));
        if(delta) return 0; // Continue iteration.
    }

    if(p->storage)
    {
        p->storage[p->idx++] = node;
    }
    else
    {
        ++p->idx;
    }

    return 0; // Continue iteration.
}

static PathDirectoryNode** collectDirectoryNodes(fontnamespaceid_t namespaceId,
    const char* like, uint* count, PathDirectoryNode** storage)
{
    collectdirectorynodeworker_params_t p;
    fontnamespaceid_t fromId, toId, iterId;

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

    p.delimiter = FONTS_PATH_DELIMITER;
    p.like = like;
    p.idx = 0;
    p.storage = storage;
    for(iterId  = fromId; iterId <= toId; ++iterId)
    {
        PathDirectory* fontDirectory = getDirectoryForNamespaceId(iterId);
        if(!fontDirectory) continue;

        PathDirectory_Iterate2(fontDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
            PATHDIRECTORY_NOHASH, collectDirectoryNodeWorker, (void*)&p);
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

    storage = (PathDirectoryNode**)malloc(sizeof *storage * (p.idx+1));
    if(!storage)
        Con_Error("collectFonts: Failed on allocation of %lu bytes for new collection.", (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectDirectoryNodes(namespaceId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(const void* nodeA, const void* nodeB)
{
    // Decode paths before determining a lexicographical delta.
    AutoStr* a = Str_PercentDecode(composePathForDirectoryNode(*(const PathDirectoryNode**)nodeA, FONTS_PATH_DELIMITER));
    AutoStr* b = Str_PercentDecode(composePathForDirectoryNode(*(const PathDirectoryNode**)nodeB, FONTS_PATH_DELIMITER));
    return stricmp(Str_Text(a), Str_Text(b));
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
static size_t printFonts3(fontnamespaceid_t namespaceId, const char* like, int flags)
{
    const boolean printNamespace = !(flags & PFF_TRANSFORM_PATH_NO_NAMESPACE);
    uint idx, count = 0;
    PathDirectoryNode** foundFonts = collectDirectoryNodes(namespaceId, like, &count, NULL);
    PathDirectoryNode** iter;
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
        PathDirectoryNode* node = *iter;
        Con_Printf(" %*u: ", numFoundDigits, idx++);
        printFontOverview(node, printNamespace);
    }

    free(foundFonts);
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
    PathDirectoryNode** foundFonts;
    ddstring_t** list = NULL;

    foundFonts = collectDirectoryNodes(FN_ANY, NULL, &count, 0);
    if(foundFonts)
    {
        PathDirectoryNode** iter;
        int idx;

        qsort(foundFonts, (size_t)count, sizeof *foundFonts, composeAndCompareDirectoryNodePaths);

        list = (ddstring_t**)malloc(sizeof *list * (count+1));
        if(!list)
            Con_Error("Fonts::CollectNames: Failed on allocation of %lu bytes for name list.", sizeof *list * (count+1));

        idx = 0;
        for(iter = foundFonts; *iter; ++iter)
        {
            list[idx++] = Str_FromAutoStr(composePathForDirectoryNode(*iter, FONTS_PATH_DELIMITER));
        }
        list[idx] = NULL; // Terminate.
    }

    if(rCount) *rCount = count;
    return list;
}

D_CMD(ListFonts)
{
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
    fontnamespaceid_t namespaceId;

    if(!Fonts_Size())
    {
        Con_Message("There are currently no fonts defined/loaded.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Font Statistics:\n");
    for(namespaceId = FONTNAMESPACE_FIRST; namespaceId <= FONTNAMESPACE_LAST; ++namespaceId)
    {
        PathDirectory* fontDirectory = getDirectoryForNamespaceId(namespaceId);
        uint size;

        if(!fontDirectory) continue;

        size = PathDirectory_Size(fontDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(Fonts_NamespaceName(namespaceId)), size, size==1? "font":"fonts");
        PathDirectory_DebugPrintHashDistribution(fontDirectory);
        PathDirectory_DebugPrint(fontDirectory, FONTS_PATH_DELIMITER);
    }
    return true;
}
#endif
