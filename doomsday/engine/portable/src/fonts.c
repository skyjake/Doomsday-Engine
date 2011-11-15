/**\file fonts.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"

#include "gl_texmanager.h"
#include "pathdirectory.h"

#include "fonts.h"

/// Number of elements to block-allocate in the font index to fontbind map.
#define FONTS_BINDINGMAP_BLOCK_ALLOC (32)

typedef struct fontlist_node_s {
    struct fontlist_node_s* next;
    font_t* font;
} fontlist_node_t;
typedef fontlist_node_t fontlist_t;

typedef struct fontbind_s {
    /// Pointer to this binding's node in the directory.
    PathDirectoryNode* _directoryNode;

    /// Bound font.
    font_t* _font;

    /// Unique identifier for this binding.
    fontid_t _id;
} fontbind_t;

/// @return  Unique identifier associated with this.
fontid_t FontBind_Id(const fontbind_t* fb);

/// @return  Font associated with this else @c NULL
font_t* FontBind_Font(const fontbind_t* fb);

/// @return  PathDirectory node associated with this.
PathDirectoryNode* FontBind_DirectoryNode(const fontbind_t* fb);

static int inited = false;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the fonts themselves and in of the system itself:
 *
 * 1) Pointers to Fonts are eternal, they are always valid and continue
 *    to reference the same logical font data even after engine reset.
 * 2) Public font identifiers (fontid_t) are similarly eternal.
 *    Note that they are used to index the font name bindings array.
 * 3) Dynamic creation/update of fonts.
 * 4) Font name bindings are semi-independant from the fonts. There
 *    may be multiple name bindings for a given font (aliases).
 *    The only requirement is that their symbolic names must be unique among
 *    those in the same namespace.
 * 5) Super-fast look up by public font identifier.
 * 6) Fast look up by font name (a hashing scheme is used).
 */
static fontlist_t* fonts;
static fontid_t bindingsCount;
static fontid_t bindingsMax;
static fontbind_t** bindings;

// Font namespaces contain mappings between names and Font instances.
static PathDirectory* namespaces[FONTNAMESPACE_COUNT];

D_CMD(ListFonts);
#if _DEBUG
D_CMD(PrintFontStats);
#endif

void Fonts_Register(void)
{
    C_CMD("listfonts",  NULL, ListFonts)
#if _DEBUG
    C_CMD("fontstats",  NULL, PrintFontStats)
#endif
}

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: fonts module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static __inline PathDirectory* getDirectoryForNamespaceId(fontnamespaceid_t id)
{
    assert(VALID_FONTNAMESPACEID(id));
    return namespaces[id-FONTNAMESPACE_FIRST];
}

static fontnamespaceid_t namespaceIdForDirectory(PathDirectory* pd)
{
    fontnamespaceid_t id;
    assert(pd);

    for(id = FONTNAMESPACE_FIRST; id <= FONTNAMESPACE_LAST; ++id)
    {
        if(namespaces[id-FONTNAMESPACE_FIRST] == pd) return id;
    }
    // Should never happen.
    Con_Error("Fonts::namespaceIdForDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
}

static fontnamespaceid_t namespaceIdForDirectoryNode(const PathDirectoryNode* node)
{
    return namespaceIdForDirectory(PathDirectoryNode_Directory(node));
}

/// @return  Newly composed path for @a node. Must be released with Str_Delete()
static ddstring_t* composePathForDirectoryNode(const PathDirectoryNode* node, char delimiter)
{
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, delimiter);
}

/// @return  Newly composed Uri for @a node. Must be released with Uri_Delete()
static Uri* composeUriForDirectoryNode(const PathDirectoryNode* node)
{
    const ddstring_t* namespaceName = Fonts_NamespaceName(namespaceIdForDirectoryNode(node));
    ddstring_t* path = composePathForDirectoryNode(node, FONTS_PATH_DELIMITER);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(namespaceName));
    Str_Delete(path);
    return uri;
}

