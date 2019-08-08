
//**************************************************************************
//**
//** Doomsday Texture Compiler 
//** by Jaakko Keränen 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include "texc.h"

// MACROS ------------------------------------------------------------------

#define STOPCHAR(x)	(isspace(x) || x == ';' || x == '#' || x == '@' \
					|| x == '%' || x == ',')
#define ISTOKEN(x)		(!stricmp(token, x))
#define RET_FAIL(fn)	if (!(fn)) return false;
#define PLURAL_S(c)		((c) != 1? "s" : "")

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

void Import(char *wadFile, char *outFile);

// DATA DEFINITIONS --------------------------------------------------------

int				myargc;
char			**myargv;
unsigned char	*source, *sourcePos;
char			sourceFileName[256];
int				lineNumber;
bool			endOfSource;
char			token[MAX_TOKEN];
texsyntax_e		syntax;
int				group;
bool			fullImport = false;

def_t			root[NUM_GROUPS];
patch_t			plist;

// CODE --------------------------------------------------------------------

#if defined(UNIX) && !defined(__CYGWIN__)
void strupr(char* str)
{
    for (char* c = str; *c; c++) *c = toupper(*c);
}
#endif

//===========================================================================
// CheckOption
//	Checks the command line for the given option, and returns its index
//	number if it exists. Zero is returned if the option is not found.
//===========================================================================
int CheckOption(const char *opt)
{
	for (int i = 1; i < myargc; i++)
		if (!stricmp(myargv[i], opt))
			return /* argpos = */ i;
	return 0;
}

//==========================================================================
// FGetC
//	Reads a single character from the input file. Increments the line
//	number counter if necessary.
//==========================================================================
int FGetC()
{
	int ch = *sourcePos; 

	if (ch) sourcePos++; else endOfSource = true;	
	if (ch == '\n') lineNumber++;
	if (ch == '\r') return FGetC();
	return ch;
}

//==========================================================================
// FUngetC
//	Undoes an FGetC.
//==========================================================================
int FUngetC(int ch)
{
	if (endOfSource) return 0;
	if (ch == '\n') lineNumber--;
	if (sourcePos > source) sourcePos--;
	return ch;
}

//==========================================================================
// SkipComment
//	Reads stuff until a newline is found.
//==========================================================================
void SkipComment()
{
	int ch = FGetC();
	bool seq = false;
	
	if (ch == '\n') return; // Comment ends right away.
	if (ch != '>') // Single-line comment?
	{
		while (FGetC() != '\n' && !endOfSource) {}
	}
	else // Multiline comment?
	{
		while (!endOfSource)
		{
			ch = FGetC();
			if (seq) 
			{
				if (ch == '#') break;
				seq = false;
			}
			if (ch == '<') seq = true;
		}
	}
}

//==========================================================================
// ReadToken
//==========================================================================
int ReadToken()
{
	int ch = FGetC();
	char *out = token;

	memset(token, 0, sizeof(token));
	if (endOfSource) return false;

	// Skip whitespace and comments in the beginning.
	while ((ch == '#' || isspace(ch))) 
	{
		if (ch == '#') SkipComment();
		ch = FGetC();
		if (endOfSource) return false;
	}
	// Always store the first character.
	*out++ = ch;
	if (STOPCHAR(ch))
	{
		// Stop here.
		*out = 0;
		return true;
	}
	while (!STOPCHAR(ch) && !endOfSource)
	{
		// Store the character in the buffer.
		ch = FGetC();
		*out++ = ch;
	}
	*(out-1) = 0;	// End token.
	// Put the last read character back in the stream.
	FUngetC(ch);
	return true;
}

//===========================================================================
// PrintBanner
//===========================================================================
void PrintBanner(void)
{
    printf("## Doomsday Texture Compiler " VERSION_STR " by Jaakko Keranen <jaakko.keranen@iki.fi>\n\n");
}

//===========================================================================
// PrintUsage
//===========================================================================
void PrintUsage(void)
{
	printf("Usage: texc [-f] [-i wad_file tx_output] [tx_input] ...\n");
	printf("Multiple input files will be merged.\n");
	printf("-f enables 'full import' mode: unused data is imported as well.\n");
}

