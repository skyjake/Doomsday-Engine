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

#include <de/ButtonWidget>
#include <de/LineEditWidget>
#include <de/IPersistent>
#include <de/ProgressWidget>

class HomeItemWidget;
class HomeMenuWidget;

/**
 * Listing of packages with search and filtering options.
 *
 * Controls its own height; other position rules must be set by the owner.
 */
class PackagesWidget : public de::GuiWidget, public de::IPersistent
{
    Q_OBJECT

public:
    /// Specified manual package is not available. @ingroup errors
    DENG2_ERROR(UnavailableError);

    /// Determines whether an item should be shown as highlighted or not.
    class IPackageStatus
    {
    public:
        virtual ~IPackageStatus();
        virtual bool isPackageHighlighted(de::String const &packageId) const = 0;
    };

    enum PopulateBehavior { PopulationDisabled, PopulationEnabled };

public:
    PackagesWidget(PopulateBehavior popBehavior = PopulationEnabled,
                   de::String const &name = de::String());

    PackagesWidget(de::StringList const &manualPackageIds,
                   de::String const &name = de::String());

    HomeMenuWidget &menu();
    de::ProgressWidget &progress();

    void setRightClickToOpenContextMenu(bool enable);
    void setPopulationEnabled(bool enable);
    void setFilterEditorMinimumY(de::Rule const &minY);
    void setPackageStatus(IPackageStatus const &packageStatus);

    /**
     * Sets the data items that determine what kind of action buttons get created
     * for the items of the list.
     *
     * @param actionItem  Items for action buttons.
     */
    void setActionItems(de::ui::Data const &actionItems);

    de::ui::Data &actionItems();

    void setActionsAlwaysShown(bool showActions);

    void setColorTheme(ColorTheme unselectedItem, ColorTheme selectedItem,
                       ColorTheme unselectedItemHilit, ColorTheme selectedItemHilit);

    void populate();
    void updateItems();
    de::dsize itemCount() const;

    /**
     * Finds the item for a package, if it is currently listed.
     * @param packageId  Package identifier.
     * @return  Item.
     */
    de::ui::Item const *itemForPackage(de::String const &packageId) const;

    /**
     * Returns the ID of the package whose action buttons were most recently interacted
     * with.
     *
     * @return Package ID.
     */
    de::String actionPackage() const;

    de::GuiWidget *actionWidget() const;

    de::ui::Item const *actionItem() const;

    void scrollToPackage(de::String const &packageId) const;

    de::LineEditWidget &searchTermsEditor();

    // Events.
    void initialize();
    void update();

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (de::PersistentState const &fromState);

public slots:
    void refreshPackages();

signals:
    void itemCountChanged(unsigned int shownItems, unsigned int totalItems);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_HOME_PACKAGESWIDGET_H
