/** @file packageswidget.h
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

#ifndef DENG_CLIENT_UI_HOME_PACKAGESWIDGET_H
#define DENG_CLIENT_UI_HOME_PACKAGESWIDGET_H

#include <de/GuiWidget>
#include <de/IPersistent>

class HomeItemWidget;

/**
 * Listing of packages with search and filtering options.
 *
 * Controls its own height; other position rules must be set by the owner.
 */
class PackagesWidget : public de::GuiWidget, public de::IPersistent
{
public:
    /// Determines whether an item should be shown as highlighted or not.
    class IPackageStatus
    {
    public:
        virtual ~IPackageStatus();
        virtual bool isPackageHighlighted(de::String const &packageId) const = 0;
    };

    class IButtonHandler
    {
    public:
        virtual ~IButtonHandler();
        virtual void packageButtonClicked(HomeItemWidget &itemWidget, de::String const &packageId) = 0;
    };

public:
    PackagesWidget(de::String const &name = "");

    void setPackageStatus(IPackageStatus const &packageStatus);
    void setButtonHandler(IButtonHandler &buttonHandler);
    void setButtonLabels(de::String const &buttonLabel,
                         de::String const &highlightedButtonLabel);

    void setColorTheme(ColorTheme unselectedItem, ColorTheme selectedItem,
                       ColorTheme loadedUnselectedItem, ColorTheme loadedSelectedItem);

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (de::PersistentState const &fromState);

public slots:
    void refreshPackages();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_HOME_PACKAGESWIDGET_H
