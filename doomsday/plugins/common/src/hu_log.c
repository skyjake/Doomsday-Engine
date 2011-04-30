/**\file hu_log.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_log.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused()
#include "d_net.h"

/**
 * @defgroup messageFlags  Message Flags.
 * @{
 */
#define MF_JUSTADDED        0x1 
#define MF_NOHIDE           0x2 /// Message cannot be hidden.
/**@}*/

typedef struct logmsg_s {
    uint ticsRemain, tics;
    int flags;
    int textMaxLen;
    char* text;
} message_t;

typedef struct msglog_s {
    /// Log message list.
    message_t msgs[LOG_MAX_MESSAGES];

    /// Number of used messages.
    int msgCount;

    /// Number of potentially visible messages.
    int pvisMsgCount;

    /// Index of the next slot to be used in msgs.
    int nextUsedMsg;
} log_t;

D_CMD(LocalMessage);

void Hu_LogPostVisibilityChangeNotification(void);

static log_t msgLogs[MAXPLAYERS];

cvartemplate_t msgLogCVars[] = {
    // Behavior
    { "msg-count",  0, CVT_INT, &cfg.msgCount, 1, 8 },
    { "msg-uptime", 0, CVT_FLOAT, &cfg.msgUptime, 1, 60 },

    // Display
    { "msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2 },
    { "msg-blink", CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0 },
    { "msg-scale", 0, CVT_FLOAT, &cfg.msgScale, 0.1f, 1 },
    { "msg-show",  0, CVT_BYTE, &cfg.hudShown[HUD_LOG], 0, 1, Hu_LogPostVisibilityChangeNotification },

    // Colour defaults
    { "msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[CR], 0, 1 },
    { "msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[CG], 0, 1 },
    { "msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[CB], 0, 1 },
    { NULL }
};

ccmdtemplate_t msgLogCCmds[] = {
    { "message", "s", CCmdLocalMessage },
    { NULL }
};

void Hu_LogRegister(void)
{
    int i;
    for(i = 0; msgLogCVars[i].name; ++i)
        Con_AddVariable(msgLogCVars + i);
    for(i = 0; msgLogCCmds[i].name; ++i)
        Con_AddCommand(msgLogCCmds + i);
}

static void logInit(log_t* log)
{
    assert(NULL != log);
    log->msgCount = 0;
    log->nextUsedMsg = 0;
    log->pvisMsgCount = 0;
    memset(log->msgs, 0, sizeof(log->msgs));
}

/// @return  Index of the first (i.e., earliest) message that is potentially visible.
static __inline int logFirstPVisMessageIdx(log_t* log)
{
    assert(NULL != log);
    if(0 != log->pvisMsgCount)
    {
        int n = log->nextUsedMsg - MIN_OF(log->pvisMsgCount, MAX_OF(0, cfg.msgCount));
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the first (i.e., earliest) message.
static __inline int logFirstMessageIdx(log_t* log)
{
    assert(NULL != log);
    if(0 != log->pvisMsgCount)
    {
        int n = log->nextUsedMsg - log->pvisMsgCount;
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the next (possibly already used) message.
static __inline int logNextMessageIdx(log_t* log, int current)
{
    assert(NULL != log);
    if(current < LOG_MAX_MESSAGES - 1)
        return current + 1;
    return 0; // Wrap around.
}

/// @return  Local player number of the owner of this log.
static int logLocalPlayer(log_t* log)
{
    return (int) (log - msgLogs);
}

/**
 * Push a new message into the log.
 *
 * @param flags  @see messageFlags
 * @param str  Message to be added.
 * @param tics  Length of time the message should be visible.
 */
static message_t* logPush(log_t* log, int flags, const char* str, int tics)
{
    assert(NULL != log && NULL != str);
    {
    message_t* msg;
    int len = (int)strlen(str);
    if(0 == len)
    {
        Con_Error("Log::Push: Attempted to log zero-length message.");
        exit(1); // Unreachable.
    }

    msg = &log->msgs[log->nextUsedMsg];
    log->nextUsedMsg = logNextMessageIdx(log, log->nextUsedMsg);
    if(len >= msg->textMaxLen)
    {
        msg->textMaxLen = len+1;
        msg->text = (char*) realloc(msg->text, msg->textMaxLen);
        if(NULL == msg->text)
            Con_Error("Log::Push: Failed on (re)allocation of %lu bytes for log message.",
                (unsigned long) msg->textMaxLen);
    }

    if(log->msgCount < LOG_MAX_MESSAGES)
        ++log->msgCount;
    if(log->pvisMsgCount < LOG_MAX_MESSAGES)
        ++log->pvisMsgCount;

    dd_snprintf(msg->text, msg->textMaxLen, "%s", str);
    msg->ticsRemain = msg->tics = tics;
    msg->flags = MF_JUSTADDED | flags;
    return msg;
    }
}

/// Remove the oldest message from the log.
static message_t* logPop(log_t* log)
{
    assert(NULL != log);
    {
    message_t* msg;
    int oldest = logFirstMessageIdx(log);
    if(0 > oldest) return NULL;

    msg = &log->msgs[oldest];
    --log->pvisMsgCount;

    msg->ticsRemain = 10;
    msg->flags &= ~MF_JUSTADDED;
    return msg;
    }
}

static void logEmpty(log_t* log)
{
    while(logPop(log)) {}
}

static void logPost(log_t* log, byte flags, const char* msg)
{
#define YELLOW_FMT      "{r=1; g=0.7; b=0.3;}"
#define YELLOW_FMT_LEN  19
#define SMALLBUF_MAXLEN 128

    assert(NULL != log && NULL != msg);
    {
    char smallBuf[SMALLBUF_MAXLEN+1];
    char* bigBuf = NULL, *p;
    size_t requiredLen = strlen(msg) + ((flags & LMF_YELLOW)? YELLOW_FMT_LEN : 0);

    if(0 == requiredLen) return;

    if(requiredLen <= SMALLBUF_MAXLEN)
    {
        p = smallBuf;
    }
    else
    {
        bigBuf = (char*) malloc(requiredLen + 1);
        if(NULL == bigBuf)
            Con_Error("Log::Post: Failed on allocation of %lu bytes for temporary "
                "local message buffer.", (unsigned long) (requiredLen + 1));
        p = bigBuf;
    }

    p[requiredLen] = '\0';
    if(flags & LMF_YELLOW)
    {
        sprintf(p, YELLOW_FMT "%s", msg);
    }
    else
    {
        sprintf(p, "%s", msg);
    }

    logPush(log, 0 | ((flags & LMF_NOHIDE) != 0? MF_NOHIDE : 0), p, cfg.msgUptime * TICSPERSEC);

    if(NULL != bigBuf)
        free(bigBuf);
    }
#undef SMALLBUF_MAXLEN
#undef YELLOW_FMT_LEN
#undef YELLOW_FMT
}

static void logRefresh(log_t* log)
{
    assert(NULL != log);
    {
    int n;

    log->pvisMsgCount = MIN_OF(log->msgCount, MAX_OF(0, cfg.msgCount));
    n = logFirstMessageIdx(log);
    if(0 > n) return;

    { int i;
    for(i = 0; i < log->pvisMsgCount; ++i, n = logNextMessageIdx(log, n))
    {
        message_t* msg = &log->msgs[n];

        // Change the tics remaining to that at post time plus a small bonus
        // so that they do not all disappear at once.
        msg->ticsRemain = msg->tics + i * TICSPERSEC;
        msg->flags &= ~MF_JUSTADDED;
    }}
    }
}

/**
 * Process gametic. Jobs include ticking messages and adjusting values
 * used when drawing the buffer for animation.
 */
static void logTicker(log_t* log)
{
    assert(NULL != log);
    {
    int oldest;

    // All messags tic away.
    { int i;
    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        message_t* msg = &log->msgs[i];
        if(0 == msg->ticsRemain) continue;
        --msg->ticsRemain;
    }}

    // Is it time to pop?
    oldest = logFirstMessageIdx(log);
    if(oldest >= 0)
    {
        message_t* msg = &log->msgs[oldest];
        if(0 == msg->ticsRemain)
        {
            logPop(log);
        }
    }
    }
}

static void logDrawer(log_t* log, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    assert(NULL != log);
    {
    const short textFlags = DTF_ALIGN_TOP|DTF_NO_EFFECTS | ((cfg.msgAlign == 0)? DTF_ALIGN_LEFT : (cfg.msgAlign == 2)? DTF_ALIGN_RIGHT : 0);
    int lineHeight, lineWidth;
    int i, n, pvisMsgCount, drawnMsgCount, firstPVisMsg, firstMsg;
    float y, yOffset, scrollFactor, col[4];
    message_t* msg;

    /// \kludge Do not draw the message log while the map title is being displayed.
    if(cfg.mapTitle && actualMapTime < 6 * 35)
        return;
    /// kludge end.

    pvisMsgCount = MIN_OF(log->pvisMsgCount, MAX_OF(0, cfg.msgCount));
    if(0 == pvisMsgCount) return;

    firstMsg = firstPVisMsg = logFirstPVisMessageIdx(log);
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        i = 0;
        while(0 == (log->msgs[firstMsg].flags & MF_NOHIDE) && ++i < pvisMsgCount)
        {
            firstMsg = logNextMessageIdx(log, firstMsg);
        }

        // Nothing visible?
        if(i == pvisMsgCount) return;

        // There is possibly fewer potentially-visible messages now.
        pvisMsgCount -= firstMsg - firstPVisMsg;
    } 

    FR_SetFont(FID(GF_FONTA));
    /// \fixme Query line height from the font.
    lineHeight = FR_CharHeight('Q')+1;

    // Scroll offset is calculated using the timeout of the first visible message.
    msg = &log->msgs[firstMsg];
    if(msg->ticsRemain > 0 && msg->ticsRemain <= (unsigned) lineHeight)
    {
        int viewW, viewH;
        float scale;
        R_GetViewPort(logLocalPlayer(log), NULL, NULL, &viewW, &viewH);
        scale = viewW >= viewH? (float)viewH/SCREENHEIGHT : (float)viewW/SCREENWIDTH;

        scrollFactor = 1.0f - (((float)msg->ticsRemain)/lineHeight);
        yOffset = -lineHeight * scrollFactor * cfg.msgScale * scale;
    }
    else
    {
        scrollFactor = 0;
        yOffset = 0;
    }

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    y = 0;
    n = firstMsg;
    drawnMsgCount = 0;

    for(i = 0; i < pvisMsgCount; ++i, n = logNextMessageIdx(log, n))
    {
        msg = &log->msgs[n];
        if(!cfg.hudShown[HUD_LOG] && !(msg->flags & MF_NOHIDE))
            continue;

        FR_TextDimensions(&lineWidth, NULL, msg->text, FID(GF_FONTA));

        // Default color and alpha.
        col[CR] = cfg.msgColor[CR];
        col[CG] = cfg.msgColor[CG];
        col[CB] = cfg.msgColor[CB];
        if(0 != i)
        {
            col[CA] = textAlpha;
        }
        else
        {
            // Fade out.
            col[CA] = textAlpha * 1.f - scrollFactor * (4/3.f);
            col[CA] = MINMAX_OF(0, col[CA], 1);
        }

        if((msg->flags & MF_JUSTADDED) && 0 != cfg.msgBlink)
        {
            const uint blinkSpeed = cfg.msgBlink;
            const uint msgTics = msg->tics - msg->ticsRemain;
            const uint td = (cfg.msgUptime * TICSPERSEC) - msg->ticsRemain;

            if(msgTics < blinkSpeed)
            {
                if(0 == msgTics || (td & 2))
                {
                    // Use the "flash" color.
                    col[CR] = col[CG] = col[CB] = 1;
                }
            }
            else if(msgTics < blinkSpeed + LOG_MESSAGE_FLASHFADETICS && msgTics >= blinkSpeed)
            {
                // Fade color to normal.
                float fade = (blinkSpeed + LOG_MESSAGE_FLASHFADETICS - msgTics);
                int c;
                for(c = 0; c < 3; ++c)
                    col[c] += (1.0f - col[c]) / LOG_MESSAGE_FLASHFADETICS * fade;
            }
        }

        // Draw using param text.
        // Messages may use the params to override the way the message is
        // is displayed (e.g., coloring of Hexen's "important" messages).
        FR_DrawText(msg->text, 0, y, FID(GF_FONTA), textFlags, .5f, 0,
            col[CR], col[CG], col[CB], col[CA], 0, 0, false);

        ++drawnMsgCount;
        y += lineHeight;

        if(lineWidth > *drawnWidth)
        {
            *drawnWidth = lineWidth;
        }
    }

    if(0 != drawnMsgCount)
    {
        *drawnHeight = /*first line*/ lineHeight * (1.f - scrollFactor) +
                       /*other lines*/ (drawnMsgCount != 1? lineHeight * (drawnMsgCount-1) : 0);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_Translatef(0, -yOffset, 0);
    }
}

static log_t* findLogForLocalPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        player_t* plr = &players[player];
        if((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame)
            return &msgLogs[player];
    }
    return NULL;
}

static void logShutdown(log_t* log)
{
    assert(NULL != log);
    {
    int i;
    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        message_t* msg = &log->msgs[i];
        if(msg->text)
        {
            free(msg->text);
            msg->text = NULL;
        }
        msg->textMaxLen = 0;
    }
    log->msgCount = 0;
    log->nextUsedMsg = 0;
    log->pvisMsgCount = 0;
    }
}

void Hu_LogStart(int player)
{
    log_t* log = findLogForLocalPlayer(player);
    if(NULL == log) return;

    logInit(log);
}

void Hu_LogShutdown(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        logShutdown(msgLogs + i);
    }
}

void Hu_LogTicker(void)
{
    // Do not tick if the game is paused.
    if(P_IsPaused()) return;

    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        logTicker(msgLogs + i);
    }}
}

void Hu_LogDrawer(int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight)
{
    log_t* log = findLogForLocalPlayer(player);
    if(NULL != drawnWidth)  *drawnWidth  = 0;
    if(NULL != drawnHeight) *drawnHeight = 0;
    if(NULL == log) return;

    logDrawer(log, textAlpha, iconAlpha, drawnWidth, drawnHeight);
}

void Hu_LogPost(int player, byte flags, const char* msg)
{
    log_t* log = findLogForLocalPlayer(player);
    if(NULL == log) return;

    logPost(log, flags, msg);
}

void Hu_LogPostVisibilityChangeNotification(void)
{
    Hu_LogPost(CONSOLEPLAYER, LMF_NOHIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
}

void Hu_LogRefresh(int player)
{
    log_t* log = findLogForLocalPlayer(player);
    if(NULL == log) return;

    logRefresh(log);
}

void Hu_LogEmpty(int player)
{
    log_t* log = findLogForLocalPlayer(player);
    if(NULL == log) return;

    logEmpty(log);
}

/**
 * Display a local game message.
 */
D_CMD(LocalMessage)
{
    D_NetMessageNoSound(CONSOLEPLAYER, argv[1]);
    return true;
}
