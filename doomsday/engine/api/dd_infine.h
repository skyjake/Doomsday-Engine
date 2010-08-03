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

/**
 * @defgroup infine InFine System
 *
 * The "In Fine" finale sequence system.
 */

/// Finale identifier. Used throughout the public API when referencing active Finales.
typedef ident_t finaleid_t;

/**
 * @defgroup finaleFlags Finale Flags.
 */
/*@{*/
#define FF_LOCAL            0x1 /// Local scripts are executed client-side.
/*@}*/

/**
 * Execute a set of Finale commands.
 * @param commands  One or more commands to be executed.
 * @param flags  @see finaleFlags.
 */
finaleid_t FI_Execute(const char* commands, int flags);

/**
 * @return  @c true iff the specified Finale is active.
 */
boolean FI_ScriptActive(finaleid_t id);

/**
 * @return  @see finaleFlags.
 */
int FI_ScriptFlags(finaleid_t id);

/**
 * Immediately halt command interpretation and mark the script for termination.
 */
void FI_ScriptTerminate(finaleid_t id);

/**
 * Suspend command interpretation.
 */
void FI_ScriptSuspend(finaleid_t id);

/**
 * Resume command interpretation. 
 */
void FI_ScriptResume(finaleid_t id);

/**
 * @return  @c true iff the specified Finale is currently suspended.
 */
boolean FI_ScriptSuspended(finaleid_t id);

/**
 * @return  @c true iff the skip request was successful.
 */
boolean FI_ScriptRequestSkip(finaleid_t id);

/**
 * @return  @c true iff command interpretation has begun.
 */
boolean FI_ScriptCmdExecuted(finaleid_t id);

/**
 * @return  @c true iff the "menu trigger" is currently active.
 */
boolean FI_ScriptIsMenuTrigger(finaleid_t id);

int FI_ScriptResponder(finaleid_t id, const void* ev);

// -------------------------------------------------------------------------
//
// Here follows all the new FIPage stuff.
// \todo Should be moved to a more suitable home.
//
// -------------------------------------------------------------------------

#include "dd_animator.h"
#include "dd_compositefont.h"

typedef ident_t fi_objectid_t;

#define FI_NAME_MAX_LENGTH          32
typedef char fi_name_t[FI_NAME_MAX_LENGTH];
typedef fi_name_t fi_objectname_t;

typedef enum {
    FI_NONE,
    FI_TEXT,
    FI_PIC
} fi_obtype_e;

struct fi_object_s;
struct fi_page_s;

/**
 * Base fi_objects_t elements. All objects MUST use this as their basis.
 *
 * @ingroup video
 */
#define FIOBJECT_BASE_ELEMENTS() \
    fi_objectid_t   id; /* Unique id of the object. */ \
    fi_objectname_t name; /* Nice name. */ \
    fi_obtype_e     type; /* Type of the object. */ \
    animatorvector3_t pos; \
    animator_t      angle; \
    animatorvector3_t scale;

struct fi_object_s* FI_NewObject(fi_obtype_e type, const char* name);
void FI_DeleteObject(struct fi_object_s* obj);
struct fi_object_s* FI_Object(fi_objectid_t id);

/**
 * A page is an aggregate visual/visual container.
 *
 * @ingroup video
 */
typedef struct {
    struct fi_object_s** vector;
    uint size;
} fi_object_collection_t;

typedef struct fi_page_s {
    struct fi_page_flags_s {
        char hidden:1; /// Currently hidden (not drawn).
        char paused:1; /// Currently paused (does not tic).
    } flags;

    /// Child visuals (objects) visible on this page.
    /// \note Unlike de::Visual the childern are not owned by the page.
    fi_object_collection_t _objects;

    struct material_s* _bgMaterial;
    animatorvector4_t _bgColor;
    animatorvector2_t _imgOffset;
    animatorvector4_t _filter;
    animatorvector3_t _textColor[9];

    uint _timer;
} fi_page_t;

fi_page_t* FI_NewPage(void);
void FI_DeletePage(fi_page_t* page);

