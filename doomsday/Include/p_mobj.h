#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#define DEFAULT_FRICTION	0xe800

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
	// If your compiler doesn't support this, you could convert 
	// ddmobj_base_s into a #define.
	struct ddmobj_base_s;
} mobj_t;

extern int tmfloorz, tmceilingz;

void P_SetState(mobj_t *mo, int statenum);
void P_XYMovement(mobj_t* mo);
void P_XYMovement2(mobj_t* mo, struct playerstate_s *playmove);
void P_ZMovement(mobj_t* mo);
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckPosition2(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_ChangeSector(sector_t *sector);

#endif