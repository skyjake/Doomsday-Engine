
//**************************************************************************
//**
//** SYS_INPUT.C
//**
//** Keyboard, mouse and joystick input using SDL.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <SDL.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define EVBUFSIZE		64

#define KEYBUFSIZE		32
#define INV(x, axis)	(joyInverseAxis[axis]? -x : x)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean novideo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int joydevice = 0;				// Joystick index to use.
boolean usejoystick = false;	// Joystick input enabled?
int joyInverseAxis[8];			// Axis inversion (default: all false).

cvar_t inputCVars[] =
{
	{ "i_JoyDevice",		CVF_HIDE|CVF_NO_ARCHIVE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one)." },
	{ "i_UseJoystick",	CVF_HIDE|CVF_NO_ARCHIVE,	CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input." },
//--------------------------------------------------------------------------
	{ "input-joy-device",	CVF_NO_MAX|CVF_PROTECTED,	CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one)." },
	{ "input-joy",		0,							CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input." },
	{ "input-joy-x-inverse",			0,	CVT_INT,	&joyInverseAxis[0],	0, 1,	"1=Inverse joystick X axis." },
	{ "input-joy-y-inverse",			0,	CVT_INT,	&joyInverseAxis[1],	0, 1,	"1=Inverse joystick Y axis." },
	{ "input-joy-z-inverse",			0,	CVT_INT,	&joyInverseAxis[2],	0, 1,	"1=Inverse joystick Z axis." },
	{ "input-joy-rx-inverse",			0,	CVT_INT,	&joyInverseAxis[3],	0, 1,	"1=Inverse joystick RX axis." },
	{ "input-joy-ry-inverse",			0,	CVT_INT,	&joyInverseAxis[4],	0, 1,	"1=Inverse joystick RY axis." },
	{ "input-joy-rz-inverse",			0,	CVT_INT,	&joyInverseAxis[5],	0, 1,	"1=Inverse joystick RZ axis." },
	{ "input-joy-slider1-inverse",	0,	CVT_INT,	&joyInverseAxis[6], 0, 1,	"1=Inverse joystick slider 1." },
	{ "input-joy-slider2-inverse",	0,	CVT_INT,	&joyInverseAxis[7], 0, 1,	"1=Inverse joystick slider 2." },
	{ NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initOk;
static boolean useMouse, useJoystick;

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

// CODE --------------------------------------------------------------------

/*
 * Returns a new key event struct from the buffer.
 */
keyevent_t *I_NewKeyEvent(void)
{
	keyevent_t *ev = keyEvents + evHead;

	evHead = (evHead + 1) % EVBUFSIZE;
	memset(ev, 0, sizeof(*ev));
	return ev;
}

/*
 * Returns the oldest event from the buffer.
 */
keyevent_t *I_GetKeyEvent(void)
{
	keyevent_t *ev;
	
	if(evHead == evTail) return NULL; // No more...
	ev = keyEvents + evTail;
	evTail = (evTail + 1) % EVBUFSIZE;
	return ev;
}

/*
 * Translate the SDL symbolic key code to a DDKEY.
 * FIXME: A translation array for these?
 */
int I_TranslateKeyCode(SDLKey sym)
{
	switch(sym)
	{
	case 167: 		// Tilde
		return 96; 	// ASCII: '`'

	case '\b': 		// Backspace
		return DDKEY_BACKSPACE;

	case SDLK_PAUSE:
		return DDKEY_PAUSE;

	case SDLK_UP:
		return DDKEY_UPARROW;

	case SDLK_DOWN:
		return DDKEY_DOWNARROW;

	case SDLK_LEFT:
		return DDKEY_LEFTARROW;

	case SDLK_RIGHT:
		return DDKEY_RIGHTARROW;

	case SDLK_RSHIFT:
	case SDLK_LSHIFT:
		return DDKEY_RSHIFT;

	case SDLK_RALT:
	case SDLK_LALT:
		return DDKEY_RALT;

	case SDLK_RCTRL:
	case SDLK_LCTRL:
		return DDKEY_RCTRL;

	case SDLK_F1:
		return DDKEY_F1;

	case SDLK_F2:
		return DDKEY_F2;
			
	case SDLK_F3:
		return DDKEY_F3;

	case SDLK_F4:
		return DDKEY_F4;

	case SDLK_F5:
		return DDKEY_F5;
		
	case SDLK_F6:
		return DDKEY_F6;

	case SDLK_F7:
		return DDKEY_F7;

	case SDLK_F8:
		return DDKEY_F8;

	case SDLK_F9:
		return DDKEY_F9;

	case SDLK_F10:
		return DDKEY_F10;

	case SDLK_F11:
		return DDKEY_F11;

	case SDLK_F12:
		return DDKEY_F12;

	case SDLK_NUMLOCK:
		return DDKEY_NUMLOCK;

	case SDLK_SCROLLOCK:
		return DDKEY_SCROLL;

	case SDLK_KP0:
		return DDKEY_NUMPAD0;

	case SDLK_KP1:
		return DDKEY_NUMPAD1;

	case SDLK_KP2:
		return DDKEY_NUMPAD2;
		
	case SDLK_KP3:
		return DDKEY_NUMPAD3;

	case SDLK_KP4:
		return DDKEY_NUMPAD4;

	case SDLK_KP5:
		return DDKEY_NUMPAD5;

	case SDLK_KP6:
		return DDKEY_NUMPAD6;

	case SDLK_KP7:
		return DDKEY_NUMPAD7;

	case SDLK_KP8:
		return DDKEY_NUMPAD8;

	case SDLK_KP9:
		return DDKEY_NUMPAD9;

	case SDLK_KP_PERIOD:
		return DDKEY_DECIMAL;

	case SDLK_KP_PLUS:
		return DDKEY_ADD;

	case SDLK_KP_MINUS:
		return DDKEY_SUBTRACT;

	case SDLK_KP_DIVIDE:
		return '/';

	case SDLK_KP_MULTIPLY:
		return '*';

	case SDLK_KP_ENTER:
		return DDKEY_ENTER;

	case SDLK_INSERT:
		return DDKEY_INS;

	case SDLK_DELETE:
		return DDKEY_DEL;

	case SDLK_HOME:
		return DDKEY_HOME;

	case SDLK_END:
		return DDKEY_END;

	case SDLK_PAGEUP:
		return DDKEY_PGUP;

	case SDLK_PAGEDOWN:
		return DDKEY_PGDN;
		
	default:
		break;
	}
	return sym;
}

/*
 * SDL's events are all returned from the same routine.  This function
 * is called periodically, and the events we are interested in a saved
 * into our own buffer.
 */
void I_PollEvents(void)
{
	SDL_Event event;
	keyevent_t *e;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			e = I_NewKeyEvent();
			e->event = (event.type == SDL_KEYDOWN? IKE_KEY_DOWN : IKE_KEY_UP);
			e->code = I_TranslateKeyCode(event.key.keysym.sym);
			//printf("sdl:%i code:%i\n", event.key.keysym.sym, e->code);
			break;

		case SDL_QUIT:
			// The system wishes to close the program immediately...
			Sys_Quit();
			break;

		default:
			// The rest of the events are ignored.
			break;
		}
	}
}

//===========================================================================
// I_InitMouse
//===========================================================================
void I_InitMouse(void)
{
	if(ArgCheck("-nomouse") || novideo) return;

	// Init was successful.
	useMouse = true;

	// Grab all input.
	SDL_WM_GrabInput(SDL_GRAB_ON);
}

//===========================================================================
// I_InitJoystick
//===========================================================================
void I_InitJoystick(void)
{
	if(ArgCheck("-nojoy")) return;
//	useJoystick = true;
}

//===========================================================================
// I_Init
//	Initialize input. Returns true if successful.
//===========================================================================
int I_Init(void)
{
	if(initOk) return true;	// Already initialized.
	I_InitMouse();
	I_InitJoystick();
	initOk = true;
	return true;
}

//===========================================================================
// I_Shutdown
//===========================================================================
void I_Shutdown(void)
{
	if(!initOk) return;	// Not initialized.
	initOk = false;
}

//===========================================================================
// I_MousePresent
//===========================================================================
boolean I_MousePresent(void)
{
	return useMouse;
}

//===========================================================================
// I_JoystickPresent
//===========================================================================
boolean I_JoystickPresent(void)
{
	return useJoystick;
}

//===========================================================================
// I_GetKeyEvents
//===========================================================================
int I_GetKeyEvents(keyevent_t *evbuf, int bufsize)
{
	keyevent_t *e;
	int i = 0;
	
	if(!initOk) return 0;

	// Get new events from SDL.
	I_PollEvents();
	
	// Get the events.
	for(i = 0; i < bufsize; i++)
	{
		e = I_GetKeyEvent();
		if(!e) break; // No more events.
		memcpy(&evbuf[i], e, sizeof(*e));
	}
	return i;
}

//===========================================================================
// I_GetMouseState
//===========================================================================
void I_GetMouseState(mousestate_t *state)
{
	Uint8 buttons;
	int i;
	
	memset(state, 0, sizeof(*state));

	// Has the mouse been initialized?
	if(!I_MousePresent() || !initOk) return;

	buttons = SDL_GetRelativeMouseState(&state->x, &state->y);

	if(buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) state->buttons |= IMB_LEFT;
	if(buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) state->buttons |= IMB_RIGHT;
	if(buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) state->buttons |= IMB_MIDDLE;
	
	// The buttons bitfield is ordered according to the numbering.
	for(i = 4; i < 8; i++)
	{
	    if(buttons & SDL_BUTTON(i + 1)) state->buttons |= 1 << i;
	}
}

//===========================================================================
// I_GetJoystickState
//===========================================================================
void I_GetJoystickState(joystate_t *state)
{
	memset(state, 0, sizeof(*state));

	// Initialization has not been done.
	if(!I_JoystickPresent() || !usejoystick || !initOk) return;

/*	state->axis[0] = INV(dijoy.lX, 0);
	state->axis[1] = INV(dijoy.lY, 1);
	state->axis[2] = INV(dijoy.lZ, 2);

	state->rotAxis[0] = INV(dijoy.lRx, 3);
	state->rotAxis[1] = INV(dijoy.lRy, 4);
	state->rotAxis[2] = INV(dijoy.lRz, 5);

	state->slider[0] = INV(dijoy.rglSlider[0], 6);
	state->slider[1] = INV(dijoy.rglSlider[1], 7);

	for(i = 0; i < IJOY_MAXBUTTONS; i++)
		state->buttons[i] = (dijoy.rgbButtons[i] & 0x80) != 0;
	pov = dijoy.rgdwPOV[0];
	if((pov & 0xffff) == 0xffff)
		state->povAngle = IJOY_POV_CENTER;
	else
	    state->povAngle = pov / 100.0f;*/
}
