
//**************************************************************************
//**
//** P_XGFILE.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef __JDOOM__
#  include "jDoom/doomdef.h"
#endif

#ifdef __JHERETIC__
#  include "jHeretic/Doomdef.h"
#endif

#ifdef __JSTRIFE__
#  include "jStrife/h2def.h"
#endif

#include "p_xg.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef enum {
	XGSEG_END,
	XGSEG_LINE,
	XGSEG_SECTOR
} xgsegenum_e;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean xgdatalumps = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FILE *file;
static byte *readptr;

static linetype_t *linetypes = 0;
static int num_linetypes = 0;

static sectortype_t *sectypes;
static int num_sectypes;

// CODE --------------------------------------------------------------------

void WriteByte(byte b)
{
	fputc(b, file);
}

void WriteShort(short s)
{
	fwrite(&s, 2, 1, file);
}

void WriteLong(long l)
{
	fwrite(&l, 4, 1, file);
}

void WriteFloat(float f)
{
	fwrite(&f, 4, 1, file);
}

void Write(void *data, int len)
{
	fwrite(data, len, 1, file);
}

void WriteString(char *str)
{
	int     len;

	if(!str)
	{
		WriteShort(0);
		return;
	}
	len = strlen(str);
	WriteShort(len);
	Write(str, len);
}

byte ReadByte()
{
	return *readptr++;
}

short ReadShort()
{
	short   s = *(short *) readptr;

	readptr += 2;
	// Swap the bytes.
	//s = (s<<8) + (s>>8);
	return s;
}

long ReadLong()
{
	long    l = *(long *) readptr;

	readptr += 4;
	// Swap the bytes.
	//l = (l<<24) + (l>>24) + ((l & 0xff0000) >> 8) + ((l & 0xff00) << 8);
	return l;
}

float ReadFloat()
{
	long    f = ReadLong();

	return *(float *) &f;
}

void Read(void *data, int len)
{
	memcpy(data, readptr, len);
	readptr += len;
}

// I could just return a pointer to the string, but that risks losing
// it somewhere. Now we can be absolutely sure it can't be lost.
void ReadString(char **str)
{
	int     len = ReadShort();

	if(!len)					// Null string?
	{
		*str = 0;
		return;
	}
	if(len < 0)
		Con_Error("ReadString: Bogus len!\n");
	// Allocate memory for the string.
	*str = Z_Malloc(len + 1, PU_STATIC, 0);
	memset(*str, 0, len + 1);
	Read(*str, len);
}

