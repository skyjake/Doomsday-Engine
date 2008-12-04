/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * hu_msg.c: Important state change messages.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_msg.h"
#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdMsgResponse);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ccmd_t msgCCmds[] = {
    {"messageyes",      "",     CCmdMsgResponse},
    {"messageno",       "",     CCmdMsgResponse},
    {"messagecancel",   "",     CCmdMsgResponse},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean awaitingResponse = false;
static int messageToPrint = 0; // 1 = message to be printed.
static msgresponse_t messageResponse = MSG_CANCEL;

static msgtype_t msgType;
static msgfunc_t msgCallback = NULL;
static const char* msgText = NULL;
static void* msgContext = NULL;

static char yesNoMessage[160];

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the important messages.
 */
void Hu_MsgRegister(void)
{
    int                 i;

    for(i = 0; msgCCmds[i].name; ++i)
        Con_AddCommand(msgCCmds + i);
}

static void stopMessage(void)
{
#if __JDOOM__ || __JDOOM64__
# define SFX_ENDMESSAGE     SFX_SWTCHX
#elif __JHERETIC__
# define SFX_ENDMESSAGE     SFX_CHAT
#elif __JHEXEN__
# define SFX_ENDMESSAGE     SFX_DOOR_LIGHT_CLOSE
#endif

    messageToPrint = 0;
    awaitingResponse = false;

    S_LocalSound(SFX_ENDMESSAGE, NULL);

    // Disable the message binding context.
    DD_Executef(true, "deactivatebcontext message");

#undef SFX_ENDMESSAGE
}

/**
 * \todo: Query the bindings to determine the actual controls bound to the
 * message response commands.
 */
static void composeYesNoMessage(void)
{
    char*               buf = yesNoMessage, *in, tmp[2];

    buf[0] = 0;
    tmp[1] = 0;

    // Get the message template.
    in = PRESSYN;

    for(; *in; in++)
    {
        if(in[0] == '%')
        {
            if(in[1] == '1')
            {
                strcat(buf, "Y");
                in++;
                continue;
            }
            if(in[1] == '2')
            {
                strcat(buf, "N");
                in++;
                continue;
            }

            if(in[1] == '%')
                in++;
        }
        tmp[0] = *in;
        strcat(buf, tmp);
    }
}

static void drawMessage(void)
{
#define BUFSIZE 80

    int                 x, y;
    uint                i, start;
    char                string[BUFSIZE];

    start = 0;

    y = 100 - M_StringHeight(msgText, huFontA) / 2;
    while(*(msgText + start))
    {
        for(i = 0; i < strlen(msgText + start); ++i)
            if(*(msgText + start + i) == '\n' || i > BUFSIZE-1)
            {
                memset(string, 0, BUFSIZE);
                strncpy(string, msgText + start, MIN_OF(i, BUFSIZE));
                string[BUFSIZE - 1] = 0;
                start += i + 1;
                break;
            }

        if(i == strlen(msgText + start))
        {
            strncpy(string, msgText + start, BUFSIZE);
            string[BUFSIZE - 1] = 0;
            start += i;
        }

        x = 160 - M_StringWidth(string, huFontA) / 2;
        M_WriteText2(x, y, string, huFontA, cfg.menuColor2[0],
                     cfg.menuColor2[1], cfg.menuColor2[2], 1);

        y += huFontA[17].height;
    }

    switch(msgType)
    {
    case MSG_ANYKEY:
        x = 160 - M_StringWidth(PRESSKEY, huFontA) / 2;
        y += huFontA[17].height;
        M_WriteText2(x, y, PRESSKEY, huFontA, cfg.menuColor2[0],
                     cfg.menuColor2[1], cfg.menuColor2[2], 1);
        break;

    case MSG_YESNO:
        x = 160 - M_StringWidth(yesNoMessage, huFontA) / 2;
        y += huFontA[17].height;
        M_WriteText2(x, y, yesNoMessage, huFontA, cfg.menuColor2[0],
                     cfg.menuColor2[1], cfg.menuColor2[2], 1);
        break;

    default:
        Con_Error("drawMessage: Internal error, unknown message type %i.\n",
                  (int) msgType);
    }

#undef BUFSIZE
}

/**
 * Draw any active message.
 */
void Hu_MsgDrawer(void)
{
    if(!messageToPrint)
        return;

    // Scale by the hudScale.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(160, 100, 0);

    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    DGL_Translatef(-160, -100, 0);

    // Draw the message.
    drawMessage();

    // Restore original matrices.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Updates on Game Tick.
 */
void Hu_MsgTicker(timespan_t time)
{
    // Check if there has been a response to a message.
    if(!messageToPrint || awaitingResponse)
        return;

    if(msgType != MSG_ANYKEY && msgCallback)
        msgCallback(messageResponse, msgContext);

    // We can now stop the message.
    stopMessage();
}

/**
 * If an "any key" message is active, respond to the event.
 */
boolean Hu_MsgResponder(event_t* ev)
{
    if(!messageToPrint || msgType != MSG_ANYKEY)
        return false;

    // We are only interested in key downs.
    if(ev->state == EVS_DOWN &&
       (ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON ||
        ev->type == EV_JOY_BUTTON))
    {
        stopMessage();
        return true;
    }

    return true;
}

boolean Hu_IsMessageActive(void)
{
    return messageToPrint;
}

/**
 * Begin a new message.
 */
void Hu_MsgStart(msgtype_t type, const char* msg, msgfunc_t callback,
                 void* context)
{
    awaitingResponse = true;
    messageResponse = 0;
    messageToPrint = 1;

    msgType = type;
    msgText = msg;
    msgCallback = callback;
    msgContext = context;

    if(msgType == MSG_YESNO)
        composeYesNoMessage();

    // If the console is open, close it. This message must be noticed!
    Con_Open(false);

    // Enable the message binding context.
    DD_Execute(true, "activatebcontext message");
}

/**
 * Handles responses to messages requiring input.
 */
DEFCC(CCmdMsgResponse)
{
    if(messageToPrint)
    {
        // Handle "Press any key to continue" messages.
        if(messageToPrint && msgType == MSG_ANYKEY)
        {
            stopMessage();
            return true;
        }

        if(!stricmp(argv[0], "messageyes"))
        {
            awaitingResponse = false;
            messageResponse = MSG_YES;
            return true;
        }
        else if(!stricmp(argv[0], "messageno"))
        {
            awaitingResponse = false;
            messageResponse = MSG_NO;
            return true;
        }
        else if(!stricmp(argv[0], "messagecancel"))
        {
            awaitingResponse = false;
            messageResponse = MSG_CANCEL;
            return true;
        }
    }

    return false;
}