static fontbind_t* bindById(uint bindId)
{
    if(0 == bindId || bindId > bindingsCount) return 0;
    return bindings[bindId-1];
}

/**
 * @defgroup validateFontUriFlags  Validate Font Uri Flags
 * @{
 */
#define VFUF_ALLOW_NAMESPACE_ANY 0x1 /// The Scheme component of the uri may be of zero-length; signifying "any namespace".
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

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Invalid path '%s' in Font uri \"%s\".\n", Str_Text(Uri_Path(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
        }
        return false;
    }

    namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
    if(!((flags & VFUF_ALLOW_NAMESPACE_ANY) && namespaceId == FN_ANY) &&
       !VALID_FONTNAMESPACEID(namespaceId))
    {
        if(!quiet)
        {
            ddstring_t* uriStr = Uri_ToString(uri);
            Con_Message("Unknown namespace '%s' in Font uri \"%s\".\n", Str_Text(Uri_Scheme(uri)), Str_Text(uriStr));
            Str_Delete(uriStr);
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
 * Given a directory and path, search the Materials collection for a match.
 *
 * @param directory  Namespace-specific PathDirectory to search in.
 * @param path  Path of the font to search for.
 * @return  Found Font else @c 0
 */
static font_t* findFontForPath(PathDirectory* fontDirectory, const char* path)
{
    PathDirectoryNode* node = PathDirectory_Find(fontDirectory,
        PCF_NO_BRANCH|PCF_MATCH_FULL, path, FONTS_PATH_DELIMITER);
    if(node)
    {
        return FontBind_Font((fontbind_t*) PathDirectoryNode_UserData(node));
    }
    return NULL; // Not found.
}

/// \assume @a uri has already been validated and is well-formed.
static font_t* findFontForUri(const Uri* uri)
{
    fontnamespaceid_t namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
    const char* path = Str_Text(Uri_Path(uri));
    font_t* font = NULL;
    if(namespaceId != FN_ANY)
    {
        // Caller wants a font in a specific namespace.
        font = findFontForPath(getDirectoryForNamespaceId(namespaceId), path);
    }
    else
    {
        // Caller does not care which namespace.
        // Check for the font in these namespaces in priority order.
        static const fontnamespaceid_t order[] = {
            FN_GAME, FN_SYSTEM, MN_ANY
        };
        int n = 0;
        do
        {
            font = findFontForPath(getDirectoryForNamespaceId(order[n]), path);
        } while(!font && order[++n] != FN_ANY);
    }
    return font;
}

font_t* Fonts_ResolveUri2(const Uri* uri, boolean quiet)
{
    font_t* font;
    if(!inited || !uri) return NULL;
    if(!validateFontUri2(uri, VFUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning:Fonts::ResolveUri: Uri \"%s\" failed to validate, returing NULL.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
#endif
        return NULL;
    }

    // Perform the search.
    font = findFontForUri(uri);
    if(font) return font;

    // Not found.
    if(!quiet)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Fonts::ResolveUri: \"%s\" not found!\n", Str_Text(path));
        Str_Delete(path);
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
font_t* Fonts_ResolveUri(const Uri* uri)
{
    return Fonts_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

font_t* Fonts_ResolveUriCString2(const char* path, boolean quiet)
{
    if(path && path[0])
    {
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        font_t* font = Fonts_ResolveUri2(uri, quiet);
        Uri_Delete(uri);
        return font;
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
font_t* Fonts_ResolveUriCString(const char* path)
{
    return Fonts_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

/// \note Part of the Doomsday public API.
fontid_t Fonts_IndexForUri(const Uri* uri)
{
    return Fonts_Id(Fonts_ResolveUri(uri));
}

ddstring_t* Fonts_ComposePath(font_t* font)
{
    fontbind_t* fb;
    if(!font)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::ComposePath: Attempted with invalid reference [%p], returning null-object.\n", (void*)font);
#endif
        return Str_New();
    }
    fb = bindById(Font_BindId(font));
    if(!fb)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::ComposePath: Attempted with non-bound font [%p], returning null-object.\n", (void*)font);
#endif
        return Str_New();
    }
    return composePathForDirectoryNode(FontBind_DirectoryNode(fb), FONTS_PATH_DELIMITER);
}

Uri* Fonts_ComposeUri(font_t* font)
{
    fontbind_t* fb;
    if(!font)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::ComposeUri: Attempted with invalid index (num==0), returning null-object.\n");
#endif
        return Uri_New();
    }
    fb = bindById(Font_BindId(font));
    if(!fb)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::ComposeUri: Attempted with non-bound font [%p], returning null-object.\n", (void*)font);
#endif
        return Uri_New();
    }
    return composeUriForDirectoryNode(FontBind_DirectoryNode(fb));
}

static boolean newFontBind(const Uri* uri, font_t* font)
{
    PathDirectory* fontDirectory = getDirectoryForNamespaceId(DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri))));
    PathDirectoryNode* node;
    fontbind_t* fb;

    node = PathDirectory_Insert(fontDirectory, Str_Text(Uri_Path(uri)), FONTS_PATH_DELIMITER);

    // Is this a new binding?
    fb = (fontbind_t*) PathDirectoryNode_UserData(node);
    if(!fb)
    {
        // Acquire a new unique identifier for this binding.
        const fontid_t bindId = ++bindingsCount;

        fb = (fontbind_t*) malloc(sizeof *fb);
        if(!fb)
        {
            Con_Error("Fontss::newFontUriBinding: Failed on allocation of %lu bytes for new FontBind.",
                (unsigned long) sizeof *fb);
            exit(1); // Unreachable.
        }

        // Init the new binding.
        fb->_font = NULL;

        fb->_directoryNode = node;
        PathDirectoryNode_AttachUserData(node, fb);

        fb->_id = bindId;
        if(font)
        {
            Font_SetBindId(font, bindId);
        }

        // Add the new binding to the bindings index/map.
        if(bindingsCount > bindingsMax)
        {
            // Allocate more memory.
            bindingsMax += FONTS_BINDINGMAP_BLOCK_ALLOC;
            bindings = (fontbind_t**) realloc(bindings, sizeof *bindings * bindingsMax);
            if(!bindings)
                Con_Error("Fontss:newFontUriBinding: Failed on (re)allocation of %lu bytes for FontBind map.",
                    (unsigned long) sizeof *bindings * bindingsMax);
        }
        bindings[bindingsCount-1] = fb; /* 1-based index */
    }

    // (Re)configure the binding.
    fb->_font = font;

    return true;
}

