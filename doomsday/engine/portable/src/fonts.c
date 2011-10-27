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

/**
 * Font Collection
 */

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_ui.h"

#include "m_misc.h"

#include "font.h"
#include "bitmapfont.h"
#include "pathdirectory.h"

/// Number of elements to block-allocate in the font index to fontbind map.
#define FONTS_BINDINGMAP_BLOCK_ALLOC (32)

typedef struct fontlist_node_s {
    struct fontlist_node_s* next;
    font_t* font;
} fontlist_node_t;
typedef fontlist_node_t fontlist_t;

typedef struct fontbind_s {
    /// Pointer to this binding's node in the directory.
    struct pathdirectory_node_s* _directoryNode;

    /// Bound font.
    font_t* _font;

    /// Unique identifier for this binding.
    fontnum_t _id;
} fontbind_t;

/// @return  Unique identifier associated with this.
fontnum_t FontBind_Id(const fontbind_t* fb);

/// @return  Font associated with this else @c NULL
font_t* FontBind_Font(const fontbind_t* fb);

/// @return  PathDirectory node associated with this.
struct pathdirectory_node_s* FontBind_DirectoryNode(const fontbind_t* fb);

/// @return  Unique identifier of the namespace within which this binding resides.
fontnamespaceid_t FontBind_NamespaceId(const fontbind_t* fb);

/// @return  Symbolic name/path-to this binding. Must be destroyed with Str_Delete().
ddstring_t* FontBind_ComposePath(const fontbind_t* fb);

/// @return  Fully qualified/absolute Uri to this binding. Must be destroyed with Uri_Delete().
Uri* FontBind_ComposeUri(const fontbind_t* fb);

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the fonts themselves and in of the system itself:
 *
 * 1) Pointers to Fonts are eternal, they are always valid and continue
 *    to reference the same logical font data even after engine reset.
 * 2) Public font identifiers (fontnum_t) are similarly eternal.
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
static fontnum_t bindingsCount;
static fontnum_t bindingsMax;
static fontbind_t** bindings;

static pathdirectory_t* namespaces[FONTNAMESPACE_COUNT];

D_CMD(ListFonts);
#if _DEBUG
D_CMD(PrintFontStats);
#endif

static int inited = false;

void Fonts_Register(void)
{
    C_CMD("listfonts", NULL, ListFonts)
#if _DEBUG
    C_CMD("fontstats", NULL, PrintFontStats)
#endif
}

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: fonts module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static __inline pathdirectory_t* directoryForFontNamespaceId(fontnamespaceid_t id)
{
    assert(VALID_FONTNAMESPACEID(id));
    return namespaces[id-FONTNAMESPACE_FIRST];
}

static fontnamespaceid_t namespaceIdForFontDirectory(pathdirectory_t* pd)
{
    materialnamespaceid_t id;
    assert(pd);
    for(id = FONTNAMESPACE_FIRST; id <= FONTNAMESPACE_LAST; ++id)
        if(namespaces[id-FONTNAMESPACE_FIRST] == pd) return id;
    // Should never happen.
    Con_Error("Fonts::namespaceIdForFontDirectory: Failed to determine id for directory %p.", (void*)pd);
    exit(1); // Unreachable.
}

static const ddstring_t* nameForFontNamespaceId(fontnamespaceid_t id)
{
    static const ddstring_t emptyString = { "" };
    static const ddstring_t namespaces[FONTNAMESPACE_COUNT] = {
        /* FN_SYSTEM */     { FN_SYSTEM_NAME },
        /* FN_SYSTEM */     { FN_GAME_NAME }
    };
    if(VALID_FONTNAMESPACEID(id))
        return &namespaces[id-FONTNAMESPACE_FIRST];
    return &emptyString;
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
static font_t* findFontForPath(pathdirectory_t* fontDirectory, const char* path)
{
    struct pathdirectory_node_s* node = DD_SearchPathDirectory(fontDirectory,
        PCF_NO_BRANCH|PCF_MATCH_FULL, path, FONTDIRECTORY_DELIMITER);
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
        font = findFontForPath(directoryForFontNamespaceId(namespaceId), path);
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
            font = findFontForPath(directoryForFontNamespaceId(order[n]), path);
        } while(!font && order[++n] != FN_ANY);
    }
    return font;
}

