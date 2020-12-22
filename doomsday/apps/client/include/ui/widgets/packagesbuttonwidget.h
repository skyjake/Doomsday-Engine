/** @file packagesbuttonwidget.h
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

#ifndef DE_CLIENT_UI_PACKAGESBUTTONWIDGET_H
#define DE_CLIENT_UI_PACKAGESBUTTONWIDGET_H

#include "ui/dialogs/packagesdialog.h"

#include <doomsday/gameprofiles.h>
#include <de/buttonwidget.h>

/**
 * Button for selecting packages.
 */
class PackagesButtonWidget : public de::ButtonWidget
{
public:
    DE_AUDIENCE(Selection, void packageSelectionChanged(const de::StringList &packageIds))

public:
    PackagesButtonWidget();

    void setDialogTitle(const de::String &title);
    void setDialogIcon(const de::DotPath &imageId);
    void setGameProfile(const GameProfile &profile);
    void setSetupCallback(std::function<void (PackagesDialog &dialog)> func);
    void setLabelPrefix(const de::String &labelPrefix);
    void setNoneLabel(const de::String &noneLabel);
    void setOverrideLabel(const de::String &overrideLabel);
    void setPackages(de::StringList packageIds);

    de::StringList packages() const;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_PACKAGESBUTTONWIDGET_H
