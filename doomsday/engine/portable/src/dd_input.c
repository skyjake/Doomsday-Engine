/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * dd_input.c: System Independent Input
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_ui.h"

#include "gl_main.h"

// MACROS ------------------------------------------------------------------

#define MAX_MOUSE_FILTER 40

#define KBDQUESIZE      32
#define MAX_DOWNKEYS    16      // Most keyboards support 6 or 7.
#define NUMKKEYS        256

#define CLAMP(x) DD_JoyAxisClamp(&x)    //x = (x < -100? -100 : x > 100? 100 : x)

// TYPES -------------------------------------------------------------------

typedef struct repeater_s {
    int     key;                // The H2 key code (0 if not in use).
    timespan_t timer;           // How's the time?
    int     count;              // How many times has been repeated?
} repeater_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     mouseFilter = 1;        // Filtering on by default.
int     mouseInverseY = false;
int     mouseWheelSensi = 10;   // I'm shooting in the dark here.
unsigned int  mouseFreq = 0;
int     joySensitivity = 5;
int     joyDeadZone = 10;

boolean allowMouseMod = true; // can mouse data be modified?

// The initial and secondary repeater delays (tics).
int     repWait1 = 15, repWait2 = 3;
int     keyRepeatDelay1 = 430, keyRepeatDelay2 = 85;    // milliseconds
int     mouseDisableX = false, mouseDisableY = false;
boolean shiftDown = false, altDown = false;
byte    showScanCodes = false;

