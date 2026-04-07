/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_DATE_H
#define LIBCORE_DATE_H

#include "de/time.h"
#include "de/log.h"

#include <sstream>

namespace de {

/**
 * Information about a date.
 *
 * @ingroup types
 */
class DE_PUBLIC Date : public LogEntry::Arg::Base
{
public:
    /**
     * Constructs a new Date out of the current time.
     */
    Date();

    Date(const Time &time);

    bool isValid() const;

    static Date fromJulianDayNumber(int jdn);

    int year() const;
    int month() const;
    int dayOfMonth() const;
    int dayOfYear() const;
    int dayOfWeek() const;
    int julianDayNumber() const;
    int hours() const;
    int minutes() const;
    ddouble seconds() const;
    int daysTo(const Date &other) const;

    /**
     * Forms a textual representation of the date.
     */
    String format(const char *format = "%F") const;

    /**
     * Converts the date back to a Time.
     *
     * @return  The corresponding Time.
     */
    Time asTime() const;

    bool isSameDay(const Date &) const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const override { return LogEntry::Arg::StringArgument; }
    String asText() const override { return format(); }

    static Date currentDate();

    static Date fromText(const String &text);

private:
    DE_PRIVATE(d)
};

DE_PUBLIC std::ostream &operator << (std::ostream &os, const Date &date);

} // namespace de

#endif /* LIBCORE_DATE_H */
