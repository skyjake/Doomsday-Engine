
//**************************************************************************
//**
//** M_STRING.C
//**
//** Simple dynamic string management (like MFC's CString, but not 
//** nearly as complex). Uses the memory zone for memory allocation.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_LENGTH	0x4000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Str_Init
//	Call this for uninitialized strings. Global variables are 
//	automatically cleared, so they don't need initialization.
//===========================================================================
void Str_Init(ddstring_t *ds)
{
	memset(ds, 0, sizeof(*ds));	
}

//===========================================================================
// Str_Free
//	Empty an existing string. Totally destroys it. The string is 
//	considered re-initialized.
//===========================================================================
void Str_Free(ddstring_t *ds)
{
	if(ds->size) 
	{
		// The string has memory allocated, free it.
		Z_Free(ds->str);
	}
	memset(ds, 0, sizeof(*ds));
}

//===========================================================================
// Str_Clear
//	Empties a string, but does not free its memory.
//===========================================================================
void Str_Clear(ddstring_t *ds)
{
	Str_Set(ds, "");	
}

//===========================================================================
// Str_Alloc
//	Allocates so much memory that for_length characters will fit.
//===========================================================================
void Str_Alloc(ddstring_t *ds, int for_length, int preserve)
{
	boolean old_data = false;
	char *buf;

	// Include the terminating null character.
	for_length++;

	if(ds->size >= for_length) return; // We're OK.

	// Already some memory allocated?
	if(ds->size) 
		old_data = true;
	else
		ds->size = 1;

	while(ds->size < for_length) ds->size *= 2;
	buf = Z_Malloc(ds->size, PU_STATIC, 0);
	memset(buf, 0, ds->size); 

	if(preserve && ds->str) 
		strncpy(buf, ds->str, ds->size - 1);

	// Replace the old string with the new buffer.
	if(old_data) Z_Free(ds->str);
	ds->str = buf;
}

//===========================================================================
// Str_Set
//===========================================================================
void Str_Set(ddstring_t *ds, const char *text)
{
	int incoming = strlen(text);

	Str_Alloc(ds, incoming, false);
	strcpy(ds->str, text);
	ds->length = incoming;
}

//===========================================================================
// Str_Append
//===========================================================================
void Str_Append(ddstring_t *ds, const char *append_text)
{
	int incoming = strlen(append_text);

	// Don't allow extremely long strings.
	if(ds->length > MAX_LENGTH) return;

	Str_Alloc(ds, ds->length + incoming, true);
	strcpy(ds->str + ds->length, append_text);
	ds->length += incoming;
}

//===========================================================================
// Str_PartAppend
//	Appends a portion of a string.
//===========================================================================
void Str_PartAppend(ddstring_t *dest, const char *src, int start, int count)
{
	Str_Alloc(dest, dest->length + count, true);
	memcpy(dest->str + dest->length, src + start, count);
	dest->length += count;

	// Terminate the appended part.
	dest->str[dest->length] = 0;
}

//===========================================================================
// Str_Prepend
//	Prepend is not even a word, is it? It should be 'prefix'?
//===========================================================================
void Str_Prepend(ddstring_t *ds, const char *prepend_text)
{
	int incoming = strlen(prepend_text);

	// Don't allow extremely long strings.
	if(ds->length > MAX_LENGTH) return;

	Str_Alloc(ds, ds->length + incoming, true);
	memmove(ds->str + incoming, ds->str, ds->length + 1);
	memcpy(ds->str, prepend_text, incoming);
	ds->length += incoming;
}

//===========================================================================
// Str_Text
//	This is safe for all strings.
//===========================================================================
char *Str_Text(ddstring_t *ds)
{
	return ds->str? ds->str : "";
}

//===========================================================================
// Str_Length
//	This is safe for all strings.
//===========================================================================
int Str_Length(ddstring_t *ds)
{
	if(ds->length) return ds->length;
	return strlen(Str_Text(ds));
}

//===========================================================================
// Str_Copy
//	Makes a true copy.
//===========================================================================
void Str_Copy(ddstring_t *dest, ddstring_t *src)
{
	Str_Free(dest);
	dest->size = src->size;
	dest->length = src->length;
	dest->str = Z_Malloc(src->size, PU_STATIC, 0);
	memcpy(dest->str, src->str, src->size);
}

//===========================================================================
// Str_StripLeft
//	Strip whitespace from beginning.
//===========================================================================
void Str_StripLeft(ddstring_t *ds)
{
	int i, num;

	if(!ds->length) return;
	
	// Find out how many whitespace chars are at the beginning.
	for(i = 0, num = 0; i < ds->length; i++)
		if(isspace(ds->str[i])) num++; else break;
	
	if(num)
	{
		// Remove 'num' chars.
		memmove(ds->str, ds->str + num, ds->length - num);
		ds->length -= num;
		ds->str[ds->length] = 0;
	}
}

//===========================================================================
// Str_StripRight
//	Strip whitespace from end.
//===========================================================================
void Str_StripRight(ddstring_t *ds)
{
	int i;

	if(!ds->length) return;

	for(i = ds->length - 1; i >= 0; i--)
		if(isspace(ds->str[i]))
		{
			// Remove this char.
			ds->str[i] = 0;
			ds->length--;
		}
		else break;
}

//===========================================================================
// Str_Strip
//	Strip whitespace from beginning and end.
//===========================================================================
void Str_Strip(ddstring_t *ds)
{
	Str_StripLeft(ds);
	Str_StripRight(ds);
}