/// \note Part of the Doomsday public API.
font_t* Fonts_FontForUri(const Uri* uri)
{
    font_t* font;
    if(!inited || !uri) return NULL;
    if(!validateFontUri2(uri, VFUF_ALLOW_NAMESPACE_ANY, true /*quiet please*/))
    {
#if _DEBUG
        ddstring_t* uriStr = Uri_ToString(uri);
        Con_Message("Warning:Fonts::FontForUri: Uri \"%s\" failed to validate, returing NULL.\n", Str_Text(uriStr));
        Str_Delete(uriStr);
#endif
        return NULL;
    }

    // Perform the search.
    font = findFontForUri(uri);
    if(font) return font;

    // Not found.
    if(verbose)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Fonts::FontForUri: \"%s\" not found!\n", Str_Text(path));
        Str_Delete(path);
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
font_t* Fonts_FontForUriCString(const char* path)
{
    if(path && path[0])
    {
        Uri* uri = Uri_NewWithPath2(path, RC_NULL);
        font_t* font = Fonts_FontForUri(uri);
        Uri_Delete(uri);
        return font;
    }
    return NULL;
}

/// \note Part of the Doomsday public API.
fontnum_t Fonts_IndexForUri(const Uri* uri)
{
    return Fonts_ToFontNum(Fonts_FontForUri(uri));
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
    return FontBind_ComposeUri(fb);
}

static boolean newFontBind(const Uri* uri, font_t* font)
{
    pathdirectory_t* fontDirectory = directoryForFontNamespaceId(DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri))));
    struct pathdirectory_node_s* node;
    fontbind_t* fb;

    node = PathDirectory_Insert(fontDirectory, Str_Text(Uri_Path(uri)), FONTDIRECTORY_DELIMITER);

    // Is this a new binding?
    fb = (fontbind_t*) PathDirectoryNode_UserData(node);
    if(!fb)
    {
        // Acquire a new unique identifier for this binding.
        const fontnum_t bindId = ++bindingsCount;

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
    fontlist_node_t* node;
    if(NULL == (node = malloc(sizeof(*node))))
        Con_Error("Fonts::LinkFontToGlobalList: Failed on allocation of %lu bytes for "
            "new FontList::Node.", (unsigned long) sizeof(*node));
    node->font = font;
    node->next = fonts;
    fonts = node;
    return font;
}

static __inline font_t* getFontByIndex(fontnum_t num)
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
    while(NULL != fonts)
    {
        fontlist_node_t* next = fonts->next;
        destroyFont(fonts->font);
        free(fonts);
        fonts = next;
    }
}

static int clearBinding(struct pathdirectory_node_s* node, void* paramaters)
{
    fontbind_t* fb = PathDirectoryNode_DetachUserData(node);
    free(fb);
    return 0; // Continue iteration.
}

static void destroyBindings(void)
{
    assert(inited);

    { int i;
    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        PathDirectory_Iterate(namespaces[i], PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearBinding);
        PathDirectory_Delete(namespaces[i]), namespaces[i] = NULL;
    }}

    // Clear the binding index/map.
    if(bindings)
    {
        free(bindings);
        bindings = NULL;
    }
    bindingsCount = bindingsMax = 0;
}

materialnum_t FontBind_Id(const fontbind_t* fb)
{
    assert(fb);
    return fb->_id;
}

font_t* FontBind_Font(const fontbind_t* fb)
{
    assert(fb);
    return fb->_font;
}

struct pathdirectory_node_s* FontBind_DirectoryNode(const fontbind_t* fb)
{
    assert(fb);
    return fb->_directoryNode;
}

fontnamespaceid_t FontBind_NamespaceId(const fontbind_t* fb)
{
    return namespaceIdForFontDirectory(PathDirectoryNode_Directory(FontBind_DirectoryNode(fb)));
}

ddstring_t* FontBind_ComposePath(const fontbind_t* fb)
{
    struct pathdirectory_node_s* node = fb->_directoryNode;
    return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, FONTDIRECTORY_DELIMITER);
}

Uri* FontBind_ComposeUri(const fontbind_t* fb)
{
    ddstring_t* path = FontBind_ComposePath(fb);
    Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, Str_Text(nameForFontNamespaceId(FontBind_NamespaceId(fb))));
    Str_Delete(path);
    return uri;
}

void Fonts_Initialize(void)
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

void Fonts_ClearRuntimeFonts(void)
{
    errorIfNotInited("Fonts::ClearRuntimeFonts");
    Fonts_ReleaseRuntimeGLResources();
    //destroyFonts(FN_GAME);
#pragma message("!!!Fonts::ClearRuntimeFonts not yet implemented!!!")
}

void Fonts_ClearSystemFonts(void)
{
    errorIfNotInited("Fonts::ClearSystemFonts");
    Fonts_ReleaseSystemGLResources();
#pragma message("!!!Fonts::ClearSystemFonts not yet implemented!!!")
}

void Fonts_Clear(void)
{
    Fonts_ClearRuntimeFonts();
    Fonts_ClearSystemFonts();
}

void Fonts_ClearDefinitionLinks(void)
{
    errorIfNotInited("Fonts::ClearDefinitionLinks");

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        if(Font_Type(font) == FT_BITMAPCOMPOSITE)
        {
            BitmapCompositeFont_SetDefinition(font, NULL);
        }
    }}
}

