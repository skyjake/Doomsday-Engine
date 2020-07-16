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

#ifndef LIBAPPFW_COMMANDWIDGET_H
#define LIBAPPFW_COMMANDWIDGET_H

#include "de/lineeditwidget.h"
#include "de/ipersistent.h"

namespace de {

class PopupWidget;

/**
 * Base class for text editors with a history buffer. Entered text is interpreted
 * as commands. Supports a Lexicon and a popup for autocompletion.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC CommandWidget : public LineEditWidget, public IPersistent
{
public:
    DE_AUDIENCE(GotFocus,  void gotFocus(CommandWidget &))
    DE_AUDIENCE(LostFocus, void lostFocus(CommandWidget &))
    DE_AUDIENCE(Command,   void commandEntered(const String &command))

public:
    CommandWidget(const String &name = {});

    PopupWidget &autocompletionPopup();

    // Events.
    void focusGained() override;
    void focusLost() override;
    bool handleEvent(const Event &event) override;
    void update() override;

    bool handleControlKey(term::Key key, const KeyModifiers &mods) override;

    // IPersistent.
    void operator>>(PersistentState &toState) const override;
    void operator<<(const PersistentState &fromState) override;

    /**
     * Moves the current contents of the command line to the history. The
     * command line contents are then cleared.
     */
    void dismissContentToHistory();

    void closeAutocompletionPopup();

protected:
    /**
     * Determines if the provided text is accepted as a command by the widget.
     * This gets called whenever the user presses Enter in the widget.
     *
     * @param text  Text to check.
     *
     * @return @c true, if the text is an executable command.
     */
    virtual bool isAcceptedAsCommand(const String &text) = 0;

    virtual void executeCommand(const String &text) = 0;

    /**
     * Shows the popup with a list of possible completions.
     *
     * @param completionsText  Styled content for the popup.
     */
    void showAutocompletionPopup(const String &completionsText);

    void autoCompletionEnded(bool accepted) override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_COMMANDWIDGET_H
