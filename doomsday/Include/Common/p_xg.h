#ifndef __EXTENDED_GENERAL_H__
#define __EXTENDED_GENERAL_H__

#include "p_xgline.h"
#include "p_xgsec.h"

// Called once, at post init.
void XG_ReadTypes(void);

// Init both XG lines and sectors. Called for each level.
void XG_Init(void);

// Thinks for XG lines and sectors.
void XG_Ticker(void);

void XG_WriteTypes(FILE *file);
void XG_ReadTypes(void);

linetype_t *XG_GetLumpLine(int id);
sectortype_t *XG_GetLumpSector(int id);

#endif