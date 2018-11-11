/** @file widgets/dialogwidget.cpp  Popup dialog.
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

#include "de/DialogWidget"
#include "de/ToggleWidget"
#include "de/ChoiceWidget"
#include "de/GuiRootWidget"
#include "de/SignalAction"
#include "de/DialogContentStylist"
#include "de/ui/FilteredData"

#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/Untrapper>
#include <QEventLoop>

namespace de {

static TimeSpan const FLASH_ANIM_SPAN = 0.75;

/**
 * Compares dialog button items to determine the order in which they
 * should appear in the UI.
 *
 * @param a  Dialog button item.
 * @param b  Dialog button item.
 *
 * @return @c true, if a < b.
 */
static bool dialogButtonOrder(ui::Item const &a, ui::Item const &b)
{
    DialogButtonItem const &left  = a.as<DialogButtonItem>();
    DialogButtonItem const &right = b.as<DialogButtonItem>();

    if (!left.role().testFlag(DialogWidget::Default) && right.role().testFlag(DialogWidget::Default))
    {
#ifdef MACOSX
        // Default buttons go to the right on macOS.
        return true;
#else
        // Default buttons to the left.
        return false;
#endif
    }
    if (left.role().testFlag(DialogWidget::Default) && !right.role().testFlag(DialogWidget::Default))
    {
#ifdef MACOSX
        // Default buttons go to the right on macOS.
        return false;
#else
        // Default buttons to the left.
        return true;
#endif
    }
    if (a.label().isEmpty() && !b.label().isEmpty())
    {
        // Label-less buttons go first.
        return true;
    }

    // Order unchanged.
    return false;
}

