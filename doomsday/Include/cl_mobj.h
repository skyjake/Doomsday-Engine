//===========================================================================
// CL_MOBJ.H
//===========================================================================
#ifndef __DOOMSDAY_CLIENT_MOBJ_H__
#define __DOOMSDAY_CLIENT_MOBJ_H__

void Cl_InitClientMobjs();
void Cl_Reset(void);
void Cl_DestroyClientMobjs();
clmobj_t *Cl_CreateMobj(thid_t id);
void Cl_DestroyMobj(clmobj_t *cmo);
boolean Cl_MobjIterator(boolean (*callback)(clmobj_t*, void*), void *parm);
void Cl_PredictMovement(void);
void Cl_UnsetThingPosition(clmobj_t *cmo);
void Cl_SetThingPosition(clmobj_t *cmo);
int Cl_ReadMobjDelta(void);
void Cl_ReadMobjDelta2(boolean allowCreate, boolean skip);
void Cl_ReadNullMobjDelta2(boolean skip);
clmobj_t *Cl_FindMobj(thid_t id);
void Cl_CheckMobj(clmobj_t *cmo, boolean justCreated);
void Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags);

#endif 