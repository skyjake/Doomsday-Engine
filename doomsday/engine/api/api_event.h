#ifndef DOOMSDAY_API_EVENT_H
#define DOOMSDAY_API_EVENT_H

#include "api_base.h"

/// Event types. @ingroup input
typedef enum {
    EV_KEY,
    EV_MOUSE_AXIS,
    EV_MOUSE_BUTTON,
    EV_JOY_AXIS,        ///< Joystick main axes (xyz + Rxyz).
    EV_JOY_SLIDER,      ///< Joystick sliders.
    EV_JOY_BUTTON,
    EV_POV,
    EV_SYMBOLIC,        ///< Symbol text pointed to by data1+data2.
    EV_FOCUS,           ///< Change in game window focus (data1=gained, data2=windowID).
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

/// @addtogroup bindings
///@{

DENG_API_TYPEDEF(B)
{
    de_api_t api;

    void (*SetContextFallback)(const char* name, int (*responderFunc)(event_t*));
    int  (*BindingsForCommand)(const char* cmd, char* buf, size_t bufSize);
    int  (*BindingsForControl)(int localPlayer, const char* controlName, int inverse, char* buf, size_t bufSize);
}
DENG_API_T(B);

#ifndef DENG_NO_API_MACROS_BINDING
#define B_SetContextFallback _api_B.SetContextFallback
#define B_BindingsForCommand _api_B.BindingsForCommand
#define B_BindingsForControl _api_B.BindingsForControl
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(B);
#endif

///@}

#endif // DOOMSDAY_API_EVENT_H
