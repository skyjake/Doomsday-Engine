
//**************************************************************************
//**
//** P_XGSAVE.C
//**
//** Saving and loading routines for the XG data.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef __JDOOM__
#include "doomdef.h"
#include "p_local.h"
#include "r_local.h"
#include "r_defs.h"
#endif

#ifdef __JHERETIC__
#include "Doomdef.h"
#include "P_local.h"
#endif

#include "p_saveg.h"
#include "p_xg.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void SV_WriteXGLine(line_t *li)
{
	xgline_t *xg = li->xg;
	linetype_t *info = &xg->info;

	// Version byte.
	SV_WriteByte(1);

	// Remember, savegames are applied on top of an initialized level.
	// No strings are saved, because they are all const strings 
	// defined either in the level's DDXGDATA lump or a DED file.
	// During loading, XL_SetLineType is called with the id in the savegame.

	SV_WriteLong(info->id);
	SV_WriteLong(info->act_count);

	SV_WriteByte(xg->active);
	SV_WriteByte(xg->disabled);
	SV_WriteLong(xg->timer);
	SV_WriteLong(xg->ticker_timer);
	SV_WriteShort(SV_ThingArchiveNum(xg->activator));
	SV_WriteLong(xg->idata);
	SV_WriteFloat(xg->fdata);
	SV_WriteLong(xg->chidx);
	SV_WriteFloat(xg->chtimer);
}

void SV_ReadXGLine(line_t *li)
{
	xgline_t *xg;

	// Read version.
	SV_ReadByte();

	// This'll set all the correct string pointers and other data.
	XL_SetLineType(li, SV_ReadLong());
	
	if(!li->xg) Con_Error("SV_ReadXGLine: Bad XG line!\n");

	xg = li->xg;

	xg->info.act_count = SV_ReadLong();
	xg->active = SV_ReadByte();
	xg->disabled = SV_ReadByte();
	xg->timer = SV_ReadLong();
	xg->ticker_timer = SV_ReadLong();

	// Will be updated later.
	xg->activator = (void*)(unsigned int) SV_ReadShort(); 

	xg->idata = SV_ReadLong();
	xg->fdata = SV_ReadFloat();
	xg->chidx = SV_ReadLong();
	xg->chtimer = SV_ReadFloat();
}

// The function must belong to the specified xgsector.
void SV_WriteXGFunction(xgsector_t *xg, function_t *fn)
{
	// Version byte.
	SV_WriteByte(1);

	SV_WriteLong(fn->flags);
	SV_WriteShort(fn->pos);
	SV_WriteShort(fn->repeat);
	SV_WriteShort(fn->timer);
	SV_WriteShort(fn->maxtimer);
	SV_WriteFloat(fn->value);
	SV_WriteFloat(fn->oldvalue);
}

void SV_ReadXGFunction(xgsector_t *xg, function_t *fn)
{
	// Version byte.
	SV_ReadByte();
	
	fn->flags = SV_ReadLong();
	fn->pos = SV_ReadShort();
	fn->repeat = SV_ReadShort();
	fn->timer = SV_ReadShort();
	fn->maxtimer = SV_ReadShort();
	fn->value = SV_ReadFloat();
	fn->oldvalue = SV_ReadFloat();
}

void SV_WriteXGSector(struct sector_s *sec)
{
	xgsector_t *xg = sec->xg;
	sectortype_t *info = &xg->info;
	int i;
	
	// Version byte.
	SV_WriteByte(1);

	SV_WriteLong(info->id);
	SV_Write(info->count, sizeof(info->count));
	SV_Write(xg->chain_timer, sizeof(xg->chain_timer));
	SV_WriteLong(xg->timer);
	SV_WriteByte(xg->disabled);
	for(i=0; i<3; i++) SV_WriteXGFunction(xg, &xg->rgb[i]);
	for(i=0; i<2; i++) SV_WriteXGFunction(xg, &xg->plane[i]);
	SV_WriteXGFunction(xg, &xg->light);
}

void SV_ReadXGSector(struct sector_s *sec)
{
	xgsector_t *xg;
	int i;

	// Version byte.
	SV_ReadByte();

	// This'll init all the data.
	XS_SetSectorType(sec, SV_ReadLong());
	xg = sec->xg;
	SV_Read(xg->info.count, sizeof(xg->info.count));
	SV_Read(xg->chain_timer, sizeof(xg->chain_timer));
	xg->timer = SV_ReadLong();
	xg->disabled = SV_ReadByte();
	for(i=0; i<3; i++) SV_ReadXGFunction(xg, &xg->rgb[i]);
	for(i=0; i<2; i++) SV_ReadXGFunction(xg, &xg->plane[i]);
	SV_ReadXGFunction(xg, &xg->light);
}

void SV_WriteXGPlaneMover(thinker_t *th)
{
	int i;

	xgplanemover_t *mov = (xgplanemover_t*) th;

	SV_WriteByte(1);	// Version.

	SV_WriteLong(mov->sector - sectors);
	SV_WriteByte(mov->ceiling);
	SV_WriteLong(mov->flags);
	
	i = mov->origin - lines;
	if(i < 0 || i >= numlines)	// Is it a real line?
		i = 0; // No...
	else
		i++;
	SV_WriteLong(i);	// Zero means there is no origin.

	SV_WriteLong(mov->destination);
	SV_WriteLong(mov->speed);
	SV_WriteLong(mov->crushspeed);
	SV_WriteLong(mov->setflat);
	SV_WriteLong(mov->setsector);
	SV_WriteLong(mov->startsound);
	SV_WriteLong(mov->endsound);
	SV_WriteLong(mov->movesound);
	SV_WriteLong(mov->mininterval);
	SV_WriteLong(mov->maxinterval);
	SV_WriteLong(mov->timer);
}

// Reads and adds the plane mover thinker.
void SV_ReadXGPlaneMover(void)
{
	xgplanemover_t *mov = Z_Malloc(sizeof(*mov), PU_LEVEL, 0);
	int i;

	memset(mov, 0, sizeof(*mov));

	SV_ReadByte();	// Version.

	mov->sector = sectors + SV_ReadLong();
	mov->ceiling = SV_ReadByte();
	mov->flags = SV_ReadLong();
	
	i = SV_ReadLong();
	if(i) mov->origin = lines + i-1;

	mov->destination = SV_ReadLong();
	mov->speed = SV_ReadLong();
	mov->crushspeed = SV_ReadLong();
	mov->setflat = SV_ReadLong();
	mov->setsector = SV_ReadLong();
	mov->startsound = SV_ReadLong();
	mov->endsound = SV_ReadLong();
	mov->movesound = SV_ReadLong();
	mov->mininterval = SV_ReadLong();
	mov->maxinterval = SV_ReadLong();
	mov->timer = SV_ReadLong();

	// Add it.
	mov->thinker.function = XS_PlaneMover;
	P_AddThinker(&mov->thinker);
}

/*
 * This is called after all thinkers have been loaded.
 * We need to set the correct pointers to the line activators.
 */
void XL_UnArchiveLines(void)
{
	line_t *line;
	mobj_t *activator;
	int i;

	for(i = 0, line = lines; i < numlines; i++, line++)
		if(line->xg)
		{
			activator = SV_GetArchiveThing((int)line->xg->activator);
			line->xg->activator = (activator? activator : &dummything);
		}
}
