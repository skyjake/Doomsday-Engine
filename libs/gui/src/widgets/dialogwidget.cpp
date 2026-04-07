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

#include "de/dialogwidget.h"
#include "de/togglewidget.h"
#include "de/choicewidget.h"
#include "de/guirootwidget.h"
#include "de/dialogcontentstylist.h"
#include "de/ui/filtereddata.h"

#include <de/keyevent.h>
#include <de/mouseevent.h>
#include <de/untrapper.h>
#include <de/eventloop.h>

namespace de {

static constexpr TimeSpan FLASH_ANIM_SPAN = 750_ms;

/**
 * Compares dialog button items to determine the order in which they
 * should appear in the UI.
 *
 * @param a  Dialog button item.
 * @param b  Dialog button item.
 *
 * @return @c true, if a < b.
 */
static bool dialogButtonOrder(const ui::Item &a, const ui::Item &b)
{
    const DialogButtonItem &left  = a.as<DialogButtonItem>();
    const DialogButtonItem &right = b.as<DialogButtonItem>();

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

DE_GUI_PIMPL(DialogWidget)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DE_OBSERVES(ui::Data, Addition)
, DE_OBSERVES(ui::Data, Removal)
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
    EventLoop subloop;
    de::Action *acceptAction;
    Animation glow;
    bool needButtonUpdate;
    float normalGlow;
    bool animatingGlow;
    std::unique_ptr<Untrapper> untrapper;
    DialogContentStylist stylist;
    IndirectRule *minWidth;
    const Rule *maxContentHeight = nullptr;

    Impl(Public *i, const Flags &dialogFlags)
        : Base(i)
        , modality(Modal)
        , flags(dialogFlags)
        , heading(nullptr)
        , acceptAction(nullptr)
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
        mainButtonItems.setFilter([] (const ui::Item &it)
        {
            if (!is<DialogButtonItem>(it)) return false;
            // Non-Action buttons only.
            return !it.as<DialogButtonItem>().role().testFlag(Action);
        });
        extraButtonItems.setFilter([] (const ui::Item &it)
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
            heading = new LabelWidget("heading");
            heading->setFont("heading");
            heading->margins()
                    .setBottom("")
                    .setTop (rule("gap") + rule("dialog.gap"))
                    .setLeft(rule("gap") + rule("dialog.gap"));
            heading->setSizePolicy(ui::Expand, ui::Expand);
            heading->setTextColor("accent");
            heading->setImageColor(style().colors().colorf("accent"));
            heading->setOverrideImageSize(heading->font().ascent());
            heading->setTextGap("dialog.gap");
            heading->setAlignment(ui::AlignLeft);
            heading->setTextAlignment(ui::AlignRight);
            heading->setTextLineAlignment(ui::AlignLeft);
            heading->setFillMode(LabelWidget::FillWithText);
            container->add(heading);

            heading->rule()
                    .setInput(Rule::Top,  self().rule().top())
                    .setInput(Rule::Left, self().rule().left());

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
        const Rule *maxHeight = holdRef(root().viewHeight());
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

    void dataItemAdded(ui::Data::Pos, const ui::Item &)
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

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        // Make sure all label-based widgets in the button area
        // manage their own size.
        if (LabelWidget *lab = maybeAs<LabelWidget>(widget))
        {
            lab->setSizePolicy(ui::Expand, ui::Expand);
        }

        // Apply dialog button specific roles.
        if (const ButtonItem *i = maybeAs<ButtonItem>(item))
        {
            ButtonWidget &but = widget.as<ButtonWidget>();
            but.setColorTheme(self().colorTheme());
            if (!i->action())
            {
                if (i->role() & (Accept | Yes))
                {
                    but.setActionFn([this]() { self().accept(); });
                }
                else if (i->role() & (Reject | No))
                {
                    but.setActionFn([this]() { self().reject(); });
                }
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        if (const ButtonItem *i = maybeAs<ButtonItem>(item))
        {
            ButtonWidget &but = widget.as<ButtonWidget>();

            // Button images must be a certain size.
            but.setOverrideImageSize(style().fonts().font("default").height());

            // Normal buttons should not be too small.
            if (~i->role() & Action)
            {
                but.setMinimumWidth(rule("dialog.button.minwidth"));
            }

            // Set default label?
            if (item.label().isEmpty())
            {
                if (i->role().testFlag(Accept))
                {
                    but.setText("OK");
                }
                else if (i->role().testFlag(Reject))
                {
                    but.setText("Cancel");
                }
                else if (i->role().testFlag(Yes))
                {
                    but.setText("Yes");
                }
                else if (i->role().testFlag(No))
                {
                    but.setText("No");
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

    const ui::ActionItem *findDefaultAction() const
    {
        // Note: extra buttons not searched because they shouldn't contain default actions.

        for (ui::Data::Pos i = 0; i < mainButtonItems.size(); ++i)
        {
            const ButtonItem *act = maybeAs<ButtonItem>(mainButtonItems.at(i));
            if (act->role().testFlag(Default) &&
                buttons->organizer().itemWidget(i)->isEnabled())
            {
                return act;
            }
        }
        return nullptr;
    }

    ButtonWidget *findDefaultButton() const
    {
        if (const ui::ActionItem *defaultAction = findDefaultAction())
        {
            return &buttonWidget(*defaultAction);
        }
        return nullptr;
    }

    ButtonWidget &buttonWidget(const ui::Item &item) const
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
            if (auto *blur = style().sharedBlurWidget())
            {
                bg.type = Background::SharedBlurWithBorderGlow;
                bg.blur = blur;
            }
            else
            {
                bg.type = Background::BlurredWithBorderGlow;
                bg.blur = nullptr;
            }
            bg.solidFill = Vec4f(0, 0, 0, .65f);
        }
        else
        {
            bg.type = Background::BorderGlow;
            bg.solidFill = style().colors().colorf("dialog.background");
        }
        self().set(bg);
    }

    DE_PIMPL_AUDIENCES(Accept, Reject)
};

DE_AUDIENCE_METHODS(DialogWidget, Accept, Reject)

DialogWidget::DialogWidget(const String &name, const Flags &flags)
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
    DE_ASSERT(d->heading != nullptr);
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

void DialogWidget::setMinimumContentWidth(const Rule &minWidth)
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

ButtonWidget &DialogWidget::buttonWidget(const String &label) const
{
    GuiWidget *w = d->buttons->organizer().itemWidget(label);
    if (w) return w->as<ButtonWidget>();

    w = d->extraButtons->organizer().itemWidget(label);
    if (w) return w->as<ButtonWidget>();

    throw UndefinedLabel("DialogWidget::buttonWidget", "Undefined label \"" + label + "\"");
}

PopupButtonWidget &DialogWidget::popupButtonWidget(const String &label) const
{
    return buttonWidget(label).as<PopupButtonWidget>();
}

ButtonWidget *DialogWidget::buttonWidget(int roleId) const
{
    for (uint i = 0; i < d->buttonItems.size(); ++i)
    {
        const DialogButtonItem &item = d->buttonItems.at(i).as<DialogButtonItem>();

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

List<ButtonWidget *> DialogWidget::buttonWidgets() const
{
    List<ButtonWidget *> buttons;
    for (GuiWidget *w : d->buttons->childWidgets())
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
    DE_ASSERT(!hasRoot());
    root.add(this);

    prepare();

    int result = 0;
    try
    {
#if defined (DE_MOBILE)
        // The subloop will likely access the root independently.
        root.unlock();
#endif

        result = d->subloop.exec();

#if defined (DE_MOBILE)
        root.lock();
#endif
    }
    catch (...)
    {
#if defined (DE_MOBILE)
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

    DE_ASSERT(hasRoot());
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

bool DialogWidget::handleEvent(const Event &event)
{
    if (!isOpen()) return false;

    if (event.isKeyDown())
    {
        const KeyEvent &key = event.as<KeyEvent>();

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
        DE_ASSERT(d->modality == Modal);
        d->subloop.quit(result);
        DE_NOTIFY(Accept, i) { i->dialogAccepted(*this, result); }
    }
    else
    {
        DE_NOTIFY(Accept, i) { i->dialogAccepted(*this, result); }
        finish(result);
    }
}

void DialogWidget::reject(int result)
{
    if (d->subloop.isRunning())
    {
        DE_ASSERT(d->modality == Modal);
        d->subloop.quit(result);
        DE_NOTIFY(Reject, i) { i->dialogRejected(*this, result); }
    }
    else
    {
        DE_NOTIFY(Reject, i) { i->dialogRejected(*this, result); }
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

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, const String &label)
    : ui::ActionItem(itemSemantics(flags), label, RefArg<de::Action>())
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, const Image &image)
    : ui::ActionItem(itemSemantics(flags), image)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                 flags,
                                     const String &            label,
                                     const RefArg<de::Action> &action)
    : ui::ActionItem(itemSemantics(flags), label, action)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                    flags,
                                     const String &               label,
                                     const std::function<void()> &action)
    : ui::ActionItem(itemSemantics(flags), label, new CallbackAction(action))
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                 flags,
                                     const Image &             image,
                                     const RefArg<de::Action> &action)
    : ui::ActionItem(itemSemantics(flags), image, "", action)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                 flags,
                                     const Image &             image,
                                     const String &            label,
                                     const RefArg<de::Action> &action)
    : ui::ActionItem(itemSemantics(flags), image, label, action)
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                    flags,
                                     const Image &                image,
                                     const String &               label,
                                     const std::function<void()> &action)
    : ui::ActionItem(itemSemantics(flags), image, label, new CallbackAction(action))
    , _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags                    flags,
                                     const Image &                image,
                                     const std::function<void()> &action)
    : ui::ActionItem(itemSemantics(flags), image, "", new CallbackAction(action))
    , _role(flags)
{}

ui::Item::Semantics DialogWidget::ButtonItem::itemSemantics(RoleFlags flags)
{
    Semantics smt = ActivationClosesPopup | ShownAsButton;
    if (flags & Popup) smt |= ShownAsPopupButton;
    return smt;
}

} // namespace de