void XG_WriteTypes(FILE * f)
{
	int     i, k;
	int     linecount = 0, sectorcount = 0;
	linetype_t line;
	sectortype_t sec;

	file = f;

	// The first four four bytes are a header.
	// They will be updated with the real counts afterwards.
	i = 0;
	fwrite(&i, 4, 1, file);		// Number of lines & sectors (two shorts).

	// This is a very simple way to get the definitions.
	for(i = 1; i < 65536; i++)
	{
		if(!Def_Get(DD_DEF_LINE_TYPE, (char *) i, &line))
			continue;

		linecount++;

		// Write marker.
		WriteByte(XGSEG_LINE);

		WriteShort(line.id);
		WriteLong(line.flags);
		WriteLong(line.flags2);
		WriteLong(line.flags3);
		WriteShort(line.line_class);
		WriteByte(line.act_type);
		WriteShort(line.act_count);
		WriteFloat(line.act_time);
		WriteLong(line.act_tag);
		Write(line.aparm, sizeof(line.aparm));
		WriteFloat(line.ticker_start);
		WriteFloat(line.ticker_end);
		WriteLong(line.ticker_interval);
		WriteShort(line.act_sound);
		WriteShort(line.deact_sound);
		WriteShort(line.ev_chain);
		WriteShort(line.act_chain);
		WriteShort(line.deact_chain);
		WriteByte(line.wallsection);
		WriteShort(line.act_tex);
		WriteShort(line.deact_tex);
		WriteString(line.act_msg);
		WriteString(line.deact_msg);
		WriteFloat(line.texmove_angle);
		WriteFloat(line.texmove_speed);
		Write(line.iparm, sizeof(line.iparm));
		Write(line.fparm, sizeof(line.fparm));
		for(k = 0; k < DDLT_MAX_SPARAMS; k++)
			WriteString(line.sparm[k]);
	}

	// Then the sectors.
	for(i = 1; i < 65536; i++)
	{
		if(!Def_Get(DD_DEF_SECTOR_TYPE, (char *) i, &sec))
			continue;

		sectorcount++;

		// Write marker.
		WriteByte(XGSEG_SECTOR);

		WriteShort(sec.id);
		WriteLong(sec.flags);
		WriteLong(sec.act_tag);
		Write(sec.chain, sizeof(sec.chain));
		Write(sec.chain_flags, sizeof(sec.chain_flags));
		Write(sec.start, sizeof(sec.start));
		Write(sec.end, sizeof(sec.end));
		Write(sec.interval, sizeof(sec.interval));
		Write(sec.count, sizeof(sec.count));
		WriteShort(sec.ambient_sound);
		Write(sec.sound_interval, sizeof(sec.sound_interval));
		Write(sec.texmove_angle, sizeof(sec.texmove_angle));
		Write(sec.texmove_speed, sizeof(sec.texmove_speed));
		WriteFloat(sec.wind_angle);
		WriteFloat(sec.wind_speed);
		WriteFloat(sec.vertical_wind);
		WriteFloat(sec.gravity);
		WriteFloat(sec.friction);
		WriteString(sec.lightfunc);
		WriteShort(sec.light_interval[0]);
		WriteShort(sec.light_interval[1]);
		WriteString(sec.colfunc[0]);
		WriteString(sec.colfunc[1]);
		WriteString(sec.colfunc[2]);
		for(k = 0; k < 3; k++)
		{
			WriteShort(sec.col_interval[k][0]);
			WriteShort(sec.col_interval[k][1]);
		}
		WriteString(sec.floorfunc);
		WriteFloat(sec.floormul);
		WriteFloat(sec.flooroff);
		WriteShort(sec.floor_interval[0]);
		WriteShort(sec.floor_interval[1]);
		WriteString(sec.ceilfunc);
		WriteFloat(sec.ceilmul);
		WriteFloat(sec.ceiloff);
		WriteShort(sec.ceil_interval[0]);
		WriteShort(sec.ceil_interval[1]);
	}

	// Write the end marker.
	WriteByte(XGSEG_END);

	// Update header.
	rewind(file);
	fwrite(&linecount, 2, 1, file);
	fwrite(&sectorcount, 2, 1, file);
}

