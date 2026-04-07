/** @file profilepickerwidget.h  Widget for selecting/manipulating settings profiles.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_PROFILEPICKERWIDGET_H
#define DE_CLIENT_PROFILEPICKERWIDGET_H

#include <de/choicewidget.h>
#include "configprofiles.h"

/**
 * Widget for selecting/manipulating settings profiles.
 *
 * While ProfilePickerWidget owns its popup menu button, the user is
 * responsible for laying it out appropriately. However, by default it is
 * attached to the right edge of the profile picker choice widget.
 */
class ProfilePickerWidget : public de::ChoiceWidget
{
public:
    DE_AUDIENCE(ProfileChange, void profileChanged())
    DE_AUDIENCE(EditorRequest, void profileEditorRequested())

public:
    /**
     * Constructs a settings profile picker.
     *
     * @param settings     Settings to operate on.
     * @param description  Human-readable description of a profile, for instance
     *                     "appearance". Appears in the UI dialogs.
     * @param name         Name for the widget.
     */
    ProfilePickerWidget(ConfigProfiles &settings, const de::String &description,
                        const de::String &name = {});

    ButtonWidget &button();

    void useInvertedStyleForPopups();

public:
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
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_PROFILEPICKERWIDGET_H
