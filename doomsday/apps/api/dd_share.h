/**
 * @file dd_share.h
 * Shared macros and constants.
 *
 * Various macros and constants used by the engine and games.
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_SHARED_H
#define DE_SHARED_H

#ifndef C_DECL
#  if defined(WIN32)
/// Defines the calling convention for compare functions. Only used on Windows.
#    define C_DECL __cdecl
#  elif defined(UNIX)
#    define C_DECL
#  else
#    define C_DECL
#  endif
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <de/legacy/aabox.h>
#include <doomsday/players.h>
#include <doomsday/player.h>
#include <doomsday/console/var.h>
#include <doomsday/console/cmd.h>
#include <doomsday/api_map.h>
#include "dengproject.h"
#include "dd_version.h"
#include "dd_types.h"
#include "api_thinker.h"
#include "api_gl.h"
#include "api_busy.h"
#include "api_event.h"
#include "api_player.h"
#include "api_infine.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------
//
// General Definitions and Macros
//
//------------------------------------------------------------------------

/// @defgroup apiFlags Flags

// The case-independent strcmps have different names.
#if WIN32
# define strcasecmp _stricmp
# define strncasecmp _strnicmp
# define strlwr _strlwr
#endif

#if UNIX
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

    // Format checking for printf-like functions in GCC2
#if defined(__GNUC__) && __GNUC__ >= 2
#   define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#else
#   define PRINTF_F(f,v)
#endif

int16_t         ShortSwap(int16_t);
int32_t         LongSwap(int32_t);
float           FloatSwap(float);

#ifdef __BIG_ENDIAN__
#define DD_SHORT(x)         ShortSwap(x)
#define DD_LONG(x)          LongSwap(x)
#define DD_FLOAT(x)         FloatSwap(x)

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
#define DD_SHORT(x)         (x)
#define DD_LONG(x)          (x)
#define DD_FLOAT(x)         (x)
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
#define DD_USHORT(x)        ((uint16_t) DD_SHORT(x))
#define DD_ULONG(x)         ((uint32_t) DD_LONG(x))
///@}

enum {
    DD_FIRST_VALUE = 0,

    DD_NOVIDEO = DD_FIRST_VALUE,
    DD_NETGAME,
    DD_SERVER,                  ///< Running in server mode + listening.
    DD_CLIENT,
    DD_CONSOLEPLAYER,
    DD_DISPLAYPLAYER,
    DD_GOTFRAME,
    DD_NUMSOUNDS,

    // Server-only:
    DD_SERVER_ALLOW_FRAMES,

    // Client-only:
    DD_RENDER_FULLBRIGHT,       ///< Render everything fullbright?
    DD_GAME_READY,
    DD_PLAYBACK,
    DD_CLIENT_PAUSED,
    DD_WEAPON_OFFSET_SCALE_Y,   ///< 1000x
    DD_GAME_DRAW_HUD_HINT,      ///< Doomsday advises not to draw the HUD.
    DD_SYMBOLIC_ECHO,
    DD_FIXEDCOLORMAP_ATTENUATE,

    DD_LAST_VALUE,

    // Other values:
    DD_GAME_EXPORTS = 0x1000,
    DD_SHIFT_DOWN,

    DD_WINDOW_WIDTH = 0x1100,
    DD_WINDOW_HEIGHT,
    DD_WINDOW_HANDLE,
    DD_USING_HEAD_TRACKING,
    DD_DYNLIGHT_TEXTURE,
    DD_PSPRITE_OFFSET_X,        ///< 10x
    DD_PSPRITE_OFFSET_Y,        ///< 10x
    DD_PSPRITE_LIGHTLEVEL_MULTIPLIER,
    DD_TORCH_RED,
    DD_TORCH_GREEN,
    DD_TORCH_BLUE,

    DD_DEFS = 0x1200,                    ///< engine definition database (DED)
    DD_NUMMOBJTYPES,

    DD_CURRENT_CLIENT_FINALE_ID = 0x1300,

    DD_GAMETIC = 0x1400,
    DD_MAP_BOUNDING_BOX,
    DD_MAP_MUSIC,
    DD_MAP_MIN_X,
    DD_MAP_MIN_Y,
    DD_MAP_MAX_X,
    DD_MAP_MAX_Y,
    DD_MAP_POLYOBJ_COUNT,
    DD_MAP_GRAVITY,
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
 * @defgroup fileFlags File Flags
 * @ingroup apiFlags fs
 */
