/** @file popupmenuwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/popupmenuwidget.h"
#include "de/menuwidget.h"
#include "de/togglewidget.h"
#include "de/guirootwidget.h"
#include "de/childwidgetorganizer.h"
#include "de/atlasproceduralimage.h"
#include "de/ui/item.h"

#include <de/app.h>
#include <de/basewindow.h>
#include <de/config.h>
#include <de/indirectrule.h>
#include <de/operatorrule.h>

namespace de {

static String const VAR_SHOW_ANNOTATIONS("ui.showAnnotations");

DE_GUI_PIMPL(PopupMenuWidget)
, DE_OBSERVES(ButtonWidget, StateChange)
, DE_OBSERVES(ButtonWidget, Triggered)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DE_OBSERVES(Variable, Change)
{
    class HeadingOverlayImage : public ProceduralImage
    {
    public:
        HeadingOverlayImage(GuiWidget &owner)
            : _owner(owner)
            , _id(Id::None)
        {
            if (_owner.hasRoot())
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
            setPointSize({1, 1});
        }

        void glInit() override
        {
            if (_id.isNone())
            {
                alloc();
            }
        }

        void glDeinit() override
        {
            _id = Id::None;
        }

        void glMakeGeometry(GuiVertexBuilder &verts, const Rectanglef &rect) override
        {
            if (!_id.isNone())
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

    ColorTheme colorTheme = Normal;
    const ButtonWidget *hover;
    int oldScrollY;
    const Rule *widestItem;
    IndirectRule *maxItemWidth;
    SafeWidgetPtr<PopupWidget> parentPopup;

    Impl(Public *i)
        : Base(i)
        , hover(0)
        , oldScrollY(0)
        , widestItem(0)
        , maxItemWidth(0)
    {
        maxItemWidth = new IndirectRule;
        App::config(VAR_SHOW_ANNOTATIONS).audienceForChange() += this;
    }

    ~Impl()
    {
        releaseRef(maxItemWidth);
        releaseRef(widestItem);
    }

    void addToMaxWidth(GuiWidget &widget)
    {
        maxInto(widestItem, widget.rule().width());
        maxItemWidth->setSource(*widestItem);
    }

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        // Popup menu items' background is provided by the popup.
        widget.set(Background());

        if (item.semantics().testFlag(ui::Item::Separator))
        {
            LabelWidget &lab = widget.as<LabelWidget>();
            lab.setTextColor(item.semantics().testFlag(ui::Item::Annotation)? "label.altaccent" :
                                                                              "label.accent");
            lab.setMaximumTextWidth(*maxItemWidth);
            lab.rule().setInput(Rule::Width, *maxItemWidth);
            return;
        }

        if (LabelWidget *lab = maybeAs<LabelWidget>(widget))
        {
            lab->margins().set("popup.menu.margin");
            lab->setMaximumTextWidth(rule("popup.menu.width.max"));
            lab->setTextLineAlignment(ui::AlignLeft);
            addToMaxWidth(widget);
        }

        // Customize buttons for use in the popup. We will observe the button
        // state for highlighting and possibly close the popup when an action
        // gets triggered.
        if (ButtonWidget *b = maybeAs<ButtonWidget>(widget))
        {
            addToMaxWidth(widget);

            setButtonColors(*b);
            b->setSizePolicy(ui::Expand, ui::Expand);
            b->audienceForStateChange() += this;

            // Triggered actions close the menu.
            if (item.semantics().testFlag(ui::Item::ActivationClosesPopup))
            {
                b->audienceForTriggered() += this;
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        if (ButtonWidget *b = maybeAs<ButtonWidget>(widget))
        {
            if (!is<ToggleWidget>(b))
            {
                b->setTextGap("dialog.gap");
                b->setOverrideImageSize(style().fonts().font("default").height());
            }
        }

        if (item.semantics().testFlag(ui::Item::Annotation))
        {
            if (!App::config().getb(VAR_SHOW_ANNOTATIONS))
            {
                widget.hide();
            }

            widget.margins().set("halfunit").setLeft("popup.menu.margin");
            widget.setFont("separator.annotation");
        }
        else if (item.semantics().testFlag(ui::Item::Separator))
        {
            // The label of a separator may change.
            if (item.label().isEmpty())
            {
                widget.margins().set("");
                widget.setFont("separator.empty");
                widget.as<LabelWidget>().setOverlayImage(nullptr);
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

    void setButtonColors(ButtonWidget &button)
    {
        /*const bool hovering = (hover == &button);
        button.setTextColor(!hovering ^ (colorTheme == Inverted)? "text" : "inverted.text");
        button.setHoverTextColor(!hovering ^ (colorTheme == Inverted)? "inverted.text" : "text",
                                 ButtonWidget::ReplaceColor);*/

        button.setTextColor     (colorTheme == Normal? "text"          : "inverted.text" );
        button.setHoverTextColor(colorTheme == Normal? "inverted.text" : "text",         ButtonWidget::ReplaceColor);
    }

    void updateItemHitRules()
    {
        const GridLayout &layout = self().menu().layout();
        AutoRef<Rule const> halfUnit = self().rule("halfunit");

        for (GuiWidget *widget : self().menu().childWidgets())
        {
            if (self().menu().isWidgetPartOfMenu(*widget))
            {
                Vec2i cell = layout.widgetPos(*widget);
                DE_ASSERT(cell.x >= 0 && cell.y >= 0);

                // We want items to be hittable throughout the width of the menu, however
                // restrict this to the item's column if there are multiple columns.
                widget->hitRule()
                        .setInput(Rule::Left,  (!cell.x?
                                                    self().rule().left()
                                                  : layout.columnLeft(cell.x)) + halfUnit)
                        .setInput(Rule::Right, ((cell.x == layout.gridSize().x - 1)?
                                                    self().rule().right()
                                                  : layout.columnRight(cell.x)) - halfUnit);
            }
        }
    }

    bool hasButtonsWithImages() const
    {
        for (GuiWidget *child : self().menu().childWidgets())
        {
            if (ButtonWidget *button = maybeAs<ButtonWidget>(child))
            {
                // Menu item images are expected to be on the left side.
                if (button->hasImage() && button->textAlignment() == ui::AlignRight)
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
        const bool useExtraPadding = hasButtonsWithImages();

        const auto &padding = rule("popup.menu.paddedmargin");
        const auto &none    = rule("popup.menu.margin");

        for (GuiWidget *widget : self().menu().childWidgets())
        {
            // Pad annotations with the full amount.
            if (LabelWidget *label = maybeAs<LabelWidget>(widget))
            {
                const ui::Item *item = self().menu().organizer().findItemForWidget(*widget);
                if (item->semantics().testFlag(ui::Item::Annotation))
                {
                    if (useExtraPadding)
                    {
                        label->setMaximumTextWidth(*maxItemWidth - padding);
                        widget->margins().setLeft(padding);
                    }
                    else
                    {
                        label->setMaximumTextWidth(*maxItemWidth);
                        widget->margins().setLeft(none);
                    }
                }
            }

            // Pad buttons according to their image size.
            if (ButtonWidget *button = maybeAs<ButtonWidget>(widget))
            {
                updateImageColor(*button);
                if (useExtraPadding)
                {
                    const Rule *padRule = holdRef(padding);
                    if (button->hasImage() && button->textAlignment() == ui::AlignRight)
                    {
                        LabelWidget::ContentLayout layout;
                        button->contentLayout(layout);
                        sumInto(padRule, -Const(layout.image.width()) -
                                rule(button->textGap()));
                    }
                    widget->margins().setLeft(*padRule);
                    releaseRef(padRule);
                }
                else
                {
                    widget->margins().setLeft(none);
                }
            }
        }
    }

    void updateImageColor(ButtonWidget &button, bool invert = false)
    {
        button.setImageColor(style().colors().colorf(invert ^ (colorTheme == Inverted)? "inverted.text"
                                                                                      : "text"));
    }

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state)
    {
        updateImageColor(button, state != ButtonWidget::Up);

        // Position item highlight.
        if (&button == hover && state == ButtonWidget::Up)
        {
            hover = nullptr;
            self().requestGeometry();
            return;
        }

        if (state == ButtonWidget::Hover || state == ButtonWidget::Down)
        {
            hover = &button;
            self().requestGeometry();
        }
    }

    Rectanglei highlightRect() const
    {
        Rectanglei hi;
        if (hover)
        {
            hi.topLeft.x     = hover->hitRule().left().valuei();
            hi.topLeft.y     = hover->hitRule().top().valuei();
            hi.bottomRight.x = hover->hitRule().right().valuei();
            hi.bottomRight.y = hover->hitRule().bottom().valuei();
        }
        // Clip the highlight to the main popup area.
        return hi & self().rule().recti();
    }

    void buttonActionTriggered(ButtonWidget &)
    {
        // The popup menu is closed when an action is triggered.
        self().close();

        if (parentPopup) parentPopup->close();
    }

    void updateIfScrolled()
    {
        // If the menu is scrolled, we need to update some things.
        int scrollY = self().menu().scrollPositionY().valuei();
        if (scrollY == oldScrollY)
        {
            return;
        }
        oldScrollY = scrollY;

        //qDebug() << "menu scrolling" << scrollY;

        // Resend the mouse position so the buttons realize they've moved.
        root().window().as<BaseWindow>().processLatestMousePosition(true);

        self().requestGeometry();
    }

    void variableValueChanged(Variable &, const Value &newValue)
    {
        bool changed = false;

        // Update widgets of annotation items.
        self().items().forAll([this, &newValue, &changed] (const ui::Item &item) {
            if (item.semantics().testFlag(ui::Item::Annotation)) {
                self().menu().itemWidget<GuiWidget>(item).show(newValue.isTrue());
                changed = true;
            }
            return LoopContinue;
        });

        if (changed)
        {
            self().menu().updateLayout();
        }
    }

    void updateButtonColors()
    {
        for (GuiWidget *w : self().menu().childWidgets())
        {
            if (ButtonWidget *btn = maybeAs<ButtonWidget>(w))
            {
                setButtonColors(*btn);
            }
        }
    }

    void updateLayout()
    {
        auto &menu = self().menu();

        menu.updateLayout();
        menu.rule().setInput(
            Rule::Height,
            OperatorRule::minimum(menu.rule().inputRule(Rule::Height),
                                  root().viewHeight() - self().margins().height()));
        updateItemHitRules();
        updateItemMargins();
    }
};

