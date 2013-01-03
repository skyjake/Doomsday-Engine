#ifndef DOOMSDAY_API_PLAYER_H
#define DOOMSDAY_API_PLAYER_H

#include "apis.h"

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

DENG_API_TYPEDEF(Player)
{
    de_api_t api;

    /**
     * Gets the data of a player.
     *
     * @param number  Console/player number.
     *
     * @return Player data.
     */
    ddplayer_t* (*GetPlayer)(int number);

    void (*NewControl)(int id, controltype_t type, const char* name, const char* bindContext);

    void (*GetControlState)(int playerNum, int control, float* pos, float* relativeOffset);

    int (*GetImpulseControlState)(int playerNum, int control);

    void (*Impulse)(int playerNum, int control);
}
DENG_API_T(Player);

#ifndef DENG_NO_API_MACROS_PLAYER
#define DD_GetPlayer                _api_Player.GetPlayer
#define P_NewPlayerControl          _api_Player.NewControl
#define P_GetControlState           _api_Player.GetControlState
#define P_GetImpulseControlState    _api_Player.GetImpulseControlState
#define P_Impulse                   _api_Player.Impulse
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Player);
#endif

#endif // DOOMSDAY_API_PLAYER_H
