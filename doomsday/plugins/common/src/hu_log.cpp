/** @file hu_log.cpp  Game message logging and display.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "hu_log.h"
#include <cstring>
#include <cstdio>
#include <de/memory.h>
#include "p_tick.h"
#include "hu_stuff.h"

using namespace de;

/**
 * @addtogroup logMessageFlags
 * Flags private to this module.
 * @{
 */
#define LMF_JUSTADDED  (0x2)
/**@}*/

/// Mask for clearing non-public flags @ref logMessageFlags
#define LOG_INTERNAL_MESSAGEFLAGMASK  (0xfe)

/// @return  Index of the first (i.e., earliest) message that is potentially visible.
static int UILog_FirstPVisMessageIdx(uiwidget_t const *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    if(0 != log->_pvisMsgCount)
    {
        int n = log->_nextUsedMsg - de::min(log->_pvisMsgCount, de::max(0, cfg.common.msgCount));
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the first (i.e., earliest) message.
static int UILog_FirstMessageIdx(uiwidget_t const *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    if(0 != log->_pvisMsgCount)
    {
        int n = log->_nextUsedMsg - log->_pvisMsgCount;
        if(n < 0) n += LOG_MAX_MESSAGES; // Wrap around.
        return n;
    }
    return -1;
}

/// @return  Index of the next (possibly already used) message.
static inline int UILog_NextMessageIdx(uiwidget_t const * /*ob*/, int current)
{
    if(current < LOG_MAX_MESSAGES - 1) return current + 1;
    return 0; // Wrap around.
}

/// @return  Index of the previous (possibly already used) message.
static inline int UILog_PrevMessageIdx(uiwidget_t const * /*ob*/, int current)
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
static guidata_log_message_t *UILog_Push(uiwidget_t *ob, int flags, char const *text, int tics)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG && text);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    int len = (int)strlen(text);
    if(!len)
    {
        DENG2_ASSERT(!"Log::Push: Attempted to log zero-length message.");
        return 0;
    }

    guidata_log_message_t *msg = &log->_msgs[log->_nextUsedMsg];
    log->_nextUsedMsg = UILog_NextMessageIdx(ob, log->_nextUsedMsg);
    if(len >= msg->textMaxLen)
    {
        msg->textMaxLen = len+1;
        msg->text = (char *) Z_Realloc(msg->text, msg->textMaxLen, PU_GAMESTATIC);
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
static guidata_log_message_t *UILog_Pop(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    int oldest = UILog_FirstMessageIdx(ob);
    if(0 > oldest) return NULL;

    guidata_log_message_t *msg = &log->_msgs[oldest];
    --log->_pvisMsgCount;

    msg->ticsRemain = LOG_MESSAGE_SCROLLTICS;
    msg->flags &= ~LMF_JUSTADDED;
    return msg;
}

void UILog_Empty(uiwidget_t *ob)
{
    while(UILog_Pop(ob)) {}
}

void UILog_Post(uiwidget_t *ob, byte flags, char const *text)
{
#define SMALLBUF_MAXLEN 128

    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG && text != 0);
    //guidata_log_t *log = (guidata_log_t *)ob->typedata;
    char smallBuf[SMALLBUF_MAXLEN+1];

    int requiredLen = (int)strlen(text);
    if(!requiredLen) return;

    flags &= ~LOG_INTERNAL_MESSAGEFLAGMASK;

    char *bigBuf = 0, *p;
    if(requiredLen <= SMALLBUF_MAXLEN)
    {
        p = smallBuf;
    }
    else
    {
        bigBuf = (char *) M_Malloc(requiredLen + 1);
        p = bigBuf;
    }
    p[requiredLen] = '\0';
    sprintf(p, "%s", text);

    UILog_Push(ob, flags, p, cfg.common.msgUptime * TICSPERSEC);

    M_Free(bigBuf);

#undef SMALLBUF_MAXLEN
}

void UILog_Refresh(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    log->_pvisMsgCount = de::min(log->_msgCount, de::max(0, cfg.common.msgCount));
    int n = UILog_FirstMessageIdx(ob);
    if(0 > n) return;

    for(int i = 0; i < log->_pvisMsgCount; ++i, n = UILog_NextMessageIdx(ob, n))
    {
        guidata_log_message_t *msg = &log->_msgs[n];

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
void UILog_Ticker(uiwidget_t *ob, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    // All messags tic away.
    for(int i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        guidata_log_message_t* msg = &log->_msgs[i];
        if(!msg->ticsRemain) continue;
        --msg->ticsRemain;
    }

    // Is it time to pop?
    int oldest = UILog_FirstMessageIdx(ob);
    if(oldest >= 0)
    {
        guidata_log_message_t *msg = &log->_msgs[oldest];
        if(!msg->ticsRemain)
        {
            UILog_Pop(ob);
        }
    }
}

void UILog_Drawer(uiwidget_t *ob, Point2Raw const *offset)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;
    int const alignFlags  = ALIGN_TOP| ((cfg.common.msgAlign == 0)? ALIGN_LEFT : (cfg.common.msgAlign == 2)? ALIGN_RIGHT : 0);
    short const textFlags = DTF_NO_EFFECTS;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    //float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;
    int lineHeight;
    int i, n, pvisMsgCount = de::min(log->_pvisMsgCount, de::max(0, cfg.common.msgCount));
    int drawnMsgCount, firstPVisMsg, firstMsg, lastMsg;
    float y, yOffset, scrollFactor, col[4];
    float offsetDueToMapTitle = 0;
    guidata_log_message_t *msg;

    if(Hu_IsMapTitleVisible() && !cfg.common.automapTitleAtBottom)
    {
        offsetDueToMapTitle = Hu_MapTitleHeight();
    }

    if(!pvisMsgCount) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Translatef(0, offsetDueToMapTitle, 0);
    DGL_Scalef(cfg.common.msgScale, cfg.common.msgScale, 1);

    firstMsg = firstPVisMsg = UILog_FirstPVisMessageIdx(ob);
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        i = 0;
        while(0 == (log->_msgs[firstMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            firstMsg = UILog_NextMessageIdx(ob, firstMsg);
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
            lastMsg = UILog_PrevMessageIdx(ob, lastMsg);
        }
    }

    FR_SetFont(ob->font);
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

    for(i = 0; i < pvisMsgCount; ++i, n = UILog_NextMessageIdx(ob, n))
    {
        msg = &log->_msgs[n];
        if(!cfg.hudShown[HUD_LOG] && !(msg->flags & LMF_NO_HIDE))
            continue;

        // Default color and alpha.
        col[CR] = cfg.common.msgColor[CR];
        col[CG] = cfg.common.msgColor[CG];
        col[CB] = cfg.common.msgColor[CB];
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

        if((msg->flags & LMF_JUSTADDED) && 0 != cfg.common.msgBlink)
        {
            uint const blinkSpeed = cfg.common.msgBlink;
            uint const msgTics = msg->tics - msg->ticsRemain;

            if(msgTics < blinkSpeed)
            {
                uint const td = (cfg.common.msgUptime * TICSPERSEC) - msg->ticsRemain;
                if(n == lastMsg && (0 == msgTics || (td & 2)))
                {
                    // Use the "flash" color.
                    col[CR] = col[CG] = col[CB] = 1;
                }
            }
            else if(msgTics < blinkSpeed + LOG_MESSAGE_FLASHFADETICS && msgTics >= blinkSpeed)
            {
                // Fade color to normal.
                float const fade = (blinkSpeed + LOG_MESSAGE_FLASHFADETICS - msgTics);
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

void UILog_UpdateGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_LOG);
    guidata_log_t *log = (guidata_log_t *)ob->typedata;
    guidata_log_message_t *msg;
    int lineHeight;
    int i, n, pvisMsgCount = de::min(log->_pvisMsgCount, de::max(0, cfg.common.msgCount));
    int drawnMsgCount, firstPVisMsg, firstMsg, lastMsg;
    float scrollFactor;
    RectRaw lineGeometry;

    Rect_SetWidthHeight(ob->geometry, 0, 0);

    if(!pvisMsgCount) return;

    firstMsg = firstPVisMsg = UILog_FirstPVisMessageIdx(ob);
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        i = 0;
        while(0 == (log->_msgs[firstMsg].flags & LMF_NO_HIDE) && ++i < pvisMsgCount)
        {
            firstMsg = UILog_NextMessageIdx(ob, firstMsg);
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
            lastMsg = UILog_PrevMessageIdx(ob, lastMsg);
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

    for(i = 0; i < pvisMsgCount; ++i, n = UILog_NextMessageIdx(ob, n))
    {
        msg = &log->_msgs[n];
        if(!cfg.hudShown[HUD_LOG] && !(msg->flags & LMF_NO_HIDE)) continue;

        ++drawnMsgCount;

        FR_TextSize(&lineGeometry.size, msg->text);
        Rect_UniteRaw(ob->geometry, &lineGeometry);

        lineGeometry.origin.y += lineHeight;
    }

    if(0 != drawnMsgCount)
    {
        // Subtract the scroll offset.
        Rect_SetHeight(ob->geometry, Rect_Height(ob->geometry) - lineHeight * scrollFactor);
    }

    Rect_SetWidthHeight(ob->geometry, Rect_Width(ob->geometry)  * cfg.common.msgScale,
                                      Rect_Height(ob->geometry) * cfg.common.msgScale);
}

void UILog_Register()
{
    // Behavior
    C_VAR_FLOAT("msg-uptime",  &cfg.common.msgUptime,          0, 1, 60);

    // Display
    C_VAR_INT2 ("msg-align",   &cfg.common.msgAlign,           0, 0, 2, ST_LogUpdateAlignment);
    C_VAR_INT  ("msg-blink",   &cfg.common.msgBlink,           CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("msg-color-r", &cfg.common.msgColor[CR],       0, 0, 1);
    C_VAR_FLOAT("msg-color-g", &cfg.common.msgColor[CG],       0, 0, 1);
    C_VAR_FLOAT("msg-color-b", &cfg.common.msgColor[CB],       0, 0, 1);
    C_VAR_INT  ("msg-count",   &cfg.common.msgCount,           0, 1, 8);
    C_VAR_FLOAT("msg-scale",   &cfg.common.msgScale,           0, 0.1f, 1);
    C_VAR_BYTE2("msg-show",    &cfg.hudShown[HUD_LOG],  0, 0, 1, ST_LogPostVisibilityChangeNotification);
}
