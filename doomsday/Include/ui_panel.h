//===========================================================================
// UI_PANEL.H
//===========================================================================
#ifndef __DOOMSDAY_CONTROL_PANEL_H__
#define __DOOMSDAY_CONTROL_PANEL_H__

#include "con_decl.h"

extern int	panel_show_help;
extern int	panel_show_tips;

// Helpful handlers.
void CP_CvarSlider(ui_object_t *ob);
void CP_InitCvarSliders(ui_object_t *ob);

// Console commands.
D_CMD( OpenPanel );

#endif 