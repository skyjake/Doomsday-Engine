//===========================================================================
// CON_CONFIG.H
//===========================================================================
#ifndef __DOOMSDAY_CONSOLE_CONFIG_H__
#define __DOOMSDAY_CONSOLE_CONFIG_H__

#include "con_decl.h"

int Con_ParseCommands(char *fileName, int setdefault);
void Con_SaveDefaults(void);
boolean Con_WriteState(const char *fileName);

D_CMD( WriteConsole );

#endif 