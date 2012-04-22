/**
 * @file dd_share.h
 * Shared macros and constants.
 *
 * Various macros and constants used by the engine and games.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SHARED_H
#define LIBDENG_SHARED_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def C_DECL
 * Calling convention for compare functions. Only for Windows.
 */
#ifndef C_DECL
#  if defined(WIN32)
#    define C_DECL __cdecl
#  elif defined(UNIX)
#    define C_DECL
#  else
#    define C_DECL
#  endif
#endif

#include <stdlib.h>
#include <stdarg.h>

#include "dengproject.h"
#include "../portable/include/dd_version.h"
#include "dd_types.h"
#include "dd_maptypes.h"
#include "dd_wad.h"
#include "dd_gl.h"
#include "dd_ui.h"
#include "dd_infine.h"
#include "def_share.h"
#include "thinker.h"

/** @file
 * @todo dd_version.h is not officially a public header file!
 */

//------------------------------------------------------------------------
//
// General Definitions and Macros
//
//------------------------------------------------------------------------

/// @defgroup apiFlags Flags

/// Maximum number of players supported by the engine.
#define DDMAXPLAYERS        16

/// Base default paths for data files.
#define DD_BASEPATH_DATA    "}data/"

/// Base default paths for definition files.
#define DD_BASEPATH_DEFS    "}defs/"

// The case-independent strcmps have different names.
#if WIN32
# define strcasecmp _stricmp
# define strncasecmp _strnicmp
# define strlwr _strlwr
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

int             dd_snprintf(char* str, size_t size, const char* format, ...);
int             dd_vsnprintf(char* str, size_t size, const char* format,
                             va_list ap);

    // Format checking for printf-like functions in GCC2
#if defined(__GNUC__) && __GNUC__ >= 2
#   define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#else
#   define PRINTF_F(f,v)
#endif

/**
 * Macro for hiding the warning about an unused parameter.
 */
#define DENG_UNUSED(x)      (void)x

int16_t         ShortSwap(int16_t);
int32_t         LongSwap(int32_t);
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
#define MACRO_SHORT(x)      ((int16_t)(( ((int16_t)(x)) & 0xff ) << 8) | (( ((int16_t)(x)) & 0xff00) >> 8))
#define MACRO_LONG(x)       ((int32_t)((( ((int32_t)(x)) & 0xff) << 24) | (( ((int32_t)(x)) & 0xff00) << 8) | \
                                       (( ((int32_t)(x)) & 0xff0000) >> 8) | (( ((int32_t)(x)) & 0xff000000) >> 24) ))
#else
/// Byte order conversion from native to little-endian. @{
#define SHORT(x)            (x)
#define LONG(x)             (x)
#define FLOAT(x)            (x)
///@}

/// Byte order conversion from native to big-endian. @{
#define BIGSHORT(x)         ShortSwap(x)
#define BIGLONG(x)          LongSwap(x)
#define BIGFLOAT(x)         FloatSwap(x)
///@}

#define MACRO_SHORT(x)      (x)
#define MACRO_LONG(x)       (x)
#endif

/// Byte order conversion from native to little-endian. @{
#define USHORT(x)           ((uint16_t) SHORT(x))
#define ULONG(x)            ((uint32_t) LONG(x))
///@}

/// Value types.
typedef enum {
    DDVT_NONE = -1, ///< Not a read/writeable value type.
    DDVT_BOOL,
    DDVT_BYTE,
    DDVT_SHORT,
    DDVT_INT,       ///< 32 or 64 bit
    DDVT_UINT,
    DDVT_FIXED,
    DDVT_ANGLE,
    DDVT_FLOAT,
    DDVT_DOUBLE,
    DDVT_LONG,
    DDVT_ULONG,
    DDVT_PTR,
    DDVT_BLENDMODE
} valuetype_t;

/// Integer values for Set/Get
enum {
    DD_FIRST_VALUE = -1,
    DD_NETGAME,
    DD_SERVER,
    DD_CLIENT,
    DD_ALLOW_FRAMES,
    DD_CONSOLEPLAYER,
    DD_DISPLAYPLAYER,
    DD_MIPMAPPING,
    DD_SMOOTH_IMAGES,
    DD_DEFAULT_RES_X,
    DD_DEFAULT_RES_Y,
    DD_UNUSED1,
    DD_MOUSE_INVERSE_Y,
    DD_FULLBRIGHT, ///< Render everything fullbright?
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
    DD_WEAPON_OFFSET_SCALE_Y, ///< 1000x
    DD_MONOCHROME_PATCHES, ///< Convert patch image data to monochrome. 1= linear 2= weighted.
    DD_GAME_DATA_FORMAT,
    DD_GAME_DRAW_HUD_HINT, ///< Doomsday advises not to draw the HUD.
    DD_UPSCALE_AND_SHARPEN_PATCHES,
    DD_SYMBOLIC_ECHO,
    DD_MAX_TEXTURE_UNITS,
    DD_CURRENT_CLIENT_FINALE_ID,
    DD_LAST_VALUE
};

/// General constants (not to be used with Get/Set).
enum {
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
    DD_UNUSED2,
    DD_UNUSED3,
    DD_COLOR_LIMIT,
    DD_PRE,
    DD_POST,
    DD_PLUGIN_VERSION_SHORT,
    DD_PLUGIN_VERSION_LONG,
    DD_HORIZON,
    DD_OLD_GAME_ID,
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
    DD_DEF_FINALE,
    DD_RENDER_RESTART_PRE,
    DD_RENDER_RESTART_POST,
    DD_DEF_SOUND_BY_NAME,
    DD_DEF_SOUND_LUMPNAME,
    DD_ID,
    DD_LUMP,
    DD_CD_TRACK,
    DD_SPRITE,
    DD_FRAME,
    DD_GAME_CONFIG, ///< String: dm/co-op, jumping, etc.
    DD_PLUGIN_NAME, ///< (e.g., jdoom, jheretic etc..., suitable for use with filepaths)
    DD_PLUGIN_NICENAME, ///< (e.g., jDoom, MyGame:Episode2 etc..., fancy name)
    DD_PLUGIN_HOMEURL,
    DD_PLUGIN_DOCSURL,
    DD_DMU_VERSION, ///< Used in the exchange of DMU API versions.

