/**\file hu_log.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "doomsday.h"

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_tick.h"
#include "hu_stuff.h"
#include "hu_log.h"

/**
 * @addtogroup logMessageFlags
 * Flags private to this module.
 * @{
 */
#define LMF_JUSTADDED               (0x2)
/**@}*/

/// Mask for clearing non-public flags @ref logMessageFlags
#define LOG_INTERNAL_MESSAGEFLAGMASK    0xfe

void UILog_Register(void)
{
    cvartemplate_t cvars[] = {
        // Behavior
        { "msg-uptime",  0, CVT_FLOAT, &cfg.msgUptime, 1, 60 },

        // Display
        { "msg-align",   0, CVT_INT, &cfg.msgAlign, 0, 2, ST_LogUpdateAlignment },
        { "msg-blink",   CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0 },
        { "msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[CR], 0, 1 },
        { "msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[CG], 0, 1 },
        { "msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[CB], 0, 1 },
        { "msg-count",   0, CVT_INT, &cfg.msgCount, 1, 8 },
        { "msg-scale",   0, CVT_FLOAT, &cfg.msgScale, 0.1f, 1 },
        { "msg-show",    0, CVT_BYTE, &cfg.hudShown[HUD_LOG], 0, 1, ST_LogPostVisibilityChangeNotification },
        { NULL }
    };
    int i;
    for(i = 0; cvars[i].path; ++i)
        Con_AddVariable(cvars + i);
}

/// @return  Index of the first (i.e., earliest) message that is potentially visible.
static int UILog_FirstPVisMessageIdx(const uiwidget_t* obj)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    assert(obj->type == GUI_LOG);

    if(0 != log->_pvisMsgCount)
    {
        int n = log->_nextUsedMsg - MIN_OF(log->_pvisMsgCount, MAX_OF(0, cfg.msgCount));
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the first (i.e., earliest) message.
static int UILog_FirstMessageIdx(const uiwidget_t* obj)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    assert(obj->type == GUI_LOG);

    if(0 != log->_pvisMsgCount)
    {
        int n = log->_nextUsedMsg - log->_pvisMsgCount;
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the next (possibly already used) message.
static __inline int UILog_NextMessageIdx(const uiwidget_t* obj, int current)
{
    if(current < LOG_MAX_MESSAGES - 1) return current + 1;
    return 0; // Wrap around.
}

/// @return  Index of the previous (possibly already used) message.
static __inline int UILog_PrevMessageIdx(const uiwidget_t* obj, int current)
{
    if(current > 0) return current - 1;
    return LOG_MAX_MESSAGES - 1; // Wrap around.
}

/**
 * Push a new message into the log.
 *
 * @param flags  @ref messageFlags
 * @param text  Message to be added.
 * @param tics  Length of time the message should be visible.
 */
static guidata_log_message_t* UILog_Push(uiwidget_t* obj, int flags, const char* text, int tics)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    guidata_log_message_t* msg;
    int len;
    assert(obj->type == GUI_LOG && text);

    len = (int)strlen(text);
    if(0 == len)
    {
        Con_Error("Log::Push: Attempted to log zero-length message.");
        exit(1); // Unreachable.
    }

    msg = &log->_msgs[log->_nextUsedMsg];
    log->_nextUsedMsg = UILog_NextMessageIdx(obj, log->_nextUsedMsg);
    if(len >= msg->textMaxLen)
    {
        msg->textMaxLen = len+1;
        msg->text = (char*) Z_Realloc(msg->text, msg->textMaxLen, PU_GAMESTATIC);
        if(!msg->text)
            Con_Error("Log::Push: Failed on (re)allocation of %lu bytes for log message.", (unsigned long) msg->textMaxLen);
    }

    if(log->_msgCount < LOG_MAX_MESSAGES)
        ++log->_msgCount;
    if(log->_pvisMsgCount < LOG_MAX_MESSAGES)
        ++log->_pvisMsgCount;

    dd_snprintf(msg->text, msg->textMaxLen, "%s", text);
    msg->ticsRemain = msg->tics = tics;
    msg->flags = LMF_JUSTADDED | flags;
    return msg;
}

/// Remove the oldest message from the log.
static guidata_log_message_t* UILog_Pop(uiwidget_t* obj)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    guidata_log_message_t* msg;
    int oldest;
    assert(obj->type == GUI_LOG);

    oldest = UILog_FirstMessageIdx(obj);
    if(0 > oldest) return NULL;

    msg = &log->_msgs[oldest];
    --log->_pvisMsgCount;

    msg->ticsRemain = LOG_MESSAGE_SCROLLTICS;
    msg->flags &= ~LMF_JUSTADDED;
    return msg;
}

void UILog_Empty(uiwidget_t* obj)
{
    while(UILog_Pop(obj)) {}
}

void UILog_Post(uiwidget_t* obj, byte flags, const char* text)
{
#define SMALLBUF_MAXLEN 128

    //guidata_log_t* log = (guidata_log_t*)obj->typedata;
    char smallBuf[SMALLBUF_MAXLEN+1];
    char* bigBuf = NULL, *p;
    size_t requiredLen = strlen(text);
    assert(obj->type == GUI_LOG && text);

    if(0 == requiredLen) return;

    flags &= ~LOG_INTERNAL_MESSAGEFLAGMASK;

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
    sprintf(p, "%s", text);

    UILog_Push(obj, flags, p, cfg.msgUptime * TICSPERSEC);
    if(NULL != bigBuf)
        free(bigBuf);

#undef SMALLBUF_MAXLEN
}

void UILog_Refresh(uiwidget_t* obj)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    int i, n;
    assert(obj->type == GUI_LOG);

    log->_pvisMsgCount = MIN_OF(log->_msgCount, MAX_OF(0, cfg.msgCount));
    n = UILog_FirstMessageIdx(obj);
    if(0 > n) return;

    for(i = 0; i < log->_pvisMsgCount; ++i, n = UILog_NextMessageIdx(obj, n))
    {
        guidata_log_message_t* msg = &log->_msgs[n];

        // Change the tics remaining to that at post time plus a small bonus
        // so that they do not all disappear at once.
        msg->ticsRemain = msg->tics + i * TICSPERSEC;
        msg->flags &= ~LMF_JUSTADDED;
    }
}

/**
 * Process gametic. Jobs include ticking messages and adjusting values
 * used when drawing the buffer for animation.
 */
void UILog_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    int i, oldest;
    assert(obj->type == GUI_LOG);

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    // All messags tic away.
    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        guidata_log_message_t* msg = &log->_msgs[i];
        if(0 == msg->ticsRemain) continue;
        --msg->ticsRemain;
    }

    // Is it time to pop?
    oldest = UILog_FirstMessageIdx(obj);
    if(oldest >= 0)
    {
        guidata_log_message_t* msg = &log->_msgs[oldest];
        if(0 == msg->ticsRemain)
        {
            UILog_Pop(obj);
        }
    }
}