static font_t* constructFont(fonttype_t type)
{
    switch(type)
    {
    case FT_BITMAP:             return BitmapFont_New();
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_New();
    default:
        Con_Error("Fonts::ConstructFont: Unknown font type %i.", (int)type);
        exit(1); // Unreachable.
    }
}

/**
 * Link the font into the global list of fonts.
 * \assume font is NOT already present in the global list.
 */
static font_t* linkFontToGlobalList(font_t* font)
{
    fontlist_node_t* node = malloc(sizeof *node);
    if(!node)
        Con_Error("Fonts::LinkFontToGlobalList: Failed on allocation of %lu bytes for new FontList::Node.", (unsigned long) sizeof *node);

    node->font = font;
    node->next = fonts;
    fonts = node;
    return font;
}

static __inline font_t* getFontByIndex(fontid_t num)
{
    if(num < bindingsCount)
        return FontBind_Font(bindings[num]);
    Con_Error("Fonts::GetFontByIndex: Invalid index #%u.", (unsigned int) num);
    return NULL; // Unreachable.
}

static void destroyFont(font_t* font)
{
    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Delete(font); return;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Delete(font); return;
    default:
        Con_Error("Fonts::DestroyFont: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

static void destroyFonts(void)
{
    assert(inited);
    while(fonts)
    {
        fontlist_node_t* next = fonts->next;
        destroyFont(fonts->font);
        free(fonts);
        fonts = next;
    }
}

static int clearBinding(PathDirectoryNode* node, void* paramaters)
{
    fontbind_t* fb = PathDirectoryNode_DetachUserData(node);
    free(fb);
    return 0; // Continue iteration.
}

static void destroyBindings(void)
{
    int i;
    assert(inited);

    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        PathDirectory_Iterate(namespaces[i], PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBinding);
        PathDirectory_Delete(namespaces[i]), namespaces[i] = NULL;
    }

    // Clear the binding index/map.
    if(bindings)
    {
        free(bindings);
        bindings = NULL;
    }
    bindingsCount = bindingsMax = 0;
}

fontid_t FontBind_Id(const fontbind_t* fb)
{
    assert(fb);
    return fb->_id;
}

font_t* FontBind_Font(const fontbind_t* fb)
{
    assert(fb);
    return fb->_font;
}

PathDirectoryNode* FontBind_DirectoryNode(const fontbind_t* fb)
{
    assert(fb);
    return fb->_directoryNode;
}

void Fonts_Init(void)
{
    if(inited) return; // Already been here.

    VERBOSE( Con_Message("Initializing Fonts collection...\n") )

    fonts = NULL;

    bindings = NULL;
    bindingsCount = bindingsMax = 0;
    { int i;
    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        namespaces[i] = PathDirectory_New();
    }}

    inited = true;
}

