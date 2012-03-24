/**\file hu_msg.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Important state change messages.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <stdlib.h>
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
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(MsgResponse);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ccmdtemplate_t msgCCmds[] = {
    {"messageyes",      "",     CCmdMsgResponse},
    {"messageno",       "",     CCmdMsgResponse},
    {"messagecancel",   "",     CCmdMsgResponse},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean awaitingResponse;
static int messageToPrint; // 1 = message to be printed.
static msgresponse_t messageResponse;

static msgtype_t msgType;
static msgfunc_t msgCallback;
static char* msgText;
static void* msgContext;

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

/**
 * Called during init.
 */
void Hu_MsgInit(void)
{
    awaitingResponse = false;
    messageToPrint = 0; // 1 = message to be printed.
    messageResponse = MSG_CANCEL;

    msgCallback = NULL;
    msgText = NULL;
    msgContext = NULL;
}

/**
 * Called during engine shutdown.
 */
void Hu_MsgShutdown(void)
{
    if(msgText)
        free(msgText);
    msgText = NULL;
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

    if(msgText)
        free(msgText);
    msgText = NULL;

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
#define LEADING             (0)

    short textFlags = MN_MergeMenuEffectWithDrawTextFlags(0);
    Point2Raw origin = { SCREENWIDTH/2, SCREENHEIGHT/2 };
    const char* questionString;

    switch(msgType)
    {
    case MSG_ANYKEY: questionString = PRESSKEY;     break;
    case MSG_YESNO:  questionString = yesNoMessage; break;
    default:
        Con_Error("drawMessage: Internal error, unknown message type %i.\n", (int) msgType);
        exit(1); // Unreachable.
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetLeading(LEADING);
    FR_SetShadowStrength(cfg.menuTextGlitter);
    FR_SetGlitterStrength(cfg.menuShadow);
    FR_SetColorAndAlpha(cfg.menuTextColors[MENU_COLOR4][CR], cfg.menuTextColors[MENU_COLOR4][CG], cfg.menuTextColors[MENU_COLOR4][CB], 1);

    FR_DrawText3(msgText, &origin, ALIGN_TOP, textFlags);
    origin.y += FR_TextHeight(msgText);
    // An additional blank line between the message and response prompt.
    origin.y += FR_CharHeight('A') * (1+LEADING);

    FR_DrawText3(questionString, &origin, ALIGN_TOP, textFlags);
    DGL_Disable(DGL_TEXTURE_2D);

#undef LEADING
}

/**
 * Draw any active message.
 */
void Hu_MsgDrawer(void)
{
    borderedprojectionstate_t bp;

    if(!messageToPrint) return;

    GL_ConfigureBorderedProjection(&bp, 0, SCREENWIDTH, SCREENHEIGHT,
          Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), cfg.menuScaleMode);
    GL_BeginBorderedProjection(&bp);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(cfg.menuScale, cfg.menuScale, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

    drawMessage();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    GL_EndBorderedProjection(&bp);
}

/**
 * Updates on Game Tick.
 */
void Hu_MsgTicker(void)
{
    // Check if there has been a response to a message.
    if(!messageToPrint || awaitingResponse)
        return;

    // We can now stop the message.
    stopMessage();

    if(msgType != MSG_ANYKEY && msgCallback)
        msgCallback(messageResponse, msgContext);
}

/**
 * If an "any key" message is active, respond to the event.
 */
int Hu_MsgResponder(event_t* ev)
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

boolean Hu_IsMessageActiveWithCallback(msgfunc_t callback)
{
    return messageToPrint && msgCallback == callback;
}

/**
 * Begin a new message.
 */
void Hu_MsgStart(msgtype_t type, const char* msg, msgfunc_t callback, void* context)
{
    assert(msg);

    awaitingResponse = true;
    messageResponse = 0;
    messageToPrint = 1;

    msgType = type;
    msgCallback = callback;
    msgContext = context;

    // Take a copy of the message string.
    msgText = calloc(1, strlen(msg)+1);
    strncpy(msgText, msg, strlen(msg));

    if(msgType == MSG_YESNO)
        composeYesNoMessage();

    if(!(Get(DD_DEDICATED) || Get(DD_NOVIDEO)))
    {
        FR_ResetTypeinTimer();
    }

    // If the console is open, close it. This message must be noticed!
    Con_Open(false);

    // Enable the message binding context.
    DD_Execute(true, "activatebcontext message");
}

/**
 * Handles responses to messages requiring input.
 */
D_CMD(MsgResponse)
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
