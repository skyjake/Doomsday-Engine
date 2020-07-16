/** @file optionspage.h  Page for game options.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef OPTIONSPAGE_H
#define OPTIONSPAGE_H

#include <de/guiwidget.h>
#include <de/record.h>

class OptionsPage : public de::GuiWidget
{
public:
    explicit OptionsPage();

    void updateWithGameState(const de::Record &gameState);

    DE_AUDIENCE(Commands, void commandsSubmitted(const de::StringList &commands))

private:
    DE_PRIVATE(d)
};

#endif // OPTIONSPAGE_H
