#ifndef DENG_CLIENT_SESSIONMENUWIDGET_H
#define DENG_CLIENT_SESSIONMENUWIDGET_H

#include "gamesessionwidget.h"
#include "gamefilterwidget.h"
#include <de/MenuWidget>
#include <de/ChildWidgetOrganizer>

/**
 * Specialized menu that contains items represented by GameSessionWidget
 * (or a class derived from it).
 *
 * Acts as the base class for the various game selection menus: singleplayer,
 * multiplayer, saved sessions. Common functionality such as sorting is
 * handled here in the base class.
 *
 * @ingroup ui
 */
class SessionMenuWidget : public de::MenuWidget,
                          public de::ChildWidgetOrganizer::IWidgetFactory
{
    Q_OBJECT

public:
    /// Items in the menu should implement this interface to allow the
    /// base class to handle them.
    class SessionItem
    {
    public:
        SessionItem(SessionMenuWidget &owner);
        virtual ~SessionItem();

        SessionMenuWidget &menu() const;

        /// Returns the title (label) for sorting.
        virtual de::String title() const = 0;

        /// Returns the game for sorting.
        virtual de::String gameIdentityKey() const = 0;

        virtual de::String sortKey() const;

    private:
        DENG2_PRIVATE(d)
    };

public:
    SessionMenuWidget(de::String const &name = "");

    /**
     * Sets the game filter widget that defines the filter and sort
     * order for the menu.
     *
     * @param filter  Filtering widget.
     */
    void setFilter(GameFilterWidget *filter);

    GameFilterWidget &filter() const;

    void setColumns(int numberOfColumns);

    /**
     * Constructs an Action for activating a game session.
     *
     * @param item  Item to activate.
     *
     * @return Action for selecting the item. Callers gets ownership (ref 1).
     */
    virtual de::Action *makeAction(de::ui::Item const &item) = 0;

signals:
    void availabilityChanged();
    void sessionSelected(de::ui::Item const *item);

public slots:
    void sort();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_SESSIONMENUWIDGET_H