/// Adds a UI object to the page if not already present.
struct fi_object_s* FIPage_AddObject(fi_page_t* page, struct fi_object_s* obj);

/// Removes a UI object from the page if present.
struct fi_object_s* FIPage_RemoveObject(fi_page_t* page, struct fi_object_s* obj);

/// Is the UI object present on the page?
boolean FIPage_HasObject(fi_page_t* page, struct fi_object_s* obj);

/// Current background Material.
struct material_s* FIPage_Background(fi_page_t* page);

/// Sets the 'is-visible' state.
void FIPage_MakeVisible(fi_page_t* page, boolean yes);

/// Sets the 'is-paused' state.
void FIPage_Pause(fi_page_t* page, boolean yes);

/// Sets the background Material.
void FIPage_SetBackground(fi_page_t* page, struct material_s* mat);

/// Sets the background color.
void FIPage_SetBackgroundColor(fi_page_t* page, float red, float green, float blue, int steps);

/// Sets the background color and alpha.
void FIPage_SetBackgroundColorAndAlpha(fi_page_t* page, float red, float green, float blue, float alpha, int steps);

/// Sets the x-axis component of the image offset.
void FIPage_SetImageOffsetX(fi_page_t* page, float x, int steps);

/// Sets the y-axis component of the image offset.
void FIPage_SetImageOffsetY(fi_page_t* page, float y, int steps);

/// Sets the image offset.
void FIPage_SetImageOffsetXY(fi_page_t* page, float x, float y, int steps);

/// Sets the filter color and alpha.
void FIPage_SetFilterColorAndAlpha(fi_page_t* page, float red, float green, float blue, float alpha, int steps);

/// Sets a predefined color.
void FIPage_SetPredefinedColor(fi_page_t* page, uint idx, float red, float green, float blue, int steps);

/**
 * Rectangle/Image sequence object.
 *
 * @ingroup video
 */
typedef struct fidata_pic_frame_s {
    int tics;
    enum {
        PFT_MATERIAL,
        PFT_PATCH,
        PFT_RAW, /// "Raw" graphic or PCX lump.
        PFT_XIMAGE /// External graphics resource.
    } type;
    struct fidata_pic_frame_flags_s {
        char flip:1;
    } flags;
    union {
        struct material_s* material;
        patchid_t patch;
        lumpnum_t lump;
        DGLuint tex;
    } texRef;
    short sound;
} fidata_pic_frame_t;

typedef struct fidata_pic_s {
    FIOBJECT_BASE_ELEMENTS()
    struct fidata_pic_flags_s {
        char looping:1; /// Frame sequence will loop.
    } flags;
    boolean animComplete; /// Animation finished (or repeated).
    int tics;
    uint curFrame;
    fidata_pic_frame_t** frames;
    uint numFrames;

    animatorvector4_t color;

    /// For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;
} fidata_pic_t;

void FIData_PicThink(fidata_pic_t* pic);
void FIData_PicDraw(fidata_pic_t* pic, const float offset[3]);
uint FIData_PicAppendFrame(fidata_pic_t* pic, int type, int tics, void* texRef, short sound, boolean flagFlipH);
void FIData_PicClearAnimation(fidata_pic_t* pic);

/**
 * Text object.
 *
 * @ingroup video
 */
typedef struct fidata_text_s {
    FIOBJECT_BASE_ELEMENTS()
    animatorvector4_t color;
    short textFlags; /// @see drawTextFlags
    boolean animComplete; /// Animation finished (text-typein complete).
    int scrollWait, scrollTimer; /// Automatic scrolling upwards.
    size_t cursorPos;
    int wait, timer;
    float lineheight;
    compositefontid_t font;
    char* text;
} fidata_text_t;

void FIData_TextThink(fidata_text_t* text);
void FIData_TextDraw(fidata_text_t* text, const float offset[3]);
void FIData_TextCopy(fidata_text_t* text, const char* str);

/**
 * @return Length of the current text as a counter.
 */
size_t FIData_TextLength(fidata_text_t* text);

#endif /* LIBDENG_API_INFINE_H */