///@{
#define FF_STARTUP          0x1 ///< A required file needed for and loaded during game start up (can't be a virtual file).
#define FF_FOUND            0x2 ///< File has been located.
///@}

/**
 * @defgroup math Math Routines
 * @ingroup base
 */

//------------------------------------------------------------------------
//
// World Data
//
//------------------------------------------------------------------------

/// @defgroup world World Data

/**
 * @defgroup dmu Map Update (DMU)
 * @ingroup world
 */
///@{

/// Map Update constants. @ingroup dmu
enum {
    // Do not change the numerical values of the constants!

    /// Flag. OR'ed with a DMU property constant. Note: these use only the most
    /// significant byte.
    /// @{
    DMU_FLAG_MASK           = 0xff000000,
    DMU_BACK_OF_LINE        = 0x80000000,
    DMU_FRONT_OF_LINE       = 0x40000000,
    DMU_TOP_OF_SIDE         = 0x20000000,
    DMU_MIDDLE_OF_SIDE      = 0x10000000,
    DMU_BOTTOM_OF_SIDE      = 0x08000000,
    DMU_FLOOR_OF_SECTOR     = 0x04000000,
    DMU_CEILING_OF_SECTOR   = 0x02000000,
    // (1 bits left)
    ///@}

    DMU_NONE = 0,

    /*
     * Element types:
     */
    DMU_FIRST_ELEMENT_TYPE_ID = 1,
    DMU_VERTEX = DMU_FIRST_ELEMENT_TYPE_ID,
    DMU_SEGMENT,
    DMU_LINE,
    DMU_SIDE,
    DMU_SECTOR,
    DMU_PLANE,
    DMU_SURFACE,
    DMU_MATERIAL,
    DMU_SUBSPACE,
    DMU_SKY,
    DMU_LAST_ELEMENT_TYPE_ID = DMU_SKY,

    /*
     * Selection methods:
     */
    DMU_LINE_BY_TAG,
    DMU_SECTOR_BY_TAG,

    DMU_LINE_BY_ACT_TAG,
    DMU_SECTOR_BY_ACT_TAG,

    /*
     * Element properties:
     */
    DMU_ARCHIVE_INDEX, ///< Relevant data/definition position in the "archived" map.

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

    DMU_FRONT,
    DMU_BACK,
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
    DMU_COLOR, ///< RGB
    DMU_COLOR_RED, ///< red component
    DMU_COLOR_GREEN, ///< green component
    DMU_COLOR_BLUE, ///< blue component
    DMU_ALPHA,
    DMU_BLENDMODE,
    DMU_LIGHT_LEVEL,
    DMT_MOBJS, ///< pointer to start of sector mobjList
    DMU_BOUNDING_BOX, ///< AABoxd
    DMU_EMITTER,
    DMU_WIDTH,
    DMU_HEIGHT,
    DMU_TARGET_HEIGHT,
    DMU_SPEED,
    DMU_FLOOR_PLANE,
    DMU_CEILING_PLANE
};

/// Determines whether @a val can be interpreted as a valid DMU element type id.
#define VALID_DMU_ELEMENT_TYPE_ID(val) ((int)(val) >= (int)DMU_FIRST_ELEMENT_TYPE_ID && (int)(val) <= (int)DMU_LAST_ELEMENT_TYPE_ID)

