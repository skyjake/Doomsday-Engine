/** @file refuge.cpp  Persistent data storage.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/refuge.h"
#include "de/app.h"
#include "de/archive.h"
#include "de/archivefolder.h"
#include "de/reader.h"
#include "de/writer.h"

namespace de {

DE_PIMPL_NOREF(Refuge)
{
    String persistentPath;
    Record names;
};

Refuge::Refuge(const String &persistentPath) : d(new Impl)
{
    d->persistentPath = persistentPath;

    try
    {
        read();
    }
    catch (const Error &er)
    {
        LOG_AS("Refuge");
        LOGDEV_RES_MSG("\"%s\" could not be read: %s") << persistentPath << er.asText();
    }
}

String Refuge::path() const
{
    return d->persistentPath;
}

Refuge::~Refuge()
{
    // Musn't throw exceptions from destructor...
    try
    {
        write();
    }
    catch (const Error &er)
    {
        LOG_AS("~Refuge");
        LOG_ERROR("\"%s\" could not be written: %s") << d->persistentPath << er.asText();
    }
}

void Refuge::read()
{
    if (App::hasPersistentData())
    {
        Reader(App::persistentData().entryBlock(d->persistentPath)).withHeader() >> d->names;
        d->names.markAllMembersUnchanged();
    }
}

void Refuge::write() const
{
    if (App::hasPersistentData())
    {
        d->names.markAllMembersUnchanged();
        Writer(App::mutablePersistentData().entryBlock(d->persistentPath)).withHeader()
            << d->names;

        App::persistPackFolder().release();
    }
}

Time Refuge::lastWrittenAt() const
{
    if (App::hasPersistentData())
    {
        return App::persistentData().entryStatus(d->persistentPath).modifiedAt;
    }
    return Time::invalidTime();
}

bool Refuge::hasModifiedVariables() const
{
    return d->names.anyMembersChanged();
}

Record &Refuge::objectNamespace()
{
    return d->names;
}

const Record &Refuge::objectNamespace() const
{
    return d->names;
}

} // namespace de