void Fonts_Shutdown(void)
{
    if(!inited) return;

    Fonts_ReleaseRuntimeGLResources();
    Fonts_ReleaseSystemGLResources();

    destroyBindings();
    destroyFonts();
    inited = false;
}

fontnamespaceid_t Fonts_ParseNamespace(const char* str)
{
    if(!str || 0 == strlen(str)) return FN_ANY;

    if(!stricmp(str, FN_GAME_NAME))     return FN_GAME;
    if(!stricmp(str, FN_SYSTEM_NAME))   return FN_SYSTEM;

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

fontnamespaceid_t Fonts_Namespace(fontbind_t* fb)
{
    if(!fb)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::Namespace: Attempted with invalid reference [%p], returning invalid id.\n", (void*)fb);
#endif
        return FN_INVALID;
    }
    return namespaceIdForDirectory(PathDirectoryNode_Directory(FontBind_DirectoryNode(fb)));
}

void Fonts_ClearRuntime(void)
{
    errorIfNotInited("Fonts::ClearRuntimeFonts");
    Fonts_ReleaseRuntimeGLResources();
    //destroyFonts(FN_GAME);
#pragma message("!!!Fonts::ClearRuntimeFonts not yet implemented!!!")
}

void Fonts_ClearSystem(void)
{
    errorIfNotInited("Fonts::ClearSystemFonts");
    Fonts_ReleaseSystemGLResources();
#pragma message("!!!Fonts::ClearSystemFonts not yet implemented!!!")
}

void Fonts_Clear(void)
{
    Fonts_ClearRuntime();
    Fonts_ClearSystem();
}

void Fonts_ClearDefinitionLinks(void)
{
    fontlist_node_t* node;
    errorIfNotInited("Fonts::ClearDefinitionLinks");

    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        if(Font_Type(font) == FT_BITMAPCOMPOSITE)
        {
            BitmapCompositeFont_SetDefinition(font, NULL);
        }
    }
}

uint Fonts_Size(void)
{
    return bindingsCount;
}

uint Fonts_Count(fontnamespaceid_t namespaceId)
{
    PathDirectory* fontDirectory;
    if(!VALID_FONTNAMESPACEID(namespaceId) || !Fonts_Size()) return 0;
    fontDirectory = getDirectoryForNamespaceId(namespaceId);
    if(!fontDirectory) return 0;
    return PathDirectory_Size(fontDirectory);
}

