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

#define FI_NAME_MAX_LENGTH          32
typedef char fi_name_t[FI_NAME_MAX_LENGTH];
typedef fi_name_t fi_objectname_t;
typedef ident_t fi_objectid_t;

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
    fi_objectid_t   id; /* Unique id of the object. */ \
    fi_objectname_t name; /* Object names are unique among objects of the same type and spawned by the same script. */ \
    fi_obtype_e     type; /* Type of the object. */ \
    animatorvector3_t pos; \
    animator_t      angle; \
    animatorvector3_t scale;

/**
 * Rectangle/Image sequence.
 */
typedef struct fidata_pic_frame_s {
    int             tics;
    enum {
        PFT_MATERIAL,
        PFT_PATCH,
        PFT_RAW,            // "Raw" graphic or PCX lump.
        PFT_XIMAGE          // External graphics resource.
    } type;
    struct fidata_pic_frame_flags_s {
        char            flip:1;
    } flags;
    union {
        struct material_s* material;
        patchid_t       patch;
        lumpnum_t       lump;
        DGLuint         tex;
    } texRef;
    short           sound;
} fidata_pic_frame_t;

typedef struct fidata_pic_s {
    FIOBJECT_BASE_ELEMENTS()
    struct fidata_pic_flags_s {
        char            looping:1; // Frame sequence will loop.
    } flags;
    boolean         animComplete; // Animation finished (or repeated).
    int             tics;
    uint            curFrame;
    fidata_pic_frame_t** frames;
    uint            numFrames;

    animatorvector4_t color;

    // For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;
} fidata_pic_t;

void                FIData_PicThink(fidata_pic_t* pic);
void                FIData_PicDraw(fidata_pic_t* pic, float x, float y);
void                FIData_PicClearAnimation(fidata_pic_t* pic);

/**
 * Text.
 */
typedef struct fidata_text_s {
    FIOBJECT_BASE_ELEMENTS()
    animatorvector4_t color;
    short           textFlags; // @see drawTextFlags
    boolean         animComplete; // Animation finished (text-typein complete).
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
