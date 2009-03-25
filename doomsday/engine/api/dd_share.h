/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * dd_share.h: Shared Macros and Constants
 *
 * Macros and constants used by the engine and games.
 */

#ifndef __DOOMSDAY_SHARED_H__
#define __DOOMSDAY_SHARED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "../portable/include/dd_version.h"
#include "dd_types.h"
#include "dd_maptypes.h"
#include "dd_gl.h"
#include "../portable/include/p_think.h" // \todo Not officially a public header file!
#include "../portable/include/def_share.h" // \todo Not officially a public header file!

//------------------------------------------------------------------------
//
// General Definitions and Macros
//
//------------------------------------------------------------------------

#define DDMAXPLAYERS        16

// The case-independent strcmps have different names.
#if WIN32
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#if UNIX
# define stricmp strcasecmp
# define strnicmp strncasecmp

/**
 * There are manual implementations for these string handling routines:
 */
char*           strupr(char *string);
char*           strlwr(char *string);
#endif

// We need to use _vsnprintf, _snprintf in Windows
#if WIN32
# if(_MSC_VER < 1500)
#   define vsnprintf _vsnprintf
# endif
# define snprintf _snprintf
#endif

    // Format checking for printf-like functions in GCC2
#if defined(__GNUC__) && __GNUC__ >= 2
#   define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#else
#   define PRINTF_F(f,v)
#endif

short           ShortSwap(short);
long            LongSwap(long);
float           FloatSwap(float);

#ifdef __BIG_ENDIAN__
#define SHORT(x)            ShortSwap(x)
#define LONG(x)             LongSwap(x)
#define FLOAT(x)            FloatSwap(x)

#define BIGSHORT(x)         (x)
#define BIGLONG(x)          (x)
#define BIGFLOAT(x)         (x)

// In these, x is evaluated multiple times, so increments and decrements
// cannot be used.
#define MACRO_SHORT(x)      ((short)(( ((short)(x)) & 0xff ) << 8) | (( ((short)(x)) & 0xff00) >> 8))
#define MACRO_LONG(x)       ((long)((( ((long)(x)) & 0xff) << 24)    | (( ((long)(x)) & 0xff00) << 8) | \
                                    (( ((long)(x)) & 0xff0000) >> 8) | (( ((long)(x)) & 0xff000000) >> 24) ))
#else
    // Little-endian.
#define SHORT(x)            (x)
#define LONG(x)             (x)
#define FLOAT(x)            (x)

#define BIGSHORT(x)         ShortSwap(x)
#define BIGLONG(x)          LongSwap(x)
#define BIGFLOAT(x)         FloatSwap(x)

#define MACRO_SHORT(x)      (x)
#define MACRO_LONG(x)       (x)
#endif

#define USHORT(x)           ((unsigned short) SHORT(x))
#define ULONG(x)            ((unsigned long) LONG(x))

#define MAX_OF(x, y)        ((x) > (y)? (x) : (y))
#define MIN_OF(x, y)        ((x) < (y)? (x) : (y))
#define MINMAX_OF(a, x, b)  ((x) < (a)? (a) : (x) > (b)? (b) : (x))
#define SIGN_OF(x)          ((x) > 0? +1 : (x) < 0? -1 : 0)
#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))
#define ROUND(x)            ((int) (((x) < 0.0f)? ((x) - 0.5f) : ((x) + 0.5f)))
#define ABS(x)              ((x) >= 0 ? (x) : -(x))

// Used to replace /255 as *reciprocal255 is less expensive with CPU cycles.
// Note that this should err on the side of being < 1/255 to prevent result
// exceeding 255 (e.g. 255 * reciprocal255).
#define reciprocal255   0.003921568627f

typedef enum // Value types.
{
    DDVT_NONE = -1, // Not a read/writeable value type.
    DDVT_BOOL,
    DDVT_BYTE,
    DDVT_SHORT,
    DDVT_INT,    // 32 or 64
    DDVT_UINT,
    DDVT_FIXED,
    DDVT_ANGLE,
    DDVT_FLOAT,
    DDVT_LONG,
    DDVT_ULONG,
    DDVT_PTR,
    DDVT_BLENDMODE
} valuetype_t;

enum {
    // Integer values for Set/Get
    DD_FIRST_VALUE = -1,
    DD_NETGAME,
    DD_SERVER,
    DD_CLIENT,
    DD_ALLOW_FRAMES,
    DD_VIEWWINDOW_X,
    DD_VIEWWINDOW_Y,
    DD_VIEWWINDOW_WIDTH,
    DD_VIEWWINDOW_HEIGHT,
    DD_CONSOLEPLAYER,
    DD_DISPLAYPLAYER,
    DD_MIPMAPPING,
    DD_SMOOTH_IMAGES,
    DD_DEFAULT_RES_X,
    DD_DEFAULT_RES_Y,
    DD_SKY_DETAIL,
    DD_SFX_VOLUME,
    DD_MUSIC_VOLUME,
    DD_MOUSE_INVERSE_Y,
    DD_FULLBRIGHT, // Render everything fullbright?
    DD_CCMD_RETURN,
    DD_GAME_READY,
    DD_DEDICATED,
    DD_NOVIDEO,
    DD_NUMMOBJTYPES,
    DD_GOTFRAME,
    DD_PLAYBACK,
    DD_NUMSOUNDS,
    DD_NUMMUSIC,
    DD_NUMLUMPS,
    DD_CLIENT_PAUSED,
    DD_WEAPON_OFFSET_SCALE_Y, // 1000x
    DD_MONOCHROME_PATCHES, // Convert patch image data to monochrome. 1= linear 2= weighted.
    DD_GAME_DATA_FORMAT,
    DD_GAME_DRAW_HUD_HINT, // Doomsday advises not to draw the HUD.
    DD_UPSCALE_AND_SHARPEN_PATCHES,
    DD_SYMBOLIC_ECHO,
    DD_MAX_TEXTURE_UNITS,
    DD_LAST_VALUE,