// A customizable mapping of the scantokey array.
char    keyMapPath[NUMKKEYS] = "}Data\\KeyMaps\\";
byte    keyMappings[NUMKKEYS];
byte    shiftKeyMappings[NUMKKEYS], altKeyMappings[NUMKKEYS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte showMouseInfo = false;

static event_t events[MAXEVENTS];
static int eventhead;
static int eventtail;

static byte downKeys[NUMKKEYS];
static byte downMouseButtons[IMB_MAXBUTTONS];
static byte downJoyButtons[IJOY_MAXBUTTONS];

/* *INDENT-OFF* */
static byte scantokey[NUMKKEYS] =
{
//  0               1           2               3               4           5                   6               7
//  8               9           A               B               C           D                   E               F
// 0
    0  ,            27,         '1',            '2',            '3',        '4',                '5',            '6',
    '7',            '8',        '9',            '0',            '-',        '=',                DDKEY_BACKSPACE,9,          // 0
// 1
    'q',            'w',        'e',            'r',            't',        'y',                'u',            'i',
    'o',            'p',        '[',            ']',            13 ,        DDKEY_RCTRL,        'a',            's',        // 1
// 2
    'd',            'f',        'g',            'h',            'j',        'k',                'l',            ';',
    39 ,            '`',        DDKEY_RSHIFT,   92,             'z',        'x',                'c',            'v',        // 2
// 3
    'b',            'n',        'm',            ',',            '.',        '/',                DDKEY_RSHIFT,   '*',
    DDKEY_RALT,     ' ',        0  ,            DDKEY_F1,       DDKEY_F2,   DDKEY_F3,           DDKEY_F4,       DDKEY_F5,   // 3
// 4
    DDKEY_F6,       DDKEY_F7,   DDKEY_F8,       DDKEY_F9,       DDKEY_F10,  DDKEY_NUMLOCK,      DDKEY_SCROLL,   DDKEY_NUMPAD7,
    DDKEY_NUMPAD8,  DDKEY_NUMPAD9, '-',         DDKEY_NUMPAD4,  DDKEY_NUMPAD5, DDKEY_NUMPAD6,   '+',            DDKEY_NUMPAD1, // 4
// 5
    DDKEY_NUMPAD2,  DDKEY_NUMPAD3, DDKEY_NUMPAD0, DDKEY_DECIMAL,0,          0,                  0,              DDKEY_F11,
    DDKEY_F12,      0  ,        0  ,            0  ,            DDKEY_BACKSLASH, 0,             0  ,            0,          // 5
// 6
    0  ,            0  ,        0  ,            0  ,            0  ,        0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,          // 6
// 7
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // 7
// 8
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0,              0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // 8
// 9
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              DDKEY_ENTER, DDKEY_RCTRL,       0  ,            0,          // 9
// A
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // A
// B
    0  ,            0  ,        0  ,            0  ,            0,          '/',                0  ,            0,
    DDKEY_RALT,     0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // B
// C
    0  ,            0  ,        0  ,            0  ,            0,          DDKEY_PAUSE,        0  ,            DDKEY_HOME,
    DDKEY_UPARROW,  DDKEY_PGUP, 0  ,            DDKEY_LEFTARROW,0  ,        DDKEY_RIGHTARROW,   0  ,            DDKEY_END,  // C
// D
    DDKEY_DOWNARROW,DDKEY_PGDN, DDKEY_INS,      DDKEY_DEL,      0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // D
// E
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // E
// F
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0           // F
//  0               1           2               3               4           5                   6               7
//  8               9           A               B               C           D                   E               F
};

static char defaultShiftTable[96] = // Contains characters 32 to 127.
{
/* 32 */    ' ', 0, 0, 0, 0, 0, 0, '"',
/* 40 */    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */    0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
/* 70 */    'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 80 */    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
/* 90 */    'z', '{', '|', '}', 0, 0, 0, 'A', 'B', 'C',
/* 100 */   'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
/* 110 */   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
/* 120 */   'X', 'Y', 'Z', 0, 0, 0, 0, 0
};
/* *INDENT-ON* */

static repeater_t keyReps[MAX_DOWNKEYS];
static int oldMouseButtons = 0;
static int oldJoyBState = 0;
static float oldPOV = IJOY_POV_CENTER;

// CODE --------------------------------------------------------------------

void DD_RegisterInput(void)
{
    C_VAR_INT("input-key-delay1", &keyRepeatDelay1, CVF_NO_MAX, 50, 0);

    C_VAR_INT("input-key-delay2", &keyRepeatDelay2, CVF_NO_MAX, 20, 0);

    C_VAR_BYTE("input-key-show-scancodes", &showScanCodes, 0, 0, 1);

    C_VAR_INT("input-joy-sensi", &joySensitivity, 0, 0, 9);

    C_VAR_INT("input-joy-deadzone", &joyDeadZone, 0, 0, 90);

    C_VAR_INT("input-mouse-wheel-sensi", &mouseWheelSensi, CVF_NO_MAX, 0, 0);

    C_VAR_INT("input-mouse-x-disable", &mouseDisableX, 0, 0, 1);

    C_VAR_INT("input-mouse-y-disable", &mouseDisableY, 0, 0, 1);

    C_VAR_INT("input-mouse-y-inverse", &mouseInverseY, 0, 0, 1);

    C_VAR_INT("input-mouse-filter", &mouseFilter, 0, 0, MAX_MOUSE_FILTER - 1);

    C_VAR_INT("input-mouse-frequency", &mouseFreq, CVF_NO_MAX, 0, 0);
    
    C_VAR_BYTE("input-info-mouse", &showMouseInfo, 0, 0, 1);
}

/*
 * Dumps the key mapping table to filename
 */
void DD_DumpKeyMappings(char *fileName)
{
    FILE   *file;
    int     i;

    file = fopen(fileName, "wt");
    for(i = 0; i < 256; i++)
    {
        fprintf(file, "%03i\t", i);
        fprintf(file, !isspace(keyMappings[i]) &&
                isprint(keyMappings[i]) ? "%c\n" : "%03i\n", keyMappings[i]);
    }

    fprintf(file, "\n+Shift\n");
    for(i = 0; i < 256; i++)
    {
        if(shiftKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(shiftKeyMappings[i]) &&
                isprint(shiftKeyMappings[i]) ? "%c\n" : "%03i\n",
                shiftKeyMappings[i]);
    }

    fprintf(file, "-Shift\n\n+Alt\n");
    for(i = 0; i < 256; i++)
    {
        if(altKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(altKeyMappings[i]) &&
                isprint(altKeyMappings[i]) ? "%c\n" : "%03i\n",
                altKeyMappings[i]);
    }
    fclose(file);
}

/*
 * Sets the key mappings to the default values
 */
void DD_DefaultKeyMapping(void)
{
    int     i;

    for(i = 0; i < 256; i++)
    {
        keyMappings[i] = scantokey[i];
        shiftKeyMappings[i] = i >= 32 && i <= 127 &&
            defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        altKeyMappings[i] = i;
    }
}

/*
 * Initializes the key mappings to the default values.
 */
void DD_InitInput(void)
{
    DD_DefaultKeyMapping();
}

/*
 * Returns either the key number or the scan code for the given token
 */
int DD_KeyOrCode(char *token)
{
    char   *end = M_FindWhite(token);

    if(end - token > 1)
    {
        // Longer than one character, it must be a number.
        return strtol(token, 0, !strnicmp(token, "0x", 2) ? 16 : 10);
    }
    // Direct mapping.
    return (unsigned char) *token;
}

/*
 * Console command to write the current keymap to a file
 */
D_CMD(DumpKeyMap)
{
    if(argc != 2)
    {
        Con_Printf("Usage: %s (file)\n", argv[0]);
        return true;
    }
    DD_DumpKeyMappings(argv[1]);
    Con_Printf("The current keymap was dumped to %s.\n", argv[1]);
    return true;
}

/*
 * Console command to load a keymap file.
 */
D_CMD(KeyMap)
{
    DFILE  *file;
    char    line[512], *ptr;
    boolean shiftMode = false, altMode = false;
    int     key, mapTo, lineNumber = 0;

    if(argc != 2)
    {
        Con_Printf("Usage: %s (dkm-file)\n", argv[0]);
        return true;
    }
    // Try with and without .DKM.
    strcpy(line, argv[1]);
    if(!F_Access(line))
    {
        // Try the path.
        M_TranslatePath(keyMapPath, line);
        strcat(line, argv[1]);
        if(!F_Access(line))
        {
            strcpy(line, argv[1]);
            strcat(line, ".dkm");
            if(!F_Access(line))
            {
                M_TranslatePath(keyMapPath, line);
                strcat(line, argv[1]);
                strcat(line, ".dkm");
            }
        }
    }
    if(!(file = F_Open(line, "rt")))
    {
        Con_Printf("%s: file not found.\n", argv[1]);
        return false;
    }
    // Any missing entries are set to the default.
    DD_DefaultKeyMapping();
    do
    {
        lineNumber++;
        M_ReadLine(line, sizeof(line), file);
        ptr = M_SkipWhite(line);
        if(!*ptr || M_IsComment(ptr))
            continue;
        // Modifiers? Only shift is supported at the moment.
        if(!strnicmp(ptr + 1, "shift", 5))
        {
            shiftMode = (*ptr == '+');
            continue;
        }
        else if(!strnicmp(ptr + 1, "alt", 3))
        {
            altMode = (*ptr == '+');
            continue;
        }
        key = DD_KeyOrCode(ptr);
        if(key < 0 || key > 255)
        {
            Con_Printf("%s(%i): Invalid key %i.\n", argv[1], lineNumber, key);
            continue;
        }
        ptr = M_SkipWhite(M_FindWhite(ptr));
        mapTo = DD_KeyOrCode(ptr);
        // Check the mapping.
        if(mapTo < 0 || mapTo > 255)
        {
            Con_Printf("%s(%i): Invalid mapping %i.\n", argv[1], lineNumber,
                       mapTo);
            continue;
        }
        if(shiftMode)
            shiftKeyMappings[key] = mapTo;
        else if(altMode)
            altKeyMappings[key] = mapTo;
        else
            keyMappings[key] = mapTo;
    }
    while(!deof(file));
    F_Close(file);
    Con_Printf("Keymap %s loaded.\n", argv[1]);
    return true;
}

/*
 * Clear the input event queue.
 */
void DD_ClearEvents(void)
{
    eventhead = eventtail;
}

/*
 * Called by the I/O functions when input is detected.
 */
void DD_PostEvent(event_t *ev)
{
    events[eventhead++] = *ev;
    eventhead &= MAXEVENTS - 1;
}

/*
 * Get the next event from the input event queue.  Returns NULL if no
 * more events are available.
 */
static event_t *DD_GetEvent(void)
{
    event_t *ev;

    if(eventhead == eventtail)
        return NULL;

    ev = &events[eventtail];
    eventtail = (eventtail + 1) & (MAXEVENTS - 1);

    return ev;
}

/*
 * Send all the events of the given timestamp down the responder
 * chain.  This gets called at least 35 times per second.  Usually
 * more frequently than that.
 */
void DD_ProcessEvents(timespan_t ticLength)
{
    event_t *ev;

    DD_ReadMouse(ticLength);
    DD_ReadJoystick();
    DD_ReadKeyboard();

    while((ev = DD_GetEvent()) != NULL)
    {
        // Track the state of Shift and Alt.
        if(ev->data1 == DDKEY_RSHIFT && ev->type == EV_KEY)
        {
            if(ev->state == EVS_DOWN)
                shiftDown = true;
            else if(ev->state == EVS_UP)
                shiftDown = false;
        }
        if(ev->data1 == DDKEY_RALT && ev->type == EV_KEY)
        {
            if(ev->state == EVS_DOWN)
                altDown = true;
            else if(ev->state == EVS_UP)
                altDown = false;
        }

        // Does the special responder use this event?
        if(gx.PrivilegedResponder)
            if(gx.PrivilegedResponder(ev))
                continue;

        if(UI_Responder(ev))
            continue;
        // The console.
        if(Con_Responder(ev))
            continue;

        // The game responder only returns true if the bindings
        // can't be used (like when chatting).
        if(gx.G_Responder(ev))
            continue;

        // The bindings responder.
        if(B_Responder(ev))
            continue;

        // The "fallback" responder. Gets the event if no one else is
        // interested.
        if(gx.FallbackResponder)
            gx.FallbackResponder(ev);
    }
}

/*
 * Converts as a scan code to the keymap key id.
 */
byte DD_ScanToKey(byte scan)
{
    return keyMappings[scan];
}

/*
 * Apply all active modifiers to the key.
 */
byte DD_ModKey(byte key)
{
    if(shiftDown)
        key = shiftKeyMappings[key];
    if(altDown)
        key = altKeyMappings[key];
    if(key >= DDKEY_NUMPAD7 && key <= DDKEY_NUMPAD0)
    {
        byte numPadKeys[10] = {
            '7', '8', '9', '4', '5', '6', '1', '2', '3', '0'
        };
        return numPadKeys[key - DDKEY_NUMPAD7];
    }
    return key;
}

/*
 * Converts a keymap key id to a scan code.
 */
byte DD_KeyToScan(byte key)
{
    int     i;

    for(i = 0; i < NUMKKEYS; i++)
        if(keyMappings[i] == key)
            return i;
    return 0;
}

/*
 * Clears the repeaters array.
 */
void DD_ClearKeyRepeaters(void)
{
    memset(keyReps, 0, sizeof(keyReps));
}

/*
 * Checks the current keyboard state, generates input events
 * based on pressed/held keys and posts them.
 */
void DD_ReadKeyboard(void)
{
    event_t ev;
    keyevent_t keyevs[KBDQUESIZE];
    int     i, k, numkeyevs;

    if(isDedicated)
    {
        // In dedicated mode, all input events come from the console.
        Sys_ConPostEvents();
        return;
    }

    // Check the repeaters.
    ev.type  = EV_KEY;
    ev.state = EVS_REPEAT;

    // Don't specify a class
    ev.useclass = -1;
    for(i = 0; i < MAX_DOWNKEYS; i++)
    {
        repeater_t *rep = keyReps + i;

        if(!rep->key)
            continue;
        ev.data1 = rep->key;

        if(!rep->count && sysTime - rep->timer >= keyRepeatDelay1 / 1000.0)
        {
            // The first time.
            rep->count++;
            rep->timer += keyRepeatDelay1 / 1000.0;
            DD_PostEvent(&ev);
        }
        if(rep->count)
        {
            while(sysTime - rep->timer >= keyRepeatDelay2 / 1000.0)
            {
                rep->count++;
                rep->timer += keyRepeatDelay2 / 1000.0;
                DD_PostEvent(&ev);
            }
        }
    }

    // Read the keyboard events.
    numkeyevs = I_GetKeyEvents(keyevs, KBDQUESIZE);

    // Translate them to Doomsday keys.
    for(i = 0; i < numkeyevs; i++)
    {
        keyevent_t *ke = keyevs + i;

        // Use the table to translate the scancode to a ddkey.
#ifdef WIN32
        ev.data1 = DD_ScanToKey(ke->code);
#endif
#ifdef UNIX
        ev.data1 = ke->code;
#endif

        // Check the type of the event.
        if(ke->event == IKE_KEY_DOWN)   // Key pressed?
        {
            ev.state = EVS_DOWN;
            downKeys[ev.data1] = true;
        }
        else if(ke->event == IKE_KEY_UP) // Key released?
        {
            ev.state = EVS_UP;
            downKeys[ev.data1] = false;
        }

        // Should we print a message in the console?
        if(showScanCodes && ev.type == EVS_DOWN)
            Con_Printf("Scancode: %i (0x%x)\n", ev.data1, ev.data1);

        // Maintain the repeater table.
        if(ev.state == EVS_DOWN)
        {
            // Find an empty repeater.
            for(k = 0; k < MAX_DOWNKEYS; k++)
                if(!keyReps[k].key)
                {
                    keyReps[k].key = ev.data1;
                    keyReps[k].timer = sysTime;
                    keyReps[k].count = 0;
                    break;
                }
        }
        else if(ev.state == EVS_UP)
        {
            // Clear any repeaters with this key.
            for(k = 0; k < MAX_DOWNKEYS; k++)
                if(keyReps[k].key == ev.data1)
                    keyReps[k].key = 0;
        }

        // Post the event.
        DD_PostEvent(&ev);
    }
}

/*
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void DD_ReadMouse(timespan_t ticLength)
{
    static int mickeys[2] = { 0, 0 }; // For filtering.

    event_t ev;
    mousestate_t mouse;
    int     change;

    if(!I_MousePresent())
        return;

    // Should we test the mouse input frequency?
    if(mouseFreq > 0)
    {
        static uint lastTime = 0;
        uint nowTime = Sys_GetRealTime();

        if(nowTime - lastTime < 1000/mouseFreq)
        {
            // Don't ask yet.
            memset(&mouse, 0, sizeof(mouse));
        }
        else
        {
            lastTime = nowTime;
            I_GetMouseState(&mouse);
        }
    }
    else
    {
        // Get the mouse state.
        I_GetMouseState(&mouse);
    }

    ev.type = EV_MOUSE_AXIS;
    ev.state = 0;
    ev.data1 = mouse.x * DD_MICKEY_ACCURACY;
    ev.data2 = mouse.y * DD_MICKEY_ACCURACY;
    ev.data3 = mouse.z * DD_MICKEY_ACCURACY;
    // Don't specify a class
    ev.useclass = -1;

    // Mouse axis data may be modified if not in UI mode.
    if(allowMouseMod)
    {
        if(mouseDisableX)
            ev.data1 = 0;
        if(mouseDisableY)
            ev.data2 = 0;
        if(!mouseInverseY)
            ev.data2 = -ev.data2;

        // Filtering ensures that mickeys are sent more evenly on each frame.
        if(mouseFilter > 0)
        {
            int i;
            float target[2];
            int dirs[2];
            int avail[2];
            int used[2];

            mickeys[0] += ev.data1;
            mickeys[1] += ev.data2;
            
            dirs[0] = (mickeys[0] > 0? 1 : mickeys[0] < 0? -1 : 0);
            dirs[1] = (mickeys[1] > 0? 1 : mickeys[1] < 0? -1 : 0);
            avail[0] = abs(mickeys[0]);
            avail[1] = abs(mickeys[1]);                

            // Determine the target mouse velocity.
            target[0] = avail[0] * (MAX_MOUSE_FILTER - mouseFilter);
            target[1] = avail[1] * (MAX_MOUSE_FILTER - mouseFilter);
                        
            for(i = 0; i < 2; ++i)
            {
                // Determine the amount of mickeys to send. It depends on the
                // current mouse velocity, and how much time has passed.
                used[i] = target[i] * ticLength;

                // Don't go over the available number of mickeys.
                if(used[i] > avail[i])
                {
                    mickeys[i] = 0;
                    used[i] = avail[i];
                }
                else
                {
                    // Reduce the number of available mickeys.
                    if(mickeys[i] > 0)
                        mickeys[i] -= used[i];
                    else
                        mickeys[i] += used[i];
                }
            }            

            if(showMouseInfo)
            {
                Con_Message("mouse velocity = %11.1f, %11.1f\n", 
                            target[0], target[1]);
            }

            // The actual mickeys sent in the event.
            ev.data1 = dirs[0] * used[0];
            ev.data2 = dirs[1] * used[1];
        }
        else
        {
            mickeys[0] = mickeys[1] = 0;
        }
    }
    else                        // In UI mode.
    {
        // Scale the movement depending on screen resolution.
        ev.data1 *= MAX_OF(1, glScreenWidth / 800.0f);
        ev.data2 *= MAX_OF(1, glScreenHeight / 600.0f);
    }

    // Don't post empty events.
    if(ev.data1 || ev.data2 || ev.data3)
        DD_PostEvent(&ev);

    // Insert the possible mouse Z axis into the button flags.
    if(abs(ev.data3) >= mouseWheelSensi)
    {
        mouse.buttons |= ev.data3 > 0 ? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
    }

    // Check the buttons and send the appropriate events.
    change = oldMouseButtons ^ mouse.buttons;   // The change mask.
    // Send the relevant events.
    if((ev.data1 = mouse.buttons & change))
    {
        ev.type = EV_MOUSE_BUTTON;
        ev.state = EVS_DOWN;
        downMouseButtons[ev.data1] = true;
        DD_PostEvent(&ev);
    }
    if((ev.data1 = oldMouseButtons & change))
    {
        ev.type = EV_MOUSE_BUTTON;
        ev.state = EVS_UP;
        downMouseButtons[ev.data1] = false;
        DD_PostEvent(&ev);
    }
    oldMouseButtons = mouse.buttons;
}

/*
 * Applies the dead zone and clamps the value to -100...100.
 */
void DD_JoyAxisClamp(int *val)
{
    if(abs(*val) < joyDeadZone)
    {
        // In the dead zone, just go to zero.
        *val = 0;
        return;
    }
    // Remove the dead zone.
    *val += *val > 0 ? -joyDeadZone : joyDeadZone;
    // Normalize.
    *val *= 100.0f / (100 - joyDeadZone);
    // Clamp.
    if(*val > 100)
        *val = 100;
    if(*val < -100)
        *val = -100;
}

/*
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void DD_ReadJoystick(void)
{
    event_t ev;
    joystate_t state;
    int     i, bstate;
    int     div = 100 - joySensitivity * 10;

    if(!I_JoystickPresent())
        return;

    I_GetJoystickState(&state);

    bstate = 0;
    // Check the buttons.
    for(i = 0; i < IJOY_MAXBUTTONS; i++)
        if(state.buttons[i])
            bstate |= 1 << i;   // Set the bits.

    // Don't specify a class
    ev.useclass = -1;
    ev.state = 0;

    // Check for button state changes.
    i = oldJoyBState ^ bstate;  // The change mask.
    // Send the relevant events.
    if((ev.data1 = bstate & i))
    {
        ev.type = EV_JOY_BUTTON;
        ev.state = EVS_DOWN;
        downJoyButtons[ev.data1] = true;
        DD_PostEvent(&ev);
    }
    if((ev.data1 = oldJoyBState & i))
    {
        ev.type = EV_JOY_BUTTON;
        ev.state = EVS_UP;
        downJoyButtons[ev.data1] = false;
        DD_PostEvent(&ev);
    }
    oldJoyBState = bstate;

    // Check for a POV change.
    if(state.povAngle != oldPOV)
    {
        if(oldPOV != IJOY_POV_CENTER)
        {
            // Send a notification that the existing POV angle is no
            // longer active.
            ev.type = EV_POV;
            ev.state = EVS_UP;
            ev.data1 = (int) (oldPOV / 45 + .5);    // Round off correctly w/.5.
            DD_PostEvent(&ev);
        }
        if(state.povAngle != IJOY_POV_CENTER)
        {
            // The new angle becomes active.
            ev.type = EV_POV;
            ev.state = EVS_DOWN;
            ev.data1 = (int) (state.povAngle / 45 + .5);
            DD_PostEvent(&ev);
        }
        oldPOV = state.povAngle;
    }

    // Send the joystick movement event (XYZ and rotation-XYZ).
    ev.type = EV_JOY_AXIS;

    // The input code returns the axis positions in the range -10000..10000.
    // The output axis data must be in range -100..100.
    // Increased sensitivity causes the axes to max out earlier.
    // Check that the divisor is valid.
    if(div < 10)
        div = 10;
    if(div > 100)
        div = 100;

    ev.data1 = state.axis[0] / div;
    ev.data2 = state.axis[1] / div;
    ev.data3 = state.axis[2] / div;
    ev.data4 = state.rotAxis[0] / div;
    ev.data5 = state.rotAxis[1] / div;
    ev.data6 = state.rotAxis[2] / div;
    CLAMP(ev.data1);
    CLAMP(ev.data2);
    CLAMP(ev.data3);
    CLAMP(ev.data4);
    CLAMP(ev.data5);
    CLAMP(ev.data6);

    DD_PostEvent(&ev);

    // The sliders.
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_JOY_SLIDER;

    ev.data1 = state.slider[0] / div;
    ev.data2 = state.slider[1] / div;
    CLAMP(ev.data1);
    CLAMP(ev.data2);

    DD_PostEvent(&ev);
}

/*
 *  DD_IsKeyDown
 *      Simply return the key state from the downKeys array
 */
int DD_IsKeyDown(int code)
{
    return downKeys[code];
}

/*
 *  DD_IsMouseBDown
 *      Simply return the key state from the downMouseButtons array
 */
int DD_IsMouseBDown(int code)
{
    return downMouseButtons[code];
}

/*
 *  DD_IsJoyBDown
 *      Simply return the key state from the downJoyButtons array
 */
int DD_IsJoyBDown(int code)
{
    return downJoyButtons[code];
}