    // Non-integer/special values for Set/Get
    DD_TRANSLATIONTABLES_ADDRESS,
    DD_TRACE_ADDRESS, ///< obsolete divline 'trace' used by PathTraverse.
    DD_SPRITE_REPLACEMENT, ///< Sprite <-> model replacement.
    DD_ACTION_LINK, ///< State action routine addresses.
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
    DD_HEDGE_COUNT,
    DD_BSPLEAF_COUNT,
    DD_BSPNODE_COUNT,
    DD_POLYOBJ_COUNT,
    DD_XGFUNC_LINK, ///< XG line classes
    DD_SHARED_FIXED_TRIGGER_OBSOLETE, ///< obsolete
    DD_GAMETIC,
    DD_OPENRANGE, ///< obsolete
    DD_OPENTOP, ///< obsolete
    DD_OPENBOTTOM, ///< obsolete
    DD_LOWFLOOR, ///< obsolete
    DD_CPLAYER_THRUST_MUL_OBSOLETE, ///< obsolete
    DD_GRAVITY,
    DD_PSPRITE_OFFSET_X, ///< 10x
    DD_PSPRITE_OFFSET_Y, ///< 10x
    DD_PSPRITE_LIGHTLEVEL_MULTIPLIER,
    DD_TORCH_RED,
    DD_TORCH_GREEN,
    DD_TORCH_BLUE,
    DD_TORCH_ADDITIVE,
    DD_TM_FLOOR_Z,  ///< output from P_CheckPosition
    DD_TM_CEILING_Z ///< output from P_CheckPosition
};

/// Bounding box coordinates.
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
// Games
//
//------------------------------------------------------------------------

/**
 * @defgroup game Game
 */

/**
 * Defines the numerous high-level properties of a logical game component.
 * Note that this is POD; no construction or destruction is needed.
 * @see DD_DefineGame() @ingroup game
 */
typedef struct gamedef_s {
   /**
    * Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
    * - Used during resource location for mode-specific assets.
    * - Sent out in netgames (a client can't connect unless mode strings match).
    */
    const char* identityKey;

    /// The base directory for all data-class resources.
    const char* dataPath;

    /// The base directory for all defs-class resources.
    const char* defsPath;

    /// Name of the config directory.
    const char* configDir;

    /// Default title. May be overridden later.
    const char* defaultTitle;

    /// Default author. May be overridden later.
    /// Used for (e.g.) the map author name if not specified in a Map Info definition.
    const char* defaultAuthor;
} GameDef;

/**
 * Extended info about a registered game component.
 * @see DD_GameInfo() @ingroup game
 */
typedef struct gameinfo_s {
    const char* title;
    const char* author;
    const char* identityKey;
} GameInfo;

/**
 * @defgroup resourceFlags Resource Flags
 * @ingroup apiFlags resource
 */
///@{
#define RF_STARTUP          0x1 ///< A required resource needed for and loaded during game start up (can't be a virtual file).
#define RF_FOUND            0x2 ///< Resource has been located.
///@}

/**
 * @defgroup math Math Routines
 * @ingroup base
 */
///@{
#define FRACBITS            16
#define FRACUNIT            (1<<FRACBITS)
#define FRACEPSILON         (1.0f/65535.f) // ~ 1.5e-5
#define FLOATEPSILON        .000001f

#define MAX_OF(x, y)        ((x) > (y)? (x) : (y))
#define MIN_OF(x, y)        ((x) < (y)? (x) : (y))
#define MINMAX_OF(a, x, b)  ((x) < (a)? (a) : (x) > (b)? (b) : (x))
#define SIGN_OF(x)          ((x) > 0? +1 : (x) < 0? -1 : 0)
#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))
#define FEQUAL(x, y)        (INRANGE_OF(x, y, FLOATEPSILON))
#define ROUND(x)            ((int) (((x) < 0.0f)? ((x) - 0.5f) : ((x) + 0.5f)))
#define ABS(x)              ((x) >= 0 ? (x) : -(x))

/// Ceiling of integer quotient of @a a divided by @a b.
#define CEILING(a, b)       ((a) % (b) == 0 ? (a)/(b) : (a)/(b)+1)

/**
 * Used to replace /255 as *reciprocal255 is less expensive with CPU cycles.
 * Note that this should err on the side of being < 1/255 to prevent result
 * exceeding 255 (e.g. 255 * reciprocal255).
 */
#define reciprocal255   0.003921568627f

#define FINEANGLES          8192
#define FINEMASK            (FINEANGLES-1)
#define ANGLETOFINESHIFT    19 ///< Shifts 0x100000000 to 0x2000.

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

#if !defined( DENG_NO_FIXED_ASM ) && !defined( GNU_X86_FIXED_ASM )

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

#else

// Don't use inline assembler in fixed-point calculations.
// (link with plugins/common/m_fixed.c)
fixed_t         FixedMul(fixed_t a, fixed_t b);
fixed_t         FixedDiv2(fixed_t a, fixed_t b);
#endif

// This one is always in plugins/common/m_fixed.c.
fixed_t         FixedDiv(fixed_t a, fixed_t b);
///@}

//------------------------------------------------------------------------
//
// Key Codes
//
//------------------------------------------------------------------------

/// @defgroup input Input

/**
 * @defgroup keyConstants Key Constants
 * @ingroup input
 * Most key data is regular ASCII so key constants correspond to ASCII codes.
 */
///@{
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
    DDKEY_CAPSLOCK,
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
    DDKEY_SUBTRACT, ///< '-' on numeric keypad.
    DDKEY_ADD, ///< '+' on numeric keypad.
    DDKEY_PRINT,
    DDKEY_ENTER, ///< on the numeric keypad.
    DDKEY_DIVIDE, ///< '/' on numeric keypad.
    DDKEY_MULTIPLY, ///< '*' on the numeric keypad.
    DDKEY_SECTION, ///< §
    DD_HIGHEST_KEYCODE
};
///@}

//------------------------------------------------------------------------
//
// Events
//
//------------------------------------------------------------------------

/// Event types. @ingroup input
typedef enum {
    EV_KEY,
    EV_MOUSE_AXIS,
    EV_MOUSE_BUTTON,
    EV_JOY_AXIS,    ///< Joystick main axes (xyz + Rxyz).
    EV_JOY_SLIDER,  ///< Joystick sliders.
    EV_JOY_BUTTON,
    EV_POV,
    EV_SYMBOLIC,    ///< Symbol text pointed to by data1+data2.
    EV_FOCUS,       ///< Change in game window focus (data1=gained, data2=windowID).
    NUM_EVENT_TYPES
} evtype_t;

/// Event states. @ingroup input
typedef enum {
    EVS_DOWN,
    EVS_UP,
    EVS_REPEAT,
    NUM_EVENT_STATES
} evstate_t;

/// Input event. @ingroup input
typedef struct event_s {
    evtype_t        type;
    evstate_t       state; ///< Only used with digital controls.
    int             data1; ///< Keys/mouse/joystick buttons.
    int             data2; ///< Mouse/joystick x move.
    int             data3; ///< Mouse/joystick y move.
    int             data4;
    int             data5;
    int             data6;
} event_t;

/// The mouse wheel is considered two extra mouse buttons.
#define DD_MWHEEL_UP        3
#define DD_MWHEEL_DOWN      4
#define DD_MICKEY_ACCURACY  1000

//------------------------------------------------------------------------
//
// Purge Levels
//
//------------------------------------------------------------------------

/**
 * @defgroup memzone Memory Zone
 * @ingroup base
 */

/**
 * @defgroup purgeLevels Purge Levels
 * @ingroup memzone
 */
