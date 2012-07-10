/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_DATE_H
#define LIBDENG2_DATE_H

#include "../Time"
#include "../Log"

#include <QTextStream>

namespace de {

/**
 * Information about a date.
 *
 * @ingroup types
 */
class DENG2_PUBLIC Date : public Time, public LogEntry::Arg::Base
{
public:
    /**
     * Constructs a new Date out of the current time.
     */
    Date();

    Date(const Time& time);

    int year() const { return asDateTime().date().year(); }
    int month() const { return asDateTime().date().month(); }
    int dayOfMonth() const { return asDateTime().date().day(); }
    int hours() const { return asDateTime().time().hour(); }
    int minutes() const { return asDateTime().time().minute(); }
    int seconds() const { return asDateTime().time().second(); }
    int daysTo(const Date& other) const { return asDateTime().date().daysTo(other.asDateTime().date()); }

    /**
     * Forms a textual representation of the date.
     */
    String asText() const;

    /**
     * Converts the date back to a Time.
     *
     * @return  The corresponding Time.
     */
    Time asTime() const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const {
        return LogEntry::Arg::STRING;
    }
};

DENG2_PUBLIC QTextStream& operator << (QTextStream& os, const Date& date);

}

#endif /* LIBDENG2_DATE_H */
