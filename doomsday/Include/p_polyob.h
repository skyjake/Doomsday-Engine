#ifndef __DOOMSDAY_POLYOB_H__
#define __DOOMSDAY_POLYOB_H__

//==========================================================================
// Data
//==========================================================================
extern byte *polyobjs;		// list of all poly-objects on the level
extern int po_NumPolyobjs;

//==========================================================================
// Functions
//==========================================================================
void	PO_SetCallback(void (*func)(mobj_t*, void*, void*));
boolean	PO_MovePolyobj(int num, int x, int y);
boolean	PO_RotatePolyobj(int num, angle_t angle);
void	PO_UnLinkPolyobj(polyobj_t *po);
void	PO_LinkPolyobj(polyobj_t *po);
int		PO_GetNumForDegen(void *degenMobj);

#endif