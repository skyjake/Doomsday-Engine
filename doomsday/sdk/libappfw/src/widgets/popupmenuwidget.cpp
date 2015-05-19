/** @file popupmenuwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/PopupMenuWidget"
#include "de/MenuWidget"
#include "de/ToggleWidget"
#include "de/GuiRootWidget"
#include "de/ChildWidgetOrganizer"
#include "de/AtlasProceduralImage"
#include "de/ui/Item"
#include "de/App"

#include <de/IndirectRule>
#include <de/OperatorRule>

namespace de {

static String const VAR_SHOW_ANNOTATIONS("ui.showAnnotations");

DENG_GUI_PIMPL(PopupMenuWidget)
, DENG2_OBSERVES(ButtonWidget, StateChange)
, DENG2_OBSERVES(ButtonWidget, Triggered)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DENG2_OBSERVES(Variable, Change)
{
    class HeadingOverlayImage : public ProceduralImage
    {
    public:
        HeadingOverlayImage(GuiWidget &owner)
            : _owner(owner)
            , _id(Id::None)
        {
            if(_owner.hasRoot())
            {
                // We can set this up right away.
                alloc();
            }
        }

        GuiRootWidget &root()
        {
            return _owner.root();
        }

        void alloc()
        {
            _id = root().solidWhitePixel();
            setSize(Vector2f(1, 1)); //root().atlas().imageRect(_id).size());
        }

        void glInit()
        {
            alloc();
        }

        void glDeinit()
        {
            _id = Id::None;
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
        {
            if(!_id.isNone())
            {
                Rectanglef visible = rect;
                visible.setWidth(_owner.rule().width().value());
                verts.makeQuad(visible, color(), root().atlas().imageRectf(_id));
            }
        }

    private:
        GuiWidget &_owner;
        Id _id;
    };

    ButtonWidget *hover;
    int oldScrollY;
    Rule const *widestItem;
    IndirectRule *maxItemWidth;

    Instance(Public *i)
        : Base(i)
        , hover(0)
        , oldScrollY(0)
        , widestItem(0)
        , maxItemWidth(0)
    {
        maxItemWidth = new IndirectRule;
        App::config(VAR_SHOW_ANNOTATIONS).audienceForChange() += this;
    }

    ~Instance()
    {
        App::config(VAR_SHOW_ANNOTATIONS).audienceForChange() -= this;
        releaseRef(maxItemWidth);
        releaseRef(widestItem);
    }

    void addToMaxWidth(GuiWidget &widget)
    {
        maxInto(widestItem, widget.rule().width());
        maxItemWidth->setSource(*widestItem);
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        // Popup menu items' background is provided by the popup.
        widget.set(Background());

        if(item.semantics().testFlag(ui::Item::Separator))
        {
            LabelWidget &lab = widget.as<LabelWidget>();
            lab.setTextColor(item.semantics().testFlag(ui::Item::Annotation)? "label.altaccent" : "label.accent");
            lab.setMaximumTextWidth(*maxItemWidth);
            lab.rule().setInput(Rule::Width, *maxItemWidth);
            return;
        }

        if(LabelWidget *lab = widget.maybeAs<LabelWidget>())
        {
            lab->margins().set("popup.menu.margin");
            addToMaxWidth(widget);
        }

        // Customize buttons for use in the popup. We will observe the button
        // state for highlighting and possibly close the popup when an action
        // gets triggered.
        if(ButtonWidget *b = widget.maybeAs<ButtonWidget>())
        {
            addToMaxWidth(widget);

            b->setHoverTextColor("inverted.text");
            b->setSizePolicy(ui::Expand, ui::Expand);

            if(!b->is<ToggleWidget>())
            {
                b->setTextGap("dialog.gap");
                b->setOverrideImageSize(style().fonts().font("default").height().valuei());
            }

            b->audienceForStateChange() += this;

            // Triggered actions close the menu.
            if(item.semantics().testFlag(ui::Item::ActivationClosesPopup))
            {
                b->audienceForTriggered() += this;
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if(item.semantics().testFlag(ui::Item::Annotation))
        {
            if(!App::config().getb(VAR_SHOW_ANNOTATIONS))
            {
                widget.hide();
            }

            widget.margins().set("halfunit").setLeft("popup.menu.margin");
            widget.setFont("separator.annotation");
        }
        else if(item.semantics().testFlag(ui::Item::Separator))
        {
            // The label of a separator may change.
            if(item.label().isEmpty())
            {
                widget.margins().set("");
                widget.setFont("separator.empty");
                widget.as<LabelWidget>().setOverlayImage(0);
            }
            else
            {
                widget.margins().set("halfunit").setLeft("popup.menu.margin");
                widget.setFont("separator.label");

                /*
                LabelWidget &lab = widget.as<LabelWidget>();
                //lab.setAlignment(ui::AlignCenter);
                HeadingOverlayImage *img = new HeadingOverlayImage(widget);
                img->setColor(style().colors().colorf("accent"));
                lab.setOverlayImage(img, ui::AlignBottomLeft);
                */
            }
        }
    }

    void updateItemHitRules()
    {
        GridLayout const &layout = self.menu().layout();

        foreach(Widget *child, self.menu().childWidgets())
        {
            GuiWidget &widget = child->as<GuiWidget>();
            if(self.menu().isWidgetPartOfMenu(widget))
            {
                Vector2i cell = layout.widgetPos(widget);
                DENG2_ASSERT(cell.x >= 0 && cell.y >= 0);

                // We want items to be hittable throughout the width of the menu, however
                // restrict this to the item's column if there are multiple columns.
                widget.hitRule()
                        .setInput(Rule::Left,  (!cell.x? self.rule().left() :
                                                         layout.columnLeft(cell.x)))
                        .setInput(Rule::Right, (cell.x == layout.gridSize().x - 1? self.rule().right() :
                                                                                   layout.columnRight(cell.x)));
            }
        }
    }

    bool hasButtonsWithImages() const
    {
        foreach(Widget *child, self.menu().childWidgets())
        {
            if(ButtonWidget *button = child->maybeAs<ButtonWidget>())
            {
                if(button->hasImage())
                    return true;
            }
        }
        return false;
    }

    /**
     * Adjusts the left margins of clickable items so that icons are aligned by their
     * text, with the possible icon hanging on the left. If there are no items with
     * icons, no extra padding is applied.
     */
    void updateItemMargins()
    {
        bool const useExtraPadding = hasButtonsWithImages();

        auto const &padding = style().rules().rule("popup.menu.paddedmargin");
        auto const &none    = style().rules().rule("popup.menu.margin");

        foreach(Widget *child, self.menu().childWidgets())
        {
            GuiWidget &widget = child->as<GuiWidget>();

            // Pad annotations with the full amount.
            if(LabelWidget *label = widget.maybeAs<LabelWidget>())
            {
                ui::Item const *item = self.menu().organizer().findItemForWidget(widget);
                if(item->semantics().testFlag(ui::Item::Annotation))
                {
                    if(useExtraPadding)
                    {
                        label->setMaximumTextWidth(*maxItemWidth - padding);
                        widget.margins().setLeft(padding);
                    }
                    else
                    {
                        label->setMaximumTextWidth(*maxItemWidth);
                        widget.margins().setLeft(none);
                    }
                }
            }

            // Pad buttons according to their image size.
            if(ButtonWidget *button = widget.maybeAs<ButtonWidget>())
            {
                if(useExtraPadding)
                {
                    Rule const *padRule = holdRef(padding);
                    if(button->hasImage())
                    {
                        LabelWidget::ContentLayout layout;
                        button->contentLayout(layout);
                        sumInto(padRule, -Const(layout.image.width()) -
                                style().rules().rule(button->textGap()));
                    }
                    widget.margins().setLeft(*padRule);
                    releaseRef(padRule);
                }
                else
                {
                    widget.margins().setLeft(none);
                }
            }
        }
    }

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state)
    {
        if(state != ButtonWidget::Up)
        {
            button.setImageColor(style().colors().colorf("inverted.text"));
        }
        else
        {
            button.setImageColor(style().colors().colorf("text"));
        }

        // Position item highlight.
        if(&button == hover && state == ButtonWidget::Up)
        {
            hover = 0;
            self.requestGeometry();
            return;
        }

        if(state == ButtonWidget::Hover || state == ButtonWidget::Down)
        {
            hover = &button;
            self.requestGeometry();
        }
    }

    Rectanglei highlightRect() const
    {
        Rectanglei hi;
        if(hover)
        {
            hi.topLeft.x     = hover->hitRule().left().valuei();
            hi.topLeft.y     = hover->hitRule().top().valuei();
            hi.bottomRight.x = hover->hitRule().right().valuei();
            hi.bottomRight.y = hover->hitRule().bottom().valuei();
        }
        // Clip the highlight to the main popup area.
        return hi & self.rule().recti();
    }

    void buttonActionTriggered(ButtonWidget &)
    {
        // The popup menu is closed when an action is triggered.
        self.close();
    }

    void updateIfScrolled()
    {
        // If the menu is scrolled, we need to update some things.
        int scrollY = self.menu().scrollPositionY().valuei();
        if(scrollY == oldScrollY)
        {
            return;
        }
        oldScrollY = scrollY;

        //qDebug() << "menu scrolling" << scrollY;

        // Resend the mouse position so the buttons realize they've moved.
        root().dispatchLatestMousePosition();

        self.requestGeometry();
    }

    void variableValueChanged(Variable &, Value const &newValue)
    {
        bool changed = false;

        // Update widgets of annotation items.
        self.items().forAll([this, &newValue, &changed] (ui::Item const &item) {
            if(item.semantics().testFlag(ui::Item::Annotation)) {
                self.menu().itemWidget<GuiWidget>(item).show(newValue.isTrue());
                changed = true;
            }
            return LoopContinue;
        });

        if(changed)
        {
            self.menu().updateLayout();
        }
    }
};

