/** @file playerlogwidget.cpp  Specialized HudWidget for logging player messages.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
#include "hud/widgets/playerlogwidget.h"

#include <cstring>
#include <cstdio>
#include "hu_stuff.h"
#include "p_tick.h"
#include "player.h"    ///< @ref LMF_NO_HIDE @todo remove me

using namespace de;

static void PlayerLogWidget_UpdateGeometry(PlayerLogWidget *log)
{
    DE_ASSERT(log);
    log->updateGeometry();
}

static void PlayerLogWidget_Draw(PlayerLogWidget *log, const Point2Raw *offset)
{
    DE_ASSERT(log);
    log->draw(offset? Vec2i(offset->xy) : Vec2i());
}

DE_PIMPL(PlayerLogWidget)
{
    LogEntry entries[LOG_MAX_ENTRIES];

    dint entryCount     = 0;  ///< Number of used entires.
    dint pvisEntryCount = 0;  ///< Number of potentially visible entries.
    dint nextUsedEntry  = 0;  ///< Index of the next used entry to be re-used.

    Impl(Public *i) : Base(i) {}

    /// @return  Index of the first (i.e., earliest) entry that is potentially visible.
    dint firstPVisEntryIdx() const
    {
        if(pvisEntryCount)
        {
            dint n = nextUsedEntry - de::min(pvisEntryCount, de::max(0, cfg.common.msgCount));
            if(n < 0) n += LOG_MAX_ENTRIES; // Wrap around.
            return n;
        }
        return -1;
    }

    /// @return  Index of the first (i.e., earliest) entry.
    dint firstEntryIdx() const
    {
        if(pvisEntryCount)
        {
            dint n = nextUsedEntry - pvisEntryCount;
            if(n < 0) n += LOG_MAX_ENTRIES;  // Wrap around.
            return n;
        }
        return -1;
    }

    /// @return  Index of the next (possibly already used) entry.
    inline dint nextEntryIdx(dint current) const
    {
        if(current < LOG_MAX_ENTRIES - 1) return current + 1;
        return 0; // Wrap around.
    }

    /// @return  Index of the previous (possibly already used) entry.
    inline dint prevEntryIdx(dint current) const
    {
        if(current > 0) return current - 1;
        return LOG_MAX_ENTRIES - 1; // Wrap around.
    }

    /**
     * Make a new entry in the log and copy the given @a message.
     *
     * @param message   Message to be added.
     * @param tics      Length of time the message should be visible.
     * @param dontHide  @c true= do not hide this message.
     */
    LogEntry *pushEntry(const String &message, dint tics, bool dontHide)
    {
        DE_ASSERT(!message.isEmpty());

        LogEntry *entry = &entries[nextUsedEntry];
        nextUsedEntry = nextEntryIdx(nextUsedEntry);
        if(entryCount < LOG_MAX_ENTRIES)
            entryCount += 1;
        if(pvisEntryCount < LOG_MAX_ENTRIES)
            pvisEntryCount += 1;

        entry->text      = message;  // make a copy
        entry->tics      = entry->ticsRemain = tics;
        entry->justAdded = true;
        entry->dontHide  = dontHide;

        return entry;
    }

    LogEntry *popEntry()
    {
        dint oldest = firstEntryIdx();
        if(0 > oldest) return nullptr;

        LogEntry *entry = &entries[oldest];
        if(pvisEntryCount > 0)
            pvisEntryCount -= 1;

        entry->ticsRemain = LOG_MESSAGE_SCROLLTICS;
        entry->justAdded  = false;
        return entry;
    }
};

PlayerLogWidget::PlayerLogWidget(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(PlayerLogWidget_UpdateGeometry),
                function_cast<DrawFunc>(PlayerLogWidget_Draw),
                player)
    , d(new Impl(this))
{}

PlayerLogWidget::~PlayerLogWidget()
{}

void PlayerLogWidget::clear()
{
    while(d->popEntry()) {}
    d->nextUsedEntry  = 0;
    d->pvisEntryCount = 0;
    for(LogEntry &entry : d->entries)
    {
        entry.text.clear();
        //entry.text.squeeze();
    }
}

void PlayerLogWidget::post(dint flags, const String &message)
{
    if(message.isEmpty()) return;

    d->pushEntry(message, cfg.common.msgUptime * TICSPERSEC, CPP_BOOL(flags & LMF_NO_HIDE));
}

void PlayerLogWidget::refresh()
{
    d->pvisEntryCount = de::min(d->entryCount, de::max(0, cfg.common.msgCount));
    dint n = d->firstEntryIdx();
    if(0 > n) return;

    for(dint i = 0; i < d->pvisEntryCount; ++i, n = d->nextEntryIdx(n))
    {
        LogEntry *entry = &d->entries[n];

        // Change the tics remaining to that at post time plus a small bonus
        // so that they do not all disappear at once.
        entry->ticsRemain = entry->tics + i * TICSPERSEC;
        entry->justAdded  = false;
    }
}

