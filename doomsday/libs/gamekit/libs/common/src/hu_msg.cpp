/** @file hu_msg.cpp  Important game state change messages.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "common.h"
#include <cstdlib>
#include <cstring>
#include <de/legacy/memory.h>
#include "hu_msg.h"
#include "hu_menu.h"
#include "hu_stuff.h"

using namespace de;
using namespace common;

static dd_bool awaitingResponse;
static int messageToPrint; // 1 = message to be printed.
static msgresponse_t messageResponse;

static msgtype_t msgType;
static msgfunc_t msgCallback;
static char *msgText;
static int msgUserValue;
static void *msgUserPointer;

static char yesNoMessage[160];

void Hu_MsgInit()
{
    awaitingResponse = false;
    messageToPrint  = 0; // 1 = message to be printed.
    messageResponse = MSG_CANCEL;

    msgCallback     = 0;
    msgText         = 0;
    msgUserValue    = 0;
    msgUserPointer  = 0;
}

void Hu_MsgShutdown()
{
    if(msgText)
    {
        M_Free(msgText); msgText = 0;
    }
}

static void stopMessage()
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
        M_Free(msgText); msgText = 0;
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
static void composeYesNoMessage()
{
    char *buf = yesNoMessage, tmp[2];

    buf[0] = 0;
    tmp[1] = 0;

    // Get the message template.
    const char *in = PRESSYN;

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

static void drawMessage()
{
#define LEADING             (0)

    short textFlags = Hu_MenuMergeEffectWithDrawTextFlags(0);
    Point2Raw origin = {{{SCREENWIDTH/2, SCREENHEIGHT/2}}};
    const char *questionString = "";

    switch(msgType)
    {
    case MSG_ANYKEY: questionString = PRESSKEY;     break;
    case MSG_YESNO:  questionString = yesNoMessage; break;

    default: DE_ASSERT_FAIL("drawMessage: Internal error, unknown message type.");
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetLeading(LEADING);
    FR_SetShadowStrength(cfg.common.menuTextGlitter);
    FR_SetGlitterStrength(cfg.common.menuShadow);
    FR_SetColorAndAlpha(cfg.common.menuTextColors[MENU_COLOR4][CR], cfg.common.menuTextColors[MENU_COLOR4][CG], cfg.common.menuTextColors[MENU_COLOR4][CB], 1);

    FR_DrawText3(msgText, &origin, ALIGN_TOP, textFlags);
    origin.y += FR_TextHeight(msgText);
    // An additional blank line between the message and response prompt.
    origin.y += FR_CharHeight('A') * (1+LEADING);

    FR_DrawText3(questionString, &origin, ALIGN_TOP, textFlags);
    DGL_Disable(DGL_TEXTURE_2D);

#undef LEADING
}

void Hu_MsgDrawer()
{
    if(!messageToPrint) return;

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, 0, SCREENWIDTH, SCREENHEIGHT,
          Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.common.menuScaleMode));
    GL_BeginBorderedProjection(&bp);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(cfg.common.menuScale, cfg.common.menuScale, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

    drawMessage();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    GL_EndBorderedProjection(&bp);
}

void Hu_MsgTicker()
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

int Hu_MsgResponder(event_t *ev)
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

dd_bool Hu_IsMessageActive()
{
    return messageToPrint;
}

dd_bool Hu_IsMessageActiveWithCallback(msgfunc_t callback)
{
    return messageToPrint && msgCallback == callback;
}

void Hu_MsgStart(msgtype_t type, const char *msg, msgfunc_t callback,
    int userValue, void *userPointer)
{
    DE_ASSERT(msg != 0);
    DE_ASSERT(!awaitingResponse);

    awaitingResponse = true;
    messageResponse  = msgresponse_t(0);
    messageToPrint   = 1;

    msgType        = type;
    msgCallback    = callback;
    msgUserValue   = userValue;
    msgUserPointer = userPointer;

    // Take a copy of the message string.
    msgText = (char *)M_Calloc(strlen(msg) + 1);
    strncpy(msgText, msg, strlen(msg));

    if(msgType == MSG_YESNO)
    {
        composeYesNoMessage();
    }

    if(!Get(DD_NOVIDEO))
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
    DE_UNUSED(src, argc);

    if(messageToPrint)
    {
        // Handle "Press any key to continue" messages.
        if(msgType == MSG_ANYKEY)
        {
            stopMessage();
            return true;
        }

        const char *cmd = argv[0] + 7;
        if(!iCmpStrCase(cmd, "yes"))
        {
            awaitingResponse = false;
            messageResponse = MSG_YES;
            return true;
        }
        if(!iCmpStrCase(cmd, "no"))
        {
            awaitingResponse = false;
            messageResponse = MSG_NO;
            return true;
        }
        if(!iCmpStrCase(cmd, "cancel"))
        {
            awaitingResponse = false;
            messageResponse = MSG_CANCEL;
            return true;
        }
    }

    return false;
}

void Hu_MsgRegister()
{
    C_CMD("messageyes",      "",     MsgResponse)
    C_CMD("messageno",       "",     MsgResponse)
    C_CMD("messagecancel",   "",     MsgResponse)
}