//===========================================================================
// NewTexture
//===========================================================================
def_t *NewTexture(void)
{
	def_t *t = new def_t;
	
	memset(t, 0, sizeof(*t));
	t->next = &root[group];
	t->prev = root[group].prev;
	root[group].prev = t;
	t->prev->next = t;
	return t;
}

//===========================================================================
// DeleteTexture
//===========================================================================
void DeleteTexture(def_t *t)
{
	t->next->prev = t->prev;
	t->prev->next = t->next;
	delete t;
}

//===========================================================================
// NewPatch
//===========================================================================
patch_t *NewPatch(void)
{
	patch_t *p = new patch_t;
	
	memset(p, 0, sizeof(*p));
	p->next = &plist;
	p->prev = plist.prev;
	plist.prev = p;
	p->prev->next = p;
	return p;
}

//===========================================================================
// DeletePatch
//===========================================================================
void DeletePatch(patch_t *p)
{
	p->next->prev = p->prev;
	p->prev->next = p->next;
	delete p;
}

//===========================================================================
// InitCompiler
//===========================================================================
void InitCompiler(void)
{
	sourcePos = source;
	lineNumber = 1;
	memset(token, 0, sizeof(token));
	endOfSource = false;
	group = 0;
	syntax = STX_SIMPLE;
}

//===========================================================================
// CloseCompiler
//===========================================================================
void CloseCompiler(void)
{
}

//===========================================================================
// InitData
//===========================================================================
void InitData(void)
{
	// Init defs and patch list.
	for (int i = 0; i < NUM_GROUPS; i++)
		root[i].next = root[i].prev = &root[i];
	plist.next = plist.prev = &plist;
}

//===========================================================================
// CloseData
//===========================================================================
void CloseData(void)
{
	// Free the def groups.
	for (int i = 0; i < NUM_GROUPS; i++)
		while (root[i].next != &root[i]) DeleteTexture(root[i].next);
	
	// Free the patch names.
	while (plist.next != &plist) DeletePatch(plist.next);			
}

