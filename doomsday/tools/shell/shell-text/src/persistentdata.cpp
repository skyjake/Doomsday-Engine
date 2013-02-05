/** @file persistentdata.cpp  Data that persists even after restarting the app.
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

#include "persistentdata.h"
#include <QSettings>

using namespace de;

PersistentData::PersistentData()
{}

PersistentData::~PersistentData()
{}

void PersistentData::set(String const &name, String const &value)
{
    QSettings st;
    st.setValue(name, value);
}

void PersistentData::set(String const &name, int value)
{
    QSettings st;
    st.setValue(name, value);
}

String PersistentData::get(String const &name, String const &defaultValue)
{
    QSettings st;
    return st.value(name, defaultValue).toString();
}

int PersistentData::geti(String const &name, int defaultValue)
{
    QSettings st;
    return st.value(name, defaultValue).toInt();
}
