/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * @ingroup types
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

        bool operator < (ddouble const &d) const {
            return _seconds < d;
        }

        bool operator > (ddouble const &d) const {
            return _seconds > d;
        }

        Delta operator + (ddouble const &d) const;

        Delta operator - (ddouble const &d) const;

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
        BuildNumberAndTime,
        FriendlyFormat,
        ISODateOnly
    };

public:
    /**
     * Constructor initializes the time to the current time.
     */
    Time();

    Time(Time const &other) : ISerializable(), _time(other._time) {}

    Time(QDateTime const &t) : ISerializable(), _time(t) {}

    static Time invalidTime();

    bool isValid() const;

    bool operator < (Time const &t) const;

    bool operator > (Time const &t) const { return t < *this; }

    bool operator <= (Time const &t) const { return !(*this > t); }

    bool operator >= (Time const &t) const { return !(*this < t); }

    bool operator == (Time const &t) const;

    /**
     * Add a delta to the point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Modified time.
     */
    Time operator + (Delta const &delta) const;

    /**
     * Subtract a delta from the point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Modified time.
     */
    Time operator - (Delta const &delta) const { return *this + (-delta); }

    /**
     * Modify point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Reference to this Time.
     */
    Time &operator += (Delta const &delta);

    /**
     * Modify point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Reference to this Time.
     */
    Time &operator -= (Delta const &delta) { return *this += -delta; }

    /**
     * Difference between two times.
     *
     * @param earlierTime  Time at some point before this time.
     */
    Delta operator - (Time const &earlierTime) const;

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
    Delta deltaTo(Time const &laterTime) const { return laterTime - *this; }

    /**
     * Makes a text representation of the time (default is ISO format, e.g.,
     * 2012-12-02 13:08:21.851).
     */
    String asText(Format format = ISOFormat) const;

    /**
     * Parses a text string into a Time.
     *
     * @param format  Format of the text string.
     *
     * @return Time that corresponds @a text.
     */
    static Time fromText(String const &text, Format format = ISOFormat);

    /**
     * Converts the time to a QDateTime.
     */
    QDateTime &asDateTime() { return _time; }

    /**
     * Converts the time to a QDateTime.
     */
    QDateTime const &asDateTime() const { return _time; }

    /**
     * Converts the time into a Date.
     */
    Date asDate() const;

    /**
     * Converts the time to a build number.
     */
    dint asBuildNumber() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    QDateTime _time;

    friend class Date;
};

DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Time const &t);

typedef Time::Delta TimeDelta;

} // namespace de

#endif // LIBDENG2_TIME_H