//===========================================================================
// Message
//===========================================================================
void Message(const char *format, ...)
{
	va_list args;

	printf("%s(%i): ", sourceFileName, lineNumber);
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

//===========================================================================
// DoKeyword
//	The next token is expected to be a keyword.
//===========================================================================
int DoKeyword(void)
{
	ReadToken();
	if (ISTOKEN("syntax"))
	{
		ReadToken();
		if (ISTOKEN("simple"))
		{
			syntax = STX_SIMPLE;
			Message("Using simple syntax.");
		}
		else
		{
			Message("Unknown syntax '%s'.", token);
			return false;
		}
	}
	else if (ISTOKEN("group"))
	{
		ReadToken();
		group = strtol(token, 0, 0);
		if (group < 1 || group > NUM_GROUPS)
		{
			Message("Illegal group number %i (1..%i allowed).", group,
				NUM_GROUPS);
			return false;
		}
		Message("Switching to group %i.", group);
		group--;	// Make it an index number, though.
	}
	return true;
}

//===========================================================================
// GetTexture
//	Returns a def_t for the given texture. If it already exists, the earlier
//	definition is deleted.
//===========================================================================
def_t *GetTexture(char *name)
{
	def_t *it;

	for (it = root[group].next; it != &root[group]; it = it->next)
	{
		if (!stricmp(name, it->tex.name))
		{
			// This'll do.
			return it;
		}
	}
	// Not found, add a new definition.
	it = NewTexture();
	strcpy(it->tex.name, name);
	return it;
}

//===========================================================================
// PatchNumber
//	Always returns a valid index (creating new patches if necessary).
//===========================================================================
int PatchNumber(char *name)
{
	int i;
	patch_t *it;

	for (i = 0, it = plist.next; it != &plist; it = it->next, i++)
		if (!stricmp(name, it->name)) return i;
	it = NewPatch();
	strcpy(it->name, name);
	return i;
}

//===========================================================================
// DoTexture
//	Called after the name of the texture has been read (in 'token').
//===========================================================================
int DoTexture(void)
{
	def_t *def;
	int i;
	mappatch_t *pat = NULL;

	// Check that it's a valid texture name (and convert to upper case).
	strupr(token);
	if (strlen(token) > 8)
	{
		Message("Too long texture name '%s'.", token);
		return false;
	}
	else if (strlen(token) <= 2)
	{
		Message("Warning: Short texture name '%s'.", token);
	}
	// Get the definition and let's start filling up those infos!
	def = GetTexture(token);
	while (!endOfSource)
	{
		ReadToken();
		if (ISTOKEN(";")) break; // End of definition.
		// Flags.
		if (ISTOKEN("masked")) 
		{
			// Masked flag (ever needed?).
			def->tex.flags |= 1;
		}
		else if (ISTOKEN("flags"))
		{
			// Custom flags.
			ReadToken();
			def->tex.flags = strtol(token, 0, 0);
		}
		else if (ISTOKEN("misc"))
		{
			// Custom integer.
			ReadToken();
			def->tex.reserved = strtol(token, 0, 0);
		}
		else if (pat && ISTOKEN("arg1"))
		{
			// Custom data for stepdir.
			ReadToken();
			pat->reserved1 = (short) strtol(token, 0, 0);
		}
		else if (pat && ISTOKEN("arg2"))
		{
			// Custom data for 'colormap'.
			ReadToken();
			pat->reserved2 = (short) strtol(token, 0, 0);
		}
		else if (isdigit(token[0]) || (pat && token[0] == '-'))
		{
			i = strtol(token, 0, 0);
			if (pat) 
				pat->originX = i; 
			else 
				def->tex.width = i;
			ReadToken(); 
			if (!ISTOKEN(","))
			{
				Message("Expected a comma after %s.",
					pat? "patch origin X" : "texture width");
				return false;
			}
			ReadToken();
			i = strtol(token, 0, 0);
			if (pat)
				pat->originY = i;
			else
				def->tex.height = i;
		}
		else if (ISTOKEN("@"))
		{
			// A patch definition follows. 
			// Allocate a new patch entry from the def.
			i = def->tex.patchCount++;
			if (i == MAX_PATCHES)
			{
				Message("Too many patches (maximum is %i).", MAX_PATCHES);
				return false;
			}
			pat = def->tex.patches + i;
			// Initialize.
			memset(pat, 0, sizeof(*pat));
			pat->reserved1 = 1; // stepdir defaults to one.
			// The name of the patch comes first.
			ReadToken(); 
			strupr(token);
			if (strlen(token) > 8)
			{
				Message("Too long patch name '%s'.", token);
				return false;
			}
			pat->patch = PatchNumber(token);
		}
		else
		{
			Message("Bad token '%s'.", token);
			return false;
		}
	}
	return true;
}

//===========================================================================
// DoCompile
//	Returns true if the compilation was a success.
//===========================================================================
int DoCompile(void)
{
	while (!endOfSource)
	{
		ReadToken();
		if (ISTOKEN("")) break;
		// Keywords.
		if (ISTOKEN("%")) 
		{
			RET_FAIL( DoKeyword() );
		}
		else 
		{
			// It must be a texture definition. 
			RET_FAIL( DoTexture() );
		}
	}
	return true;
}

//===========================================================================
// Compile
//===========================================================================
void Compile(char *fileName)
{
	FILE *file;
	int length;

	// Try to first open the file "as is".
	strcpy(sourceFileName, fileName);
	if ((file = fopen(sourceFileName, "rb")) == NULL)
	{
		// OK, what about adding an extension?
		strcat(sourceFileName, ".tx");
		if ((file = fopen(sourceFileName, "rb")) == NULL)
		{
			perror(fileName);
			return;
		}
	}
	printf("Compiling %s...\n", sourceFileName);

	// Load in the defs.
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	rewind(file);
	sourcePos = source = new unsigned char[length + 1];
    if (!fread(source, length, 1, file))
    {
        perror(fileName);
        return;
    }
	source[length] = 0;
	fclose(file);

	// Let's compile it!
	InitCompiler();
	if (!DoCompile())
	{
		printf("Compilation of %s was aborted!\n", sourceFileName);
	}
	CloseCompiler();

	// Free the buffer, we don't need it any more.
	delete [] source;
}

//===========================================================================
// OutShort
//===========================================================================
void OutShort(FILE *file, short num)
{
	fwrite(&num, 2, 1, file);
}

//===========================================================================
// OutLong
//===========================================================================
void OutLong(FILE *file, int num)
{
	fwrite(&num, 4, 1, file);
}

//===========================================================================
// Out
//===========================================================================
void Out(FILE *file, void *data, int size)
{
	fwrite(data, size, 1, file);
}

//===========================================================================
// WritePatchNames
//	Creates PNAMES.LMP.
//===========================================================================
void WritePatchNames(void)
{
	char fn[256];
	FILE *file;
	int count;
	patch_t *it;

	if (plist.next == &plist) return;

	strcpy(fn, "PNAMES.LMP");
	if ((file = fopen(fn, "wb")) == NULL)
	{
		perror(fn);
		return;
	}
	OutLong(file, 0); // Number of patches, update later.
	for (count = 0, it = plist.next; it != &plist; it = it->next, count++)
		Out(file, it->name, 8);
	rewind(file);
	OutLong(file, count);
	fclose(file);
	// We're done.
	printf("%i patch name%s written to %s.\n", count, PLURAL_S(count), fn);
}

//===========================================================================
// WriteTextureGroup
//===========================================================================
void WriteTextureGroup(int idx)
{
	char fn[256];
	FILE *file;
	int count, dirStart;
	int pos, i, k;
	def_t *it;

	sprintf(fn, "TEXTURE%i.LMP", idx + 1);
	// Count the number of texture definitions in the group.
	for (count = 0, it = root[idx].next; it != &root[idx]; 
		it = it->next, count++) {}
	if (!count) return; // Nothing to write!
	if ((file = fopen(fn, "wb")) == NULL)
	{
		perror(fn);
		return;
	}
	OutLong(file, count); // Number of textures.
	// The directory (uninitialized).
	dirStart = ftell(file);
	for (i = 0; i < count; i++) OutLong(file, 0);
	// Write each texture def and update the directory entry.
	for (i = 0, it = root[idx].next; it != &root[idx]; it = it->next, i++)
	{
		pos = ftell(file);
		// Go back to the directory.
		fseek(file, dirStart + i*4, SEEK_SET);
		OutLong(file, pos);
		// Return to the def's position.
		fseek(file, pos, SEEK_SET);
		Out(file, it->tex.name, 8);
		OutLong(file, it->tex.flags);
		OutShort(file, it->tex.width);
		OutShort(file, it->tex.height);
		OutLong(file, it->tex.reserved);
		OutShort(file, it->tex.patchCount);
		// Write the patches.
		for (k = 0; k < it->tex.patchCount; k++)
		{
			OutShort(file, it->tex.patches[k].originX);
			OutShort(file, it->tex.patches[k].originY);
			OutShort(file, it->tex.patches[k].patch);
			OutShort(file, it->tex.patches[k].reserved1);
			OutShort(file, it->tex.patches[k].reserved2);
		}
	}
	fclose(file);
	// We're done.
	printf("%i texture%s written to %s.\n", count, PLURAL_S(count), fn);
}

//===========================================================================
// WriteLumps
//===========================================================================
void WriteLumps(void)
{
	WritePatchNames();
	for (int i = 0; i < NUM_GROUPS; i++) 
		WriteTextureGroup(i);
}

//===========================================================================
// main
//===========================================================================
int main(int argc, char **argv)
{
	int i;

	PrintBanner();
	
	myargc = argc;
	myargv = argv;
	if (CheckOption("-f")) fullImport = true;
	
	if (argc == 1)
	{
		PrintUsage();
		return 0;
	}
	InitData();
	// Go through each command line option and process them.
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-') // This is an option.
		{
			if (!stricmp(argv[i], "-i")) // Import (decompile).
			{
				if (i + 2 >= argc) 
				{
					printf("Too few parameters for import.\n");
					return 1;
				}
				Import(argv[i + 1], argv[i + 2]);
				i += 2;
			}
			continue; 
		}
		// Try to open this TX source and parse it.
		Compile(argv[i]);
	}
	WriteLumps();
	CloseData();
	return 0;
}