void Fonts_Rebuild(font_t* font, ded_compositefont_t* def)
{
    int i;
    assert(font);
    errorIfNotInited("Fonts::RebuildBitmapComposite");

    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildBitmapComposite: Font is of invalid type %i.", (int) Font_Type(font));
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

font_t* Fonts_ToFont(fontid_t num)
{
    fontbind_t* fb;
    errorIfNotInited("Fonts::ToFont");
    fb = bindById((uint)num);
    if(!fb) return NULL;
    return FontBind_Font(fb);
}

fontid_t Fonts_Id(font_t* font)
{
    fontbind_t* fb;
    errorIfNotInited("Fonts::ToFontNum");
    if(!font) return 0;
    fb = bindById(Font_BindId(font));
    if(!fb) return 0;
    return FontBind_Id(fb);
}

font_t* Fonts_CreateFromDef(ded_compositefont_t* def)
{
    font_t* font;
    Uri* uri;
    int i;
    assert(def);
    errorIfNotInited("Fonts::CreateFromDef");

    // We require a properly formed uri.
    uri = def->uri;
    if(!validateFontUri2(uri, 0, (verbose >= 1)))
    {
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning, failed to create Font \"%s\" from definition %p, ignoring.\n", Str_Text(uriStr), (void*)def);
        Str_Delete(uriStr);
        return NULL;
    }

    // Have we already created a font for this?
    font = findFontForUri(uri);
    if(font)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:Fonts::CreateBitmapCompositeFromDef: A Font with the uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return font;
    }

    // A new Font.
    font = linkFontToGlobalList(constructFont(FT_BITMAPCOMPOSITE));
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
    newFontBind(uri, font);

    // Lets try and prepare it right away.
    BitmapCompositeFont_Prepare(font);

    VERBOSE(
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("New font \"%s\".\n", Str_Text(path));
        Str_Delete(path);
    )

    return font;
}

/**
 * Creates a new font record for a file and attempts to prepare it.
 */
static font_t* loadExternalFont(const Uri* uri, const char* filePath)
{
    font_t* font;
    if(!inited) return NULL;

    // We require a properly formed uri.
    if(!validateFontUri2(uri, 0, (verbose >= 1)))
    {
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning, failed to create Material \"%s\" from file \"%s\", ignoring.\n", Str_Text(uriStr), filePath);
        Str_Delete(uriStr);
        return NULL;
    }

    // Have we already created a font for this?
    font = findFontForUri(uri);
    if(font)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning:Fonts::loadExternalFont: A Font with uri \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return font;
    }

    // A new Font.
    font = linkFontToGlobalList(constructFont(FT_BITMAP));
    BitmapFont_SetFilePath(font, filePath);
    newFontBind(uri, font);

    // Lets try and prepare it right away.
    BitmapFont_Prepare(font);

    VERBOSE(
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("New font \"%s\".\n", Str_Text(path));
        Str_Delete(path);
    )

    return font;
}

static void Fonts_Prepare(font_t* font)
{
    errorIfNotInited("Fonts::Prepare");

    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Prepare(font); break;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Prepare(font); break;
    default:
        Con_Error("Fonts::Prepare: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

font_t* Fonts_CreateFromFile(const char* name, const char* searchPath)
{
    Uri* uri = Uri_NewWithPath2(name, RC_NULL);
    font_t* font = NULL;
    assert(name && searchPath && searchPath[0]);
    errorIfNotInited("Fonts::LoadExternal");

    if(0 == F_Access(searchPath))
    {   
        Uri_Delete(uri);
        return font; // Error.
    }

    VERBOSE2(
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Preparing bitmap font \"%s\" as \"%s\"...\n", F_PrettyPath(searchPath), Str_Text(path));
        Str_Delete(path)
    )

    font = loadExternalFont(uri, searchPath);
    if(!font)
    {
        Con_Message("Warning: Unknown format for %s\n", searchPath);
        Uri_Delete(uri);
        return 0; // Error.
    }
    Uri_Delete(uri);

    return font;
}

void Fonts_ReleaseGLTexturesByNamespace(fontnamespaceid_t namespaceId)
{
    fontlist_node_t* node;
    errorIfNotInited("Fonts::ReleaseGLTexturesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLTexturesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated) return;

    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindById(Font_BindId(font));
            if(fb && Fonts_Namespace(fb) != namespaceId) continue;
        }

        switch(Font_Type(font))
        {
        case FT_BITMAP:
            BitmapFont_DeleteGLTexture(node->font);
            break;
        case FT_BITMAPCOMPOSITE:
            BitmapCompositeFont_DeleteGLTextures(node->font);
            break;
        default:
            Con_Error("Fonts::ReleaseGLTexturesByNamespace: Invalid font type %i.", (int) Font_Type(font));
            exit(1); // Unreachable.
        }
    }
}

