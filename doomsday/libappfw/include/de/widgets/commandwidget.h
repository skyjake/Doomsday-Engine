/** @file commandwidget.h  Abstract command line based widget.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_COMMANDWIDGET_H
#define LIBAPPFW_COMMANDWIDGET_H

#include "../LineEditWidget"

namespace de {

class PopupWidget;

/**
 * Base class for text editors with a history buffer. Entered text is interpreted
 * as commands. Supports a Lexicon and a popup for autocompletion.
 *
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC CommandWidget : public LineEditWidget
{
    Q_OBJECT

public:
    CommandWidget(String const &name = "");

    PopupWidget &autocompletionPopup();

    // Events.
    void focusGained();
    void focusLost();
    bool handleEvent(Event const &event);

public slots:
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
    virtual bool isAcceptedAsCommand(String const &text) = 0;

    virtual void executeCommand(String const &text) = 0;

    /**
     * Shows the popup with a list of possible completions.
     *
     * @param completionsText  Styled content for the popup.
     */
    void showAutocompletionPopup(String const &completionsText);

    void autoCompletionEnded(bool accepted);

signals:
    void gotFocus();
    void lostFocus();
    void commandEntered(de::String const &command);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_COMMANDWIDGET_H