PopupMenuWidget::PopupMenuWidget(String const &name)
    : PopupWidget(name), d(new Instance(this))
{
    setContent(new MenuWidget(name.isEmpty()? "" : name + "-content"));

    menu().setGridSize(1, ui::Expand, 0, ui::Expand);

    menu().organizer().audienceForWidgetCreation() += d;
    menu().organizer().audienceForWidgetUpdate() += d;
}

MenuWidget &PopupMenuWidget::menu() const
{
    return static_cast<MenuWidget &>(content());
}

void PopupMenuWidget::update()
{
    PopupWidget::update();
    d->updateIfScrolled();
}

void PopupMenuWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    PopupWidget::glMakeGeometry(verts);

    if(d->hover && d->hover->isEnabled())
    {
        verts.makeQuad(d->highlightRect(),
                       d->hover->state() == ButtonWidget::Hover?
                           style().colors().colorf("inverted.background") :
                           style().colors().colorf("accent"),
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());
    }
}

void PopupMenuWidget::preparePanelForOpening()
{
    // Redo the layout.
    menu().updateLayout();
    d->updateItemHitRules();
    d->updateItemMargins();

    // Make sure the menu doesn't go beyond the top of the view.
    if(openingDirection() == ui::Up)
    {
        menu().rule().setInput(Rule::Height,
                OperatorRule::minimum(menu().contentRule().height() + menu().margins().height(),
                                      anchorY() - menu().margins().top()));
    }

    PopupWidget::preparePanelForOpening();
}

void PopupMenuWidget::panelClosing()
{
    PopupWidget::panelClosing();

    if(d->hover)
    {
        d->hover->setTextModulationColorf(Vector4f(1, 1, 1, 1));
        d->hover->setImageColor(style().colors().colorf("text"));
        d->hover = 0;
        requestGeometry();
    }

    menu().dismissPopups();
}

} // namespace de
