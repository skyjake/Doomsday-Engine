/** @file persistentdata.h  Data that persists even after restarting the app.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef PERSISTENTDATA_H
#define PERSISTENTDATA_H

#include <de/String>

/**
 * Data that persists even after restarting the app.
 *
 * A singleton class.
 */
class PersistentData
{
public:
    PersistentData();
    ~PersistentData();

    static void set(de::String const &name, de::String const &value);
    static void set(de::String const &name, int value);

    static de::String get(de::String const &name, de::String const &defaultValue = "");
    static int geti(de::String const &name, int defaultValue = 0);
};

#endif // PERSISTENTDATA_H
