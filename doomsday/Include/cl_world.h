//===========================================================================
// CL_WORLD.H
//===========================================================================
#ifndef __DOOMSDAY_CLIENT_WORLD_H__
#define __DOOMSDAY_CLIENT_WORLD_H__

void Cl_InitTranslations(void);
void Cl_InitMovers();
void Cl_RemoveMovers();

int Cl_ReadSectorDelta(void);
int Cl_ReadLumpDelta(void);
int Cl_ReadSideDelta(void);
int Cl_ReadPolyDelta(void);

void Cl_ReadSectorDelta2(boolean skip);
void Cl_ReadSideDelta2(boolean skip);
void Cl_ReadPolyDelta2(boolean skip);

#endif 