uint Fonts_Count(void)
{
    if(inited)
        return bindingsCount;
    return 0;
}

void Fonts_RebuildBitmapComposite(font_t* font, ded_compositefont_t* def)
{
    assert(NULL != font);

    errorIfNotInited("Fonts::RebuildBitmapComposite");
    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildBitmapComposite: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    BitmapCompositeFont_SetDefinition(font, def);
    if(NULL == def)
        return;

    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
}

font_t* Fonts_ToFont(fontnum_t num)
{
    fontbind_t* fb;
    errorIfNotInited("Fonts::ToFont");
    fb = bindById((uint)num);
    if(!fb) return NULL;
    return FontBind_Font(fb);
}

fontnum_t Fonts_ToFontNum(font_t* font)
{
    fontbind_t* fb;
    errorIfNotInited("Fonts::ToFontNum");
    if(!font) return 0;
    fb = bindById(Font_BindId(font));
    if(!fb) return 0;
    return FontBind_Id(fb);
}

font_t* Fonts_CreateBitmapCompositeFromDef(ded_compositefont_t* def)
{
    assert(def);
    {
    const Uri* uri = def->id;
    font_t* font;

    errorIfNotInited("Fonts::CreateFromDef");

    // We require a properly formed uri.
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
    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
    newFontBind(uri, font);

    // Lets try and prepare it right away.
    BitmapCompositeFont_Prepare(font);

    { ddstring_t* path = Uri_ToString(uri);
    VERBOSE( Con_Message("New font \"%s\".\n", Str_Text(path)) )
    Str_Delete(path);
    }

    return font;
    }
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

    { ddstring_t* path = Uri_ToString(uri);
    VERBOSE( Con_Message("New font \"%s\".\n", Str_Text(path)) )
    Str_Delete(path);
    }

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

font_t* Fonts_LoadExternal(const char* name, const char* searchPath)
{
    assert(name && searchPath && searchPath[0]);
    {
    font_t* font = NULL;
    Uri* uri = Uri_NewWithPath2(name, RC_NULL);

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
    if(NULL == font)
    {
        Con_Message("Warning: Unknown format for %s\n", searchPath);
        Uri_Delete(uri);
        return 0; // Error.
    }
    Uri_Delete(uri);

    return font;
    }
}

void Fonts_ReleaseGLTexturesByNamespace(fontnamespaceid_t namespaceId)
{
    errorIfNotInited("Fonts::ReleaseGLTexturesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLTexturesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated)
        return;

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindById(Font_BindId(font));
            if(fb && FontBind_NamespaceId(fb) != namespaceId)
                continue;
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
    }}
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
    errorIfNotInited("Fonts::ReleaseGLResourcesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLResourcesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated)
        return;

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindById(Font_BindId(font));
            if(fb && FontBind_NamespaceId(fb) != namespaceId)
                continue;
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
    }}
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