void PlayerLogWidget::tick(timespan_t /*tickLength*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    // All messags tic away.
    for(dint i = 0; i < LOG_MAX_ENTRIES; ++i)
    {
        LogEntry *entry = &d->entries[i];
        if(!entry->ticsRemain) continue;
        --entry->ticsRemain;
    }

    // Is it time to pop?
    dint oldest = d->firstEntryIdx();
    if(oldest >= 0)
    {
        LogEntry *entry = &d->entries[oldest];
        if(!entry->ticsRemain)
        {
            d->popEntry();
        }
    }
}

void PlayerLogWidget::draw(const Vec2i &offset)
{
    dint pvisEntryCount = de::min(d->pvisEntryCount, de::max(0, cfg.common.msgCount));
    dint firstEntry     = d->firstPVisEntryIdx();

    // Flow control note:
    // This block may or may not exit the method early.
    // It was refactored so as not to use GOTO.
    // If it does not exit the method early, it will modify the value of firstEntry
    if(!cfg.hudShown[HUD_LOG])
    {
        // Linear search for the first visible entry
        // Traverses forward and updates firstEntry until it reaches an entry marked as dontHide
        // If it traverses all entries and fails to find a visible entry, it will exit the method
        {
            dint hiddenEntryCount = 0;
            while(!d->entries[firstEntry].dontHide && ++hiddenEntryCount < pvisEntryCount)
            {
                firstEntry = d->nextEntryIdx(firstEntry);
            }

            // Nothing visible?
            if(hiddenEntryCount == pvisEntryCount) return;
        }

        // There is possibly fewer potentially-visible entry now.
        pvisEntryCount -= firstEntry - d->firstPVisEntryIdx();
    }

    if(pvisEntryCount > 0)
    {
        // GL Setup
        // ================================================================================================
        {
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();
            DGL_Translatef(offset.x, offset.y, 0);

            // Calculate Y offset for map title and translate the origin
            {
                dfloat offsetDueToMapTitle = 0;
                if(Hu_IsMapTitleVisible() && !cfg.common.automapTitleAtBottom)
                {
                    offsetDueToMapTitle = Hu_MapTitleHeight();
                }
                DGL_Translatef(0, offsetDueToMapTitle, 0);
            }

            DGL_Scalef(cfg.common.msgScale, cfg.common.msgScale, 1);
        }

        // Render
        // ================================================================================================
        {
            dint firstEntryVisibleToPlayer  = firstEntry;
            DE_UNUSED(firstEntryVisibleToPlayer);

            dint lastEntry = firstEntry + pvisEntryCount - 1;
            if(lastEntry > LOG_MAX_ENTRIES - 1)
                lastEntry -= LOG_MAX_ENTRIES;  // Wrap around.

            if(!cfg.hudShown[HUD_LOG])
            {
                // Rewind to the last non-hidden entry.
                dint i = 0;
                while(!d->entries[lastEntry].dontHide && ++i < pvisEntryCount)
                {
                    lastEntry = d->prevEntryIdx(lastEntry);
                }
            }

            FR_SetFont(font());
            /// @todo Query line height from the font.
            const dint lineHeight = FR_CharHeight('Q') + 1;

            // Scroll offset is calculated using the timeout of the first visible entry.
            LogEntry *entry = &d->entries[firstEntry];
            dfloat yOffset      = 0;
            dfloat scrollFactor = 0;
            if(entry->ticsRemain > 0 && entry->ticsRemain <= (unsigned) lineHeight)
            {
                scrollFactor = 1.0f - (((dfloat)entry->ticsRemain) / lineHeight);
                yOffset = -lineHeight * scrollFactor;
            }

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_Translatef(0, yOffset, 0);
            DGL_Enable(DGL_TEXTURE_2D);

            dint n = firstEntry;
            dint drawnEntryCount = 0;
            dfloat y = 0;

            for(dint i = 0; i < pvisEntryCount; ++i, n = d->nextEntryIdx(n))
            {
                LogEntry *entry = &d->entries[n];

                if(!cfg.hudShown[HUD_LOG] && !entry->dontHide)
                    continue;

                const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

                // ::w is used for opacity
                Vec4f rgba(Vec3f(cfg.common.msgColor), textOpacity);

                // Fading HUD messages:
                // If fading, update colour opacity each pass until it has completely faded
                if(n == firstEntry)
                {
                    rgba.w = textOpacity * 1.f - scrollFactor * (4/3.f);
                    rgba.w = de::clamp(0.f, rgba.w, 1.f);
                }

                // Flash new messages
                // Will either set the message text to white every n tics, or transition to the correct value
                if(entry->justAdded && cfg.common.msgBlink)
                {
                    const duint blinkSpeed = cfg.common.msgBlink;
                    const duint msgTics    = entry->tics - entry->ticsRemain;

                    if(msgTics < blinkSpeed)
                    {
                        const duint td = (cfg.common.msgUptime * TICSPERSEC) - entry->ticsRemain;
                        if(n == lastEntry && (0 == msgTics || (td & 2)))
                        {
                            // Use the "flash" color.
                            rgba.x = rgba.y = rgba.z = 1;
                        }
                    }
                    else if(msgTics < blinkSpeed + LOG_MESSAGE_FLASHFADETICS && msgTics >= blinkSpeed)
                    {
                        // Fade color to normal.
                        const dfloat fade = (blinkSpeed + LOG_MESSAGE_FLASHFADETICS - msgTics);
                        rgba.x += (1.0f - rgba.x) / LOG_MESSAGE_FLASHFADETICS * fade;
                        rgba.y += (1.0f - rgba.y) / LOG_MESSAGE_FLASHFADETICS * fade;
                        rgba.z += (1.0f - rgba.z) / LOG_MESSAGE_FLASHFADETICS * fade;
                    }
                }

                // Draw entry text
                {
                    const dint alignFlags    = ALIGN_TOP| ((cfg.common.msgAlign == 0)? ALIGN_LEFT : (cfg.common.msgAlign == 2)? ALIGN_RIGHT : 0);
                    const dshort textFlags   = DTF_NO_EFFECTS;
                    FR_SetColorAndAlpha(rgba.x, rgba.y, rgba.z, rgba.w);
                    FR_DrawTextXY3(entry->text, 0, y, alignFlags, textFlags);
                }

                ++drawnEntryCount;
                y += lineHeight;
        }

        }

        // GL Cleanup
        // ================================================================================================
        {
            DGL_Disable(DGL_TEXTURE_2D);
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }

}

void PlayerLogWidget::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    dint pvisEntryCount = de::min(d->pvisEntryCount, de::max(0, cfg.common.msgCount));
    if(!pvisEntryCount) return;

    dint firstEntry     = d->firstPVisEntryIdx();
    dint firstPVisEntry = firstEntry;
    if(!cfg.hudShown[HUD_LOG])
    {
        // Advance to the first non-hidden message.
        dint i = 0;
        while(!d->entries[firstEntry].dontHide && ++i < pvisEntryCount)
        {
            firstEntry = d->nextEntryIdx(firstEntry);
        }

        // Nothing visible?
        if(i == pvisEntryCount) return;

        // There is possibly fewer potentially-visible messages now.
        pvisEntryCount -= firstEntry - firstPVisEntry;
    }

    dint lastEntry = firstEntry + pvisEntryCount - 1;
    if(lastEntry > LOG_MAX_ENTRIES-1)
        lastEntry -= LOG_MAX_ENTRIES; // Wrap around.

    if(!cfg.hudShown[HUD_LOG])
    {
        // Rewind to the last non-hidden message.
        dint i = 0;
        while(!d->entries[lastEntry].dontHide && ++i < pvisEntryCount)
        {
            lastEntry = d->prevEntryIdx(lastEntry);
        }
    }

    FR_SetFont(FID(GF_FONTA));
    /// @todo Query line height from the font.
    const dint lineHeight = FR_CharHeight('Q') + 1;

    // Scroll offset is calculated using the timeout of the first visible message.
    dfloat scrollFactor;
    LogEntry *entry = &d->entries[firstEntry];
    if(entry->ticsRemain > 0 && entry->ticsRemain <= (unsigned) lineHeight)
    {
        scrollFactor = 1.0f - (((dfloat)entry->ticsRemain) / lineHeight);
    }
    else
    {
        scrollFactor = 0;
    }

    dint n = firstEntry;
    dint drawnEntryCount = 0;

    RectRaw lineGeometry{};
    for(dint i = 0; i < pvisEntryCount; ++i, n = d->nextEntryIdx(n))
    {
        entry = &d->entries[n];
        if(!cfg.hudShown[HUD_LOG] && !entry->dontHide) continue;

        drawnEntryCount += 1;

        FR_TextSize(&lineGeometry.size, entry->text);
        Rect_UniteRaw(&geometry(), &lineGeometry);

        lineGeometry.origin.y += lineHeight;
    }

    if(drawnEntryCount)
    {
        // Subtract the scroll offset.
        Rect_SetHeight(&geometry(), Rect_Height(&geometry()) - lineHeight * scrollFactor);
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * cfg.common.msgScale,
                                     Rect_Height(&geometry()) * cfg.common.msgScale);
}

static void playerLogVisibilityChanged()
{
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NO_HIDE, !::cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void PlayerLogWidget::consoleRegister()  // static
{
    // Behavior
    C_VAR_FLOAT("msg-uptime",  &cfg.common.msgUptime,   0, 1, 60);

    // Display
    C_VAR_INT2 ("msg-align",   &cfg.common.msgAlign,    0, 0, 2, ST_LogUpdateAlignment);
    C_VAR_INT  ("msg-blink",   &cfg.common.msgBlink,    CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("msg-color-r", &cfg.common.msgColor[0], 0, 0, 1);
    C_VAR_FLOAT("msg-color-g", &cfg.common.msgColor[1], 0, 0, 1);
    C_VAR_FLOAT("msg-color-b", &cfg.common.msgColor[2], 0, 0, 1);
    C_VAR_INT  ("msg-count",   &cfg.common.msgCount,    0, 1, 8);
    C_VAR_FLOAT("msg-scale",   &cfg.common.msgScale,    0, 0.1f, 1);
    C_VAR_BYTE2("msg-show",    &cfg.hudShown[HUD_LOG],  0, 0, 1, playerLogVisibilityChanged);
}

