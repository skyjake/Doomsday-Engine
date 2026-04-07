/** @file commander.h  Controller for the Gloom viewer app.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GLOOMED_COMMANDER_H
#define GLOOMED_COMMANDER_H

#include <de/String>

/**
 * Sends commands to the Gloom viewer app and listens to beacon messages.
 */
class Commander
{
    DE_PRIVATE(d)

public:
    Commander();

    bool launch();
    void sendCommand(const de::String &command);

    bool isLaunched() const;
    bool isConnected() const;
};

#endif // GLOOMED_COMMANDER_H