///@{
#define PU_APPSTATIC         1 ///< Static entire execution time.
#define PU_GAMESTATIC       40 ///< Static until the game plugin which allocated it is unloaded.
#define PU_MAP              50 ///< Static until map exited (may still be freed during the map, though).
#define PU_MAPSTATIC        52 ///< Not freed until map exited.

#define PU_PURGELEVEL       100 ///< Tags >= 100 are purgable whenever needed.
#define PU_CACHE            101
///@}

//------------------------------------------------------------------------
//
// Map Data
//
//------------------------------------------------------------------------

/// @defgroup map Map Data

/**
 * @defgroup dmu Map Update (DMU)
 * @ingroup map
 */
///@{

/// Public DMU API version number. Requested by the engine during init. @ingroup dmu
#define DMUAPI_VER          1

/// Map Update constants. @ingroup dmu
enum {
    // Do not change the numerical values of the constants!

    /// Flag. OR'ed with a DMU property constant. Note: these use only the most
    /// significant byte.
    /// @{
    DMU_FLAG_MASK           = 0xff000000,
    DMU_SIDEDEF1_OF_LINE    = 0x80000000,
    DMU_SIDEDEF0_OF_LINE    = 0x40000000,
    DMU_TOP_OF_SIDEDEF      = 0x20000000,
    DMU_MIDDLE_OF_SIDEDEF   = 0x10000000,
    DMU_BOTTOM_OF_SIDEDEF   = 0x08000000,
    DMU_FLOOR_OF_SECTOR     = 0x04000000,
    DMU_CEILING_OF_SECTOR   = 0x02000000,
    // (1 bits left)
    ///@}

    DMU_NONE = 0,

    DMU_VERTEX = 1,
    DMU_HEDGE,
    DMU_LINEDEF,
    DMU_SIDEDEF,
    DMU_BSPNODE,
    DMU_BSPLEAF,
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

    DMU_TANGENT_X,
    DMU_TANGENT_Y,
    DMU_TANGENT_Z,
    DMU_TANGENT_XYZ,

    DMU_BITANGENT_X,
    DMU_BITANGENT_Y,
    DMU_BITANGENT_Z,
    DMU_BITANGENT_XYZ,

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
    DMU_SLOPETYPE,
    DMU_ANGLE,
    DMU_OFFSET,

    DMU_OFFSET_X,
    DMU_OFFSET_Y,
    DMU_OFFSET_XY,

    DMU_VALID_COUNT,
    DMU_LINEDEF_COUNT,
    DMU_COLOR, ///< RGB
    DMU_COLOR_RED, ///< red component
    DMU_COLOR_GREEN, ///< green component
    DMU_COLOR_BLUE, ///< blue component
    DMU_ALPHA,
    DMU_BLENDMODE,
    DMU_LIGHT_LEVEL,
    DMT_MOBJS, ///< pointer to start of sector mobjList
    DMU_BOUNDING_BOX, ///< AABoxd
    DMU_BASE,
    DMU_WIDTH,
    DMU_HEIGHT,
    DMU_TARGET_HEIGHT,
    DMU_SPEED,
    DMU_HEDGE_COUNT,
    DMU_FLOOR_PLANE,
    DMU_CEILING_PLANE
};

/**
 * @defgroup ldefFlags Linedef Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_LINEDEF, n, DMU_FLAGS).
 */

/// @addtogroup ldefFlags
///@{
#define DDLF_BLOCKING           0x0001
#define DDLF_DONTPEGTOP         0x0002
#define DDLF_DONTPEGBOTTOM      0x0004
///@}

/**
 * @defgroup sdefFlags Sidedef Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_SIDEDEF, n, DMU_FLAGS).
 */

/// @addtogroup sdefFlags
///@{
#define SDF_BLENDTOPTOMID       0x0001
#define SDF_BLENDMIDTOTOP       0x0002
#define SDF_BLENDMIDTOBOTTOM    0x0004
#define SDF_BLENDBOTTOMTOMID    0x0008
#define SDF_MIDDLE_STRETCH      0x0010 ///< Stretch the middle surface to reach from floor to ceiling.
///@}

/**
 * @defgroup sufFlags Surface Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_SURFACE, n, DMU_FLAGS).
 */

/// @addtogroup sufFlags
///@{
#define DDSUF_MATERIAL_FLIPH    0x00000001 ///< Surface material is flipped horizontally.
#define DDSUF_MATERIAL_FLIPV    0x00000002 ///< Surface material is flipped vertically.
///@}

/// Map Update status code constants. @ingroup dmu
/// Sent to the game when various map update events occur.
enum { /* Do NOT change the numerical values of the constants. */
    DMUSC_LINE_FIRSTRENDERED
};

///@}

/// @ingroup mobj
#define DD_BASE_DDMOBJ_ELEMENTS() \
    thinker_t       thinker;            /* thinker node */ \
    coord_t         origin[3];          /* origin [x,y,z] */

/**
 * All map think-able objects must use this as a base. Also used for sound
 * origin purposes for all of: mobj_t, Polyobj, Sector/Plane
 * @ingroup mobj
 */
typedef struct ddmobj_base_s {
    DD_BASE_DDMOBJ_ELEMENTS()
} ddmobj_base_t;

/// R_SetupMap() modes. @ingroup map
enum {
    DDSMM_AFTER_LOADING,    ///< After loading a savegame...
    DDSMM_FINALIZE,         ///< After everything else is done.
    DDSMM_INITIALIZE,       ///< Before anything else if done.
    DDSMM_AFTER_BUSY        ///< After leaving busy mode, which was used during setup.
};

/// Sector reverb data indices. @ingroup map
enum {
    SRD_VOLUME,
    SRD_SPACE,
    SRD_DECAY,
    SRD_DAMPING,
    NUM_REVERB_DATA
};

/// SideDef section indices. @ingroup map
typedef enum sidedefsection_e {
    SS_MIDDLE,
    SS_BOTTOM,
    SS_TOP
} SideDefSection;

/// Helper macro for converting SideDefSection indices to their associated DMU flag. @ingroup map
#define DMU_FLAG_FOR_SIDEDEFSECTION(s) (\
    (s) == SS_MIDDLE? DMU_MIDDLE_OF_SIDEDEF : \
    (s) == SS_BOTTOM? DMU_BOTTOM_OF_SIDEDEF : DMU_TOP_OF_SIDEDEF)

typedef struct {
    fixed_t origin[2];
    fixed_t direction[2];
} divline_t;

typedef struct {
    float origin[2];
    float direction[2];
} fdivline_t;

/**
 * @defgroup pathTraverseFlags  Path Traverse Flags
 * @ingroup apiFlags map
 */
///@{
#define PT_ADDLINES            1 ///< Intercept with LineDefs.
#define PT_ADDMOBJS            2 ///< Intercept with Mobjs.
///@}

