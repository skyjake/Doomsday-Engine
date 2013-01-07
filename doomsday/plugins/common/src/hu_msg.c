/**
 * @file hu_msg.c
 * Important state change messages.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "hu_msg.h"
#include "hu_menu.h"
#include "hu_stuff.h"

D_CMD(MsgResponse);

static boolean awaitingResponse;
static int messageToPrint; // 1 = message to be printed.
static msgresponse_t messageResponse;

static msgtype_t msgType;
static msgfunc_t msgCallback;
static char* msgText;
static int msgUserValue;
static void* msgUserPointer;

static char yesNoMessage[160];

void Hu_MsgRegister(void)
{
    C_CMD("messageyes",      "",     MsgResponse)
    C_CMD("messageno",       "",     MsgResponse)
    C_CMD("messagecancel",   "",     MsgResponse)
}

void Hu_MsgInit(void)
{
    awaitingResponse = false;
    messageToPrint = 0; // 1 = message to be printed.
    messageResponse = MSG_CANCEL;

    msgCallback = NULL;
    msgText = NULL;
    msgUserValue = 0;
    msgUserPointer = NULL;
}

void Hu_MsgShutdown(void)
{
    if(msgText)
    {
        free(msgText);
        msgText = NULL;
    }
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
    {
        free(msgText);
        msgText = NULL;
    }

    S_LocalSound(SFX_ENDMESSAGE, NULL);

    // Disable the message binding context.
    DD_Executef(true, "deactivatebcontext message");

#undef SFX_ENDMESSAGE
}

/**
 * @todo: Query the bindings to determine the actual controls bound to the
 * message response commands.
 */
static void composeYesNoMessage(void)
{
    char* buf = yesNoMessage, *in, tmp[2];

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

void Hu_MsgDrawer(void)
{
    dgl_borderedprojectionstate_t bp;

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

void Hu_MsgTicker(void)
{
    // Check if there has been a response to a message.
    if(!messageToPrint || awaitingResponse)
        return;

    // We can now stop the message.
    stopMessage();

    if(msgType != MSG_ANYKEY && msgCallback)
    {
        msgCallback(messageResponse, msgUserValue, msgUserPointer);
    }
}

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

void Hu_MsgStart(msgtype_t type, const char* msg, msgfunc_t callback,
    int userValue, void* userPointer)
{
    DENG_ASSERT(msg);
    DENG_ASSERT(!awaitingResponse);

    awaitingResponse = true;
    messageResponse = 0;
    messageToPrint = 1;

    msgType = type;
    msgCallback = callback;
    msgUserValue = userValue;
    msgUserPointer = userPointer;

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
        const char* cmd;

        // Handle "Press any key to continue" messages.
        if(msgType == MSG_ANYKEY)
        {
            stopMessage();
            return true;
        }

        cmd = argv[0] + 7;
        if(!stricmp(cmd, "yes"))
        {
            awaitingResponse = false;
            messageResponse = MSG_YES;
            return true;
        }
        if(!stricmp(cmd, "no"))
        {
            awaitingResponse = false;
            messageResponse = MSG_NO;
            return true;
        }
        if(!stricmp(cmd, "cancel"))
        {
            awaitingResponse = false;
            messageResponse = MSG_CANCEL;
            return true;
        }
    }

    return false;
}
