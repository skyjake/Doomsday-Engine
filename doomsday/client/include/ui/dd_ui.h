/**
 * @file dd_ui.h
 * User interface.
 * @ingroup gui
 *
 * @todo The meaning of the "fi_" prefix is unclear; rename to "ui_"?
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DOOMSDAY_GUI_H
#define DOOMSDAY_GUI_H

#include <de/animator.h>
#include "api_fontrender.h"
#include "api_svg.h"

#include "Material"

/**
 * @defgroup gui GUI
 */
///@{

/// Numeric identifiers of predefined colors.
enum {
    UIC_TEXT,
    UIC_TITLE,
    UIC_SHADOW,
    UIC_BG_LIGHT,
    UIC_BG_MEDIUM,
    UIC_BG_DARK,
    UIC_BRD_HI,
    UIC_BRD_MED,
    UIC_BRD_LOW,
    UIC_HELP,
    NUM_UI_COLORS
};

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
    fi_obtype_e     type; /* Type of the object. */ \
    struct fi_page_s *page; /* Owning page */ \
    int             group; \
    struct { \
        char looping:1; /* Animation will loop. */ \
    } flags; \
    boolean         animComplete; /* Animation finished (or repeated). */ \
    fi_objectid_t   id; /* Unique id of the object. */ \
    fi_objectname_t name; /* Nice name. */ \
    animatorvector3_t pos; \
    animator_t      angle; \
    animatorvector3_t scale; \
    void          (*drawer) (struct fi_object_s *, float const offset[3]); \
    void          (*thinker) (struct fi_object_s *obj);

