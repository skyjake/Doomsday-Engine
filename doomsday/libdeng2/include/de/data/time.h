/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TIME_H
#define LIBDENG2_TIME_H

#include "../libdeng2.h"
#include "../ISerializable"

#include <QDateTime>
#include <QTextStream>

namespace de {

class Date;
class String;

/**
 * The Time class represents a single time measurement. It represents
 * one absolute point in time (since the epoch).  Instances of Time should be
 * used wherever time needs to be measured, calculated or stored.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Time : public ISerializable
{
public:
    /**
     * Difference between two points in time. @ingroup types
     */
    class DENG2_PUBLIC Delta
    {
    public:
        /**
         * Constructs a time delta.
         *
         * @param seconds  Length of the delta.
         */
        Delta(ddouble seconds = 0) : _seconds(seconds) {}

        /**
         * Conversion to the numeric type (floating-point seconds).
         */
        operator ddouble() const { return _seconds; }

        bool operator < (const ddouble& d) const {
            return _seconds < d;
        }

        bool operator > (const ddouble& d) const {
            return _seconds > d;
        }

        Delta operator + (const ddouble& d) const;

        Delta operator - (const ddouble& d) const;

        /**
         * Convert the delta to milliseconds.
         *
         * @return  Milliseconds.
         */
        duint64 asMilliSeconds() const;

        ddouble asMinutes() const;

        ddouble asHours() const;

        ddouble asDays() const;

        static Delta fromMilliSeconds(duint64 milliseconds) {
            return Delta(milliseconds/1000.0);
        }

        /**
         * Blocks the thread.
         */
        void sleep() const;

    private:
        ddouble _seconds;
    };

    enum Format {
        ISOFormat,
        BuildNumberAndTime
    };

public:
    /**
     * Constructor initializes the time to the current time.
     */
    Time();

    Time(const Time& other) : ISerializable(), _time(other._time) {}

    Time(const QDateTime& t) : ISerializable(), _time(t) {}

    bool operator < (const Time& t) const;

    bool operator > (const Time& t) const { return t < *this; }

    bool operator <= (const Time& t) const { return !(*this > t); }

    bool operator >= (const Time& t) const { return !(*this < t); }

    bool operator == (const Time& t) const;

    /**
     * Add a delta to the point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Modified time.
     */
    Time operator + (const Delta& delta) const;

    /**
     * Subtract a delta from the point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Modified time.
     */
    Time operator - (const Delta& delta) const { return *this + (-delta); }

    /**
     * Modify point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Reference to this Time.
     */
    Time& operator += (const Delta& delta);

    /**
     * Modify point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Reference to this Time.
     */
    Time& operator -= (const Delta& delta) { return *this += -delta; }

    /**
     * Difference between two times.
     *
     * @param earlierTime  Time at some point before this time.
     */
    Delta operator - (const Time& earlierTime) const;

    /**
     * Difference between this time and the current point of time.
     * Returns positive deltas if current time is past this time.
     *
     * @return  Delta.
     */
    Delta since() const { return deltaTo(Time()); }

    /**
     * Difference between current time and this time.
     * Returns positive deltas if current time is before this time.
     *
     * @return  Delta.
     */
    Delta until() const { return Time().deltaTo(*this); }

    /**
     * Difference to a later point in time.
     *
     * @param laterTime  Time.
     *
     * @return  Delta.
     */
    Delta deltaTo(const Time& laterTime) const { return laterTime - *this; }

    /**
     * Makes a text representation of the time (default is seconds since the epoch).
     */
    String asText(Format format = ISOFormat) const;

    /**
     * Converts the time to a QDateTime.
     */
    QDateTime& asDateTime() { return _time; }

    /**
     * Converts the time to a QDateTime.
     */
    const QDateTime& asDateTime() const { return _time; }

    /**
     * Converts the time into a Date.
     */
    Date asDate() const;

    /**
     * Converts the time to a build number.
     */
    dint asBuildNumber() const;

    // Implements ISerializable.
    void operator >> (Writer& to) const;
    void operator << (Reader& from);

private:
    QDateTime _time;

    friend class Date;
};

DENG2_PUBLIC QTextStream& operator << (QTextStream& os, const Time& t);

typedef Time::Delta TimeDelta;

} // namespace de

#endif // LIBDENG2_TIME_H
