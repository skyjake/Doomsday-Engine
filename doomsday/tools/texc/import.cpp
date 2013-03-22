
//**************************************************************************
//**
//** Import.cpp
//**
//** Doomsday Texture Compiler: WAD => TX conversion.
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

#define LINE_PATCHES	3

// TYPES -------------------------------------------------------------------

struct wadpatch_t
{
	short		originX;
	short		originY;
	short		patch;
	short		stepdir;
	short		colormap;
};

struct wadtexture_t
{
	char		name[8];
	int			masked;	
	short		width;
	short		height;
	int			obsolete;
	short		patchcount;
	wadpatch_t	patches[1];
};

struct patchname_t
{
	char		name[8];
};

struct patchdir_t
{
	int			count;
	patchname_t list[1];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// FindLump
//===========================================================================
int FindLump(const char *name, lumpinfo_t *lumps, int numLumps)
{
	for(int i = 0; i < numLumps; i++)
		if(!strnicmp(name, lumps[i].name, 8))
			return i;
	return -1;
}

//===========================================================================
// Import
//===========================================================================
void Import(char *wadFile, char *outFile)
{
	FILE			*file, *out;
	wadinfo_t		info;
	lumpinfo_t		*lumps;
	int				i, k, count, group, *dict;
	int				texCount = 0, groupCount = 0;
	patchdir_t		*dir = NULL;
	char			name[10], buf[128], *data;
	wadtexture_t	*tex;
	bool			showSize;
	int				linePatches = fullImport? 1 : LINE_PATCHES;

	if((file = fopen(wadFile, "rb")) == NULL)
	{
		perror(wadFile);
		return;
	}
	printf("Importing textures from %s.\n", wadFile);
	
    if(!fread(&info, sizeof(info), 1, file))
    {
        perror(wadFile);
        return;
    }
	lumps = new lumpinfo_t[info.numlumps];
	fseek(file, info.infotableofs, SEEK_SET);
    if(fread(lumps, sizeof(*lumps), info.numlumps, file) < size_t(info.numlumps))
    {
        perror(wadFile);
        return;
    }

	// Open the output file.
	if((out = fopen(outFile, "wt")) == NULL)
	{
		perror(outFile);
		return;
	}
    fprintf(out, "# Textures from %s (TexC " VERSION_STR ").\n\n", wadFile);
	
	// Read in PNAMES.
	i = FindLump("PNAMES", lumps, info.numlumps);
	if(i < 0) goto not_found;
	fseek(file, lumps[i].filepos, SEEK_SET);
	dir = (patchdir_t*) malloc(lumps[i].size);
    if(!fread(dir, lumps[i].size, 1, file))
    {
        perror(wadFile);
        return;
    }

	// Process the texture lumps.
	for(group = 1; group <= 2; group++)
	{
		sprintf(name, "TEXTURE%i", group);
		i = FindLump(name, lumps, info.numlumps);
		if(i < 0) continue;

		// Begin a new group.
		if(group > 1) fprintf(out, "\n%%Group %i\n\n", group);
		groupCount++;

		fseek(file, lumps[i].filepos, SEEK_SET);
		data = new char[lumps[i].size];
        if(!fread(data, lumps[i].size, 1, file))
        {
            perror(wadFile);
            return;
        }
		count = *(int*) data;
		texCount += count;
		dict = (int*) data + 1;
		for(i = 0; i < count; i++)
		{
			tex = (wadtexture_t*) (data + dict[i]);

			// Name of the texture.
			memset(buf, 0, sizeof(buf));
			strncpy(buf, tex->name, 8);
			fprintf(out, "%-8s ", buf);

			// Width and height.
			if(tex->width < 0 || tex->width > 1024
				|| tex->height < 0 || tex->height > 256)
			{
				// Print in hexadecimal; the values are suspicious.
				sprintf(buf, "0x%x,0x%x", tex->width, tex->height);
			}
			else
			{
				sprintf(buf, "%i,%i", tex->width, tex->height);
			}
			fprintf(out, tex->patchcount > linePatches? "%s" : "%-8s\t", buf);

			// Possible flags?
			if(tex->masked) fprintf(out, " masked");
			if(fullImport)
			{
				fprintf(out, " flags 0x%x misc %i ", 
					tex->masked, tex->obsolete);
			}
			
			// The patches.
			for(k = 0; k < tex->patchcount; k++)
			{
				fprintf(out, tex->patchcount > linePatches? 
					"\n\t @ " : (k? " @ " : "@ "));
				memset(buf, 0, 10);
				strncpy(buf, dir->list[tex->patches[k].patch].name, 8);
				showSize = tex->patches[k].originX || tex->patches[k].originY;
				fprintf(out, tex->patchcount > linePatches && showSize? 
					"%-8s" : "%s", buf);
				if(showSize)
				{
					fprintf(out, "%s%i,%i", 
						tex->patchcount > linePatches? "\t" : " ",						
						tex->patches[k].originX,
						tex->patches[k].originY);
				}
				if(fullImport)
				{
					fprintf(out, "%sarg1 %i arg2 %i", 
						tex->patchcount > linePatches? "\t" : " ",
						tex->patches[k].stepdir,
						tex->patches[k].colormap);
				}
			}
			fprintf(out, ";\n");
		}
		delete [] data;
	}
	printf("%s: %i textures in %i groups.\n", outFile, texCount, groupCount);
	
not_found:
	if(dir) free(dir);
	delete [] lumps;
	fclose(file);
	fclose(out);
}