#ifdef __cplusplus
extern "C" {
#endif

struct fi_object_s *FI_NewObject(fi_obtype_e type, char const *name);

void FI_DeleteObject(struct fi_object_s* obj);

struct fi_object_s *FI_Object(fi_objectid_t id);

/**
 * @return  Page which presently owns this object. Can be @c NULL if the
 *     object is presently being moved from one page to another.
 */
struct fi_page_s *FIObject_Page(struct fi_object_s *obj);

/**
 * Change/setup a reverse link between this object and it's owning page.
 * @note Changing this relationship here does not complete the task of
 * linking an object with a page (not enough information). It is therefore
 * the page's responsibility to call this when adding/removing objects.
 */
void FIObject_SetPage(struct fi_object_s *obj, struct fi_page_s *page);

/**
 * A page is an aggregate visual/visual container.
 *
 * @ingroup video
 */
typedef struct {
    struct fi_object_s **vector;
    uint size;
} fi_object_collection_t;

typedef struct fi_page_s {
    struct fi_page_flags_s {
        char hidden:1; /// Currently hidden (not drawn).
        char paused:1; /// Currently paused (does not tic).
        char showBackground:1; /// Draw the background?
    } flags;

    /// Child visuals (objects) visible on this page.
    /// @note Unlike de::Visual the children are not owned by the page.
    fi_object_collection_t _objects;

    /// Offset the world origin.
    animatorvector3_t _offset;

    void (*drawer) (struct fi_page_s *page);
    void (*ticker) (struct fi_page_s *page, timespan_t ticLength);

    /// Pointer to the previous page, if any.
    struct fi_page_s *previous;

    struct fi_page_background_s {
        Material *material;
        animatorvector4_t topColor;
        animatorvector4_t bottomColor;
    } _bg;

    animatorvector4_t _filter;
    animatorvector3_t _preColor[FIPAGE_NUM_PREDEFINED_COLORS];
    fontid_t _preFont[FIPAGE_NUM_PREDEFINED_FONTS];

    uint _timer;
} fi_page_t;

fi_page_t *FI_NewPage(fi_page_t *prevPage);
void FI_DeletePage(fi_page_t *page);

/// Draws the page if not hidden.
void FIPage_Drawer(fi_page_t *page);

/// Tic the page if not paused.
void FIPage_Ticker(fi_page_t *page, timespan_t ticLength);

/// Adds UI object to the page if not already present. Page takes ownership.
struct fi_object_s *FIPage_AddObject(fi_page_t *page, struct fi_object_s *obj);

/// Removes UI object from the page if present. Page gives up ownership.
struct fi_object_s *FIPage_RemoveObject(fi_page_t *page, struct fi_object_s *obj);

/// Is the UI object present on the page?
boolean FIPage_HasObject(fi_page_t *page, struct fi_object_s *obj);

/// Current background Material.
Material *FIPage_BackgroundMaterial(fi_page_t *page);

/// Sets the 'is-visible' state.
void FIPage_MakeVisible(fi_page_t *page, boolean yes);

/// Sets the 'is-paused' state.
void FIPage_Pause(fi_page_t *page, boolean yes);

/// Sets the background Material.
void FIPage_SetBackgroundMaterial(fi_page_t *page, Material *mat);

/// Sets the background top color.
void FIPage_SetBackgroundTopColor(fi_page_t *page, float red, float green, float blue, int steps);

/// Sets the background top color and alpha.
void FIPage_SetBackgroundTopColorAndAlpha(fi_page_t *page, float red, float green, float blue, float alpha, int steps);

/// Sets the background bottom color.
void FIPage_SetBackgroundBottomColor(fi_page_t *page, float red, float green, float blue, int steps);

/// Sets the background bottom color and alpha.
void FIPage_SetBackgroundBottomColorAndAlpha(fi_page_t *page, float red, float green, float blue, float alpha, int steps);

/// Sets the x-axis component of the world offset.
void FIPage_SetOffsetX(fi_page_t *page, float x, int steps);

/// Sets the y-axis component of the world offset.
void FIPage_SetOffsetY(fi_page_t *page, float y, int steps);

/// Sets the world offset.
void FIPage_SetOffsetXYZ(fi_page_t *page, float x, float y, float z, int steps);

/// Sets the filter color and alpha.
void FIPage_SetFilterColorAndAlpha(fi_page_t *page, float red, float green, float blue, float alpha, int steps);

/// Sets a predefined color.
void FIPage_SetPredefinedColor(fi_page_t *page, uint idx, float red, float green, float blue, int steps);

/// @return  Animator which represents the identified predefined color.
animatorvector3_t const *FIPage_PredefinedColor(fi_page_t *page, uint idx);

/// Sets a predefined font.
void FIPage_SetPredefinedFont(fi_page_t *page, uint idx, fontid_t font);

/// @return  Unique identifier of the predefined font.
fontid_t FIPage_PredefinedFont(fi_page_t *page, uint idx);

typedef enum {
    PFT_MATERIAL,
    PFT_PATCH,
    PFT_RAW, /// "Raw" graphic or PCX lump.
    PFT_XIMAGE /// External graphics resource.
} fi_pic_type_t;

/**
 * Rectangle/Image sequence object.
 *
 * @ingroup video
 */
typedef struct fidata_pic_frame_s {
    int tics;
    fi_pic_type_t type;
    struct fidata_pic_frame_flags_s {
        char flip:1;
    } flags;
    union {
        Material *material;
        patchid_t patch;
        lumpnum_t lumpNum;
        DGLuint tex;
    } texRef;
    short sound;
} fidata_pic_frame_t;

typedef struct fidata_pic_s {
    FIOBJECT_BASE_ELEMENTS()
    int tics;
    uint curFrame;
    fidata_pic_frame_t **frames;
    uint numFrames;

    animatorvector4_t color;

    /// For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;
} fidata_pic_t;

void FIData_PicThink(struct fi_object_s *pic);
void FIData_PicDraw(struct fi_object_s *pic, float const offset[3]);
uint FIData_PicAppendFrame(struct fi_object_s *pic, fi_pic_type_t type, int tics, void *texRef, short sound, boolean flagFlipH);
void FIData_PicClearAnimation(struct fi_object_s *pic);

/**
 * Text object.
 *
 * @ingroup video
 */
typedef struct fidata_text_s {
    FIOBJECT_BASE_ELEMENTS()
    animatorvector4_t color;
    uint pageColor; /// Identifier of the owning page's predefined color. Zero means use our own color.
    int alignFlags; /// @ref alignmentFlags
    short textFlags; /// @ref drawTextFlags
    int scrollWait, scrollTimer; /// Automatic scrolling upwards.
    size_t cursorPos;
    int wait, timer;
    float lineHeight;
    fontid_t fontNum;
    char *text;
} fidata_text_t;

void FIData_TextThink(struct fi_object_s *text);
void FIData_TextDraw(struct fi_object_s *text, float const offset[3]);
void FIData_TextCopy(struct fi_object_s *text, char const *str);
void FIData_TextSetFont(struct fi_object_s *text, fontid_t fontNum);
void FIData_TextSetPreColor(struct fi_object_s* text, uint id);
void FIData_TextSetColor(struct fi_object_s *text, float red, float green, float blue, int steps);
void FIData_TextSetAlpha(struct fi_object_s *text, float alpha, int steps);
void FIData_TextSetColorAndAlpha(struct fi_object_s *text, float red, float green, float blue, float alpha, int steps);
void FIData_TextAccelerate(struct fi_object_s *obj);

/**
 * @return Length of the current text as a counter.
 */
size_t FIData_TextLength(struct fi_object_s *text);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DOOMSDAY_GUI_H */