void UILog_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    const int alignFlags = ALIGN_TOP| ((cfg.msgAlign == 0)? ALIGN_LEFT : (cfg.msgAlign == 2)? ALIGN_RIGHT : 0);
    const short textFlags = DTF_NO_EFFECTS;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    //const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    int lineHeight;
    int i, n, pvisMsgCount = MIN_OF(log->_pvisMsgCount, MAX_OF(0, cfg.msgCount));
    int drawnMsgCount, firstPVisMsg, firstMsg, lastMsg;
    float y, yOffset, scrollFactor, col[4];
    guidata_log_message_t* msg;
    assert(obj->type == GUI_LOG);

    // Do not draw message logs while the map title is being displayed.
    // Rather a kludge...
    if(cfg.mapTitle && actualMapTime < 6 * 35) return;

    if(!pvisMsgCount) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    firstMsg = firstPVisMsg = UILog_FirstPVisMessageIdx(obj);
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        i = 0;
        while(0 == (log->_msgs[firstMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            firstMsg = UILog_NextMessageIdx(obj, firstMsg);
        }

        // Nothing visible?
        if(i == pvisMsgCount) goto stateCleanup;

        // There is possibly fewer potentially-visible messages now.
        pvisMsgCount -= firstMsg - firstPVisMsg;
    }

    lastMsg = firstMsg + pvisMsgCount-1;
    if(lastMsg > LOG_MAX_MESSAGES-1)
        lastMsg -= LOG_MAX_MESSAGES; // Wrap around.

    if(!cfg.hudShown[HUD_LOG])
    {
        // Rewind to the last non-hidden message.
        i = 0;
        while(0 == (log->_msgs[lastMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            lastMsg = UILog_PrevMessageIdx(obj, lastMsg);
        }
    }

    FR_SetFont(obj->font);
    /// @todo Query line height from the font.
    lineHeight = FR_CharHeight('Q')+1;

    // Scroll offset is calculated using the timeout of the first visible message.
    msg = &log->_msgs[firstMsg];
    if(msg->ticsRemain > 0 && msg->ticsRemain <= (unsigned) lineHeight)
    {
        scrollFactor = 1.0f - (((float)msg->ticsRemain)/lineHeight);
        yOffset = -lineHeight * scrollFactor;
    }
    else
    {
        scrollFactor = 0;
        yOffset = 0;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    y = 0;
    n = firstMsg;
    drawnMsgCount = 0;

    for(i = 0; i < pvisMsgCount; ++i, n = UILog_NextMessageIdx(obj, n))
    {
        msg = &log->_msgs[n];
        if(!cfg.hudShown[HUD_LOG] && !(msg->flags & LMF_NO_HIDE))
            continue;

        // Default color and alpha.
        col[CR] = cfg.msgColor[CR];
        col[CG] = cfg.msgColor[CG];
        col[CB] = cfg.msgColor[CB];
        if(n != firstMsg)
        {
            col[CA] = textAlpha;
        }
        else
        {
            // Fade out.
            col[CA] = textAlpha * 1.f - scrollFactor * (4/3.f);
            col[CA] = MINMAX_OF(0, col[CA], 1);
        }

        if((msg->flags & LMF_JUSTADDED) && 0 != cfg.msgBlink)
        {
            const uint blinkSpeed = cfg.msgBlink;
            const uint msgTics = msg->tics - msg->ticsRemain;

            if(msgTics < blinkSpeed)
            {
                const uint td = (cfg.msgUptime * TICSPERSEC) - msg->ticsRemain;
                if(n == lastMsg && (0 == msgTics || (td & 2)))
                {
                    // Use the "flash" color.
                    col[CR] = col[CG] = col[CB] = 1;
                }
            }
            else if(msgTics < blinkSpeed + LOG_MESSAGE_FLASHFADETICS && msgTics >= blinkSpeed)
            {
                // Fade color to normal.
                const float fade = (blinkSpeed + LOG_MESSAGE_FLASHFADETICS - msgTics);
                col[CR] += (1.0f - col[CR]) / LOG_MESSAGE_FLASHFADETICS * fade;
                col[CG] += (1.0f - col[CG]) / LOG_MESSAGE_FLASHFADETICS * fade;
                col[CB] += (1.0f - col[CB]) / LOG_MESSAGE_FLASHFADETICS * fade;
            }
        }

        FR_SetColorAndAlpha(col[CR], col[CG], col[CB], col[CA]);
        FR_DrawTextXY3(msg->text, 0, y, alignFlags, textFlags);

        ++drawnMsgCount;
        y += lineHeight;
    }

stateCleanup:
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void UILog_UpdateGeometry(uiwidget_t* obj)
{
    guidata_log_t* log = (guidata_log_t*)obj->typedata;
    guidata_log_message_t* msg;
    int lineHeight;
    int i, n, pvisMsgCount = MIN_OF(log->_pvisMsgCount, MAX_OF(0, cfg.msgCount));
    int drawnMsgCount, firstPVisMsg, firstMsg, lastMsg;
    float scrollFactor;
    RectRaw lineGeometry;
    assert(obj->type == GUI_LOG);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(0 == pvisMsgCount) return;

    firstMsg = firstPVisMsg = UILog_FirstPVisMessageIdx(obj);
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        i = 0;
        while(0 == (log->_msgs[firstMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            firstMsg = UILog_NextMessageIdx(obj, firstMsg);
        }

        // Nothing visible?
        if(i == pvisMsgCount) return;

        // There is possibly fewer potentially-visible messages now.
        pvisMsgCount -= firstMsg - firstPVisMsg;
    }

    lastMsg = firstMsg + pvisMsgCount-1;
    if(lastMsg > LOG_MAX_MESSAGES-1)
        lastMsg -= LOG_MAX_MESSAGES; // Wrap around.

    if(!cfg.hudShown[HUD_LOG])
    {
        // Rewind to the last non-hidden message.
        i = 0;
        while(0 == (log->_msgs[lastMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            lastMsg = UILog_PrevMessageIdx(obj, lastMsg);
        }
    }

    FR_SetFont(FID(GF_FONTA));
    /// @todo Query line height from the font.
    lineHeight = FR_CharHeight('Q')+1;

    // Scroll offset is calculated using the timeout of the first visible message.
    msg = &log->_msgs[firstMsg];
    if(msg->ticsRemain > 0 && msg->ticsRemain <= (unsigned) lineHeight)
    {
        scrollFactor = 1.0f - (((float)msg->ticsRemain)/lineHeight);
    }
    else
    {
        scrollFactor = 0;
    }

    n = firstMsg;
    drawnMsgCount = 0;

    lineGeometry.origin.x = lineGeometry.origin.y = 0;

    for(i = 0; i < pvisMsgCount; ++i, n = UILog_NextMessageIdx(obj, n))
    {
        msg = &log->_msgs[n];
        if(!cfg.hudShown[HUD_LOG] && !(msg->flags & LMF_NO_HIDE)) continue;

        ++drawnMsgCount;

        FR_TextSize(&lineGeometry.size, msg->text);
        Rect_UniteRaw(obj->geometry, &lineGeometry);

        lineGeometry.origin.y += lineHeight;
    }

    if(0 != drawnMsgCount)
    {
        // Subtract the scroll offset.
        Rect_SetHeight(obj->geometry, Rect_Height(obj->geometry) - lineHeight * scrollFactor);
    }

    Rect_SetWidthHeight(obj->geometry, Rect_Width(obj->geometry)  * cfg.msgScale,
                                       Rect_Height(obj->geometry) * cfg.msgScale);
}
