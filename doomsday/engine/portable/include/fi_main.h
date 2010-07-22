/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_INFINE_MAIN_H
#define LIBDENG_INFINE_MAIN_H

#include "finaleinterpreter.h"

// We'll use the base template directly as our object.
typedef struct fi_object_s {
    FIOBJECT_BASE_ELEMENTS()
} fi_object_t;

typedef struct fi_namespace_s {
    uint            num;
    fi_object_t**   vector;
} fi_namespace_t;

typedef enum {
    FVT_INT,
    FVT_FLOAT,
    FVT_SCRIPT_STRING, // ptr points to a char*, which points to the string.
    FVT_OBJECT
} fi_operand_type_t;

typedef struct {
    fi_operand_type_t type;
    union {
        int         integer;
        float       flt;
        const char* cstring;
        fi_object_t* obj;
    } data;
} fi_operand_t;

/**
 * @defgroup playsimServerFinaleFlags Play-simulation Server-side Finale Flags.
 *
 * Packet: PSV_FINALE Finale flags. Used with GPT_FINALE and GPT_FINALE2
 */
/*@{*/
#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04   // Script included.
#define FINF_AFTER          0x08   // Otherwise before.
#define FINF_SKIP           0x10
#define FINF_OVERLAY        0x20   // Otherwise before (or after).
/*@}*/

#define FINALE_SCRIPT_EXTRADATA_SIZE      gx.finaleConditionsSize

extern float fiDefaultTextRGB[];

void                FI_Register(void);

void                FI_Init(void);
void                FI_Shutdown(void);
int                 FI_Responder(ddevent_t* ev);
void                FI_Drawer(void);

void                FI_Ticker(timespan_t time);

int                 FI_SkipRequest(void);
boolean             FI_CmdExecuted(void);

fidata_text_t*      P_CreateText(fi_objectid_t id, const char* name);
void                P_DestroyText(fidata_text_t* text);

fidata_pic_t*       P_CreatePic(fi_objectid_t id, const char* name);
void                P_DestroyPic(fidata_pic_t* pic);

fi_namespace_t*     FI_ScriptNamespace(void);
finaleinterpreter_t* FI_ScriptInterpreter(void);

void*               FI_ScriptExtraData(void);

fi_objectid_t       FI_FindObjectIdForName(fi_namespace_t* names, const char* name, fi_obtype_e type);
boolean             FI_ObjectInNamespace(fi_namespace_t* names, fi_object_t* obj);
fi_object_t*        FI_AddObjectInNamespace(fi_namespace_t* names, fi_object_t* obj);
fi_object_t*        FI_RemoveObjectInNamespace(fi_namespace_t* names, fi_object_t* obj);

fi_object_t*        FI_NewObject(fi_obtype_e type, const char* name);
void                FI_DeleteObject(fi_object_t* obj);

void                FI_SetBackground(struct material_s* mat);
void                FI_SetBackgroundColor(float red, float green, float blue, int steps);
void                FI_SetBackgroundColorAndAlpha(float red, float green, float blue, float alpha, int steps);

void                FI_SetImageOffsetX(float x, int steps);
void                FI_SetImageOffsetY(float y, int steps);

void                FI_SetFilterColorAndAlpha(float red, float green, float blue, float alpha, int steps);

void                FI_SetPredefinedColor(uint idx, float red, float green, float blue, int steps);

#endif /* LIBDENG_INFINE_MAIN_H */
