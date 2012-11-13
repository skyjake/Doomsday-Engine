/**\file dd_help.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Help Text Strings.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_filesys.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRINGS             16

// TYPES -------------------------------------------------------------------

typedef struct helpstring_s {
    int             type;
    char*           text;
} helpstring_t;

typedef struct helpnode_s {
    struct helpnode_s *prev, *next;
    char*           id;
    helpstring_t    str[MAX_STRINGS];
} helpnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(LoadHelp);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean helpInited = false;
static helpnode_t helpRoot;

// CODE --------------------------------------------------------------------

void DH_Register(void)
{
    C_CMD("loadhelp",          "",    LoadHelp);
}

/**
 * Create and link a new help node.
 *
 * @return              Ptr to the newly created helpnode.
 */
static helpnode_t* DH_NewNode(void)
{
    helpnode_t*         n = M_Calloc(sizeof(*n));

    n->next = &helpRoot;
    n->prev = helpRoot.prev;
    helpRoot.prev = n->prev->next = n;
    return n;
}

/**
 * Unlink the helpnode from the dlist and destroy it.
 *
 * @param   node        Ptr to the node to be destroyed.
 */
static void DH_DeleteNode(helpnode_t* node)
{
    int                 i;

    if(!node)
        return;

    node->prev->next = node->next;
    node->next->prev = node->prev;
    // Free all memory associated with the node.
    if(node->id)
        M_Free(node->id);

    for(i = 0; i < MAX_STRINGS; ++i)
        M_Free(node->str[i].text);

    M_Free(node);
}

/**
 * Parses the given file looking for help strings.
 *
 * @param fileName      Path to the file to be read.
 *
 * @return              Non-zero if the file was read successfully.
 */
static int DH_ReadStrings(const char* fileName)
{
    FileHandle* file = F_Open(fileName, "rt");
    char line[2048], *ptr, *eol, *end;
    helpnode_t* node = 0;
    int count = 0, length;

    if(!file)
    {
        Con_Message("DH_ReadStrings: Warning, %s not found.\n", fileName);
        return false;
    }

    while(!FileHandle_AtEnd(file))
    {
        M_ReadLine(line, sizeof(line), file);
        if(M_IsComment(line))
            continue;

        // Where does the line begin?
        ptr = M_SkipWhite(line);
        if(!*ptr)
            continue; // An empty line.

        eol = ptr + strlen(ptr); // End of line.
        // A new node?
        if(*ptr == '[')
        {
            node = DH_NewNode();
            count = 0;
            if(!(end = strchr(ptr, ']')))
                end = eol;

            node->id = M_Calloc(end - ptr);
            strncpy(node->id, ptr + 1, end - ptr - 1);
        }
        else if(node && (end = strchr(ptr, '='))) // It must be a key?
        {
            helpstring_t* hst = node->str + count;

            if(count == MAX_STRINGS)
                continue; // No more room.

            count++;
            // The type of the string.
            if(!strnicmp(ptr, "des", 3))
                hst->type = HST_DESCRIPTION;
            else if(!strnicmp(ptr, "cv", 2))
                hst->type = HST_CONSOLE_VARIABLE;
            else if(!strnicmp(ptr, "def", 3))
                hst->type = HST_DEFAULT_VALUE;
            else if(!strnicmp(ptr , "inf", 3))
                hst->type = HST_INFO;
            ptr = M_SkipWhite(end + 1);

            // The value may be split over multiple lines.
            // 64 kb should be quite enough.
            length = 0, hst->text = M_Malloc(0x10000);
            while(*ptr)
            {
                // Backslash escapes.
                if(*ptr == '\\')
                {
                    // Is this the last visible char?
                    if(!*M_SkipWhite(ptr + 1))
                    {
                        // Read the next line.
                        M_ReadLine(line, sizeof(line), file);
                        ptr = M_SkipWhite(line);
                    }
                    else // \ is not the last char on the line.
                    {
                        ptr++;
                        if(*ptr == '\\')
                            hst->text[length++] = '\\';
                        if(*ptr == 'n')
                            hst->text[length++] = '\n';
                        if(*ptr == 'b')
                            hst->text[length++] = '\b';
                        ptr++;
                    }
                }
                else
                {
                    // Append to the text.
                    hst->text[length++] = *ptr++;
                }
            }

            // Resize the memory to fit the text.
            hst->text[length] = 0;
            hst->text = M_Realloc(hst->text, length + 1);
        }
    }

    F_Delete(file);
    // The file was read successfully.
    return true;
}

