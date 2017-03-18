/** @file commandwidget.h  Abstract command line based widget.
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

#include "de/CommandWidget"
#include "de/DocumentPopupWidget"
#include "de/PersistentState"
#include "de/Style"

#include <de/shell/EditorHistory>
#include <de/KeyEvent>

namespace de {

DENG_GUI_PIMPL(CommandWidget)
{
    shell::EditorHistory history;
    DocumentPopupWidget *popup; ///< Popup for autocompletions.
    bool allowReshow;   ///< Contents must still be valid.

    Impl(Public *i) : Base(i), history(i), allowReshow(false)
    {
        // Popup for autocompletions.
        popup = new DocumentPopupWidget;
        popup->document().setMaximumLineWidth(640);
        popup->document().setScrollBarColor("inverted.accent");

        // Height for the content: depends on the document height (plus margins), but at
        // most 400; never extend outside the view, though.
        popup->setPreferredHeight(rule("editor.completion.height"),
                                  self().rule().top() - rule("gap"));

        self().add(popup);
    }
};

CommandWidget::CommandWidget(String const &name)
    : LineEditWidget(name), d(new Impl(this))
{}

PopupWidget &CommandWidget::autocompletionPopup()
{
    return *d->popup;
}

void CommandWidget::focusGained()
{
    LineEditWidget::focusGained();
    emit gotFocus();
}

void CommandWidget::focusLost()
{
    LineEditWidget::focusLost();

    // Get rid of the autocompletion popup.
    closeAutocompletionPopup();

    emit lostFocus();
}

bool CommandWidget::handleEvent(Event const &event)
{
    if (isDisabled()) return false;

    if (hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if (d->allowReshow &&
            isSuggestingCompletion() &&
            key.qtKey() == Qt::Key_Tab && !d->popup->isOpen() &&
            suggestedCompletions().size() > 1)
        {
            // The completion popup has been manually dismissed, but the editor is
            // still in autocompletion mode. Let's just reopen the popup with its
            // old content.
            d->popup->open();
            return true;
        }

        // Override the handling of the Enter key.
        if (key.qtKey() == Qt::Key_Return || key.qtKey() == Qt::Key_Enter)
        {
            if (isAcceptedAsCommand(text()))
            {
                // We must make sure that the ongoing autocompletion ends.
                acceptCompletion();

                String const entered = d->history.enter();
                executeCommand(entered);
                emit commandEntered(entered);
            }
            return true;
        }
    }

    if (LineEditWidget::handleEvent(event))
    {
        // Editor handled the event normally.
        return true;
    }

    if (hasFocus())
    {
        // All Tab keys are eaten by a focused command widget.
        if (event.isKey() && event.as<KeyEvent>().ddKey() == DDKEY_TAB)
        {
            return true;
        }
    }
    return false;
}

void CommandWidget::update()
{
    LineEditWidget::update();

    setAttribute(FocusCyclingDisabled, !text().isEmpty());
}

bool CommandWidget::handleControlKey(int qtKey, KeyModifiers const &mods)
{
    if (LineEditWidget::handleControlKey(qtKey, mods))
    {
        return true;
    }
    if (d->history.handleControlKey(qtKey))
    {
        return true;
    }
    return false;
}

void CommandWidget::operator >> (PersistentState &toState) const
{
    int const MAX_PERSISTENT_HISTORY = 200;
    toState.objectNamespace().set(name().concatenateMember("history"),
                                  new ArrayValue(d->history.fullHistory(MAX_PERSISTENT_HISTORY)));
}

void CommandWidget::operator << (PersistentState const &fromState)
{
    d->history.setFullHistory(fromState.objectNamespace()
                              .getStringList(name().concatenateMember("history")));
}

void CommandWidget::dismissContentToHistory()
{
    d->history.goToLatest();

    if (!text().isEmpty())
    {
        d->history.enter();
    }
}

void CommandWidget::closeAutocompletionPopup()
{
    d->popup->close();
    d->allowReshow = false;
}

void CommandWidget::showAutocompletionPopup(String const &completionsText)
{
    d->popup->document().setText(completionsText);
    d->popup->document().scrollToTop(0);

    d->popup->setAnchorX(cursorRect().middle().x);
    d->popup->setAnchorY(rule().top());
    d->popup->open();

    d->allowReshow = true;
}

void CommandWidget::autoCompletionEnded(bool accepted)
{
    LineEditWidget::autoCompletionEnded(accepted);
    closeAutocompletionPopup();
}

} // namespace de