/**
 * @defgroup ldefFlags Line Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_LINE, n, DMU_FLAGS).
 */

/// @addtogroup ldefFlags
///@{
#define DDLF_BLOCKING           0x0001
#define DDLF_DONTPEGTOP         0x0002
#define DDLF_DONTPEGBOTTOM      0x0004
///@}

/**
 * @defgroup sdefFlags Side Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_SIDE, n, DMU_FLAGS).
 */

/// @addtogroup sdefFlags
///@{
#define SDF_BLENDTOPTOMID           0x0001
#define SDF_BLENDMIDTOTOP           0x0002
#define SDF_BLENDMIDTOBOTTOM        0x0004
#define SDF_BLENDBOTTOMTOMID        0x0008
#define SDF_MIDDLE_STRETCH          0x0010 ///< Stretch the middle surface to reach from floor to ceiling.

/// Suppress the relative back sector and consider this as one-sided for the
/// purposes of rendering and line of sight tests.
#define SDF_SUPPRESS_BACK_SECTOR    0x0020
///@}

/**
 * @defgroup skyLayerFlags Sky Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_SKY, n, DMU_FLAGS).
 */

/// @addtogroup skyFlags
///@{
#define SKYF_LAYER0_ENABLED     0x00000100  ///< Layer 0 is enabled.
#define SKYF_LAYER1_ENABLED     0x00010000  ///< Layer 1 is enabled.
///@}

/**
 * @defgroup surfaceFlags Surface Flags
 * @ingroup dmu apiFlags
 * For use with P_Set/Get(DMU_SURFACE, n, DMU_FLAGS).
 */

/// @addtogroup surfaceFlags
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
    thinker_t       thinker;   /* thinker node */ \
    coord_t         origin[3]; /* origin [x,y,z] */ \
    void           *_bspLeaf;  /* BSP leaf in which this resides (if known) */

/**
 * All map think-able objects must use this as a base. Also used for sound
 * origin purposes for all of: mobj_t, Polyobj, Sector/Plane
 * @ingroup mobj
 */
typedef struct ddmobj_base_s {
    DD_BASE_DDMOBJ_ELEMENTS()
} ddmobj_base_t;

/// A base mobj instance is used as a "sound emitter".
typedef ddmobj_base_t SoundEmitter;

//------------------------------------------------------------------------
//
// Mobjs
//
//------------------------------------------------------------------------

/// @defgroup mobj Map Objects
/// @ingroup world

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
#define DDMF_MOVEBLOCKEDX   0x00000080 ///< Indicates that mobj was unable to move last tick.
#define DDMF_MOVEBLOCKEDY   0x00000100 ///< Indicates that mobj was unable to move last tick.
#define DDMF_MOVEBLOCKEDZ   0x00000200 ///< Indicates that mobj was unable to move last tick.
#define DDMF_MOVEBLOCKED    0x00000380 ///< Combination of XYZ move blocked.
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
#define DDMF_CLEAR_MASK     0xc0000380

//#define DDMF_LIGHTSCALESHIFT 7
//#define DDMF_LIGHTOFFSETSHIFT 12
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

/// Base mobj_t elements. Games MUST use this as the basis for mobj_t. @ingroup mobj
#define DD_BASE_MOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    nodeindex_t     lineRoot; /* lines to which this is linked */ \
    struct mobj_s  *sNext, **sPrev; /* links in sector (if needed) */ \
