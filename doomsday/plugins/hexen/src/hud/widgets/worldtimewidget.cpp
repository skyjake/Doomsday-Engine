/** @file worldtimewidget.cpp  GUI widget for -.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "hud/widgets/worldtimewidget.h"

#include "jhexen.h"

using namespace de;

static void WorldTimeWidget_Draw(guidata_worldtime_t *time, Point2Raw const *offset)
{
    DENG2_ASSERT(time);
    time->draw(offset? Vector2i(offset->xy) : Vector2i());
}

static void WorldTimeWidget_UpdateGeometry(guidata_worldtime_t *time)
{
    DENG2_ASSERT(time);
    time->updateGeometry();
}

DENG2_PIMPL_NOREF(guidata_worldtime_t)
{
    dint days    = 0;
    dint hours   = 0;
    dint minutes = 0;
    dint seconds = 0;
};

guidata_worldtime_t::guidata_worldtime_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(WorldTimeWidget_UpdateGeometry),
                function_cast<DrawFunc>(WorldTimeWidget_Draw),
                player)
    , d(new Instance)
{}

guidata_worldtime_t::~guidata_worldtime_t()
{}

void guidata_worldtime_t::reset()
{
    d->days = d->hours = d->minutes = d->seconds = 0;
}

void guidata_worldtime_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const &plr = ::players[player()];

    dint wt = plr.worldTimer / TICRATE;
    d->days    = wt / 86400; wt -= d->days * 86400;
    d->hours   = wt / 3600;  wt -= d->hours * 3600;
    d->minutes = wt / 60;    wt -= d->minutes * 60;
    d->seconds = wt;
}

void guidata_worldtime_t::draw(Vector2i const &offset) const
{
#define LEADING         ( 0.5 )

    dfloat const textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(!ST_AutomapIsOpen(player())) return;

    auto const secondsAsText = String("%1").arg(d->seconds, 2, 10, QChar('0'));
    auto const minutesAsText = String("%1").arg(d->minutes, 2, 10, QChar('0'));
    auto const hoursAsText   = String("%1").arg(d->hours  , 2, 10, QChar('0'));

    FR_SetFont(font());
    FR_SetTracking(0);
    FR_SetColorAndAlpha(1, 1, 1, textOpacity);

    dint const counterWidth = FR_TextWidth("00");
    dint const spacerWidth  = FR_TextWidth(" : ");
    dint const lineHeight   = FR_TextHeight("00");

    dint x = -counterWidth;
    dint y = 0;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_DrawTextXY(secondsAsText.toUtf8().constData(), x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    FR_DrawTextXY(minutesAsText.toUtf8().constData(), x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    FR_DrawTextXY(hoursAsText.toUtf8().constData(), x, y);
    y += lineHeight;

    if(d->days)
    {
        auto const daysAsText = String("%1").arg(d->days, 2, 10, QChar('0')) + " day" + DENG2_PLURAL_S(d->days);

        y += lineHeight * LEADING;  // Extra padding.

        FR_DrawTextXY(daysAsText.toUtf8().constData(), 0, y);
        y += lineHeight;

        if(d->days >= 5)
        {
            y += lineHeight * LEADING;  // Extra padding.

            FR_DrawTextXY("You Freak!!!", 0, y);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef LEADING
}

void guidata_worldtime_t::updateGeometry()
{
#define LEADING             ( 0.5 )

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!ST_AutomapIsOpen(player())) return;

    FR_SetFont(font());
    FR_SetTracking(0);

    dint const counterWidth = FR_TextWidth("00");
    dint const spacerWidth  = FR_TextWidth(" : ");
    dint const lineHeight   = FR_TextHeight("00");

    dint x = -(counterWidth * 2 + spacerWidth * 3);
    dint y = lineHeight;

    if(d->days)
    {
        y += lineHeight * LEADING;  // Extra padding.
        y += lineHeight;

        if(d->days >= 5)
        {
            y += lineHeight * LEADING;  // Extra padding.

            x = -de::max(std::abs(x), FR_TextWidth("You Freak!!!"));
            y += lineHeight;
        }
    }

    Rect_SetWidthHeight(&geometry(), x * cfg.common.hudScale, y * cfg.common.hudScale);

#undef LEADING
}