/**
 * Finds a node matching the ID. Use DH_GetString to read strings from it.
 *
 * @param id            Help node ID to be searched for.
 *
 * @return              Ptr to helpnode if matched ELSE @c NULL,.
 */
void* DH_Find(const char* id)
{
    helpnode_t* n;
    size_t length;

    if(!helpInited)
        return NULL;

    length = strlen(id);
    for(n = helpRoot.next; n != &helpRoot; n = n->next)
    {
        /**
         * Don't compare unless the string is long enough.
         * This also stops us returning a false positive when a substring
         * matches the search string e.g:
         *     [rend-light] != [rend-light-ambient]
         */
        if(strlen(n->id) < length)
            continue;

        if(!stricmp(n->id, id))
            return n;
    }

    return NULL;
}

/**
 * Return a string from within the helpnode. Strings are stored internally
 * and indexed by their type (e.g. HST_DESCRIPTION).
 *
 * @param foundNode     The helpnode to return the string from.
 * @param type          The string type (index) to look for within the node.
 *
 * @return              Ptr to the found string ELSE @c NULL,. Note,
 *                      may also return @c NULL, if passed an
 *                      invalid helpnode ptr OR the help string database has
 *                      not beeen initialized yet.
 */
char* DH_GetString(void* foundNode, int type)
{
    helpnode_t*         n = foundNode;
    int                 i;

    if(!n || !helpInited || type < 0 || type > NUM_HELPSTRING_TYPES)
        return NULL;

    for(i = 0; i < MAX_STRINGS; ++i)
        if(n->str[i].text && n->str[i].type == type)
            return n->str[i].text;

    return NULL;
}

/**
 * Initializes the help string database. After which, attempts to read the engine's
 * own help string file.
 */
void DD_InitHelp(void)
{
    float starttime;

    if(helpInited) return; // Already inited.

    VERBOSE( Con_Message("Initializing Help subsystem...\n") )
    starttime = (verbose >= 2? Timer_Seconds() : 0);

    // Init the links.
    helpRoot.next = helpRoot.prev = &helpRoot;

    // Parse the control panel help file.
    { ddstring_t helpFileName; Str_Init(&helpFileName);
    Str_Set(&helpFileName, DD_BASEPATH_DATA"cphelp.txt");
    F_ExpandBasePath(&helpFileName, &helpFileName);

    DH_ReadStrings(Str_Text(&helpFileName));

    Str_Free(&helpFileName);
    }

    // Help is now available.
    helpInited = true;

    VERBOSE2( Con_Message("DD_InitHelp: Done in %.2f seconds.\n", Timer_Seconds() - starttime) );
}

/**
 * Attempts to read help strings from the game-specific help file.
 */
void DD_ReadGameHelp(void)
{
    if(helpInited && DD_GameLoaded())
    {
        Uri* helpFileUri = Uri_NewWithPath2("$(App.DataPath)/$(GamePlugin.Name)/conhelp.txt", RC_NULL);
        AutoStr* resolvedPath = Uri_Resolved(helpFileUri);
        if(resolvedPath)
        {
            DH_ReadStrings(Str_Text(resolvedPath));
        }
        Uri_Delete(helpFileUri);
    }
}

/**
 * Shuts down the help string database. Frees all storage and destroys
 * database itself.
 */
void DD_ShutdownHelp(void)
{
    if(!helpInited)
        return;

    helpInited = false;
    // Delete all the nodes.
    while(helpRoot.next != &helpRoot)
        DH_DeleteNode(helpRoot.next);
}

D_CMD(LoadHelp)
{
    DD_ShutdownHelp();
    DD_InitHelp();
    return true;
}