/**
 * @defgroup lineSightFlags Line Sight Flags
 * Flags used to dictate logic within P_CheckLineSight().
 * @ingroup apiFlags map
 */
///@{
#define LS_PASSLEFT            0x1 ///< Ray may cross one-sided linedefs from left to right.
#define LS_PASSOVER            0x2 ///< Ray may cross over sector ceiling height on ray-entry side.
#define LS_PASSUNDER           0x4 ///< Ray may cross under sector floor height on ray-entry side.
///@}

// For (un)linking.
#define DDLINK_SECTOR           0x1
#define DDLINK_BLOCKMAP         0x2
#define DDLINK_NOLINE           0x4

typedef enum intercepttype_e {
    ICPT_MOBJ,
    ICPT_LINE
} intercepttype_t;

typedef struct intercept_s {
    float           distance; // Along trace vector as a fraction.
    intercepttype_t type;
    union {
        struct mobj_s* mobj;
        struct linedef_s* lineDef;
    } d;
} intercept_t;

typedef int (*traverser_t) (const intercept_t* intercept, void* paramaters);

/**
 * A simple POD data structure for representing line trace openings.
 */
typedef struct {
    /// Top and bottom z of the opening.
    float top, bottom;

    /// Distance from top to bottom.
    float range;

    /// Z height of the lowest Plane at the opening on the X|Y axis.
    /// @todo Does not belong here?
    float lowFloor;
} TraceOpening;

//------------------------------------------------------------------------
//
// Mobjs
//
//------------------------------------------------------------------------

/// @defgroup mobj Map Objects
/// @ingroup map

/**
 * Linknodes are used when linking mobjs to lines. Each mobj has a ring
 * of linknodes, each node pointing to a line the mobj has been linked to.
 * Correspondingly each line has a ring of nodes, with pointers to the
 * mobjs that are linked to that particular line. This way it is possible
 * that a single mobj is linked simultaneously to multiple lines (which
 * is common).
 *
 * All these rings are maintained by P_MobjLink() and P_MobjUnlink().
 * @ingroup mobj
 */
typedef struct linknode_s {
    nodeindex_t     prev, next;
    void*           ptr;
    int             data;
} linknode_t;

/**
 * @defgroup stateFlags State Flags
 * @ingroup mobj
 */
///@{
#define STF_FULLBRIGHT      0x00000001
#define STF_NOAUTOLIGHT     0x00000002 ///< Don't automatically add light if fullbright.
///@}

/**
 * @defgroup mobjFlags Mobj Flags
 * @ingroup mobj
 */
///@{
#define DDMF_DONTDRAW       0x00000001
#define DDMF_SHADOW         0x00000002
#define DDMF_ALTSHADOW      0x00000004
#define DDMF_BRIGHTSHADOW   0x00000008
#define DDMF_VIEWALIGN      0x00000010
#define DDMF_FITTOP         0x00000020 ///< Don't let the sprite go into the ceiling.
#define DDMF_NOFITBOTTOM    0x00000040
//#define DDMF_UNUSED1        0x00000080 // Formerly DDMF_NOBLOCKMAP
#define DDMF_LIGHTSCALE     0x00000180 ///< Light scale (0: full, 3: 1/4).
#define DDMF_LIGHTOFFSET    0x0000f000 ///< How to offset light (along Z axis).
//#define DDMF_RESERVED       0x00030000 // Don't touch these!! (translation class).
#define DDMF_BOB            0x00040000 ///< Bob the Z coord up and down.
#define DDMF_LOWGRAVITY     0x00080000 ///< 1/8th gravity (predict).
#define DDMF_MISSILE        0x00100000 ///< Client removes mobj upon impact.
#define DDMF_FLY            0x00200000 ///< Flying object (doesn't matter if airborne).
#define DDMF_NOGRAVITY      0x00400000 ///< Isn't affected by gravity (predict).
#define DDMF_ALWAYSLIT      0x00800000 ///< Always process DL even if hidden.

#define DDMF_SOLID          0x20000000 ///< Solid on client side.
#define DDMF_LOCAL          0x40000000
#define DDMF_REMOTE         0x80000000 ///< This mobj is really on the server.

/// Clear masks (flags the game plugin is not allowed to touch).
#define DDMF_CLEAR_MASK     0xc0000000

#define DDMF_LIGHTSCALESHIFT 7
#define DDMF_LIGHTOFFSETSHIFT 12
///@}

#define DDMOBJ_RADIUS_MAX   32

/// The high byte of the selector is not used for modeldef selecting.
/// 1110 0000 = alpha level (0: opaque => 7: transparent 7/8) @ingroup mobj
#define DDMOBJ_SELECTOR_MASK 0x00ffffff
#define DDMOBJ_SELECTOR_SHIFT 24

#define VISIBLE             1
#define INVISIBLE           -1

/// Momentum axis indices. @ingroup mobj
enum { MX, MY, MZ };

/**
 * Axis-aligned bounding box with integer precision.
 * Handy POD structure for manipulation of bounding boxes. @ingroup map
 */
typedef struct aabox_s {
    union {
        struct {
            int vec4[4];
        };
        struct {
            int arvec2[2][2];
        };
        struct {
            int min[2];
            int max[2];
        };
        struct {
            int minX;
            int minY;
            int maxX;
            int maxY;
        };
    };
} AABox;

/**
 * Axis-aligned bounding box with floating-point precision.
 * Handy POD structure for manipulation of bounding boxes. @ingroup map
 */
typedef struct aaboxf_s {
    union {
        struct {
            float vec4[4];
        };
        struct {
            float arvec2[2][2];
        };
        struct {
            float min[2];
            float max[2];
        };
        struct {
            float minX;
            float minY;
            float maxX;
            float maxY;
        };
    };
} AABoxf;

/**
 * Axis-aligned bounding box with double floating-point precision.
 * Handy POD structure for manipulation of bounding boxes. @ingroup map
 */
typedef struct aaboxd_s {
    union {
        struct {
            double vec4[4];
        };
        struct {
            double arvec2[2][2];
        };
        struct {
            double min[2];
            double max[2];
        };
        struct {
            double minX;
            double minY;
            double maxX;
            double maxY;
        };
    };
} AABoxd;

/// Base mobj_t elements. Games MUST use this as the basis for mobj_t. @ingroup mobj
#define DD_BASE_MOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    nodeindex_t     lineRoot; /* lines to which this is linked */ \
    struct mobj_s*  sNext, **sPrev; /* links in sector (if needed) */ \