    // General constants (not to be used with Get/Set).
    DD_NEW = -2,
    DD_SKY = -1,
    DD_DISABLE,
    DD_ENABLE,
    DD_MASK,
    DD_YES,
    DD_NO,
    DD_MATERIAL,
    DD_OFFSET,
    DD_HEIGHT,
    DD_COLUMNS,
    DD_ROWS,
    DD_COLOR_LIMIT,
    DD_PRE,
    DD_POST,
    DD_VERSION_SHORT,
    DD_VERSION_LONG,
    DD_HORIZON,
    DD_GAME_ID,
    DD_DEF_MOBJ,
    DD_DEF_MOBJ_BY_NAME,
    DD_DEF_STATE,
    DD_DEF_SPRITE,
    DD_DEF_SOUND,
    DD_DEF_MUSIC,
    DD_DEF_MAP_INFO,
    DD_DEF_TEXT,
    DD_DEF_VALUE,
    DD_DEF_LINE_TYPE,
    DD_DEF_SECTOR_TYPE,
    DD_PSPRITE_BOB_X,
    DD_PSPRITE_BOB_Y,
    DD_DEF_FINALE_AFTER,
    DD_DEF_FINALE_BEFORE,
    DD_RENDER_RESTART_PRE,
    DD_RENDER_RESTART_POST,
    DD_DEF_SOUND_BY_NAME,
    DD_DEF_SOUND_LUMPNAME,
    DD_ID,
    DD_LUMP,
    DD_CD_TRACK,
    DD_GAME_MODE, // 16 chars max (swdoom, doom1, udoom, tnt, heretic...)
    DD_GAME_CONFIG, // String: dm/co-op, jumping, etc.
    DD_DEF_FINALE,
    DD_GAME_NAME, // (e.g., jdoom, jheretic etc..., suitable for use with filepaths)
    DD_GAME_NICENAME, // (e.g., jDoom, MyGame:Episode2 etc..., fancy name)
    DD_GAME_DMUAPI_VER, // Version of the DMU API the game is using.

    // Non-integer/special values for Set/Get
    DD_TRANSLATIONTABLES_ADDRESS,
    DD_TRACE_ADDRESS, // divline 'trace' used by PathTraverse.
    DD_SPRITE_REPLACEMENT, // Sprite <-> model replacement.
    DD_ACTION_LINK, // State action routine addresses.
    DD_MAP_NAME,
    DD_MAP_AUTHOR,
    DD_MAP_MUSIC,
    DD_MAP_MIN_X,
    DD_MAP_MIN_Y,
    DD_MAP_MAX_X,
    DD_MAP_MAX_Y,
    DD_WINDOW_WIDTH,
    DD_WINDOW_HEIGHT,
    DD_WINDOW_HANDLE,
    DD_DYNLIGHT_TEXTURE,
    DD_GAME_EXPORTS,
    DD_SECTOR_COUNT,
    DD_LINE_COUNT,
    DD_SIDE_COUNT,
    DD_VERTEX_COUNT,
    DD_SEG_COUNT,
    DD_SUBSECTOR_COUNT,
    DD_NODE_COUNT,
    DD_POLYOBJ_COUNT,
    DD_MATERIAL_COUNT,
    DD_XGFUNC_LINK, // XG line classes
    DD_SHARED_FIXED_TRIGGER,
    DD_GAMETIC,
    DD_OPENRANGE,
    DD_OPENTOP,
    DD_OPENBOTTOM,
    DD_LOWFLOOR,
    DD_VIEWX,
    DD_VIEWY,
    DD_VIEWZ,
    DD_VIEWX_OFFSET,
    DD_VIEWY_OFFSET,
    DD_VIEWZ_OFFSET,
    DD_VIEWANGLE,
    DD_VIEWANGLE_OFFSET,
    DD_CPLAYER_THRUST_MUL,
    DD_GRAVITY,
    DD_PSPRITE_OFFSET_X, // 10x
    DD_PSPRITE_OFFSET_Y, // 10x
    DD_TORCH_RED,
    DD_TORCH_GREEN,
    DD_TORCH_BLUE,
    DD_TORCH_ADDITIVE
};

// Bounding box coordinates.
enum {
    BOXTOP      = 0,
    BOXBOTTOM   = 1,
    BOXLEFT     = 2,
    BOXRIGHT    = 3,
    BOXFLOOR    = 4,
    BOXCEILING  = 5
};

//------------------------------------------------------------------------
//
// Fixed-Point Math
//
//------------------------------------------------------------------------

#define FRACBITS            16
#define FRACUNIT            (1<<FRACBITS)

#define FINEANGLES          8192
#define FINEMASK            (FINEANGLES-1)
#define ANGLETOFINESHIFT    19 // 0x100000000 to 0x2000

#define ANGLE_45            0x20000000
#define ANGLE_90            0x40000000
#define ANGLE_180           0x80000000
#define ANGLE_MAX           0xffffffff
#define ANGLE_1             (ANGLE_45/45)
#define ANGLE_60            (ANGLE_180/3)

#define ANG45               0x20000000
#define ANG90               0x40000000
#define ANG180              0x80000000
#define ANG270              0xc0000000