void Fonts_ReleaseRuntimeGLTextures(void)
{
    errorIfNotInited("Fonts::ReleaseRuntimeGLTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_GAME);
}

void Fonts_ReleaseSystemGLTextures(void)
{
    errorIfNotInited("Fonts::ReleaseSystemGLTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_SYSTEM);
}

void Fonts_ReleaseGLResourcesByNamespace(fontnamespaceid_t namespaceId)
{
    fontlist_node_t* node;
    errorIfNotInited("Fonts::ReleaseGLResourcesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLResourcesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated) return;

    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindById(Font_BindId(font));
            if(fb && Fonts_Namespace(fb) != namespaceId) continue;
        }

        switch(Font_Type(font))
        {
        case FT_BITMAP:
            BitmapFont_DeleteGLTexture(node->font);
            BitmapFont_DeleteGLDisplayLists(node->font);
            break;
        case FT_BITMAPCOMPOSITE:
            BitmapCompositeFont_DeleteGLTextures(node->font);
            BitmapCompositeFont_DeleteGLDisplayLists(node->font);
            break;
        default:
            Con_Error("Fonts::ReleaseGLResourcesByNamespace: Invalid font type %i.", (int) Font_Type(font));
            exit(1); // Unreachable.
        }
    }
}

void Fonts_ReleaseRuntimeGLResources(void)
{
    errorIfNotInited("Fonts::ReleaseRuntimeTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_GAME);
}

void Fonts_ReleaseSystemGLResources(void)
{
    errorIfNotInited("Fonts::ReleaseSystemTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_SYSTEM);
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
    assert(NULL != font);
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
    assert(NULL != font);
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

void Fonts_CharDimensions(font_t* font, int* width, int* height, unsigned char ch)
{
    assert(NULL != font);
    if(!width && !height)
        return;
    if(width)  *width  = Fonts_CharWidth(font, ch);
    if(height) *height = Fonts_CharHeight(font, ch);
}

static void printFontOverview(const fontbind_t* fb, boolean printNamespace)
{
    int numDigits = MAX_OF(3/*uid*/, M_NumDigits(Fonts_Size()));
    font_t* font = FontBind_Font(fb);
    Uri* uri = Fonts_ComposeUri(font);
    const ddstring_t* path = (printNamespace? Uri_ToString(uri) : Uri_Path(uri));

    Con_Printf("%-*s %*u %s",
        printNamespace? 22 : 14, F_PrettyPath(Str_Text(path)),
        numDigits, (unsigned int) Fonts_Id(font),
        Font_Type(font) == FT_BITMAP? "bitmap" : "bitmap_composite");

    if(Font_IsPrepared(font))
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
    if(printNamespace)
        Str_Delete((ddstring_t*)path);
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

static int collectDirectoryNodeWorker(PathDirectoryNode* node, void* paramaters)
{
    collectdirectorynodeworker_params_t* p = (collectdirectorynodeworker_params_t*)paramaters;

    if(p->like && p->like[0])
    {
        ddstring_t* path = composePathForDirectoryNode(node, p->delimiter);
        int delta = strnicmp(Str_Text(path), p->like, strlen(p->like));
        Str_Delete(path);
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
    const char* like, int* count, PathDirectoryNode** storage)
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
        PathDirectory_Iterate2(fontDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
            PATHDIRECTORY_NOHASH, collectDirectoryNodeWorker, (void*)&p);
    }

    if(storage)
    {
        storage[p.idx] = 0; // Terminate.
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
    ddstring_t* a = composePathForDirectoryNode(*(const PathDirectoryNode**)nodeA, FONTS_PATH_DELIMITER);
    ddstring_t* b = composePathForDirectoryNode(*(const PathDirectoryNode**)nodeB, FONTS_PATH_DELIMITER);
    int delta = stricmp(Str_Text(a), Str_Text(b));
    Str_Delete(b);
    Str_Delete(a);
    return delta;
}

static size_t printFonts2(fontnamespaceid_t namespaceId, const char* like,
    boolean printNamespace)
{
    int numFoundDigits, numUidDigits, idx, count = 0;
    PathDirectoryNode** foundFonts = collectDirectoryNodes(namespaceId, like, &count, NULL);
    PathDirectoryNode** iter;

    if(!foundFonts) return 0;

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known fonts in namespace '%s'", Str_Text(Fonts_NamespaceName(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known fonts");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits(count));
    numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Fonts_Size()));
    Con_Printf(" %*s: %-*s %*s type", numFoundDigits, "idx",
        printNamespace? 22 : 14, printNamespace? "namespace:path" : "path", numUidDigits, "uid");

    // Fonts may be prepared only if GL is inited thus if we can't prepare, we can't list property values.
    if(GL_IsInited())
    {
        Con_Printf(" (<property-name>:<value>, ...)");
    }
    Con_Printf("\n");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundFonts, count, sizeof *foundFonts, composeAndCompareDirectoryNodePaths);

    idx = 0;
    for(iter = foundFonts; *iter; ++iter)
    {
        const PathDirectoryNode* node = *iter;
        fontbind_t* fb = (fontbind_t*)PathDirectoryNode_UserData(node);
        Con_Printf(" %*i: ", numFoundDigits, idx++);
        printFontOverview(fb, printNamespace);
    }

    free(foundFonts);
    return count;
}