\
    struct bspleaf_s* bspLeaf; /* bspLeaf in which this resides */ \
    coord_t         mom[3]; \
    angle_t         angle; \
    spritenum_t     sprite; /* used to find patch_t and flip value */ \
    int             frame; \
    coord_t         radius; \
    coord_t         height; \
    int             ddFlags; /* Doomsday mobj flags (DDMF_*) */ \
    coord_t         floorClip; /* value to use for floor clipping */ \
    int             valid; /* if == valid, already checked */ \
    int             type; /* mobj type */ \
    struct state_s* state; \
    int             tics; /* state tic counter */ \
    coord_t         floorZ; /* highest contacted floor */ \
    coord_t         ceilingZ; /* lowest contacted ceiling */ \
    struct mobj_s*  onMobj; /* the mobj this one is on top of. */ \
    boolean         wallHit; /* the mobj is hitting a wall. */ \
    struct ddplayer_s* dPlayer; /* NULL if not a player mobj. */ \
    coord_t         srvo[3]; /* short-range visual offset (xyz) */ \
    short           visAngle; /* visual angle ("angle-servo") */ \
    int             selector; /* multipurpose info */ \
    int             validCount; /* used in iterating */ \
    int             addFrameCount; \
    unsigned int    lumIdx; /* index+1 of the lumobj/bias source, or 0 */ \
    byte            haloFactors[DDMAXPLAYERS]; /* strengths of halo */ \
    byte            translucency; /* default = 0 = opaque */ \
    short           visTarget; /* -1 = mobj is becoming less visible, */ \
                               /* 0 = no change, 2= mobj is becoming more visible */ \
    int             reactionTime; /* if not zero, freeze controls */ \
    int             tmap, tclass;\
    int             flags;\
    int             flags2;\
    int             flags3;\
    int             health;\
    mobjinfo_t     *info; /* &mobjinfo[mobj->type] */

typedef struct povertex_s {
    coord_t         origin[2];
} povertex_t;

/// Base Polyobj elements. Games MUST use this as the basis for Polyobj. @ingroup map
#define DD_BASE_POLYOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    struct bspleaf_s* bspLeaf; /* bspLeaf in which this resides */ \
    unsigned int    idx; /* Idx of polyobject. */ \
    int             tag; /* Reference tag. */ \
    int             validCount; \
    AABoxd          aaBox; \
    coord_t         dest[2]; /* Destination XY. */ \
    angle_t         angle; \
    angle_t         destAngle; /* Destination angle. */ \
    angle_t         angleSpeed; /* Rotation speed. */ \
    struct linedef_s** lines; \
    unsigned int    lineCount; \
    struct povertex_s* originalPts; /* Used as the base for the rotations. */ \
    struct povertex_s* prevPts; /* Use to restore the old point values. */ \
    double          speed; /* Movement speed. */ \
    boolean         crush; /* Should the polyobj attempt to crush mobjs? */ \
    int             seqType; \
    struct { \
        int         index; \
    } buildData;

//------------------------------------------------------------------------
//
// Refresh
//
//------------------------------------------------------------------------

#define TICRATE             35 // Number of tics / second.
#define TICSPERSEC          35
#define SECONDSPERTIC       (1.0f/TICSPERSEC)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200

/**
 * @defgroup alignmentFlags  Alignment Flags
 * @ingroup apiFlags
 */
///@{
#define ALIGN_LEFT          (0x1)
#define ALIGN_RIGHT         (0x2)
#define ALIGN_TOP           (0x4)
#define ALIGN_BOTTOM        (0x8)

#define ALIGN_TOPLEFT       (ALIGN_TOP|ALIGN_LEFT)
#define ALIGN_TOPRIGHT      (ALIGN_TOP|ALIGN_RIGHT)
#define ALIGN_BOTTOMLEFT    (ALIGN_BOTTOM|ALIGN_LEFT)
#define ALIGN_BOTTOMRIGHT   (ALIGN_BOTTOM|ALIGN_RIGHT)

#define ALL_ALIGN_FLAGS     (ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM)
///@}

typedef enum {
    ORDER_NONE = 0,
    ORDER_LEFTTORIGHT,
    ORDER_RIGHTTOLEFT
} order_t;

typedef enum {
    SCALEMODE_FIRST = 0,
    SCALEMODE_SMART_STRETCH = SCALEMODE_FIRST,
    SCALEMODE_NO_STRETCH, // Never.
    SCALEMODE_STRETCH, // Always.
    SCALEMODE_LAST = SCALEMODE_STRETCH,
    SCALEMODE_COUNT
} scalemode_t;

/// Can the value be interpreted as a valid scale mode identifier?
#define VALID_SCALEMODE(val)    ((val) >= SCALEMODE_FIRST && (val) <= SCALEMODE_LAST)

#define DEFAULT_SCALEMODE_STRETCH_EPSILON   (.38f)

/**
 * @defgroup borderedProjectionFlags  Bordered Projection Flags
 * @ingroup apiFlags
 * @{
 */
#define BPF_OVERDRAW_MASK   0x1
#define BPF_OVERDRAW_CLIP   0x2
/**@}*/

typedef struct {
    int flags;
    scalemode_t scaleMode;
    int width, height;
    int availWidth, availHeight;
    boolean alignHorizontal; /// @c false= align vertically instead.
    float scaleFactor;
    int scissorState;
    RectRaw scissorRegion;
} borderedprojectionstate_t;

//------------------------------------------------------------------------
//
// Sound
//
//------------------------------------------------------------------------

/**
 * @defgroup soundFlags  Sound Flags
 * @ingroup apiFlags
 * Flags specifying the logical behavior of a sound.
 */
///@{
#define DDSF_FLAG_MASK      0xff000000
#define DDSF_NO_ATTENUATION 0x80000000
#define DDSF_REPEAT         0x40000000
///@}

/**
 * @defgroup soundStopFlags  Sound Stop Flags
 * @ingroup apiFlags
 * Flags for use with S_StopSound()
 */
///@{
#define SSF_SECTOR                  0x1 ///< Stop sounds from the sector's emitter.
#define SSF_SECTOR_LINKED_PLANES    0x2 ///< Stop sounds from plane emitters in the same sector.
#define SSF_SECTOR_LINKED_SIDEDEFS  0x4 ///< Stop sounds from sidedef emitters in the same sector.
#define SSF_ALL_SECTOR              (SSF_SECTOR | SSF_SECTOR_LINKED_PLANES | SSF_SECTOR_LINKED_SIDEDEFS)
///@}

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

/// @defgroup resource Resources
///@{

/// Special value used to signify an invalid material id.
#define NOMATERIALID            0

#define DDMAX_MATERIAL_LAYERS   1
///@}

/**
 * @defgroup materialFlags  Material Flags
 * @ingroup apiFlags resource
 */

/// @addtogroup materialFlags
///@{
//#define MATF_UNUSED1            0x1
#define MATF_NO_DRAW            0x2 ///< Material should never be drawn.
#define MATF_SKYMASK            0x4 ///< Sky-mask surfaces using this material.
///@}

/**
 * @defgroup animationGroupFlags  Animation Group Flags
 * @ingroup apiFlags resource
 * @{
 */
#define AGF_SMOOTH              0x1
#define AGF_FIRST_ONLY          0x2
#define AGF_PRECACHE            0x4000 // Group is just for precaching.
/**@}*/

/**
 * @defgroup namespace Namespaces
 * @ingroup resource
 */

