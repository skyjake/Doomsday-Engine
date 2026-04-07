/** @file homewidget.h  Root widget for the Home UI.
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

#ifndef DE_CLIENT_UI_HOMEWIDGET_H
#define DE_CLIENT_UI_HOMEWIDGET_H

#include <de/popupwidget.h>
#include <de/ipersistent.h>

/**
 * Root widget for the Home UI.
 *
 * Lays out children in horizontal columns, fitting a suitable number on
 * screen at once.
 */
class HomeWidget : public de::GuiWidget, public de::IPersistent
{
public:
    HomeWidget();

    void viewResized() override;
    bool handleEvent(const de::Event &event) override;
    bool dispatchEvent(const de::Event &event,
                       bool (de::Widget::*memberFunc)(const de::Event &)) override;

    void moveOnscreen(de::TimeSpan span = 1.5);
    void moveOffscreen(de::TimeSpan span = 1.5);

    void tabChanged();
    //void mouseActivityInColumn(const de::GuiWidget *);

    // Events.
    void update() override;

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (const de::PersistentState &fromState);

    static de::PopupWidget *makeSettingsPopup();

protected:
    void updateStyle() override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOMEWIDGET_H
