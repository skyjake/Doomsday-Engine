#ifndef __DOOMSDAY_DYN_STRING_H__
#define __DOOMSDAY_DYN_STRING_H__

// You can init with string constants, for example:
// ddstring_t mystr = { "Hello world." };

typedef struct
{
	char *str;
	int length;		// String length (no terminating nulls).
	int size;		// Allocated memory (not necessarily string size).
} ddstring_t;

void Str_Init(ddstring_t *ds);
void Str_Free(ddstring_t *ds);
void Str_Clear(ddstring_t *ds);
void Str_Reserve(ddstring_t *ds, int length);
void Str_Set(ddstring_t *ds, const char *text);
void Str_Append(ddstring_t *ds, const char *append_text);
void Str_PartAppend(ddstring_t *dest, const char *src, int start, int count);
void Str_Prepend(ddstring_t *ds, const char *prepend_text);
int Str_Length(ddstring_t *ds);
char *Str_Text(ddstring_t *ds);
void Str_Copy(ddstring_t *dest, ddstring_t *src);
void Str_StripLeft(ddstring_t *ds);
void Str_StripRight(ddstring_t *ds);
void Str_Strip(ddstring_t *ds);

#endif