/**
 * Material Namespaces
 */

/**
 * @defgroup materialNamespaceNames  Material Namespace Names
 * @ingroup namespace
 * @{
 */
#define MN_SYSTEM_NAME          "System"
#define MN_FLATS_NAME           "Flats"
#define MN_TEXTURES_NAME        "Textures"
#define MN_SPRITES_NAME         "Sprites"
/**@}*/

typedef enum materialnamespaceid_e {
    MN_ANY = -1,
    MATERIALNAMESPACE_FIRST = 1000,
    MN_SYSTEM = MATERIALNAMESPACE_FIRST,
    MN_FLATS,
    MN_TEXTURES,
    MN_SPRITES,
    MATERIALNAMESPACE_LAST = MN_SPRITES,
    MN_INVALID /// Special value used to signify an invalid namespace identifier.
} materialnamespaceid_t;

#define MATERIALNAMESPACE_COUNT  (MATERIALNAMESPACE_LAST - MATERIALNAMESPACE_FIRST + 1)

/// @c true= val can be interpreted as a valid material namespace identifier.
#define VALID_MATERIALNAMESPACEID(val) ((val) >= MATERIALNAMESPACE_FIRST && (val) <= MATERIALNAMESPACE_LAST)

/**
 * Texture Namespaces
 */

/**
 * @defgroup textureNamespaceNames  Texture Namespace Names
 * @ingroup namespace
 */
///@{
#define TN_SYSTEM_NAME          "System"
#define TN_FLATS_NAME           "Flats"
#define TN_TEXTURES_NAME        "Textures"
#define TN_SPRITES_NAME         "Sprites"
#define TN_PATCHES_NAME         "Patches"
#define TN_DETAILS_NAME         "Details"
#define TN_REFLECTIONS_NAME     "Reflections"
#define TN_MASKS_NAME           "Masks"
#define TN_MODELSKINS_NAME      "ModelSkins"
#define TN_MODELREFLECTIONSKINS_NAME "ModelReflectionSkins"
#define TN_LIGHTMAPS_NAME       "Lightmaps"
#define TN_FLAREMAPS_NAME       "Flaremaps"
///@}

/// Texture namespace identifiers. @ingroup namespace
typedef enum texturenamespaceid_e {
    TN_ANY = -1,
    TEXTURENAMESPACE_FIRST = 2000,
    TN_SYSTEM = TEXTURENAMESPACE_FIRST,
    TN_FLATS,
    TN_TEXTURES,
    TN_SPRITES,
    TN_PATCHES,
    TN_DETAILS,
    TN_REFLECTIONS,
    TN_MASKS,
    TN_MODELSKINS,
    TN_MODELREFLECTIONSKINS,
    TN_LIGHTMAPS,
    TN_FLAREMAPS,
    TEXTURENAMESPACE_LAST = TN_FLAREMAPS,
    TN_INVALID /// Special value used to signify an invalid namespace identifier.
} texturenamespaceid_t;

#define TEXTURENAMESPACE_COUNT  (TEXTURENAMESPACE_LAST - TEXTURENAMESPACE_FIRST + 1)

/// @c true= val can be interpreted as a valid texture namespace identifier.
#define VALID_TEXTURENAMESPACEID(val) ((val) >= TEXTURENAMESPACE_FIRST && (val) <= TEXTURENAMESPACE_LAST)

/**
 * Font Namespaces
 */

/**
 * @defgroup fontNamespaceNames  Font Namespace Names
 * @ingroup namespace
 * @{
 */
#define FN_SYSTEM_NAME          "System"
#define FN_GAME_NAME            "Game"
///@}

/// Font namespace identifier. @ingroup namespace
typedef enum fontnamespaceid_e {
    FN_ANY = -1,
    FONTNAMESPACE_FIRST = 3000,
    FN_SYSTEM = FONTNAMESPACE_FIRST,
    FN_GAME,
    FONTNAMESPACE_LAST = FN_GAME,
    FN_INVALID ///< Special value used to signify an invalid namespace identifier.
} fontnamespaceid_t;

#define FONTNAMESPACE_COUNT         (FONTNAMESPACE_LAST - FONTNAMESPACE_FIRST + 1)

/**
 * Determines whether @a val can be interpreted as a valid font namespace
 * identifier. @ingroup namespace
 * @param val Integer value.
 * @return @c true or @c false.
 */
#define VALID_FONTNAMESPACEID(val)  ((val) >= FONTNAMESPACE_FIRST && (val) <= FONTNAMESPACE_LAST)

/// Patch Info
typedef struct {
    patchid_t id;
    struct patchinfo_flags_s {
        uint isCustom:1; ///< Patch does not originate from the current game.
        uint isEmpty:1; ///< Patch contains no color information.
    } flags;
    RectRaw geometry;
    // Temporary until the big DGL drawing rewrite.
    short extraOffset[2]; // Only used with upscaled and sharpened patches.
} patchinfo_t;

/// Sprite Info
typedef struct {
    struct material_s* material;
    int flip;
    RectRaw geometry;
    float texCoord[2]; // Prepared texture coordinates.
    int numFrames; // Number of frames the sprite has.
} spriteinfo_t;

/**
 * Processing modes for GL_LoadGraphics().
 */
typedef enum gfxmode_e {
    LGM_NORMAL = 0,
    LGM_GRAYSCALE = 1,
    LGM_GRAYSCALE_ALPHA = 2,
    LGM_WHITE_ALPHA = 3
} gfxmode_t;

typedef unsigned int colorpaletteid_t;

//------------------------------------------------------------------------
//
// Console
//
//------------------------------------------------------------------------

/// @defgroup console Console

/**
 * @defgroup bindings Bindings
 * @ingroup input
 * Event and controller bindings.
 * Input events and input controller state can be bound to console commands
 * and player controls.
 */

/**
 * @defgroup cvar Console Variables
 * @ingroup console
 */

/**
 * @defgroup ccmd Console Commands
 * @ingroup console
 */

/**
 * @defgroup busyModeFlags Busy Mode Flags
 * @ingroup console apiFlags
 */
///@{
#define BUSYF_LAST_FRAME    0x1
#define BUSYF_CONSOLE_OUTPUT 0x2
#define BUSYF_PROGRESS_BAR  0x4
#define BUSYF_ACTIVITY      0x8  ///< Indicate activity.
#define BUSYF_NO_UPLOADS    0x10 ///< Deferred uploads not completed.
#define BUSYF_STARTUP       0x20 ///< Startup mode: normal fonts, texman not available.
#define BUSYF_TRANSITION    0x40 ///< Do a transition effect when busy mode ends.
///@}

/**
 * @defgroup consolePrintFlags Console Print Flags
 * @ingroup console apiFlags
 */
