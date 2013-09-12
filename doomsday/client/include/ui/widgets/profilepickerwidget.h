/** @file profilepickerwidget.h  Widget for selecting/manipulating settings profiles.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_PROFILEPICKERWIDGET_H
#define DENG_CLIENT_PROFILEPICKERWIDGET_H

#include "choicewidget.h"
#include "SettingsRegister"

/**
 * Widget for selecting/manipulating settings profiles.
 *
 * While ProfilePickerWidget owns its popup menu button, the user is
 * responsible for laying it out appropriately. However, by default it is
 * attached to the right edge of the profile picker choice widget.
 */
class ProfilePickerWidget : public ChoiceWidget
{
    Q_OBJECT

public:
    /**
     * Constructs a settings profile picker.
     *
     * @param settings     Settings to operate on.
     * @param description  Human-readable description of a profile, for instance
     *                     "appearance". Appears in the UI dialogs.
     * @param name         Name for the widget.
     */
    ProfilePickerWidget(SettingsRegister &settings, de::String const &description,
                        de::String const &name = "");

    ButtonWidget &button();

signals:
    void profileChanged();
    void profileEditorRequested();

public slots:
    void openMenu();
    void edit();
    void rename();
    void duplicate();
    void reset();
    void remove();
    void applySelectedProfile();

protected:
    void updateStyle();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_PROFILEPICKERWIDGET_H
