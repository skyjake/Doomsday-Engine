/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_help.c: Help Text Strings
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRINGS	16

// TYPES -------------------------------------------------------------------

typedef struct helpstring_s {
	int	type;
	char *text;
} helpstring_t;

typedef struct helpnode_s {
	struct helpnode_s *prev, *next;
	char *id;
	helpstring_t str[MAX_STRINGS];
} helpnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean		helpInited = false;
static helpnode_t	helpRoot;

// CODE --------------------------------------------------------------------

//===========================================================================
// DH_NewNode
//	Create and link.
//===========================================================================
helpnode_t *DH_NewNode(void)
{
	helpnode_t *n = calloc(sizeof(*n), 1);
	n->next = &helpRoot;
	n->prev = helpRoot.prev;
	helpRoot.prev = n->prev->next = n;
	return n;
}

//===========================================================================
// DH_DeleteNode
//	Unlink and destroy.
//===========================================================================
void DH_DeleteNode(helpnode_t *node)
{
	int i;

	node->prev->next = node->next;
	node->next->prev = node->prev;
	// Free all memory associated with the node.
	free(node->id);
	for(i = 0; i < MAX_STRINGS; i++) free(node->str[i].text);
	free(node);	
}

//===========================================================================
// DH_ReadStrings
//===========================================================================
int DH_ReadStrings(char *fileName)
{
	DFILE *file = F_Open(fileName, "rt");
	char line[2048], *ptr, *eol, *end;
	helpnode_t *node = NULL;
	int count, length;

	if(!file) 
	{
		Con_Message("DH_ReadStrings: %s not found.\n", fileName);
		return false; // The file was not found.
	}

	while(!deof(file))
	{
		M_ReadLine(line, sizeof(line), file);
		if(M_IsComment(line)) continue;
		// Where does the line begin?
		ptr = M_SkipWhite(line);
		if(!*ptr) continue; // An empty line.
		eol = ptr + strlen(ptr); // End of line.
		// A new node?
		if(*ptr == '[')
		{
			node = DH_NewNode();
			count = 0;
			if(!(end = strchr(ptr, ']'))) end = eol;
			node->id = calloc(end - ptr, 1);
			strncpy(node->id, ptr + 1, end - ptr - 1);
		}
		else if(node && (end = strchr(ptr, '='))) // It must be a key?
		{
			helpstring_t *hst = node->str + count;
			if(count == MAX_STRINGS) continue; // No more room.
			count++;
			// The type of the string.
			if(!strnicmp(ptr, "des", 3)) hst->type = HST_DESCRIPTION;
			else if(!strnicmp(ptr, "cv", 2)) hst->type = HST_CONSOLE_VARIABLE;
			else if(!strnicmp(ptr, "def", 3)) hst->type = HST_DEFAULT_VALUE;
			ptr = M_SkipWhite(end + 1);
			// The value may be split over multiple lines.
			// 64 kb should be quite enough.
			length = 0, hst->text = malloc(0x10000);
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
						if(*ptr == '\\') hst->text[length++] = '\\';
						if(*ptr == 'n') hst->text[length++] = '\n';
						if(*ptr == 'b') hst->text[length++] = '\b';
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
			hst->text = realloc(hst->text, length + 1);
		}
	}
		
	// The file was read successfully.
	F_Close(file);
	return true;
}

//===========================================================================
// DH_Find
//	Finds a node matching the ID. Use DH_GetString to read strings from it.
//===========================================================================
void *DH_Find(char *id)
{
	helpnode_t *n;

	if(!helpInited) return NULL;
	for(n = helpRoot.next; n != &helpRoot; n = n->next)
		if(!strnicmp(id, n->id, strlen(n->id))) return n;
	return NULL;
}

//===========================================================================
// DH_GetString
//===========================================================================
char *DH_GetString(void *foundNode, int type)
{
	helpnode_t *n = foundNode;
	int i;

	if(!n || !helpInited) return NULL;
	for(i = 0; i < MAX_STRINGS; i++)
		if(n->str[i].text && n->str[i].type == type)
			return n->str[i].text;
	return NULL;
}

//===========================================================================
// DD_InitHelp
//===========================================================================
void DD_InitHelp(void)
{
	char helpFileName[256];

	if(helpInited) return;

	// Init the links.
	helpRoot.next = helpRoot.prev = &helpRoot;

	// Control Panel help.
	M_TranslatePath("}Data\\cphelp.txt", helpFileName);
	DH_ReadStrings(helpFileName);	

	// Help is now available.
	helpInited = true;
}

//===========================================================================
// DD_ShutdownHelp
//===========================================================================
void DD_ShutdownHelp(void)
{
	if(!helpInited) return;
	helpInited = false;
	// Delete all the nodes.
	while(helpRoot.next != &helpRoot) DH_DeleteNode(helpRoot.next);
}