#define FIX2FLT(x)      ( (x) / (float) FRACUNIT )
#define Q_FIX2FLT(x)    ( (float)((x)>>FRACBITS) )
#define FLT2FIX(x)      ( (fixed_t) ((x)*FRACUNIT) )

#if !defined( NO_FIXED_ASM ) && !defined( GNU_X86_FIXED_ASM )

    /* *INDENT-OFF* */
    __inline fixed_t FixedMul(fixed_t a, fixed_t b) {
        __asm {
            // The parameters in eax and ebx.
            mov eax, a
            mov ebx, b
            // The multiplying.
            imul ebx
            shrd eax, edx, 16
            // eax should hold the return value.
        }
        // A value is returned regardless of the compiler warning.
    }
    __inline fixed_t FixedDiv2(fixed_t a, fixed_t b) {
        __asm {
            // The parameters.
            mov eax, a
            mov ebx, b
            // The operation.
            cdq
            shld edx, eax, 16
            sal eax, 16
            idiv ebx
            // And the value returns in eax.
        }
        // A value is returned regardless of the compiler warning.
    }
    /* *INDENT-ON* */

#else

// Don't use inline assembler in fixed-point calculations.
// (link with plugins/common/m_fixed.c)
fixed_t         FixedMul(fixed_t a, fixed_t b);
fixed_t         FixedDiv2(fixed_t a, fixed_t b);
#endif

// This one is always in plugins/common/m_fixed.c.
fixed_t         FixedDiv(fixed_t a, fixed_t b);

//------------------------------------------------------------------------
//
// Key Codes
//
//------------------------------------------------------------------------

// Most key data is simple ASCII.
#define DDKEY_ESCAPE        27
#define DDKEY_RETURN        13
#define DDKEY_TAB           9
#define DDKEY_BACKSPACE     127
#define DDKEY_EQUALS        0x3d
#define DDKEY_MINUS         0x2d
#define DDKEY_BACKSLASH     0x5C

// Extended keys (above 127).
enum {
    DDKEY_RIGHTARROW = 0x80,
    DDKEY_LEFTARROW,
    DDKEY_UPARROW,
    DDKEY_DOWNARROW,
    DDKEY_F1,
    DDKEY_F2,
    DDKEY_F3,
    DDKEY_F4,
    DDKEY_F5,
    DDKEY_F6,
    DDKEY_F7,
    DDKEY_F8,
    DDKEY_F9,
    DDKEY_F10,
    DDKEY_F11,
    DDKEY_F12,
    DDKEY_NUMLOCK,
    DDKEY_SCROLL,
    DDKEY_NUMPAD7,
    DDKEY_NUMPAD8,
    DDKEY_NUMPAD9,
    DDKEY_NUMPAD4,
    DDKEY_NUMPAD5,
    DDKEY_NUMPAD6,
    DDKEY_NUMPAD1,
    DDKEY_NUMPAD2,
    DDKEY_NUMPAD3,
    DDKEY_NUMPAD0,
    DDKEY_DECIMAL,
    DDKEY_PAUSE,
    DDKEY_RSHIFT,
    DDKEY_LSHIFT = DDKEY_RSHIFT,
    DDKEY_RCTRL,
    DDKEY_LCTRL = DDKEY_RCTRL,
    DDKEY_RALT,
    DDKEY_LALT = DDKEY_RALT,
    DDKEY_INS,
    DDKEY_DEL,
    DDKEY_PGUP,
    DDKEY_PGDN,
    DDKEY_HOME,
    DDKEY_END,
    DDKEY_SUBTRACT, // '-' on numeric keypad.
    DDKEY_ADD, // '+' on numeric keypad.
    DD_HIGHEST_KEYCODE
};

//------------------------------------------------------------------------
//
// Events
//
//------------------------------------------------------------------------

typedef enum {
    EV_KEY,
    EV_MOUSE_AXIS,
    EV_MOUSE_BUTTON,
    EV_JOY_AXIS, // Joystick main axes (xyz + Rxyz).
    EV_JOY_SLIDER, // Joystick sliders.
    EV_JOY_BUTTON,
    EV_POV,
    EV_SYMBOLIC, // Symbol text pointed to by data1+data2.
    NUM_EVENT_TYPES
} evtype_t;

typedef enum {
    EVS_DOWN,
    EVS_UP,
    EVS_REPEAT,
    NUM_EVENT_STATES
} evstate_t;

typedef struct {
    evtype_t        type;
    evstate_t       state; // Only used with digital controls.
    int             data1; // Keys/mouse/joystick buttons.
    int             data2; // Mouse/joystick x move.
    int             data3; // Mouse/joystick y move.
    int             data4;
    int             data5;
    int             data6;
} event_t;

// The mouse wheel is considered two extra mouse buttons.
#define DD_MWHEEL_UP        3
#define DD_MWHEEL_DOWN      4
#define DD_MICKEY_ACCURACY  1000

// Control classes.
typedef enum
{
    CC_AXIS,
    CC_TOGGLE,
    CC_IMPULSE,
    NUM_CONTROL_CLASSES
} ctlclass_t;

//------------------------------------------------------------------------
//
// Purge Levels
//
//------------------------------------------------------------------------

#define PU_STATIC           1 // Static entire execution time.
#define PU_SOUND            2 // Static while playing.
#define PU_MUSIC            3 // Static while playing.

#define PU_USER1            40
#define PU_USER2            41
#define PU_USER3            42
#define PU_USER4            43
#define PU_USER5            44
#define PU_USER6            45
#define PU_USER7            46
#define PU_USER8            47
#define PU_USER9            48
#define PU_USER10           49

#define PU_MAP              50 // Static until map exited (may still be
                               // freed during the map, though).