\
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
    struct state_s *state; \
    int             tics; /* state tic counter */ \
    coord_t         floorZ; /* highest contacted floor */ \
    coord_t         ceilingZ; /* lowest contacted ceiling */ \
    struct mobj_s  *onMobj; /* the mobj this one is on top of. */ \
    dd_bool         wallHit; /* the mobj is hitting a wall. */ \
    struct ddplayer_s *dPlayer; /* NULL if not a player mobj. */ \
    coord_t         srvo[3]; /* short-range visual offset (xyz) */ \
    short           visAngle; /* visual angle ("angle-servo") */ \
    int             selector; /* multipurpose info */ \
    int             validCount; /* used in iterating */ \
    int             addFrameCount; \
    int             lumIdx; /* index of the lumobj or -1 */ \
    byte            haloFactors[DDMAXPLAYERS]; /* strengths of halo */ \
    byte            translucency; /* default = 0 = opaque */ \
    short           visTarget; /* -1 = mobj is becoming less visible, */ \
                               /* 0 = no change, 2= mobj is becoming more visible */ \
    int             reactionTime; /* if not zero, freeze controls */ \
    int             tmap, tclass; /* color translation (0, 0 == none) */ \
    int             flags;\
    int             flags2;\
    int             flags3;\
    int             health;\
    mobjinfo_t     *info; /* &mobjinfo[mobj->type] */\
    int             mapSpotNum; /* map spot number that spawned this, or -1 */

/// Base Polyobj elements. Games MUST use this as the basis for Polyobj. @ingroup world
#define DD_BASE_POLYOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    int             tag; /* Reference tag. */ \
    int             validCount; \
    AABoxd          bounds; \
    coord_t         dest[2]; /* Destination XY. */ \
    angle_t         angle; \
    angle_t         destAngle; /* Destination angle. */ \
    angle_t         angleSpeed; /* Rotation speed. */ \
    double          speed; /* Movement speed. */ \
    dd_bool         crush; /* Should the polyobj attempt to crush mobjs? */ \
    int             seqType; \

//------------------------------------------------------------------------
//
// Refresh
//
//------------------------------------------------------------------------

#define SCREENWIDTH         320
#define SCREENHEIGHT        200

#define DD_SCREENSHOT_CHECK_EXISTS  0x1

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

/// Can the value be interpreted as a valid scale mode identifier?
#define VALID_SCALEMODE(val)    ((val) >= SCALEMODE_FIRST && (val) <= SCALEMODE_LAST)

#define DEFAULT_SCALEMODE_STRETCH_EPSILON   (.1f)

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
#define SSF_SECTOR_LINKED_SURFACES  0x2 ///< Stop sounds from surface emitters in the same sector.
#define SSF_ALL_SECTOR              (SSF_SECTOR | SSF_SECTOR_LINKED_SURFACES)
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

///@}

/// Unique identifier associated with each archived material.
/// @ingroup resource
typedef unsigned short materialarchive_serialid_t;

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
 * @defgroup animationGroupFlags  (Material) Animation Group Flags
 * @ingroup apiFlags resource
 * @{
 */
#define AGF_SMOOTH              0x1
#define AGF_FIRST_ONLY          0x2
#define AGF_PRECACHE            0x4000 ///< Group is just for precaching.
/**@}*/

/*
 * Font Schemes
 */

/// Font scheme identifier. @ingroup scheme
typedef enum fontschemeid_e {
    FS_ANY = -1,
    FONTSCHEME_FIRST = 3000,
    FS_SYSTEM = FONTSCHEME_FIRST,
    FS_GAME,
    FONTSCHEME_LAST = FS_GAME,
    FS_INVALID ///< Special value used to signify an invalid scheme identifier.
} fontschemeid_t;

#define FONTSCHEME_COUNT         (FONTSCHEME_LAST - FONTSCHEME_FIRST + 1)

/**
 * Determines whether @a val can be interpreted as a valid font scheme
 * identifier. @ingroup scheme
 * @param val Integer value.
 * @return @c true or @c false.
 */
#define VALID_FONTSCHEMEID(val)  ((val) >= FONTSCHEME_FIRST && (val) <= FONTSCHEME_LAST)

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
    world_Material *material;
    int flip;
    RectRaw geometry;
    float texCoord[2]; // Prepared texture coordinates.
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

typedef uint colorpaletteid_t;

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_SHARED_H */
