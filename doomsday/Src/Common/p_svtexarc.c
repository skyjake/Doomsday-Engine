
//**************************************************************************
//**
//** P_SVTEXARC.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#include "doomdef.h"
#include "r_local.h"
#elif __JHERETIC__
#include "Doomdef.h"
#else // __JHEXEN__
#include "h2def.h"
#include "r_local.h"
#endif

#include "p_svtexarc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// Savegame I/O:
#if !__JHEXEN__
void SV_Write(void *data, int len);
void SV_WriteShort(short val);
#else
void StreamOutBuffer(void *buffer, int size);
void StreamOutWord(unsigned short val);
#endif

void SV_Read(void *data, int len);
short SV_ReadShort();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

texarchive_t flat_archive;
texarchive_t tex_archive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// SV_PrepareTexture
//	Called for every texture and flat in the level before saving by
//	Sv_InitTextureArchives.
//==========================================================================
void SV_PrepareTexture(int tex, boolean isflat, texarchive_t *arc)
{
	int c;
	char name[9];

	// Get the name of the texture/flat.
	if(isflat) 
		strcpy(name, W_CacheLumpNum(tex, PU_GETNAME));
	else
	{
		strncpy(name, R_TextureNameForNum(tex), 8);
		name[8] = 0;
	}
	// Has this already been registered?
	for(c=0; c<arc->count; c++)
	{
		if(!stricmp(arc->table[c].name, name))
		{
			// Yes, skip it...
			break;
		}
	}	
	if(c == arc->count)
		strcpy(arc->table[arc->count++].name, name);
}

//==========================================================================
// SV_InitTextureArchives
//	Initializes the texture and flat archives (translation tables).
//	Must be called before saving. The tables are written before any
//	world data is saved.
//==========================================================================
void SV_InitTextureArchives(void)
{
	int	i;
	sector_t *sect;
	side_t *sid;

	// Init flats.
	flat_archive.count = 0;
	for(sect=sectors, i=0; i<numsectors; i++, sect++)
	{
		SV_PrepareTexture(sect->floorpic, true, &flat_archive);
		SV_PrepareTexture(sect->ceilingpic, true, &flat_archive);
	}
	// Init textures.
	tex_archive.count = 0;
	for(sid=sides, i=0; i<numsides; i++, sid++)
	{
		SV_PrepareTexture(sid->midtexture, false, &tex_archive);
		SV_PrepareTexture(sid->toptexture, false, &tex_archive);
		SV_PrepareTexture(sid->bottomtexture, false, &tex_archive);
	}
}

//==========================================================================
// SV_SearchArchive
//==========================================================================
unsigned short SV_SearchArchive(texarchive_t *arc, char *name)
{
	int i;

	for(i=0; i<arc->count; i++)
		if(!stricmp(arc->table[i].name, name))
			return i;
	// Not found?!!!
	return 0;
}

//==========================================================================
// SV_TextureArchiveNum
//	Returns the archive number of the given texture.
//	It will be written to the savegame file.
//==========================================================================
unsigned short SV_TextureArchiveNum(int texnum)
{
	char name[9];
	
	strncpy(name, R_TextureNameForNum(texnum), 8);
	name[8] = 0;
	return SV_SearchArchive(&tex_archive, name);
}

//==========================================================================
// SV_TextureArchiveNum
//	Returns the archive number of the given flat.
//	It will be written to the savegame file.
//==========================================================================
unsigned short SV_FlatArchiveNum(int flatnum)
{
	return SV_SearchArchive(&flat_archive, W_CacheLumpNum(flatnum, 
		PU_GETNAME));
}

//==========================================================================
// SV_GetArchiveFlat
//==========================================================================
int SV_GetArchiveFlat(int archivenum)
{
	return R_FlatNumForName(flat_archive.table[archivenum].name);
}

//==========================================================================
// SV_GetArchiveTexture
//==========================================================================
int SV_GetArchiveTexture(int archivenum)
{
	return R_TextureNumForName(tex_archive.table[archivenum].name);
}

//==========================================================================
// SV_WriteTexArchive
//==========================================================================
void SV_WriteTexArchive(texarchive_t *arc)
{
	int i;
	
#if !__JHEXEN__
	SV_WriteShort(arc->count);
#else
	StreamOutWord(arc->count);
#endif
	for(i=0; i<arc->count; i++)
	{
#if !__JHEXEN__
		SV_Write(arc->table[i].name, 8);
#else
		StreamOutBuffer(arc->table[i].name, 8);
#endif
	}
}

//==========================================================================
// SV_ReadTexArchive
//==========================================================================
void SV_ReadTexArchive(texarchive_t *arc)
{
	int i;

	arc->count = SV_ReadShort();
	for(i=0; i<arc->count; i++)
	{
		SV_Read(arc->table[i].name, 8);
		arc->table[i].name[8] = 0;
	}
}

//==========================================================================
// SV_WriteTextureArchive
//==========================================================================
void SV_WriteTextureArchive(void)
{
	SV_WriteTexArchive(&flat_archive);
	SV_WriteTexArchive(&tex_archive);
}

//==========================================================================
// SV_ReadTextureArchive
//==========================================================================
void SV_ReadTextureArchive(void)
{
	SV_ReadTexArchive(&flat_archive);
	SV_ReadTexArchive(&tex_archive);
}