#define PU_MAPSTATIC        52 // Not freed until map exited.

// Tags >= 100 are purgable whenever needed.
#define PU_PURGELEVEL       100
#define PU_CACHE            101

// Special purgelevel.
#define PU_GETNAME          100000L

//------------------------------------------------------------------------
//
// Map Data
//
//------------------------------------------------------------------------

#define DMUAPI_VER          1 // Public DMU API version number.
                              // Requested by the engine during init.

// Map Update constants.
enum /* Do not change the numerical values of the constants! */
{
    // Flags. OR'ed with a DMU property constant. The most significant
    // byte is used for the flags.
    DMU_FLAG_MASK           = 0xff000000,
    DMU_SIDEDEF1_OF_LINE    = 0x80000000,
    DMU_SIDEDEF0_OF_LINE    = 0x40000000,
    DMU_TOP_OF_SIDEDEF      = 0x20000000,
    DMU_MIDDLE_OF_SIDEDEF   = 0x10000000,
    DMU_BOTTOM_OF_SIDEDEF   = 0x08000000,
    DMU_FLOOR_OF_SECTOR     = 0x04000000,
    DMU_CEILING_OF_SECTOR   = 0x02000000,
    // (1 bits left)

    DMU_NONE = 0,

    DMU_VERTEX = 1,
    DMU_SEG,
    DMU_LINEDEF,
    DMU_SIDEDEF,
    DMU_NODE,
    DMU_SUBSECTOR,
    DMU_SECTOR,
    DMU_PLANE,
    DMU_SURFACE,
    DMU_MATERIAL,

    DMU_LINEDEF_BY_TAG,
    DMU_SECTOR_BY_TAG,

    DMU_LINEDEF_BY_ACT_TAG,
    DMU_SECTOR_BY_ACT_TAG,

    DMU_X,
    DMU_Y,
    DMU_XY,

    DMU_NORMAL_X,
    DMU_NORMAL_Y,
    DMU_NORMAL_Z,
    DMU_NORMAL_XYZ,

    DMU_VERTEX0,
    DMU_VERTEX1,

    DMU_FRONT_SECTOR,
    DMU_BACK_SECTOR,
    DMU_SIDEDEF0,
    DMU_SIDEDEF1,
    DMU_FLAGS,
    DMU_DX,
    DMU_DY,
    DMU_DXY,
    DMU_LENGTH,
    DMU_SLOPE_TYPE,
    DMU_ANGLE,
    DMU_OFFSET,

    DMU_OFFSET_X,
    DMU_OFFSET_Y,
    DMU_OFFSET_XY,

    DMU_VALID_COUNT,
    DMU_LINEDEF_COUNT,
    DMU_COLOR, // RGB
    DMU_COLOR_RED, // red component
    DMU_COLOR_GREEN, // green component
    DMU_COLOR_BLUE, // blue component
    DMU_ALPHA,
    DMU_BLENDMODE,
    DMU_LIGHT_LEVEL,
    DMT_MOBJS, // pointer to start of sector mobjList
    DMU_BOUNDING_BOX, // float[4]
    DMU_SOUND_ORIGIN,
    DMU_WIDTH,
    DMU_HEIGHT,
    DMU_TARGET_HEIGHT,
    DMU_SPEED,
    DMU_SEG_COUNT,
    DMU_NAMESPACE
};

// Linedef flags:
// For use with P_Set/Get(DMU_LINEDEF, n, DMU_FLAGS).
#define DDLF_BLOCKING           0x0001
#define DDLF_DONTPEGTOP         0x0002
#define DDLF_DONTPEGBOTTOM      0x0004

// Sidedef flags
// For use with P_Set/Get(DMU_SIDEDEF, n, DMU_FLAGS).
#define SDF_BLENDTOPTOMID       0x0001
#define SDF_BLENDMIDTOTOP       0x0002
#define SDF_BLENDMIDTOBOTTOM    0x0004
#define SDF_BLENDBOTTOMTOMID    0x0008
#define SDF_MIDDLE_STRETCH      0x0010 // Stretch the middle surface to reach from floor to ceiling.

// Surface flags:
// For use with P_Set/Get(DMU_SURFACE, n, DMU_FLAGS).
#define DDSUF_GLOW              0x00000001 // Surface glows (fully bright).
#define DDSUF_MATERIAL_FLIPH    0x00000002 // Surface material is flipped horizontally.
#define DDSUF_MATERIAL_FLIPV    0x00000004 // Surface material is flipped vertically.

// Map Update status code constants.
// Sent to the game when various map update events occur.
enum /* Do NOT change the numerical values of the constants. */
{
    DMUSC_LINE_FIRSTRENDERED
};

// All map think-able objects must use this as a base. Also used for sound
// origin purposes for all of: mobj_t, polyobj_t, sector_t/plane_t
#define DD_BASE_DDMOBJ_ELEMENTS() \
    thinker_t       thinker;            /* thinker node */ \
    float           pos[3];             /* position [x,y,z] */

typedef struct ddmobj_base_s {
    DD_BASE_DDMOBJ_ELEMENTS()
} ddmobj_base_t;

// Fixed-point vertex position. Utility struct for the game, not used by
// the engine.
typedef struct ddvertex_s {
    fixed_t         pos[2];
} ddvertex_t;

// Floating-point vertex position. Utility struct for the game, not used
// by the engine.
typedef struct ddvertexf_s {
    float           pos[2];
} ddvertexf_t;