static void printFontInfo(const fontbind_t* fb, boolean printNamespace)
{
    int numDigits = M_NumDigits(Fonts_Count());
    font_t* font = FontBind_Font(fb);
    ddstring_t* path = FontBind_ComposePath(fb);

    Con_Printf(" %*u: \"", numDigits, (unsigned int) Fonts_ToFontNum(font));
    if(printNamespace)
        Con_Printf("%s:", Str_Text(nameForFontNamespaceId(FontBind_NamespaceId(fb))));

    Con_Printf("%s\" %s ", Str_Text(path), Font_Type(font) == FT_BITMAP? "bitmap" : "bitmap_composite");
    if(Font_IsPrepared(font))
    {
        Con_Printf("(ascent:%i, descent:%i, leading:%i", Fonts_Ascent(font), Fonts_Descent(font), Fonts_Leading(font));
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

    Str_Delete(path);
}

/**
 * \todo A horridly inefficent algorithm. This should be implemented in PathDirectory
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the font search/listing console commands so
 * is not hugely important right now.
 */
typedef struct {
    const char* like;
    int idx;
    fontbind_t** storage;
} collectfontworker_paramaters_t;

static int collectFontWorker(const struct pathdirectory_node_s* node, void* paramaters)
{
    fontbind_t* fb = (fontbind_t*)PathDirectoryNode_UserData(node);
    collectfontworker_paramaters_t* p = (collectfontworker_paramaters_t*)paramaters;

    if(p->like && p->like[0])
    {
        Uri* uri = FontBind_ComposeUri(fb);
        int delta = strnicmp(Str_Text(Uri_Path(uri)), p->like, strlen(p->like));
        Uri_Delete(uri);
        if(delta) return 0; // Continue iteration.
    }

    if(p->storage)
    {
        p->storage[p->idx++] = fb;
    }
    else
    {
        ++p->idx;
    }

    return 0; // Continue iteration.
}

static fontbind_t** collectFonts(fontnamespaceid_t namespaceId, const char* like,
    int* count, fontbind_t** storage)
{
    collectfontworker_paramaters_t p;
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

    p.idx = 0;
    p.like = like;
    p.storage = storage;
    for(iterId  = fromId; iterId <= toId; ++iterId)
    {
        pathdirectory_t* fontDirectory = directoryForFontNamespaceId(iterId);
        PathDirectory_Iterate2_Const(fontDirectory, PCF_NO_BRANCH|PCF_MATCH_FULL, NULL,
            PATHDIRECTORY_NOHASH, collectFontWorker, (void*)&p);
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
        return 0;
    }

    storage = (fontbind_t**)malloc(sizeof *storage * (p.idx+1));
    if(!storage)
        Con_Error("collectFonts: Failed on allocation of %lu bytes for new collection.",
            (unsigned long) (sizeof* storage * (p.idx+1)));
    return collectFonts(namespaceId, like, count, storage);
}

static int compareFontBindByPath(const void* fbA, const void* fbB)
{
    Uri* a = FontBind_ComposeUri(*(const fontbind_t**)fbA);
    Uri* b = FontBind_ComposeUri(*(const fontbind_t**)fbB);
    int delta = stricmp(Str_Text(Uri_Path(a)), Str_Text(Uri_Path(b)));
    Uri_Delete(b);
    Uri_Delete(a);
    return delta;
}

static size_t printFonts2(fontnamespaceid_t namespaceId, const char* like,
    boolean printNamespace)
{
    int numDigits = M_NumDigits(Fonts_Count());
    int count = 0;
    fontbind_t** foundFonts = collectFonts(namespaceId, like, &count, NULL);

    if(!printNamespace)
        Con_FPrintf(CPF_YELLOW, "Known fonts in namespace '%s'", Str_Text(nameForFontNamespaceId(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CPF_YELLOW, "Known fonts");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    if(!foundFonts)
        return 0;

    // Print the result index key.
    Con_Printf(" uid: \"%s\" font-type", VALID_FONTNAMESPACEID(namespaceId)? "font-name" : "<namespace>:font-name");
    // Fonts may be prepared only if GL is inited thus if we can't prepare, we can't list property values.
    if(GL_IsInited())
    {
        Con_Printf(" (<property-name>:<value>, ...)");
    }
    Con_Printf("\n");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundFonts, count, sizeof *foundFonts, compareFontBindByPath);

    { fontbind_t* const* ptr;
    for(ptr = foundFonts; *ptr; ++ptr)
    {
        const fontbind_t* fb = *ptr;
        printFontInfo(fb, printNamespace);
    }}

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

ddstring_t** Fonts_CollectNames(int* count)
{
    ddstring_t** list = NULL;
    int numBinds = 0;
    fontbind_t** foundFonts;

    errorIfNotInited("Fonts::CollectNames");

    foundFonts = collectFonts(FN_ANY, NULL, &numBinds, 0);
    if(foundFonts)
    {
        fontbind_t* const* ptr;
        int i;

        qsort(foundFonts, (size_t)numBinds, sizeof *foundFonts, compareFontBindByPath);

        list = (ddstring_t**)malloc(sizeof *list * (numBinds+1));
        if(!list)
            Con_Error("Fonts::CollectNames: Failed on allocation of %lu bytes for name list.", sizeof *list * (numBinds+1));

        ptr = foundFonts;
        for(i = 0; i < numBinds; ++i, ptr++)
        {
            const fontbind_t* fb = *ptr;
            list[i] = FontBind_ComposePath(fb);
        }
        list[i] = NULL; // Terminate.
    }

    if(count) *count = numBinds;
    return list;
}

D_CMD(ListFonts)
{
    fontnamespaceid_t namespaceId = FN_ANY;
    const char* like = NULL;
    Uri* uri = NULL;

    if(!Fonts_Count())
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

    if(uri != NULL)
        Uri_Delete(uri);
    return true;
}

#if _DEBUG
D_CMD(PrintFontStats)
{
    fontnamespaceid_t namespaceId;
    Con_FPrintf(CPF_YELLOW, "Font Statistics:\n");
    for(namespaceId = FONTNAMESPACE_FIRST; namespaceId <= FONTNAMESPACE_LAST; ++namespaceId)
    {
        pathdirectory_t* fontDirectory = directoryForFontNamespaceId(namespaceId);
        uint size;

        if(!fontDirectory) continue;

        size = PathDirectory_Size(fontDirectory);
        Con_Printf("Namespace: %s (%u %s)\n", Str_Text(nameForFontNamespaceId(namespaceId)), size, size==1? "font":"fonts");
        PathDirectory_PrintHashDistribution(fontDirectory);
        PathDirectory_Print(fontDirectory, FONTDIRECTORY_DELIMITER);
    }
    return true;
}
#endif
