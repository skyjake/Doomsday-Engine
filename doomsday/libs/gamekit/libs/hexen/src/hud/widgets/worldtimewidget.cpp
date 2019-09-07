/** @file worldtimewidget.cpp  GUI widget for -.
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

#include "hud/widgets/worldtimewidget.h"

#include "jhexen.h"

using namespace de;

static void WorldTimeWidget_Draw(guidata_worldtime_t *time, const Point2Raw *offset)
{
    DE_ASSERT(time);
    time->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void WorldTimeWidget_UpdateGeometry(guidata_worldtime_t *time)
{
    DE_ASSERT(time);
    time->updateGeometry();
}

DE_PIMPL_NOREF(guidata_worldtime_t)
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
    , d(new Impl)
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

    const player_t &plr = ::players[player()];

    dint wt = plr.worldTimer / TICRATE;
    d->days    = wt / 86400; wt -= d->days * 86400;
    d->hours   = wt / 3600;  wt -= d->hours * 3600;
    d->minutes = wt / 60;    wt -= d->minutes * 60;
    d->seconds = wt;
}

void guidata_worldtime_t::draw(const Vec2i &offset) const
{
#define LEADING         ( 0.5 )

    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(!ST_AutomapIsOpen(player())) return;

    const auto secondsAsText = Stringf("%02i", d->seconds);
    const auto minutesAsText = Stringf("%02i", d->minutes);
    const auto hoursAsText   = Stringf("%02i", d->hours);

    FR_SetFont(font());
    FR_SetTracking(0);
    FR_SetColorAndAlpha(1, 1, 1, textOpacity);

    const dint counterWidth = FR_TextWidth("00");
    const dint spacerWidth  = FR_TextWidth(" : ");
    const dint lineHeight   = FR_TextHeight("00");

    dint x = -counterWidth;
    dint y = 0;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_DrawTextXY(secondsAsText, x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    FR_DrawTextXY(minutesAsText, x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    FR_DrawTextXY(hoursAsText, x, y);
    y += lineHeight;

    if(d->days)
    {
        const auto daysAsText = Stringf("%02i", d->days) + " day" + DE_PLURAL_S(d->days);

        y += lineHeight * LEADING;  // Extra padding.

        FR_DrawTextXY(daysAsText, 0, y);
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

    const dint counterWidth = FR_TextWidth("00");
    const dint spacerWidth  = FR_TextWidth(" : ");
    const dint lineHeight   = FR_TextHeight("00");

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
