/* DE1: $Id: cl_def.h 3323 2006-06-13 17:25:33Z skyjake $
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * g_controls.h: Common code for game controls
 *
 */

#ifndef __COMMON_CONTROLS_H__
#define __COMMON_CONTROLS_H__

#include "p_ticcmd.h"
#include "mn_def.h"

#define CTLCFG_TYPE void

CTLCFG_TYPE SCControlConfig(int option, void *data);

// Control flags.
#define CLF_ACTION      0x1     // The control is an action (+/- in front).
#define CLF_REPEAT      0x2     // Bind down + repeat.

typedef struct {
    char   *command;            // The command to execute.
    int     flags;
    int     bindClass;          // Class it should be bound into
    int     defKey;             //
    int     defMouse;           // Zero means there is no default.
    int     defJoy;             //
} Control_t;

extern const Control_t controls[];

// Game registered bindClasses
enum {
    GBC_CLASS1 = NUM_DDBINDCLASSES,
    GBC_CLASS2,
    GBC_CLASS3,
    GBC_MENUHOTKEY,
    GBC_CHAT,
    GBC_MESSAGE
};

extern const Control_t *grabbing;

void        G_ControlRegister(void);
void        G_DefaultBindings(void);
void        G_BindClassRegistration(void);

void        G_BuildTiccmd(ticcmd_t *cmd, float elapsedTime);
void        G_MergeTiccmd(ticcmd_t *dest, ticcmd_t *src);

void        G_LookAround(void);
boolean     G_AdjustControlState(event_t* ev);
void        G_ResetMousePos(void);

#endif