PopupMenuWidget::PopupMenuWidget(const String &name)
    : PopupWidget(name), d(new Impl(this))
{
    setContent(new MenuWidget(name.isEmpty()? "" : name + "-content"));
    setOutlineColor("popup.outline");

    menu().setGridSize(1, ui::Expand, 0, ui::Expand);

    menu().organizer().audienceForWidgetCreation() += d;
    menu().organizer().audienceForWidgetUpdate() += d;
}

void PopupMenuWidget::setParentPopup(PopupWidget *parentPopup)
{
    // The parent will be closed, too, if the submenu is closed due to activation.
    d->parentPopup.reset(parentPopup);
}

PopupWidget *PopupMenuWidget::parentPopup() const
{
    return d->parentPopup;
}

MenuWidget &PopupMenuWidget::menu() const
{
    return static_cast<MenuWidget &>(content());
}

void PopupMenuWidget::useInfoStyle(bool yes)
{
    setColorTheme(yes? Inverted : Normal);
}

void PopupMenuWidget::setColorTheme(ColorTheme theme)
{
    PopupWidget::setColorTheme(theme);
    d->colorTheme = theme;
    d->updateButtonColors();
}

void PopupMenuWidget::offerFocus()
{
    menu().offerFocus();
}

void PopupMenuWidget::update()
{
    PopupWidget::update();
    d->updateIfScrolled();
}

void PopupMenuWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    PopupWidget::glMakeGeometry(verts);

    if (d->hover && d->hover->isEnabled())
    {
        verts.makeQuad(d->highlightRect(),
                       d->hover->state() == ButtonWidget::Hover?
                           style().colors().colorf(d->colorTheme == Normal? "inverted.background" : "background") :
                           style().colors().colorf(d->colorTheme == Normal? "accent" : "inverted.accent"),
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());
    }
}

void PopupMenuWidget::preparePanelForOpening()
{
    d->updateLayout();
    PopupWidget::preparePanelForOpening();
}

void PopupMenuWidget::panelClosing()
{
    PopupWidget::panelClosing();

    if (d->hover)
    {
        auto &btn = *const_cast<ButtonWidget *>(d->hover);
        d->hover = nullptr;
        btn.setState(ButtonWidget::Up);
        //d->setButtonColors(btn);
        d->updateImageColor(btn);
        //btn.setImageColor(style().colors().colorf(!d->infoStyle? "text" : "inverted.text"));
        requestGeometry();
    }

    menu().dismissPopups();
}

void PopupMenuWidget::updateStyle()
{
    PopupWidget::updateStyle();
    for (ui::DataPos i = 0; i < menu().items().size(); ++i)
    {
        // Force update of the item widgets.
        menu().items().at(i).notifyChange();
    }
    d->updateLayout();
}

} // namespace de