///@{
#define CPF_BLACK           0x00000001 ///< These correspond to the good old text mode VGA colors.
#define CPF_BLUE            0x00000002
#define CPF_GREEN           0x00000004
#define CPF_CYAN            0x00000008
#define CPF_RED             0x00000010
#define CPF_MAGENTA         0x00000020
#define CPF_YELLOW          0x00000040
#define CPF_WHITE           0x00000080
#define CPF_LIGHT           0x00000100
#define CPF_UNUSED1         0x00000200
#define CPF_CENTER          0x00000400
#define CPF_TRANSMIT        0x80000000 ///< If server, sent to all clients.
///@}

/// Argument type for B_BindingsForControl(). @ingroup bindings
typedef enum bfcinverse_e {
    BFCI_BOTH,
    BFCI_ONLY_NON_INVERSE,
    BFCI_ONLY_INVERSE
} bfcinverse_t;

/**
 * @defgroup consoleCommandFlags Console Command Flags
 * @ingroup ccmd apiFlags
 */
///@{
#define CMDF_NO_NULLGAME    0x00000001 ///< Not available unless a game is loaded.
#define CMDF_NO_DEDICATED   0x00000002 ///< Not available in dedicated server mode.
///@}

/**
 * @defgroup consoleUsageFlags Console Command Usage Flags
 * @ingroup ccmd apiFlags
 * The method(s) that @em CANNOT be used to invoke a console command, used with
 * @ref commandSource.
 */
///@{
#define CMDF_DDAY           0x00800000
#define CMDF_GAME           0x01000000
#define CMDF_CONSOLE        0x02000000
#define CMDF_BIND           0x04000000
#define CMDF_CONFIG         0x08000000
#define CMDF_PROFILE        0x10000000
#define CMDF_CMDLINE        0x20000000
#define CMDF_DED            0x40000000
#define CMDF_CLIENT         0x80000000 ///< Sent over the net from a client.
///@}

/**
 * @defgroup commandSource Command Sources
 * @ingroup ccmd
 * Where a console command originated.
 */
///@{
#define CMDS_UNKNOWN        0
#define CMDS_DDAY           1 ///< Sent by the engine.
#define CMDS_GAME           2 ///< Sent by a game library.
#define CMDS_CONSOLE        3 ///< Sent via direct console input.
#define CMDS_BIND           4 ///< Sent from a binding/alias.
#define CMDS_CONFIG         5 ///< Sent via config file.
#define CMDS_PROFILE        6 ///< Sent via player profile.
#define CMDS_CMDLINE        7 ///< Sent via the command line.
#define CMDS_SCRIPT         8 ///< Sent based on a def in a DED file eg (state->execute).
///@}

/**
 * Console command template. Used with Con_AddCommand().
 * @ingroup ccmd
 */
typedef struct ccmdtemplate_s {
    /// Name of the command.
    const char* name;

    /// Argument template.
    const char* argTemplate;

    /// Execute function.
    int (*execFunc) (byte src, int argc, char** argv);

    /// @ref consoleCommandFlags
    int flags;
} ccmdtemplate_t;

/// Helper macro for declaring console command functions. @ingroup console
#define D_CMD(x)            int CCmd##x(byte src, int argc, char** argv)

/**
 * Helper macro for registering new console commands.
 * @ingroup ccmd
 */
