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

#ifndef LIBDENG_API_INFINE_H
#define LIBDENG_API_INFINE_H

#include "dd_animator.h"
#include "dd_compositefont.h"

typedef char fi_name_t[32];
typedef fi_name_t fi_objectname_t;

typedef enum infinemode_e {
    FIMODE_LOCAL,
    FIMODE_OVERLAY,
    FIMODE_BEFORE,
    FIMODE_AFTER
} finale_mode_t;

boolean         FI_Active(void);
boolean         FI_IsMenuTrigger(void);
void            FI_Reset(void);

void            FI_SetClientsideDefaultState(void* data);
void*           FI_GetClientsideDefaultState(void);

void            FI_ScriptBegin(const char* scriptSrc, finale_mode_t mode, int gameState, void* extraData);
void            FI_ScriptTerminate(void);

typedef enum {
    FI_NONE,
    FI_TEXT,
    FI_PIC
} fi_obtype_e;

struct fi_object_s;

// Base fi_objects_t elements. All objects MUST use this as their basis.
#define FIOBJECT_BASE_ELEMENTS() \
    fi_obtype_e     type; /* Type of the object. */ \
    fi_objectname_t name; \
    boolean         used; \
    animatorvector2_t pos; \
    animator_t      angle; \
    animatorvector4_t color; \
    animatorvector2_t scale;

/**
 * Image sequence.
 */
#define FIDATA_PIC_MAX_SEQUENCE     64

typedef struct fidata_pic_s {
    FIOBJECT_BASE_ELEMENTS()
    struct fidata_pic_flags_s {
        char            is_patch:1; // Raw image or patch.
        char            done:1; // Animation finished (or repeated).
        char            is_rect:1;
        char            is_ximage:1; // External graphics resource.
    } flags;

    int             seq, seqTimer;
    int             seqWait[FIDATA_PIC_MAX_SEQUENCE];
    int             tex[FIDATA_PIC_MAX_SEQUENCE];
    char            flip[FIDATA_PIC_MAX_SEQUENCE];
    short           sound[FIDATA_PIC_MAX_SEQUENCE];

    // For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;
} fidata_pic_t;

void                FIData_PicThink(fidata_pic_t* pic);
void                FIData_PicDraw(fidata_pic_t* pic, float x, float y);
void                FIData_PicClearAnimation(fidata_pic_t* pic);
void                FIData_PicDeleteXImage(fidata_pic_t* pic);

int                 FIData_PicNextFrame(fidata_pic_t* pic);
void                FIData_PicRotationOrigin(const fidata_pic_t* pic, float center[2]);

/**
 * Text.
 */
typedef struct fidata_text_s {
    FIOBJECT_BASE_ELEMENTS()
    struct fidata_text_flags_s {
        char            centered:1;
        char            all_visible:1;
    } flags;

    int             scrollWait, scrollTimer; // Automatic scrolling upwards.
    int             cursorPos;
    int             wait, timer;
    float           lineheight;
    compositefontid_t font;
    char*           text;
} fidata_text_t;

void                FIData_TextThink(fidata_text_t* text);
void                FIData_TextDraw(fidata_text_t* tex, float xOffset, float yOffset);
void                FIData_TextCopy(fidata_text_t* text, const char* str);
int                 FIData_TextLength(fidata_text_t* tex);

#endif /* LIBDENG_API_INFINE_H */
