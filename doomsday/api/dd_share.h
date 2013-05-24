/**
 * @file dd_share.h
 * Shared macros and constants.
 *
 * Various macros and constants used by the engine and games.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_SHARED_H
#define LIBDENG_SHARED_H

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
#include <de/aabox.h>
#include "dengproject.h"
#include "dd_version.h"
#include "dd_types.h"
#include "def_share.h"
#include "api_wad.h"
#include "api_thinker.h"
#include "api_map.h"
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

/// Maximum number of players supported by the engine.
#define DDMAXPLAYERS        16

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
    DD_GAME_DATA_FORMAT,
    DD_GAME_DRAW_HUD_HINT, ///< Doomsday advises not to draw the HUD.
    DD_SYMBOLIC_ECHO,
    DD_MAX_TEXTURE_UNITS,
    DD_FIXEDCOLORMAP_ATTENUATE,
    DD_LAST_VALUE,

    DD_CURRENT_CLIENT_FINALE_ID
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
    DD_DEF_VALUE_BY_INDEX,
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
    DD_DEF_ACTION,

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
    DD_TM_FLOOR_Z,              ///< output from P_CheckPosition
    DD_TM_CEILING_Z,            ///< output from P_CheckPosition
    DD_SHIFT_DOWN,
    DD_GAME_RECOMMENDS_SAVING,  ///< engine asks whether game should be saved (e.g., when upgrading) (game's GetInteger)
    DD_NOTIFY_GAME_SAVED        ///< savegame was written
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
///@{

/**
 * Used to replace /255 as *reciprocal255 is less expensive with CPU cycles.
 * Note that this should err on the side of being < 1/255 to prevent result
 * exceeding 255 (e.g. 255 * reciprocal255).
 */
#define reciprocal255   0.003921568627f

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
    DMU_HEDGE,
    DMU_LINE,
    DMU_SIDE,
    DMU_BSPNODE,
    DMU_BSPLEAF,
    DMU_SECTOR,
    DMU_PLANE,
    DMU_SURFACE,
    DMU_MATERIAL,
    DMU_LAST_ELEMENT_TYPE_ID = DMU_MATERIAL,

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
    DDSMM_INITIALIZE        ///< Before anything else if done.
};

/// Sector reverb data indices. @ingroup map
enum {
    SRD_VOLUME,
    SRD_SPACE,
    SRD_DECAY,
    SRD_DAMPING,
    NUM_REVERB_DATA
};

/// Environmental audio characteristics. @ingroup map
typedef float AudioEnvironmentFactors[NUM_REVERB_DATA];

/// Side section indices. @ingroup map
typedef enum sidesection_e {
    SS_MIDDLE,
    SS_BOTTOM,
    SS_TOP
} SideSection;

#define VALID_SIDESECTION(v) ((v) >= SS_MIDDLE && (v) <= SS_TOP)

/// Helper macro for converting SideSection indices to their associated DMU flag. @ingroup map
#define DMU_FLAG_FOR_SIDESECTION(s) (\
    (s) == SS_MIDDLE? DMU_MIDDLE_OF_SIDE : \
    (s) == SS_BOTTOM? DMU_BOTTOM_OF_SIDE : DMU_TOP_OF_SIDE)

typedef struct {
    float origin[2];
    float direction[2];
} fdivline_t;

/**
 * @defgroup pathTraverseFlags  Path Traverse Flags
 * @ingroup apiFlags map
 */
///@{
#define PT_ADDLINES            1 ///< Intercept with Lines.
#define PT_ADDMOBJS            2 ///< Intercept with Mobjs.
///@}

/**
 * @defgroup lineSightFlags Line Sight Flags
 * Flags used to dictate logic within P_CheckLineSight().
 * @ingroup apiFlags map
 */
///@{
#define LS_PASSLEFT            0x1 ///< Ray may cross one-sided lines from left to right.
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
    float distance; ///< Along trace vector as a fraction.
    intercepttype_t type;
    union {
        struct mobj_s *mobj;
        Line *line;
    } d;
} intercept_t;

typedef int (*traverser_t) (intercept_t const *intercept, void *parameters);

/**
 * A simple POD data structure for representing line trace openings.
 */
typedef struct traceopening_s {
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

/// Base mobj_t elements. Games MUST use this as the basis for mobj_t. @ingroup mobj
#define DD_BASE_MOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    nodeindex_t     lineRoot; /* lines to which this is linked */ \
    struct mobj_s*  sNext, **sPrev; /* links in sector (if needed) */ \
\
    BspLeaf *bspLeaf; /* bspLeaf in which this resides */ \
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
    int             tmap, tclass; /* color translation (0, 0 == none) */ \
    int             flags;\
    int             flags2;\
    int             flags3;\
    int             health;\
    mobjinfo_t     *info; /* &mobjinfo[mobj->type] */

/// Base Polyobj elements. Games MUST use this as the basis for Polyobj. @ingroup map
#define DD_BASE_POLYOBJ_ELEMENTS() \
    DD_BASE_DDMOBJ_ELEMENTS() \
\
    BspLeaf        *_bspLeaf; /* bspLeaf in which this resides */ \
    int             _indexInMap; \
    int             tag; /* Reference tag. */ \
    int             validCount; \
    AABoxd          aaBox; \
    coord_t         dest[2]; /* Destination XY. */ \
    angle_t         angle; \
    angle_t         destAngle; /* Destination angle. */ \
    angle_t         angleSpeed; /* Rotation speed. */ \
    void           *_lines; \
    void           *_uniqueVertexes; \
    void           *_originalPts; /* Used as the base for the rotations. */ \
    void           *_prevPts; /* Use to restore the old point values. */ \
    double          speed; /* Movement speed. */ \
    boolean         crush; /* Should the polyobj attempt to crush mobjs? */ \
    int             seqType; \
    uint            _origIndex;

//------------------------------------------------------------------------
//
// Refresh
//
//------------------------------------------------------------------------

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

/// Can the value be interpreted as a valid scale mode identifier?
#define VALID_SCALEMODE(val)    ((val) >= SCALEMODE_FIRST && (val) <= SCALEMODE_LAST)

#define DEFAULT_SCALEMODE_STRETCH_EPSILON   (.38f)

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
    Material *material;
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
    unsigned int flags;
} ccmdtemplate_t;

/// Helper macro for declaring console command functions. @ingroup console
#define D_CMD(x)        DENG_EXTERN_C int CCmd##x(byte src, int argc, char** argv)

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
    unsigned int    loadedFilesCRC;
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

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SHARED_H */
