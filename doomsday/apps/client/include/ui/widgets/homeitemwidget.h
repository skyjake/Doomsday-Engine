/** @file homeitemwidget.h  Label with action buttons and an icon.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_UI_HOME_HOMEITEMWIDGET_H
#define DENG_CLIENT_UI_HOME_HOMEITEMWIDGET_H

#include <de/ButtonWidget>

class Game;
namespace res { class LumpCatalog; }

/**
 * Label with action buttons and an icon. The item can be in either selected
 * or non-selected state.
 */
class HomeItemWidget : public de::GuiWidget
{
    Q_OBJECT

public:
    HomeItemWidget(de::String const &name = "");

    de::LabelWidget &icon();
    de::LabelWidget &label();

    void addButton(de::ButtonWidget *button);

    virtual void setSelected(bool selected);
    bool isSelected() const;

    void useNormalStyle();
    void useInvertedStyle();
    void useColorTheme(ColorTheme style);
    void useColorTheme(ColorTheme unselected, ColorTheme selected);

    void acquireFocus();

public:
    enum LogoFlag
    {
        UnmodifiedAppearance = 0,
        ColorizedByFamily    = 0x1,
        Downscale50Percent   = 0x2,

        DefaultLogoFlags     = ColorizedByFamily | Downscale50Percent,
    };
    Q_DECLARE_FLAGS(LogoFlags, LogoFlag)

    /**
     * Prepares a game logo image to be used in items. The image is based on the
     * game's title screen image in its WAD file(s).
     *
     * @param game     Game.
     * @param catalog  Catalog of selected lumps.
     *
     * @return Image.
     *
     * @todo This could be moved to a better location / other class. -jk
     */
    static de::Image makeGameLogo(Game const &game, res::LumpCatalog const &catalog,
                                  LogoFlags flags = DefaultLogoFlags);

signals:
    void mouseActivity();
    void doubleClicked();
    void openContextMenu();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HomeItemWidget::LogoFlags)

#endif // DENG_CLIENT_UI_HOME_HOMEITEMWIDGET_H