// SetupMap modes.
enum
{
    DDSMM_AFTER_LOADING, // After loading a savegame...
    DDSMM_FINALIZE, // After everything else is done.
    DDSMM_INITIALIZE, // Before anything else if done.
    DDSMM_AFTER_BUSY // After leaving busy mode, which was used during setup.
};

enum // Sector reverb data indices.
{
    SRD_VOLUME,
    SRD_SPACE,
    SRD_DECAY,
    SRD_DAMPING,
    NUM_REVERB_DATA
};

#define DD_MAX_MATERIAL_LAYERS     1 //// \temp

typedef struct {
    fixed_t         pos[2], dX, dY;
} divline_t;

typedef struct {
    float           pos[2], dX, dY;
} fdivline_t;

// For PathTraverse.
#define PT_ADDLINES     1
#define PT_ADDMOBJS     2
#define PT_EARLYOUT     4

typedef enum {
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

// For (un)linking.
#define DDLINK_SECTOR       0x1
#define DDLINK_BLOCKMAP     0x2
#define DDLINK_NOLINE       0x4

typedef enum intercepttype_e {
    ICPT_MOBJ,
    ICPT_LINE
} intercepttype_t;

typedef struct intercept_s {
    float           frac; // Along trace line.
    intercepttype_t type;
    union {
        struct mobj_s* mo;
        struct linedef_s* lineDef;
    } d;
} intercept_t;

typedef boolean (*traverser_t) (intercept_t* in);

#define NO_INDEX            0xffffffff

//------------------------------------------------------------------------
//
// Mobjs
//
//------------------------------------------------------------------------

/**
 * Linknodes are used when linking mobjs to lines. Each mobj has a ring
 * of linknodes, each node pointing to a line the mobj has been linked to.
 * Correspondingly each line has a ring of nodes, with pointers to the
 * mobjs that are linked to that particular line. This way it is possible
 * that a single mobj is linked simultaneously to multiple lines (which
 * is common).
 *
 * All these rings are maintained by P_Mobj(Un)Link().
 */
typedef struct linknode_s {
    nodeindex_t     prev, next;
    void*           ptr;
    int             data;
} linknode_t;

// State flags.
#define STF_FULLBRIGHT      0x00000001
#define STF_NOAUTOLIGHT     0x00000002 // Don't automatically add light if fullbright.

// Doomsday mobj flags.
#define DDMF_DONTDRAW       0x00000001
#define DDMF_SHADOW         0x00000002
#define DDMF_ALTSHADOW      0x00000004
#define DDMF_BRIGHTSHADOW   0x00000008
#define DDMF_VIEWALIGN      0x00000010
#define DDMF_FITTOP         0x00000020 // Don't let the sprite go into the ceiling.
#define DDMF_NOFITBOTTOM    0x00000040
#define DDMF_NOBLOCKMAP     0x00000080
#define DDMF_LIGHTSCALE     0x00000180 // Light scale (0: full, 3: 1/4).
#define DDMF_LIGHTOFFSET    0x0000f000 // How to offset light (along Z axis).
#define DDMF_RESERVED       0x00030000 // Don't touch these!! (translation class).
#define DDMF_BOB            0x00040000 // Bob the Z coord up and down.
#define DDMF_LOWGRAVITY     0x00080000 // 1/8th gravity (predict).
#define DDMF_MISSILE        0x00100000 // Client removes mobj upon impact.
#define DDMF_FLY            0x00200000 // Flying object (doesn't matter if airborne).
#define DDMF_NOGRAVITY      0x00400000 // Isn't affected by gravity (predict).
#define DDMF_ALWAYSLIT      0x00800000 // Always process DL even if hidden.
#define DDMF_TRANSLATION    0x1c000000 // Use a translation table (>>MF_TRANSHIFT).
#define DDMF_SOLID          0x20000000 // Solid on client side.
#define DDMF_LOCAL          0x40000000
#define DDMF_REMOTE         0x80000000 // This mobj is really on the server.

// Clear masks (flags the Game DLL is not allowed to touch).
#define DDMF_CLEAR_MASK     0xc0000000

#define DDMF_TRANSSHIFT     26 // table for player colormaps
#define DDMF_CLASSTRSHIFT   16
#define DDMF_LIGHTSCALESHIFT 7
#define DDMF_LIGHTOFFSETSHIFT 12

#define DDMOBJ_RADIUS_MAX   32

// The high byte of the selector is not used for modeldef selecting.
// 1110 0000 = alpha level (0: opaque => 7: transparent 7/8)
#define DDMOBJ_SELECTOR_MASK 0x00ffffff
#define DDMOBJ_SELECTOR_SHIFT 24

#define VISIBLE             1
#define INVISIBLE           -1

enum { MX, MY, MZ }; // Momentum axis indices.

    // Base mobj_t elements. Games MUST use this as the basis for mobj_t.
#define DD_BASE_MOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    nodeindex_t     lineRoot; /* lines to which this is linked */ \
    struct mobj_s*  sNext, **sPrev; /* links in sector (if needed) */ \
\
    struct subsector_s* subsector; /* subsector in which this resides */ \
    float           mom[3]; \
    angle_t         angle; \
    spritenum_t     sprite; /* used to find patch_t and flip value */ \
    int             frame; \
    float           radius; \
    float           height; \
    int             ddFlags; /* Doomsday mobj flags (DDMF_*) */ \
    float           floorClip; /* value to use for floor clipping */ \
    int             valid; /* if == valid, already checked */ \
    int             type; /* mobj type */ \
    struct state_s* state; \
    int             tics; /* state tic counter */ \
    float           floorZ; /* highest contacted floor */ \
    float           ceilingZ; /* lowest contacted ceiling */ \
    struct mobj_s*  onMobj; /* the mobj this one is on top of. */ \
    boolean         wallHit; /* the mobj is hitting a wall. */ \
    struct ddplayer_s* dPlayer; /* NULL if not a player mobj. */ \
    float           srvo[3]; /* short-range visual offset (xyz) */ \
    short           visAngle; /* visual angle ("angle-servo") */ \
    int             selector; /* multipurpose info */ \
    int             validCount; /* used in iterating */ \
    int             addFrameCount; \
    unsigned int    lumIdx; /* index+1 of the lumobj/bias source, or 0 */ \
    byte            haloFactors[DDMAXPLAYERS]; /* strengths of halo */ \
    byte            translucency; /* default = 0 = opaque */ \
    short           visTarget; /* -1 = mobj is becoming less visible, */ \
                               /* 0 = no change, 2= mobj is becoming more visible */ \
    int             reactionTime; /* if not zero, freeze controls */

    // Base polyobj_t elements. Games MUST use this as the basis for polyobj_t.
#define DD_BASE_POLYOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    struct subsector_s* subsector; /* subsector in which this resides */ \
    unsigned int    idx; /* Idx of polyobject. */ \
    int             tag; /* Reference tag. */ \
    int             validCount; \
    float           box[2][2]; \
    float           dest[2]; /* Destination XY. */ \
    angle_t         angle; \
    angle_t         destAngle; /* Destination angle. */ \
    angle_t         angleSpeed; /* Rotation speed. */ \
    unsigned int    numSegs; \
    struct seg_s**  segs; \
    struct fvertex_s* originalPts; /* Used as the base for the rotations. */ \
    struct fvertex_s* prevPts; /* Use to restore the old point values. */ \
    float           speed; /* Movement speed. */ \
    boolean         crush; /* Should the polyobj attempt to crush mobjs? */ \
    int             seqType; \
    struct { \
        int         index; \
        unsigned int lineCount; \
        struct linedef_s** lineDefs; \
    } buildData;

//------------------------------------------------------------------------
//
// Refresh
//
//------------------------------------------------------------------------

#define TICRATE             35 // Number of tics / second.
#define TICSPERSEC          35

#define SCREENWIDTH         320
#define SCREENHEIGHT        200

//------------------------------------------------------------------------
//
// Sound
//
//------------------------------------------------------------------------

#define DDSF_FLAG_MASK      0xff000000
#define DDSF_NO_ATTENUATION 0x80000000
#define DDSF_REPEAT         0x40000000

typedef struct {
    float           volume; // 0..1
    float           decay; // Decay factor: 0 (acoustically dead) ... 1 (live)
    float           damping; // High frequency damping factor: 0..1
    float           space; // 0 (small space) ... 1 (large space)
} reverb_t;

    // Use with PlaySong().
#define DDMUSICF_EXTERNAL   0x80000000

//------------------------------------------------------------------------
//
// Graphics
//
//------------------------------------------------------------------------

#define DDMAX_MATERIAL_LAYERS   1

typedef enum material_namespace_e {
    MN_ANY = -1,
    MN_FIRST,
    MN_TEXTURES = MN_FIRST,
    MN_FLATS,
    MN_SPRITES,
    MN_SYSTEM,
    NUM_MATERIAL_NAMESPACES
} material_namespace_t;

// Material flags:
#define MATF_CUSTOM             0x0001 // Material is not derived from an IWAD resource (directly, at least).
#define MATF_NO_DRAW            0x0002 // Material should never be drawn.
#define MATF_GLOW               0x0004 // Glowing material.
#define MATF_SKYMASK            0x0008 // Sky-mask surfaces using this material.

// Animation group flags.
#define AGF_SMOOTH          0x1
#define AGF_FIRST_ONLY      0x2
#define AGF_PRECACHE        0x4000 // Group is just for precaching.

typedef struct lumppatch_s {
    short           width; // Bounding box size.
    short           height;
    short           leftOffset; // Pixels to the left of origin.
    short           topOffset; // Pixels below the origin.
    int             columnOfs[8]; /* Only [width] used the [0] is
                                   &columnofs[width] */
} lumppatch_t;

typedef struct {
    int             lump; // Lump number.
    int             realLump; // Real lump number.
    int             flip;
    int             offset;
    int             topOffset;
    int             width;
    int             height;
    int             numFrames; // Number of frames the sprite has.
} patchinfo_t;

typedef struct {
    struct material_s* material;
    int             realLump; // Real lump number.
    int             flip;
    int             offset;
    int             topOffset;
    int             width;
    int             height;
    int             numFrames; // Number of frames the sprite has.
} spriteinfo_t;

//------------------------------------------------------------------------
//
// Console
//
//------------------------------------------------------------------------

// Busy mode flags.
#define BUSYF_LAST_FRAME    0x1
#define BUSYF_CONSOLE_OUTPUT 0x2
#define BUSYF_PROGRESS_BAR  0x4
#define BUSYF_ACTIVITY      0x8  // Indicate activity.
#define BUSYF_NO_UPLOADS    0x10 // Deferred uploads not completed.
#define BUSYF_STARTUP       0x20 // Startup mode: normal fonts, texman not available.

// These correspond the good old text mode VGA colors.
#define CBLF_BLACK          0x00000001
#define CBLF_BLUE           0x00000002
#define CBLF_GREEN          0x00000004
#define CBLF_CYAN           0x00000008
#define CBLF_RED            0x00000010
#define CBLF_MAGENTA        0x00000020
#define CBLF_YELLOW         0x00000040
#define CBLF_WHITE          0x00000080
#define CBLF_LIGHT          0x00000100
#define CBLF_RULER          0x00000200
#define CBLF_CENTER         0x00000400
#define CBLF_TRANSMIT       0x80000000 // If server, sent to all clients.

// Font flags.
#define DDFONT_WHITE            0x1 // The font data is white, can be colored.

typedef struct {
    int             flags;
    float           sizeX, sizeY;   // The scale.
    int             height;
    int           (*drawText) (const char* text, int x, int y);
    int           (*getWidth) (const char* text);
    void          (*filterText) (char* text); // Maybe alters text.
} ddfont_t;


/// Argument type for B_BindingsForControl().
typedef enum bfcinverse_e {
    BFCI_BOTH,
    BFCI_ONLY_NON_INVERSE,
    BFCI_ONLY_INVERSE
} bfcinverse_t;

// Console command.
typedef struct ccmd_s {
    const char*     name;
    const char*     params;
    int           (*func) (byte src, int argc, char **argv);
    int             flags;
} ccmd_t;

// Command sources (where the console command originated from)
// These are sent with every (sub)ccmd so we can decide whether or not to execute.
enum {
    CMDS_UNKNOWN,
    CMDS_DDAY, // Sent by the engine
    CMDS_GAME, // Sent by the game dll
    CMDS_CONSOLE, // Sent via direct console input
    CMDS_BIND, // Sent from a binding/alias
    CMDS_CONFIG, // Sent via config file
    CMDS_PROFILE, // Sent via player profile
    CMDS_CMDLINE, // Sent via the command line
    CMDS_DED // Sent based on a def in a DED file eg (state->execute)
};

// Helper macro for defining console command functions.
#define DEFCC(name)         int name(byte src, int argc, char** argv)

// Console command flags.
#define CMDF_NO_DEDICATED   0x00000001 // Not available in dedicated server mode.

// Console command usage flags.
// (what method(s) CAN NOT be used to invoke a ccmd (used with the CMDS codes above)).
#define CMDF_DDAY           0x00800000
#define CMDF_GAME           0x01000000
#define CMDF_CONSOLE        0x02000000
#define CMDF_BIND           0x04000000
#define CMDF_CONFIG         0x08000000
#define CMDF_PROFILE        0x10000000
#define CMDF_CMDLINE        0x20000000
#define CMDF_DED            0x40000000
#define CMDF_CLIENT         0x80000000 // sent over the net from a client.

    // Console variable flags.
#define CVF_NO_ARCHIVE      0x1 // Not written in/read from the defaults file.
#define CVF_PROTECTED       0x2 // Can't be changed unless forced.
#define CVF_NO_MIN          0x4 // Don't use the minimum.
#define CVF_NO_MAX          0x8 // Don't use the maximum.
#define CVF_CAN_FREE        0x10 // The string can be freed.
#define CVF_HIDE            0x20
#define CVF_READ_ONLY       0x40 // Can't be changed manually at all

    // Console variable types.
    typedef enum {
        CVT_NULL,
        CVT_BYTE,
        CVT_INT,
        CVT_FLOAT,
        CVT_CHARPTR // ptr points to a char*, which points to the string.
    } cvartype_t;

    // Console variable.
    typedef struct cvar_s {
        char*           name;
        int             flags;
        cvartype_t      type;
        void*           ptr; // Pointer to the data.
        float           min, max; /* Minimum and maximum values
                                     (for ints and floats). */
        void          (*notifyChanged)(struct cvar_s* cvar);
    } cvar_t;

    //------------------------------------------------------------------------
    //
    // Networking
    //
    //------------------------------------------------------------------------

/**
 * Tick Commands. Usually only a part of this data is transferred over
 * the network. In addition to tick commands, clients will sent 'impulses'
 * to the server when they want to change a weapon, use an artifact, or
 * maybe commit suicide.
 */
typedef struct ticcmd_s {
    char        forwardMove; // *2048 for real move.
    char        sideMove; // *2048 for real move.
    char        upMove; // *2048 for real move.
    unsigned short angle; // <<16 for angle (view angle).
    short       pitch; // View pitch.
    short       actions; // On/off action flags.
} ticcmd_t;

    // Network Player Events
    enum {
        DDPE_ARRIVAL, // A player has arrived.
        DDPE_EXIT, // A player has exited the game.
        DDPE_CHAT_MESSAGE, // A player has sent a chat message.
        DDPE_DATA_CHANGE // The data for this player has been changed.
    };

    // World events (handled by clients)
    enum {
        DDWE_HANDSHAKE, // Shake hands with a new player.
        DDWE_PROJECTILE, // Spawn a projectile.
        DDWE_SECTOR_SOUND, // Play a sector sound.
        DDWE_DEMO_END // Demo playback ends.
    };

    /*
     * Do not modify this structure: Servers send it as-is to clients.
     * Only add elements to the end.
     */
    typedef struct serverinfo_s {
        int             version;
        char            name[64];
        char            description[80];
        int             numPlayers, maxPlayers;
        char            canJoin;
        char            address[64];
        int             port;
        unsigned short  ping; // Milliseconds.
        char            game[32]; // DLL and version.
        char            gameMode[17];
        char            gameConfig[40];
        char            map[20];
        char            clientNames[128];
        unsigned int    wadNumber;
        char            iwad[32];
        char            pwads[128];
        int             data[3];
    } serverinfo_t;

    typedef struct {
        int             num; // How many serverinfo_t's in data?
        int             found; // How many servers were found?
        serverinfo_t*   data;
    } serverdataquery_t;

    // For querying the list of available modems.
    typedef struct {
        int             num;
        char**          list;
    } modemdataquery_t;

    // All packet types handled by the game should be >= 64.
#define DDPT_HELLO              0
#define DDPT_OK                 1
#define DDPT_CANCEL             2
#define DDPT_COMMANDS           32
#define DDPT_FIRST_GAME_EVENT   64
#define DDPT_MESSAGE            67

    // SendPacket flags (OR with to_player).
#define DDSP_ORDERED        0x20000000 // Confirm delivery in correct order.
#define DDSP_CONFIRM        0x40000000 // Confirm delivery.
#define DDSP_ALL_PLAYERS    0x80000000 // Broadcast (for server).

    // World handshake: update headers. A varying number of
    // data follows, depending on the flags. Notice: must be not
    // be padded, needs to be byte-aligned.
#pragma pack(1)
    typedef struct {
        byte            flags;
        unsigned int    sector;
    } packet_sectorupdate_t;

    typedef struct {
        byte            flags;
        unsigned int    side;
    } packet_wallupdate_t;
#pragma pack()

    // World handshake flags. Sent by the game server:
    // Sector update flags.
#define DDSU_FLOOR_HEIGHT   0x01
#define DDSU_FLOOR_MOVING   0x02   // Destination and speed sent.
#define DDSU_CEILING_HEIGHT 0x04
#define DDSU_CEILING_MOVING 0x08   // Destination and speed.
#define DDSU_FLOORPIC       0x10   // Floorpic changed.
#define DDSU_CEILINGPIC     0x20   // Ceilingpic changed.
#define DDSU_LIGHT_LEVEL    0x40

    // Wall update flags.
#define DDWU_TOP            0x01   // Top texture.
#define DDWU_MID            0x02   // Mid texture.
#define DDWU_BOTTOM         0x04   // Bottom texture.

    // Missile spawn request flags.
#define DDMS_FLAG_MASK      0xc000
#define DDMS_MOVEMENT_XY    0x4000 // XY movement included.
#define DDMS_MOVEMENT_Z     0x8000 // Z movement included.

    //------------------------------------------------------------------------
    //
    // Player Data
    //
    //------------------------------------------------------------------------

    // Built-in control identifiers.
    enum
    {
        CTL_WALK = 1,           ///< Forward/backwards.
        CTL_SIDESTEP = 2,       ///< Left/right sideways movement.
        CTL_ZFLY = 3,           ///< Up/down movement.
        CTL_TURN = 4,           ///< Turning horizontally.
        CTL_LOOK = 5,           ///< Turning up and down.
        CTL_FIRST_GAME_CONTROL = 1000
    };

    typedef enum controltype_e {
        CTLT_NUMERIC,
        CTLT_IMPULSE
    } controltype_t;

    // Player flags.
#define DDPF_FIXANGLES      0x1 // Server: send angle/pitch to client.
#define DDPF_FILTER         0x2 // Server: send filter to client.
#define DDPF_FIXPOS         0x4 // Server: send coords to client.
#define DDPF_DEAD           0x8 // Cl & Sv: player is dead.
#define DDPF_CAMERA         0x10 // Player is a cameraman.
#define DDPF_LOCAL          0x20 // Player is local (e.g. player zero).
#define DDPF_FIXMOM         0x40 // Server: send momentum to client.
#define DDPF_NOCLIP         0x80 // Client: don't clip movement.
#define DDPF_CHASECAM       0x100 // Chase camera mode (third person view).
#define DDPF_INTERYAW       0x200 // Interpolate view yaw angles (used with locking).
#define DDPF_INTERPITCH     0x400 // Interpolate view pitch angles (used with locking).
#define DDPF_VIEW_FILTER    0x800 // Cl & Sv: Draw the current view filter.

#define PLAYERNAMELEN       81

    // Normally one for the weapon and one for the muzzle flash.
#define DDMAXPSPRITES 2

    enum                           // Psprite states.
    {
        DDPSP_BOBBING,
        DDPSP_FIRE,
        DDPSP_DOWN,
        DDPSP_UP
    };

    // Player sprites.
    typedef struct {
        state_t*        statePtr;
        int             tics;
        float           light, alpha;
        float           pos[2];
        int             flags;
        int             state;
        float           offset[2];
    } ddpsprite_t;

    // Lookdir conversions.
#define LOOKDIR2DEG(x)  ((x) * 85.0/110.0)
#define LOOKDIR2RAD(x)  (LOOKDIR2DEG(x)/180*PI)

    struct mobj_s;
    struct polyobj_s;

    typedef struct fixcounters_s {
        int             angles;
        int             pos;
        int             mom;
    } fixcounters_t;

    typedef struct ddplayer_s {
        ticcmd_t        cmd;
        struct mobj_s*  mo; // Pointer to a (game specific) mobj.
        float           viewZ; // Focal origin above r.z.
        float           viewHeight; // Base height above floor for viewZ.
        float           viewHeightDelta;
        float           lookDir; // For mouse look.
        int             fixedColorMap; // Can be set to REDCOLORMAP, etc.
        int             extraLight; // So gun flashes light up areas.
        int             inGame; // Is this player in game?
        int             inVoid; // True if player is in the void
                                // (not entirely accurate so it shouldn't
                                // be used for anything critical).
        int             flags;
        float           filterColor[4]; // RGBA filter for the camera.
        fixcounters_t   fixCounter;
        fixcounters_t   fixAcked;
        angle_t         lastAngle; // For calculating turndeltas.
        ddpsprite_t     pSprites[DDMAXPSPRITES]; // Player sprites.
        void*           extraData; // Pointer to any game-specific data.
    } ddplayer_t;

#ifdef __cplusplus
}
#endif
#endif
