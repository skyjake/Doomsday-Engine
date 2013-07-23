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
#include "de/HighPerformanceTimer"

#include <QThread>
#include <QDataStream>

namespace de {

static String const ISO_FORMAT = "yyyy-MM-dd hh:mm:ss.zzz";
static HighPerformanceTimer highPerfTimer;

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

duint64 TimeDelta::asMilliSeconds() const
{
    return duint64(_seconds * 1000);
}

ddouble TimeDelta::asMinutes() const
{
    return _seconds / 60;
}

ddouble TimeDelta::asHours() const
{
    return _seconds / 3600;
}

ddouble TimeDelta::asDays() const
{
    return asHours() / 24;
}

void TimeDelta::sleep() const
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

void TimeDelta::operator >> (Writer &to) const
{
    to << _seconds;
}

void TimeDelta::operator << (Reader &from)
{
    from >> _seconds;
}

TimeDelta TimeDelta::operator + (ddouble const &d) const
{
    return _seconds + d;
}

TimeDelta &TimeDelta::operator += (ddouble const &d)
{
    _seconds += d;
    return *this;
}

TimeDelta TimeDelta::operator - (ddouble const &d) const
{
    return _seconds - d;
}

DENG2_PIMPL_NOREF(Time)
{
    enum Flag {
        DateTime        = 0x1,
        HighPerformance = 0x2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    Flags flags;
    QDateTime dateTime;
    Delta highPerfElapsed;

    Instance()
        : flags(DateTime | HighPerformance),
          dateTime(QDateTime::currentDateTime()),
          highPerfElapsed(highPerfTimer.elapsed())
    {}

    Instance(QDateTime const &dt) : flags(DateTime), dateTime(dt) {}

    Instance(Delta const &delta) : flags(HighPerformance), highPerfElapsed(delta) {}

    Instance(Instance const &other)
        : flags(other.flags),
          dateTime(other.dateTime),
          highPerfElapsed(other.highPerfElapsed)
    {}

    bool hasDateTime() const
    {
        return flags.testFlag(DateTime);
    }

    bool isValid() const
    {
        if(flags.testFlag(DateTime))
        {
            return dateTime.isValid();
        }
        return flags.testFlag(HighPerformance);
    }

    bool isLessThan(Instance const &other) const
    {
        if(flags.testFlag(DateTime) && other.flags.testFlag(DateTime))
        {
            // Full date and time comparison.
            return dateTime < other.dateTime;
        }
        if(flags.testFlag(HighPerformance) && other.flags.testFlag(HighPerformance))
        {
            return highPerfElapsed < other.highPerfElapsed;
        }
        /**
         * @todo Implement needed conversion to compare DateTime with high
         * performance delta time.
         */
        DENG2_ASSERT(false);
        return false;
    }

    bool isEqualTo(Instance const &other) const
    {
        if(flags.testFlag(DateTime) && other.flags.testFlag(DateTime))
        {
            return dateTime == other.dateTime;
        }
        if(flags.testFlag(HighPerformance) && other.flags.testFlag(HighPerformance))
        {
            return highPerfElapsed == other.highPerfElapsed;
        }
        /**
         * @todo Implement needed conversion to compare DateTime with high
         * performance delta time.
         */
        DENG2_ASSERT(false);
        return false;
    }

    void add(Delta const &delta)
    {
        if(flags.testFlag(DateTime))
        {
            dateTime = dateTime.addMSecs(delta.asMilliSeconds());
        }
        if(flags.testFlag(HighPerformance))
        {
            highPerfElapsed += delta;
        }
    }

    Delta delta(Instance const &earlier) const
    {
        if(flags.testFlag(DateTime) && earlier.flags.testFlag(DateTime))
        {
            return earlier.dateTime.msecsTo(dateTime) / 1000.0;
        }
        if(flags.testFlag(HighPerformance) && earlier.flags.testFlag(HighPerformance))
        {
            return highPerfElapsed - earlier.highPerfElapsed;
        }
        /**
         * @todo Implement needed conversion to compare DateTime with high
         * performance delta time.
         */
        DENG2_ASSERT(false);
        return 0;
    }
};

Time::Time() : d(new Instance)
{}

Time::Time(Time const &other) : ISerializable(), d(new Instance(*other.d))
{}

Time::Time(QDateTime const &t) : ISerializable(), d(new Instance(t))
{}

Time::Time(TimeDelta const &highPerformanceDelta)
    : ISerializable(), d(new Instance(highPerformanceDelta))
{}

Time Time::invalidTime()
{
    return Time(QDateTime());
}

Time &Time::operator = (Time const &other)
{
    d.reset(new Instance(*other.d));
    return *this;
}

bool Time::isValid() const
{
    return d->isValid();
}

bool Time::operator < (Time const &t) const
{
    return d->isLessThan(*t.d);
}

bool Time::operator == (Time const &t) const
{
    return d->isEqualTo(*t.d);
}

Time Time::operator + (Delta const &delta) const
{    
    Time result = *this;
    result += delta;
    return result;
}

Time &Time::operator += (Delta const &delta)
{
    d->add(delta);
    return *this;
}

TimeDelta Time::operator - (Time const &earlierTime) const
{
    return d->delta(*earlierTime.d);
}

dint Time::asBuildNumber() const
{
    if(d->hasDateTime())
    {
        return (d->dateTime.date().year() - 2011)*365 + d->dateTime.date().dayOfYear();
    }
    return 0;
}

String Time::asText(Format format) const
{
    if(!isValid())
    {
        return "(undefined time)";
    }
    if(d->hasDateTime())
    {
        if(format == ISOFormat)
        {
            return d->dateTime.toString(ISO_FORMAT);
        }
        else if(format == ISODateOnly)
        {
            return d->dateTime.toString("yyyy-MM-dd");
        }
        else if(format == FriendlyFormat)
        {
            return d->dateTime.toString(Qt::TextDate);
        }
        else
        {
            return QString("#%1 ").arg(asBuildNumber(), -4) + d->dateTime.toString("hh:mm:ss.zzz");
        }
    }
    if(d->flags.testFlag(Instance::HighPerformance))
    {
        return QString("+%1 sec").arg(d->highPerfElapsed, 0, 'f', 3);
    }
    return "";
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

QDateTime &de::Time::asDateTime()
{
    DENG2_ASSERT(d->hasDateTime());
    return d->dateTime;
}

QDateTime const &de::Time::asDateTime() const
{
    DENG2_ASSERT(d->hasDateTime());
    return d->dateTime;
}

Date Time::asDate() const
{
    DENG2_ASSERT(d->hasDateTime());
    return Date(*this);
}

// Flags for serialization.
static duint8 const HAS_DATETIME  = 0x01;
static duint8 const HAS_HIGH_PERF = 0x02;

void Time::operator >> (Writer &to) const
{
    duint8 flags = (d->flags & Instance::DateTime?        HAS_DATETIME  : 0) |
                   (d->flags & Instance::HighPerformance? HAS_HIGH_PERF : 0);
    to << flags;

    if(d->flags.testFlag(Instance::DateTime))
    {
        Block bytes;
        QDataStream s(&bytes, QIODevice::WriteOnly);
        s << d->dateTime;
        to << bytes;
    }

    if(d->flags.testFlag(Instance::HighPerformance))
    {
        to << d->highPerfElapsed;
    }
}

void Time::operator << (Reader &from)
{
    if(from.version() >= DENG2_PROTOCOL_1_11_0_BUILD_926)
    {
        /*
         * Starting from build 926, Time can optionally contain a
         * high-performance delta component.
         */

        duint8 flags;
        from >> flags;

        d->flags = 0;

        if(flags & HAS_DATETIME)
        {
            d->flags |= Instance::DateTime;

            Block bytes;
            from >> bytes;
            QDataStream s(bytes);
            s >> d->dateTime;
        }

        if(flags & HAS_HIGH_PERF)
        {
            d->flags |= Instance::HighPerformance;

            from >> d->highPerfElapsed;
        }
    }
    else
    {
        // This serialization only has a QDateTime.
        Block bytes;
        from >> bytes;
        QDataStream s(bytes);
        s >> d->dateTime;
        d->flags = Instance::DateTime;
    }
}

Time Time::currentHighPerformanceTime()
{
    return Time(highPerfTimer.elapsed());
}

QTextStream &operator << (QTextStream &os, Time const &t)
{
    os << t.asText();
    return os;
}

} // namespace de
