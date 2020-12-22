/** @file homeitemwidget.h  Label with action buttons and an icon.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_HOME_HOMEITEMWIDGET_H
#define DE_CLIENT_UI_HOME_HOMEITEMWIDGET_H

#include <de/asset.h>
#include <de/buttonwidget.h>

class Game;
namespace res { class LumpCatalog; }
class HomeMenuWidget;

/**
 * Label with action buttons and an icon. The item can be in either selected
 * or non-selected state.
 */
class HomeItemWidget : public de::GuiWidget, public de::IAssetGroup
{
public:
    enum Flag {
        NonAnimatedHeight = 0,
        AnimatedHeight    = 0x1,
        WithoutIcon       = 0x2,
    };

    DE_AUDIENCE(Activity,    void mouseActivity(HomeItemWidget &))
    DE_AUDIENCE(DoubleClick, void itemCoubleClicked(HomeItemWidget &))
    DE_AUDIENCE(ContextMenu, void openItemContextMenu(HomeItemWidget &))
    DE_AUDIENCE(Selection,   void itemSelected(HomeItemWidget &))

public:
    HomeItemWidget(de::Flags flags = AnimatedHeight, const de::String &name = de::String());

    de::AssetGroup &assets() override;

    de::LabelWidget &       icon();
    de::LabelWidget &       label();
    const de::LabelWidget & label() const;

    void addButton(de::GuiWidget *button);
    de::GuiWidget &buttonWidget(int index) const;
    void setKeepButtonsVisible(bool yes);
    void setLabelMinimumRightMargin(const de::Rule &rule);

    virtual void setSelected(bool selected);
    bool isSelected() const;

    void useNormalStyle();
    void useInvertedStyle();
    void useColorTheme(ColorTheme style);
    void useColorTheme(ColorTheme unselected, ColorTheme selected);
    const de::DotPath &textColorId() const;

    void acquireFocus();

    HomeMenuWidget *parentMenu();

    // Events.
    bool handleEvent(const de::Event &event) override;
    void focusGained() override;
    void focusLost() override;

    virtual void itemRightClicked();

protected:
    void updateButtonLayout();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_HOMEITEMWIDGET_H