static void printFonts(fontnamespaceid_t namespaceId, const char* like)
{
    size_t printTotal = 0;
    // Do we care which namespace?
    if(namespaceId == FN_ANY && like && like[0])
    {
        printTotal = printFonts2(namespaceId, like, true);
        Con_PrintRuler();
    }
    // Only one namespace to print?
    else if(VALID_FONTNAMESPACEID(namespaceId))
    {
        printTotal = printFonts2(namespaceId, like, false);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each namespace separately.
        int i;
        for(i = FONTNAMESPACE_FIRST; i <= FONTNAMESPACE_LAST; ++i)
        {
            size_t printed = printFonts2((fontnamespaceid_t)i, like, false);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Font" : "Fonts");
}

ddstring_t** Fonts_CollectNames(int* rCount)
{
    int count = 0;
    PathDirectoryNode** foundFonts;
    ddstring_t** list = NULL;
    errorIfNotInited("Fonts::CollectNames");

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
            fontbind_t* fb = (fontbind_t*)PathDirectoryNode_UserData(*iter);
            list[idx++] = Fonts_ComposePath(FontBind_Font(fb));
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

        namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
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
            namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
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
            namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Path(uri)));

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
    Con_FPrintf(CPF_YELLOW, "Font Statistics:\n");
    for(namespaceId = FONTNAMESPACE_FIRST; namespaceId <= FONTNAMESPACE_LAST; ++namespaceId)
    {
        PathDirectory* fontDirectory = getDirectoryForNamespaceId(namespaceId);
        uint size;

        if(!fontDirectory) continue;

        size = PathDirectory_Size(fontDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(Fonts_NamespaceName(namespaceId)), size, size==1? "font":"fonts");
        PathDirectory_PrintHashDistribution(fontDirectory);
        PathDirectory_Print(fontDirectory, FONTS_PATH_DELIMITER);
    }
    return true;
}
#endif
