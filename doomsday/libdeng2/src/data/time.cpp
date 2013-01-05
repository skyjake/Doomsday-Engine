/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Time"
#include "de/Date"
#include "de/String"
#include "de/Writer"
#include "de/Reader"

#include <QThread>
#include <QDataStream>

namespace de {

static String const ISO_FORMAT = "yyyy-MM-dd hh:mm:ss.zzz";

namespace internal {

class SleeperThread : public QThread
{
public:
    void run() {}
    static void msleep(unsigned long milliseconds)
    {
        QThread::msleep(milliseconds);
    }
    static void usleep(unsigned long microseconds)
    {
        QThread::usleep(microseconds);
    }
};

} // namespace internal

duint64 Time::Delta::asMilliSeconds() const
{
    return duint64(_seconds * 1000);
}

ddouble Time::Delta::asMinutes() const
{
    return _seconds / 60;
}

ddouble Time::Delta::asHours() const
{
    return _seconds / 3600;
}

ddouble Time::Delta::asDays() const
{
    return asHours() / 24;
}

void Time::Delta::sleep() const
{
    if(_seconds < 60)
    {
        internal::SleeperThread::usleep((unsigned long)(_seconds * 1e6));
    }
    else
    {
        internal::SleeperThread::msleep((unsigned long)(_seconds * 1e3));
    }
}

Time::Delta Time::Delta::operator + (ddouble const &d) const
{
    return _seconds + d;
}

Time::Delta Time::Delta::operator - (ddouble const &d) const
{
    return _seconds - d;
}

Time::Time() : _time(QDateTime::currentDateTime())
{}

Time Time::invalidTime()
{
    return Time(QDateTime());
}

bool Time::isValid() const
{
    return _time.isValid();
}

bool Time::operator < (Time const &t) const
{
    return (_time < t._time);
}

bool Time::operator == (Time const &t) const
{
    return (_time == t._time);
}

Time Time::operator + (Delta const &delta) const
{
    Time result = *this;
    result += delta;
    return result;
}

Time &Time::operator += (Delta const &delta)
{
    _time = _time.addMSecs(delta.asMilliSeconds());
    return *this;
}

Time::Delta Time::operator - (Time const &earlierTime) const
{
#ifdef DENG2_QT_4_7_OR_NEWER
    return earlierTime._time.msecsTo(_time) / 1000.0;
#else
    return earlierTime._time.time().msecsTo(_time.time()) / 1000.0;
#endif
}

dint Time::asBuildNumber() const
{
    return QDate(2011, 1, 1).daysTo(_time.date()) + 1;
}

String Time::asText(Format format) const
{
    if(format == ISOFormat)
    {
        return _time.toString(ISO_FORMAT);
    }
    else if(format == ISODateOnly)
    {
        return _time.toString("yyyy-MM-dd");
    }
    else if(format == FriendlyFormat)
    {
        return _time.toString(Qt::TextDate);
    }
    else
    {
        return QString("#%1 ").arg(asBuildNumber(), -4) + _time.toString("hh:mm:ss.zzz");
    }
}

Time Time::fromText(String const &text, Time::Format format)
{
    DENG2_ASSERT(format == ISOFormat || format == ISODateOnly || format == FriendlyFormat);

    if(format == ISOFormat)
    {
        return Time(QDateTime::fromString(text, ISO_FORMAT));
    }
    else if(format == ISODateOnly)
    {
        return Time(QDateTime::fromString(text, "yyyy-MM-dd"));
    }
    else if(format == FriendlyFormat)
    {
        return Time(QDateTime::fromString(text, Qt::TextDate));
    }
    return Time();
}

Date Time::asDate() const
{
    return Date(*this);
}

void Time::operator >> (Writer &to) const
{
    Block bytes;
    QDataStream s(&bytes, QIODevice::WriteOnly);
    s << _time;
    to << bytes;
}

void Time::operator << (Reader &from)
{
    Block bytes;
    from >> bytes;
    QDataStream s(bytes);
    s >> _time;
}

QTextStream &operator << (QTextStream &os, Time const &t)
{
    os << t.asText();
    return os;
}

} // namespace de
