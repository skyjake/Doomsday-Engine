/** @file consoleeffect.h
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

#ifndef DENG_CLIENT_CONSOLEEFFECT_H
#define DENG_CLIENT_CONSOLEEFFECT_H

#include <de/libdeng2.h>
#include <de/Rectangle>

/**
 * Draws camera lens effects for a particular player console. Maintains
 * console-specific state and carries out the actual GL operations.
 */
class ConsoleEffect
{
public:
    ConsoleEffect(int console);

    virtual ~ConsoleEffect();

    /**
     * Returns the console number.
     */
    int console() const;

    virtual void draw(de::Rectanglei const &viewRect) = 0;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_CONSOLEEFFECT_H