#define C_CMD(name, argTemplate, fn) \
    { ccmdtemplate_t _template = { name, argTemplate, CCmd##fn, 0 }; Con_AddCommand(&_template); }

/**
 * Helper macro for registering new console commands.
 * @ingroup ccmd
 */
#define C_CMD_FLAGS(name, argTemplate, fn, flags) \
    { ccmdtemplate_t _template = { name, argTemplate, CCmd##fn, flags }; Con_AddCommand(&_template); }

/**
 * @defgroup consoleVariableFlags Console Variable Flags
 * @ingroup apiFlags cvar
 */
///@{
#define CVF_NO_ARCHIVE      0x1 ///< Not written in/read from the defaults file.
#define CVF_PROTECTED       0x2 ///< Can't be changed unless forced.
#define CVF_NO_MIN          0x4 ///< Minimum is not in affect.
#define CVF_NO_MAX          0x8 ///< Maximum is not in affect.
#define CVF_CAN_FREE        0x10 ///< The string can be freed.
#define CVF_HIDE            0x20 ///< Do not include in listings or add to known words.
#define CVF_READ_ONLY       0x40 ///< Can't be changed manually at all.
///@}

/**
 * @defgroup setVariableFlags Console Set Variable Flags
 * @ingroup apiFlags cvar
 * Use with the various Con_Set* routines (e.g., Con_SetInteger2()).
 */
///@{
#define SVF_WRITE_OVERRIDE  0x1 ///< Override a read-only restriction.
///@}

/// Console variable types. @ingroup console
typedef enum {
    CVT_NULL,
    CVT_BYTE,
    CVT_INT,
    CVT_FLOAT,
    CVT_CHARPTR, ///< ptr points to a char*, which points to the string.
    CVT_URIPTR, ///< ptr points to a Uri*, which points to the uri.
    CVARTYPE_COUNT
} cvartype_t;

#define VALID_CVARTYPE(val) ((val) >= CVT_NULL && (val) < CVARTYPE_COUNT)

/**
 * Console variable template. Used with Con_AddVariable().
 * @ingroup cvar
 */
typedef struct cvartemplate_s {
    /// Path of the variable.
    const char* path;

    /// @ref consoleVariableFlags
    int flags;

    /// Type of variable.
    cvartype_t type;

    /// Pointer to the user data.
    void* ptr;

    /// Minimum and maximum values (for ints and floats).
    float min, max;

    /// On-change notification callback.
    void (*notifyChanged)(void);
} cvartemplate_t;

#define C_VAR(path, ptr, type, flags, min, max, notifyChanged)            \
    { cvartemplate_t _template = { path, flags, type, ptr, min, max, notifyChanged };    \
        Con_AddVariable(&_template); }

/// Helper macro for registering a new byte console variable. @ingroup cvar
#define C_VAR_BYTE(path, ptr, flags, min, max)    \
    C_VAR(path, ptr, CVT_BYTE, flags, min, max, NULL)

/// Helper macro for registering a new integer console variable. @ingroup cvar
#define C_VAR_INT(path, ptr, flags, min, max)     \
    C_VAR(path, ptr, CVT_INT, flags, min, max, NULL)

/// Helper macro for registering a new float console variable. @ingroup cvar
#define C_VAR_FLOAT(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_FLOAT, flags, min, max, NULL)

/// Helper macro for registering a new text console variable. @ingroup cvar
#define C_VAR_CHARPTR(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_CHARPTR, flags, min, max, NULL)

/// Helper macro for registering a new Uri console variable. @ingroup cvar
#define C_VAR_URIPTR(path, ptr, flags, min, max) \
    C_VAR(path, ptr, CVT_URIPTR, flags, min, max, NULL)

/// Same as C_VAR_BYTE() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_BYTE2(path, ptr, flags, min, max, notifyChanged)    \
    C_VAR(path, ptr, CVT_BYTE, flags, min, max, notifyChanged)

/// Same as C_VAR_INT() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_INT2(path, ptr, flags, min, max, notifyChanged)     \
    C_VAR(path, ptr, CVT_INT, flags, min, max, notifyChanged)

/// Same as C_VAR_FLOAT() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_FLOAT2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_FLOAT, flags, min, max, notifyChanged)

/// Same as C_VAR_CHARPTR() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_CHARPTR2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_CHARPTR, flags, min, max, notifyChanged)

/// Same as C_VAR_URIPTR() except allows specifying a callback function for
/// change notification. @ingroup cvar
#define C_VAR_URIPTR2(path, ptr, flags, min, max, notifyChanged) \
    C_VAR(path, ptr, CVT_URIPTR, flags, min, max, notifyChanged)


//------------------------------------------------------------------------
//
// Networking
//
//------------------------------------------------------------------------

/// @defgroup network Network

/**
 * @defgroup netEvents Network Events
 * @ingroup network
 * @{
 */
/// Network player event.
enum {
    DDPE_ARRIVAL, ///< A player has arrived.
    DDPE_EXIT, ///< A player has exited the game.
    DDPE_CHAT_MESSAGE, ///< A player has sent a chat message.
    DDPE_DATA_CHANGE ///< The data for this player has been changed.
};

/// Network world events (handled by clients).
enum {
    DDWE_HANDSHAKE, // Shake hands with a new player.
    DDWE_DEMO_END // Demo playback ends.
};
///@}

/**
 * Information about a multiplayer server. @ingroup network
 *
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
    unsigned short  ping;       ///< Milliseconds.
    char            plugin[32]; ///< Game plugin and version.
    char            gameIdentityKey[17];
    char            gameConfig[40];
    char            map[20];
    char            clientNames[128];
    unsigned int    wadNumber;
    char            iwad[32];   ///< Obsolete.
    char            pwads[128];
    int             data[3];
} serverinfo_t;

/**
 * @defgroup netPackets Network Packets
 * @ingroup network
 * @{
 */
#define DDPT_HELLO              0
#define DDPT_OK                 1
#define DDPT_CANCEL             2
#define DDPT_FIRST_GAME_EVENT   64  ///< All packet types handled by the game should be >= 64.
#define DDPT_MESSAGE            67
///@}

/**
 * @defgroup netSendFlags Packet Send Flags
 * @ingroup network apiFlags
 * The flags are OR'd with @a to_player.
 * @see Net_SendPacket()
 * @{
 */
#define DDSP_ALL_PLAYERS    0x80000000 ///< Broadcast (for server).
///@}

//------------------------------------------------------------------------
//
// Player Data
//
//------------------------------------------------------------------------

/**
 * @defgroup player Player
 * @ingroup playsim
 */

/// Built-in control identifiers. @ingroup player
enum {
    CTL_WALK = 1,           ///< Forward/backwards.
    CTL_SIDESTEP = 2,       ///< Left/right sideways movement.
    CTL_ZFLY = 3,           ///< Up/down movement.
    CTL_TURN = 4,           ///< Turning horizontally.
    CTL_LOOK = 5,           ///< Turning up and down.
    CTL_MODIFIER_1 = 6,
    CTL_MODIFIER_2 = 7,
    CTL_MODIFIER_3 = 8,
    CTL_MODIFIER_4 = 9,
    CTL_FIRST_GAME_CONTROL = 1000
};

/// Control type.
typedef enum controltype_e {
    CTLT_NUMERIC,               ///< Control with a numeric value determined by current device state.
    CTLT_NUMERIC_TRIGGERED,     ///< Numeric, but accepts triggered states as well.
    CTLT_IMPULSE                ///< Always accepts triggered states.
} controltype_t;

/**
 * @defgroup playerFlags Player Flags
 * @ingroup player apiFlags
 * @{
 */
#define DDPF_FIXANGLES          0x0001 ///< Server: send angle/pitch to client.
//#define DDPF_FILTER             0x0002 // Server: send filter to client.
#define DDPF_FIXORIGIN          0x0004 ///< Server: send coords to client.
#define DDPF_DEAD               0x0008 ///< Cl & Sv: player is dead.
#define DDPF_CAMERA             0x0010 ///< Player is a cameraman.
#define DDPF_LOCAL              0x0020 ///< Player is local (e.g. player zero).
#define DDPF_FIXMOM             0x0040 ///< Server: send momentum to client.
#define DDPF_NOCLIP             0x0080 ///< Client: don't clip movement.
#define DDPF_CHASECAM           0x0100 ///< Chase camera mode (third person view).
#define DDPF_INTERYAW           0x0200 ///< Interpolate view yaw angles (used with locking).
#define DDPF_INTERPITCH         0x0400 ///< Interpolate view pitch angles (used with locking).
#define DDPF_VIEW_FILTER        0x0800 ///< Cl & Sv: Draw the current view filter.
#define DDPF_REMOTE_VIEW_FILTER 0x1000 ///< Client: Draw the view filter (has been set remotely).
#define DDPF_USE_VIEW_FILTER    (DDPF_VIEW_FILTER | DDPF_REMOTE_VIEW_FILTER)
#define DDPF_UNDEFINED_ORIGIN   0x2000 ///< Origin of the player is undefined (view not drawn).
#define DDPF_UNDEFINED_WEAPON   0x4000 ///< Weapon of the player is undefined (not sent yet).
///@}

/// Maximum length of a player name.
#define PLAYERNAMELEN       81

/// Normally one for the weapon and one for the muzzle flash.
#define DDMAXPSPRITES       2

/// Psprite states. @ingroup player
enum {
    DDPSP_BOBBING,
    DDPSP_FIRE,
    DDPSP_DOWN,
    DDPSP_UP
};

/**
 * @defgroup pspriteFlags PSprite Flags
 * @ingroup player apiFlags
 */
///@{
#define DDPSPF_FULLBRIGHT 0x1
///@}

/// Player sprite. @ingroup player
typedef struct {
    state_t*        statePtr;
    int             tics;
    float           alpha;
    float           pos[2];
    byte            flags; /// @ref pspriteFlags
    int             state;
    float           offset[2];
} ddpsprite_t;

/// Player lookdir (view pitch) conversion to degrees. @ingroup player
#define LOOKDIR2DEG(x)  ((x) * 85.0/110.0)
/// Player lookdir (view pitch) conversion to radians. @ingroup player
#define LOOKDIR2RAD(x)  (LOOKDIR2DEG(x)/180*PI)

struct mobj_s;
struct polyobj_s;

    typedef struct fixcounters_s {
        int             angles;
        int             origin;
        int             mom;
    } fixcounters_t;

    typedef struct ddplayer_s {
        float           forwardMove; // Copied from player brain (read only).
        float           sideMove; // Copied from player brain (read only).
        struct mobj_s*  mo; // Pointer to a (game specific) mobj.
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
} // extern "C"
#endif

#endif /* LIBDENG_SHARED_H */