void XG_ReadXGLump(char *name)
{
	int     lump = W_CheckNumForName(name);
	void   *buffer;
	int     lc = 0, sc = 0, len, i;
	linetype_t *li;
	sectortype_t *sec;
	boolean done = false;

	if(lump < 0)
		return;					// No such lump.

	xgdatalumps = true;

	Con_Message("XG_ReadTypes: Reading XG types from DDXGDATA.\n");

	readptr = buffer = W_CacheLumpNum(lump, PU_STATIC);

	num_linetypes = ReadShort();
	num_sectypes = ReadShort();

	// Allocate the arrays.
	linetypes = Z_Malloc(len =
						 sizeof(*linetypes) * num_linetypes, PU_STATIC, 0);
	memset(linetypes, 0, len);

	sectypes = Z_Malloc(len = sizeof(*sectypes) * num_sectypes, PU_STATIC, 0);
	memset(sectypes, 0, len);

	while(!done)
	{
		// Get next segment.
		switch (ReadByte())
		{
		case XGSEG_END:
			done = true;
			break;

		case XGSEG_LINE:
			li = linetypes + lc++;
			// Read the def.
			li->id = ReadShort();
			li->flags = ReadLong();
			li->flags2 = ReadLong();
			li->flags3 = ReadLong();
			li->line_class = ReadShort();
			li->act_type = ReadByte();
			li->act_count = ReadShort();
			li->act_time = ReadFloat();
			li->act_tag = ReadLong();
			Read(li->aparm, sizeof(li->aparm));
			li->ticker_start = ReadFloat();
			li->ticker_end = ReadFloat();
			li->ticker_interval = ReadLong();
			li->act_sound = ReadShort();
			li->deact_sound = ReadShort();
			li->ev_chain = ReadShort();
			li->act_chain = ReadShort();
			li->deact_chain = ReadShort();
			li->wallsection = ReadByte();
			li->act_tex = ReadShort();
			li->deact_tex = ReadShort();
			ReadString(&li->act_msg);
			ReadString(&li->deact_msg);
			li->texmove_angle = ReadFloat();
			li->texmove_speed = ReadFloat();
			Read(li->iparm, sizeof(li->iparm));
			Read(li->fparm, sizeof(li->fparm));
			for(i = 0; i < DDLT_MAX_SPARAMS; i++)
				ReadString(&li->sparm[i]);
			break;

		case XGSEG_SECTOR:
			sec = sectypes + sc++;
			// Read the def.
			sec->id = ReadShort();
			sec->flags = ReadLong();
			sec->act_tag = ReadLong();
			Read(sec->chain, sizeof(sec->chain));
			Read(sec->chain_flags, sizeof(sec->chain_flags));
			Read(sec->start, sizeof(sec->start));
			Read(sec->end, sizeof(sec->end));
			Read(sec->interval, sizeof(sec->interval));
			Read(sec->count, sizeof(sec->count));
			sec->ambient_sound = ReadShort();
			Read(sec->sound_interval, sizeof(sec->sound_interval));
			Read(sec->texmove_angle, sizeof(sec->texmove_angle));
			Read(sec->texmove_speed, sizeof(sec->texmove_speed));
			sec->wind_angle = ReadFloat();
			sec->wind_speed = ReadFloat();
			sec->vertical_wind = ReadFloat();
			sec->gravity = ReadFloat();
			sec->friction = ReadFloat();
			ReadString(&sec->lightfunc);
			sec->light_interval[0] = ReadShort();
			sec->light_interval[1] = ReadShort();
			ReadString(&sec->colfunc[0]);
			ReadString(&sec->colfunc[1]);
			ReadString(&sec->colfunc[2]);
			for(i = 0; i < 3; i++)
			{
				sec->col_interval[i][0] = ReadShort();
				sec->col_interval[i][1] = ReadShort();
			}
			ReadString(&sec->floorfunc);
			sec->floormul = ReadFloat();
			sec->flooroff = ReadFloat();
			sec->floor_interval[0] = ReadShort();
			sec->floor_interval[1] = ReadShort();
			ReadString(&sec->ceilfunc);
			sec->ceilmul = ReadFloat();
			sec->ceiloff = ReadFloat();
			sec->ceil_interval[0] = ReadShort();
			sec->ceil_interval[1] = ReadShort();
			break;

		default:
			Con_Error("XG_ReadXGLump: Bad segment!\n");
		}
	}

	Z_Free(buffer);
}

// See if any line or sector types are saved in a DDXGDATA lump.
void XG_ReadTypes(void)
{
	num_linetypes = 0;
	num_sectypes = 0;
	if(linetypes)
		Z_Free(linetypes);
	if(sectypes)
		Z_Free(sectypes);
	linetypes = 0;
	sectypes = 0;

	XG_ReadXGLump("DDXGDATA");
}

linetype_t *XG_GetLumpLine(int id)
{
	int     i;

	for(i = 0; i < num_linetypes; i++)
		if(linetypes[i].id == id)
			return linetypes + i;
	return NULL;				// Not found.
}

sectortype_t *XG_GetLumpSector(int id)
{
	int     i;

	for(i = 0; i < num_sectypes; i++)
		if(sectypes[i].id == id)
			return sectypes + i;
	return NULL;				// Not found.
}