DENG_GUI_PIMPL(DialogWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DENG2_OBSERVES(ui::Data, Addition)
, DENG2_OBSERVES(ui::Data, Removal)
{
    Modality modality;
    Flags flags;
    ScrollAreaWidget *area = nullptr;
    ScrollAreaWidget *rightArea = nullptr;
    LabelWidget *heading;
    MenuWidget *buttons;
    MenuWidget *extraButtons;
    ui::ListData buttonItems;
    ui::FilteredData mainButtonItems  { buttonItems };
    ui::FilteredData extraButtonItems { buttonItems };
    QEventLoop subloop;
    de::Action *acceptAction;
    Animation glow;
    bool needButtonUpdate;
    float normalGlow;
    bool animatingGlow;
    QScopedPointer<Untrapper> untrapper;
    DialogContentStylist stylist;
    IndirectRule *minWidth;
    Rule const *maxContentHeight = nullptr;

    Impl(Public *i, Flags const &dialogFlags)
        : Base(i)
        , modality(Modal)
        , flags(dialogFlags)
        , heading(0)
        , acceptAction(0)
        , needButtonUpdate(false)
        , animatingGlow(false)
    {
        minWidth = new IndirectRule;

        // Initialize the border glow.
        normalGlow = style().colors().colorf("glow").w;
        glow.setValue(normalGlow);
        glow.setStyle(Animation::Linear);

        // Set up widget structure.
        GuiWidget *container = new GuiWidget("container");

        area = new ScrollAreaWidget("area");

        buttons = new MenuWidget("buttons");
        buttons->margins().setTop("");
        buttons->setItems(mainButtonItems);
        buttons->organizer().audienceForWidgetCreation() += this;
        buttons->organizer().audienceForWidgetUpdate() += this;

        extraButtons = new MenuWidget("extra");
        extraButtons->margins().setTop("");
        extraButtons->setItems(extraButtonItems);
        extraButtons->organizer().audienceForWidgetCreation() += this;
        extraButtons->organizer().audienceForWidgetUpdate() += this;

        buttonItems.audienceForAddition() += this;
        buttonItems.audienceForRemoval()  += this;

        // Segregate Action buttons into the extra buttons set.
        mainButtonItems.setFilter([] (ui::Item const &it)
        {
            if (!is<DialogButtonItem>(it)) return false;
            // Non-Action buttons only.
            return !it.as<DialogButtonItem>().role().testFlag(Action);
        });
        extraButtonItems.setFilter([] (ui::Item const &it)
        {
            if (!is<DialogButtonItem>(it)) return false;
            // Only Action buttons allowed.
            return it.as<DialogButtonItem>().role().testFlag(Action);
        });

        // The menu maintains its own width and height based on children.
        // Set up one row with variable number of columns.
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand);
        extraButtons->setGridSize(0, ui::Expand, 1, ui::Expand);

        area->rule()
                .setInput(Rule::Left,  self().rule().left())
                .setInput(Rule::Top,   self().rule().top())
                .setInput(Rule::Width, area->contentRule().width() + area->margins().width());

        // Will a title be included?
        if (flags & WithHeading)
        {
            heading = new LabelWidget;
            heading->setFont("heading");
            heading->margins()
                    .setBottom("")
                    .setTop (rule("gap") + rule("dialog.gap"))
                    .setLeft(rule("gap") + rule("dialog.gap"));
            heading->setSizePolicy(ui::Filled, ui::Expand);
            heading->setTextColor("accent");
            heading->setImageColor(style().colors().colorf("accent"));
            heading->setOverrideImageSize(heading->font().ascent().valuei());
            heading->setTextGap("dialog.gap");
            heading->setTextAlignment(ui::AlignRight);
            heading->setTextLineAlignment(ui::AlignLeft);
            heading->setFillMode(LabelWidget::FillWithText);
            container->add(heading);

            heading->rule()
                    .setInput(Rule::Top,   self().rule().top())
                    .setInput(Rule::Left,  self().rule().left())
                    .setInput(Rule::Right, area->rule().right());

            area->rule().setInput(Rule::Top, heading->rule().bottom());
        }

        area->rule().setInput(Rule::Height, container->rule().height() - buttons->rule().height());

        // Buttons below the area.
        buttons->rule()
                .setInput(Rule::Bottom, container->rule().bottom())
                .setInput(Rule::Right, self().rule().right());
        extraButtons->rule()
                .setInput(Rule::Top, buttons->rule().top())
                .setInput(Rule::Left, self().rule().left());

        // A blank container widget acts as the popup content parent.
        container->rule().setInput(Rule::Width, OperatorRule::maximum(
                                       area->rule().width(),
                                       buttons->rule().width() + extraButtons->rule().width(),
                                       *minWidth));

        if (flags.testFlag(WithHeading))
        {
            area->rule().setInput(Rule::Height,
                                  container->rule().height()
                                  - heading->rule().height()
                                  - buttons->rule().height());
        }

        container->add(area);
        container->add(extraButtons);
        container->add(buttons);
        self().setContent(container);
    }

    ~Impl()
    {
        releaseRef(minWidth);
        releaseRef(maxContentHeight);
        releaseRef(acceptAction);
    }

    void setupForTwoColumns()
    {
        // Create an additional content area.
        if (!rightArea)
        {
            rightArea = new ScrollAreaWidget("rightArea");
            self().content().add(rightArea);

            rightArea->rule()
                    .setInput(Rule::Top,    area->rule().top())
                    .setInput(Rule::Left,   area->rule().right())
                    .setInput(Rule::Height, area->rule().height())
                    .setInput(Rule::Width,  rightArea->contentRule().width() + rightArea->margins().width());

            if (heading)
            {
                heading->rule().setInput(Rule::Right, rightArea->rule().right());
            }

            // Content size is now wider.
            self().content().rule().setInput(Rule::Width, OperatorRule::maximum(
                                           area->rule().width() + rightArea->rule().width(),
                                           buttons->rule().width() + extraButtons->rule().width(),
                                           *minWidth));

            if (self().isOpen()) updateContentHeight();
        }
    }

    void updateContentHeight()
    {
        // Determine suitable maximum height.
        Rule const *maxHeight = holdRef(root().viewHeight());
        if (self().openingDirection() == ui::Down)
        {
            changeRef(maxHeight, *maxHeight - self().anchor().top() - rule("gap"));
        }
        if (maxContentHeight)
        {
            changeRef(maxHeight, OperatorRule::minimum(*maxHeight, *maxContentHeight));
        }

        // Scrollable area content height.
        AutoRef<Rule> areaContentHeight = area->contentRule().height() + area->margins().height();
        if (rightArea)
        {
            areaContentHeight.reset(OperatorRule::maximum(areaContentHeight,
                    rightArea->contentRule().height() + rightArea->margins().height()));
        }

        // The container's height is limited by the height of the view. Normally
        // the dialog tries to show the full height of the content area.
        if (!flags.testFlag(WithHeading))
        {
            self().content().rule().setInput(Rule::Height,
                    OperatorRule::minimum(*maxHeight, areaContentHeight + buttons->rule().height()));
        }
        else
        {
            self().content().rule().setInput(Rule::Height,
                    OperatorRule::minimum(*maxHeight, (heading? heading->rule().height() : Const(0)) +
                                                       areaContentHeight + buttons->rule().height()));
        }

        releaseRef(maxHeight);
    }

    void dataItemAdded(ui::Data::Pos, ui::Item const &)
    {
        needButtonUpdate = true;
    }

    void dataItemRemoved(ui::Data::Pos, ui::Item &)
    {
        needButtonUpdate = true;
    }

    void updateButtonLayout()
    {
        buttonItems.stableSort(dialogButtonOrder);
        needButtonUpdate = false;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        // Make sure all label-based widgets in the button area
        // manage their own size.
        if (LabelWidget *lab = maybeAs<LabelWidget>(widget))
        {
            lab->setSizePolicy(ui::Expand, ui::Expand);
        }

        // Apply dialog button specific roles.
        if (ButtonItem const *i = maybeAs<ButtonItem>(item))
        {
            ButtonWidget &but = widget.as<ButtonWidget>();
            but.setColorTheme(self().colorTheme());
            if (!i->action())
            {
                if (i->role() & (Accept | Yes))
                {
                    but.setAction(new SignalAction(thisPublic, SLOT(accept())));
                }
                else if (i->role() & (Reject | No))
                {
                    but.setAction(new SignalAction(thisPublic, SLOT(reject())));
                }
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if (ButtonItem const *i = maybeAs<ButtonItem>(item))
        {
            ButtonWidget &but = widget.as<ButtonWidget>();

            // Button images must be a certain size.
            but.setOverrideImageSize(style().fonts().font("default").height().valuei());

            // Set default label?
            if (item.label().isEmpty())
            {
                if (i->role().testFlag(Accept))
                {
                    but.setText(tr("OK"));
                }
                else if (i->role().testFlag(Reject))
                {
                    but.setText(tr("Cancel"));
                }
                else if (i->role().testFlag(Yes))
                {
                    but.setText(tr("Yes"));
                }
                else if (i->role().testFlag(No))
                {
                    but.setText(tr("No"));
                }
            }

            // Highlight the default button(s).
            if (i->role().testFlag(Default))
            {
                but.setTextColor(self().colorTheme() == Normal? "dialog.default" : "inverted.text");
                if (self().colorTheme() == Normal)
                {
                    but.setHoverTextColor("dialog.default", ButtonWidget::ReplaceColor);
                }
                but.setText(_E(b) + but.text());
            }
            else
            {
                but.setTextColor(self().colorTheme() == Normal? "text" : "inverted.text");
            }
        }
    }

    ui::ActionItem const *findDefaultAction() const
    {
        // Note: extra buttons not searched because they shouldn't contain default actions.

        for (ui::Data::Pos i = 0; i < mainButtonItems.size(); ++i)
        {
            ButtonItem const *act = maybeAs<ButtonItem>(mainButtonItems.at(i));
            if (act->role().testFlag(Default) &&
                buttons->organizer().itemWidget(i)->isEnabled())
            {
                return act;
            }
        }
        return 0;
    }

    ButtonWidget *findDefaultButton() const
    {
        if (ui::ActionItem const *defaultAction = findDefaultAction())
        {
            return &buttonWidget(*defaultAction);
        }
        return nullptr;
    }

    ButtonWidget &buttonWidget(ui::Item const &item) const
    {
        GuiWidget *w = extraButtons->organizer().itemWidget(item);
        if (w) return w->as<ButtonWidget>();
        // Try the normal buttons.
        return buttons->organizer().itemWidget(item)->as<ButtonWidget>();
    }

    void startBorderFlash()
    {
        animatingGlow = true;
        glow.setValueFrom(1, normalGlow, FLASH_ANIM_SPAN);
        Background bg = self().background();
        bg.color.w = glow;
        self().set(bg);
    }

    void updateBorderFlash()
    {
        Background bg = self().background();
        bg.color.w = glow;
        self().set(bg);

        if (glow.done()) animatingGlow = false;
    }

    void updateBackground()
    {
        Background bg = self().background();
        if (self().isUsingInfoStyle())
        {
            bg = self().infoStyleBackground();
        }
        else if (style().isBlurringAllowed())
        {
            /// @todo Should use the Style for this.
            bg.type = Background::SharedBlurWithBorderGlow;
            bg.blur = style().sharedBlurWidget();
            bg.solidFill = Vector4f(0, 0, 0, .65f);
        }
        else
        {
            bg.type = Background::BorderGlow;
            bg.solidFill = style().colors().colorf("dialog.background");
        }
        self().set(bg);
    }
};

DialogWidget::DialogWidget(String const &name, Flags const &flags)
    : PopupWidget(name), d(new Impl(this, flags))
{
    d->stylist.setContainer(area());
    setOpeningDirection(ui::NoDirection);
    d->updateBackground();
    area().enableIndicatorDraw(true);
}

DialogWidget::Modality DialogWidget::modality() const
{
    return d->modality;
}

LabelWidget &DialogWidget::heading()
{
    DENG2_ASSERT(d->heading != 0);
    return *d->heading;
}

ScrollAreaWidget &DialogWidget::area()
{
    return *d->area;
}

ScrollAreaWidget &DialogWidget::leftArea()
{
    d->setupForTwoColumns();
    return *d->area;
}

ScrollAreaWidget &DialogWidget::rightArea()
{
    d->setupForTwoColumns();
    return *d->rightArea;
}

void DialogWidget::setMinimumContentWidth(Rule const &minWidth)
{
    d->minWidth->setSource(minWidth);
}

void DialogWidget::setMaximumContentHeight(const de::Rule &maxHeight)
{
    changeRef(d->maxContentHeight, maxHeight);
}

MenuWidget &DialogWidget::buttonsMenu()
{
    return *d->buttons;
}

MenuWidget &DialogWidget::extraButtonsMenu()
{
    return *d->extraButtons;
}

ui::Data &DialogWidget::buttons()
{
    return d->buttonItems;
}

ButtonWidget &DialogWidget::buttonWidget(String const &label) const
{
    GuiWidget *w = d->buttons->organizer().itemWidget(label);
    if (w) return w->as<ButtonWidget>();

    w = d->extraButtons->organizer().itemWidget(label);
    if (w) return w->as<ButtonWidget>();

    throw UndefinedLabel("DialogWidget::buttonWidget", "Undefined label \"" + label + "\"");
}

PopupButtonWidget &DialogWidget::popupButtonWidget(String const &label) const
{
    return buttonWidget(label).as<PopupButtonWidget>();
}

ButtonWidget *DialogWidget::buttonWidget(int roleId) const
{
    for (uint i = 0; i < d->buttonItems.size(); ++i)
    {
        DialogButtonItem const &item = d->buttonItems.at(i).as<DialogButtonItem>();

        if ((item.role() & IdMask) == uint(roleId))
        {
            return &d->buttonWidget(item);
        }
    }
    return nullptr;
}

PopupButtonWidget *DialogWidget::popupButtonWidget(int roleId) const
{
    if (auto *btn = buttonWidget(roleId))
    {
        return &btn->as<PopupButtonWidget>();
    }
    return nullptr;
}

QList<ButtonWidget *> DialogWidget::buttonWidgets() const
{
    QList<ButtonWidget *> buttons;
    foreach (GuiWidget *w, d->buttons->childWidgets())
    {
        if (auto *but = maybeAs<ButtonWidget>(w))
        {
            buttons << but;
        }
    }
    return buttons;
}

void DialogWidget::setAcceptanceAction(RefArg<de::Action> action)
{
    changeRef(d->acceptAction, action);
}

Action *DialogWidget::acceptanceAction() const
{
    return d->acceptAction;
}

int DialogWidget::exec(GuiRootWidget &root)
{
    d->modality = Modal;

    // The widget is added to the root temporarily (as top child).
    DENG2_ASSERT(!hasRoot());
    root.add(this);

    prepare();

    int result = 0;
    try
    {
#if defined (DENG_MOBILE)
        // The subloop will likely access the root independently.
        root.unlock();
#endif

        result = d->subloop.exec();

#if defined (DENG_MOBILE)
        root.lock();
#endif
    }
    catch (...)
    {
#if defined (DENG_MOBILE)
        // The lock needs to be reacquired in any case.
        root.lock();
#endif
        throw;
    }

    finish(result);
    return result;
}

void DialogWidget::open()
{
    open(NonModal);
}

void DialogWidget::open(Modality modality)
{
    d->modality = modality;

    DENG2_ASSERT(hasRoot());
    prepare(); // calls base class's open()
}

ui::ActionItem *DialogWidget::defaultActionItem()
{
    return const_cast<ui::ActionItem *>(d->findDefaultAction());
}

void DialogWidget::offerFocus()
{
    root().setFocus(d->findDefaultButton());
}

void DialogWidget::update()
{
    PopupWidget::update();

    if (d->needButtonUpdate)
    {
        d->updateButtonLayout();
    }

    if (d->animatingGlow)
    {
        d->updateBorderFlash();
    }
}

bool DialogWidget::handleEvent(Event const &event)
{
    if (!isOpen()) return false;

    if (event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if (key.ddKey() == DDKEY_ENTER  ||
            key.ddKey() == DDKEY_RETURN ||
            key.ddKey() == ' ')
        {
            if (ButtonWidget *but = d->findDefaultButton())
            {
                but->trigger();
                return true;
            }
        }

        if (key.ddKey() == DDKEY_ESCAPE)
        {
            // Esc always cancels a dialog.
            reject();
            return true;
        }
    }

    if (d->modality == Modal)
    {
        // The event should already have been handled by the children.
        if ((event.isKeyDown() && !event.as<KeyEvent>().isModifier()) ||
            (event.type() == Event::MouseButton &&
             event.as<MouseEvent>().state() == MouseEvent::Pressed &&
             !hitTest(event)))
        {
            d->startBorderFlash();
        }
        return true;
    }
    else
    {
        if ((event.type() == Event::MouseButton || event.type() == Event::MousePosition ||
             event.type() == Event::MouseWheel) &&
           hitTest(event))
        {
            // Non-modal dialogs eat mouse clicks/position inside the dialog.
            return true;
        }
    }

    return PopupWidget::handleEvent(event);
}

void DialogWidget::accept(int result)
{
    if (d->subloop.isRunning())
    {
        DENG2_ASSERT(d->modality == Modal);
        d->subloop.exit(result);
        emit accepted(result);
    }
    else
    {
        emit accepted(result);
        finish(result);
    }
}

void DialogWidget::reject(int result)
{
    if (d->subloop.isRunning())
    {
        DENG2_ASSERT(d->modality == Modal);
        d->subloop.exit(result);
        emit rejected(result);
    }
    else
    {
        emit rejected(result);
        finish(result);
    }
}

void DialogWidget::prepare()
{
    // Mouse needs to be untrapped for the user to be access the dialog.
    d->untrapper.reset(new Untrapper(root().window()));

    if (openingDirection() == ui::NoDirection)
    {
        // Center the dialog.
        setAnchor(root().viewWidth() / 2, root().viewHeight() / 2);
    }

    d->updateContentHeight();

    PopupWidget::open();
}

void DialogWidget::preparePanelForOpening()
{
    PopupWidget::preparePanelForOpening();

    // Redo the layout (items visible now).
    d->buttons->updateLayout();
    d->extraButtons->updateLayout();

    d->updateBackground();
}

void DialogWidget::finish(int result)
{
    root().setFocus(nullptr);
    close();

    d->untrapper.reset();

    if (result > 0)
    {
        // Success!
        if (d->acceptAction)
        {
            AutoRef<de::Action> held = *d->acceptAction;
            held->trigger();
        }
    }
}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label)
    : ui::ActionItem(itemSemantics(flags), label, 0)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, Image const &image)
    : ui::ActionItem(itemSemantics(flags), image)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label, RefArg<de::Action> action)
    : ui::ActionItem(itemSemantics(flags), label, action)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, Image const &image, RefArg<de::Action> action)
    : ui::ActionItem(itemSemantics(flags), image, "", action)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, Image const &image, String const &label, RefArg<de::Action> action)
    : ui::ActionItem(itemSemantics(flags), image, label, action)
    , _role(flags)
{}

ui::Item::Semantics DialogWidget::ButtonItem::itemSemantics(RoleFlags flags)
{
    Semantics smt = ActivationClosesPopup | ShownAsButton;
    if (flags & Popup) smt |= ShownAsPopupButton;
    return smt;
}

} // namespace de
