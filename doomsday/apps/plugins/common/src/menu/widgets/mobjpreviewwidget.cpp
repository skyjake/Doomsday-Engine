/** @file mobjpreviewwidget.cpp  UI widget for previewing a mobj.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "menu/widgets/mobjpreviewwidget.h"

#include "hu_menu.h" // menuTime
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DE_PIMPL_NOREF(MobjPreviewWidget)
{
    int mobjType    = 0;
    int playerClass = 0;
    int xlatClass   = 0;  ///< Color translation class.
    int xlatMap     = 0;  ///< Color translation map.
};

MobjPreviewWidget::MobjPreviewWidget()
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
    setFlags(NoFocus); // never focusable.
}

MobjPreviewWidget::~MobjPreviewWidget()
{}

static void findSpriteForMobjType(int mobjType, spritetype_e *sprite, int *frame)
{
    DE_ASSERT(mobjType >= MT_FIRST && mobjType < NUMMOBJTYPES && sprite && frame);

    mobjinfo_t *info = &MOBJINFO[mobjType];
    int stateNum = info->states[SN_SPAWN];
    *sprite = spritetype_e(STATES[stateNum].sprite);
    *frame  = ((menuTime >> 3) & 3);
}

void MobjPreviewWidget::setMobjType(int newMobjType)
{
    d->mobjType = newMobjType;
}

void MobjPreviewWidget::setPlayerClass(int newPlayerClass)
{
    d->playerClass = newPlayerClass;
}

void MobjPreviewWidget::setTranslationClass(int newTranslationClass)
{
    d->xlatClass = newTranslationClass;
}

void MobjPreviewWidget::setTranslationMap(int newTranslationMap)
{
    d->xlatMap = newTranslationMap;
}

/// @todo We can do better - the engine should be able to render this visual for us.
void MobjPreviewWidget::draw() const
{
    if(MT_NONE == d->mobjType) return;

    spritetype_e sprite;
    int spriteFrame;
    findSpriteForMobjType(d->mobjType, &sprite, &spriteFrame);

    spriteinfo_t info;
    if(!R_GetSpriteInfo(sprite, spriteFrame, &info)) return;

    Point2Raw origin = {{{info.geometry.origin.x, info.geometry.origin.y}}};
    Size2Raw size = {{{info.geometry.size.width, info.geometry.size.height}}};

    float scale = (size.height > size.width? (float)MNDATA_MOBJPREVIEW_HEIGHT / size.height :
                                             (float)MNDATA_MOBJPREVIEW_WIDTH  / size.width);

    float s = info.texCoord[0];
    float t = info.texCoord[1];

    int tClassCycled = d->xlatClass;
    int tMapCycled   = d->xlatMap;
    // Are we cycling the translation map?
    if(tMapCycled == NUMPLAYERCOLORS)
    {
        tMapCycled = menuTime / 5 % NUMPLAYERCOLORS;
    }
#if __JHEXEN__
    if(gameMode == hexen_v10)
    {
        // Cycle through the four available colomnRendState->
        if(d->xlatMap == NUMPLAYERCOLORS) tMapCycled = menuTime / 5 % 4;
    }
    if(d->playerClass >= PCLASS_FIGHTER)
    {
        R_GetTranslation(d->playerClass, tMapCycled, &tClassCycled, &tMapCycled);
    }
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(geometry().topLeft.x, geometry().topLeft.y, 0);
    DGL_Scalef(scale, scale, 1);
    // Translate origin to the top left.
    DGL_Translatef(-origin.x, -origin.y, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPSprite2(info.material, tClassCycled, tMapCycled);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(0, 0);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(size.width, 0);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(size.width, size.height);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(0, size.height);
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_Disable(DGL_TEXTURE_2D);
}

void MobjPreviewWidget::updateGeometry()
{
    // @todo calculate visible dimensions properly!
    geometry().setSize(Vec2ui(MNDATA_MOBJPREVIEW_WIDTH, MNDATA_MOBJPREVIEW_HEIGHT));
}

} // namespace menu
} // namespace common
