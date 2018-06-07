/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Time"

#include "de/Date"
#include "de/HighPerformanceTimer"
#include "de/Reader"
#include "de/String"
#include "de/Thread"
#include "de/Writer"

namespace de {

static String const ISO_FORMAT = "yyyy-MM-dd hh:mm:ss.zzz";

static HighPerformanceTimer &highPerfTimer()
{
    static HighPerformanceTimer hpt;
    return hpt;
}

static TimeSpan currentHighPerfDelta;

duint64 Time::Span::asMicroSeconds() const
{
    return duint64(_seconds * 1000000);
}

duint64 TimeSpan::asMilliSeconds() const
{
    return duint64(_seconds * 1000);
}

ddouble TimeSpan::asMinutes() const
{
    return _seconds / 60;
}

ddouble TimeSpan::asHours() const
{
    return _seconds / 3600;
}

ddouble TimeSpan::asDays() const
{
    return asHours() / 24;
}

Time::Span Time::Span::sinceStartOfProcess()
{
    return highPerfTimer().elapsed();
}

void TimeSpan::sleep() const
{
    Thread::sleep(_seconds);
}

void TimeSpan::operator >> (Writer &to) const
{
    to << _seconds;
}

void TimeSpan::operator << (Reader &from)
{
    from >> _seconds;
}

DE_PIMPL_NOREF(Time)
{
    enum Flag {
        SysTime         = 0x1,
        HighPerformance = 0x2
    };

    Flags     flags;
    TimePoint sysTime;
    Span      highPerfElapsed;

    Impl()
        : flags(SysTime | HighPerformance)
        , dateTime(std::chrono::system_clock::now())
        , highPerfElapsed(highPerfTimer().elapsed())
    {}

    Impl(const TimePoint &tp) : flags(SysTime), sysTime(tp) {}

    Impl(Span const &span) : flags(HighPerformance), highPerfElapsed(span) {}

    Impl(Impl const &other)
        : de::IPrivate()
        , flags(other.flags)
        , sysTime(other.sysTime)
        , highPerfElapsed(other.highPerfElapsed)
    {}

    bool isValid() const
    {
        if (flags & SysTime)
        {
            return sysTime != TimePoint::zero();
        }
        return (flags & HighPerformance) != 0;
    }

    bool isLessThan(Impl const &other) const
    {
        if ((flags & HighPerformance) && (other.flags & HighPerformance))
        {
            return highPerfElapsed < other.highPerfElapsed;
        }
        if ((flags & SysTime) && (other.flags & SysTime))
        {
            // Full date and time comparison.
            return sysTime < other.sysTime;
        }
        /**
         * @todo Implement needed conversion to compare DateTime with high
         * performance delta time.
         */
        DE_ASSERT(false);
        return false;
    }

    bool isEqualTo(Impl const &other) const
    {
        if ((flags & HighPerformance) && (other.flags & HighPerformance))
        {
            return highPerfElapsed == other.highPerfElapsed;
        }
        if ((flags & SysTime) && (other.flags & SysTime))
        {
            return sysTime == other.sysTime;
        }
        if (flags & SysTime)
        {
            // This is DateTime but other is high-perf.
            DE_ASSERT(false);
//            return fequal(highPerfTimer().startedAt().asDateTime().msecsTo(dateTime)/1000.0,
//                          other.highPerfElapsed);
        }
        if (flags & HighPerformance)
        {
            // This is high-perf and the other is DateTime.
            DE_ASSERT(false);

//            return fequal(highPerfElapsed,
//                          highPerfTimer().startedAt().asDateTime().msecsTo(other.dateTime)/1000.0);
        }
        return false;
    }

    void add(Span const &delta)
    {
        if (flags & SysTime)
        {
            sysTime += std::chrono::milliseconds(delta.asMilliSeconds()); //= sysTime.addMSecs(delta.asMilliSeconds());
        }
        if (flags & HighPerformance)
        {
            highPerfElapsed += delta;
        }
    }

    Span delta(Impl const &earlier) const
    {
        if ((flags & HighPerformance) && (earlier.flags & HighPerformance))
        {
            return highPerfElapsed - earlier.highPerfElapsed;
        }
        if ((flags & DateTime) && (earlier.flags & DateTime))
        {
            //return earlier.dateTime.msecsTo(dateTime) / 1000.0;
            using namespace std::chrono;
            return duration_cast<milliseconds>(sysTime - earlier.sysTime).count() / 1.0e3;
        }
        /**
         * @todo Implement needed conversion to compare DateTime with high
         * performance delta time.
         */
        DE_ASSERT(false);
        return 0;
    }

    void setSysTimeFromHighPerf()
    {
        sysTime = (highPerfTimer().startedAt() + highPerfElapsed).d->sysTime;
        flags |= SysTime;
    }
};

Time::Time() : d(new Impl)
{}

Time::Time(Time const &other) : d(new Impl(*other.d))
{}

Time::Time(Time &&moved) : d(std::move(moved.d))
{}

Time::Time(const TimePoint &tp) : d(new Impl(tp))
{}

Time::Time(TimeSpan const &highPerformanceDelta)
    : ISerializable(), d(new Impl(highPerformanceDelta))
{}

Time Time::invalidTime()
{
    return Time(TimePoint());
}

Time &Time::operator = (Time const &other)
{
    *d = *other.d;
    return *this;
}

Time &Time::operator = (Time &&moved)
{
    d = std::move(moved.d);
    return *this;
}

time_t Time::toTime_t() const
{
    if (d->flags & Impl::SysTime)
    {
        return std::chrono::system_clock::to_time_t(d->sysTime);
    }
    if (d->flags & Impl::HighPerformance)
    {
        return std::chrono::system_clock::to_time_t
                ((highPerfTimer().startedAt() + d->highPerfElapsed()).d->sysTime);
    }
    return 0;
}

Time::TimePoint Time::toTimePoint() const
{
    if (d->flags & Impl::SysTime)
    {
        return d->sysTime;
    }
    if (d->flags & Impl::HighPerformance)
    {
        const Time tm = highPerfTimer().startedAt() + d->highPerfElapsed;
        return tm.d->sysTime;
    }
    return {};
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

Time Time::operator + (Span const &span) const
{
    Time result = *this;
    result += span;
    return result;
}

Time &Time::operator += (Span const &span)
{
    d->add(span);
    return *this;
}

TimeSpan Time::operator - (Time const &earlierTime) const
{
    return d->delta(*earlierTime.d);
}

dint Time::asBuildNumber() const
{
    if (d->hasDateTime())
    {
        return (d->dateTime.date().year() - 2011)*365 + d->dateTime.date().dayOfYear();
    }
    return 0;
}

String Time::asText(Format format) const
{
    if (!isValid())
    {
        return "(undefined time)";
    }
    if (d->hasDateTime())
    {
        if (format == ISOFormat)
        {
            return d->dateTime.toString(ISO_FORMAT);
        }
        else if (format == ISODateOnly)
        {
            return d->dateTime.toString("yyyy-MM-dd");
        }
        else if (format == FriendlyFormat)
        {
            // Is it today?
            if (d->dateTime.date() == QDateTime::currentDateTime().date())
            {
                return d->dateTime.toString("HH:mm");
            }
            else if (d->dateTime.date().year() == QDateTime::currentDateTime().date().year())
            {
                return d->dateTime.toString("MMM dd HH:mm");
            }
            else
            {
                return d->dateTime.toString("yyyy MMM dd");
            }
        }
        else if (format == BuildNumberAndSecondsSinceStart ||
                 format == SecondsSinceStart)
        {
            TimeSpan elapsed;
            if (d->flags.testFlag(Impl::HighPerformance))
            {
                elapsed = d->highPerfElapsed;
            }
            else if (d->flags.testFlag(Impl::DateTime))
            {
                elapsed = highPerfTimer().startedAt().deltaTo(Time(d->dateTime));
            }
            int hours = int(elapsed.asHours());
            TimeSpan sec = elapsed - hours * 3600.0;
            QString prefix;
            if (format == BuildNumberAndSecondsSinceStart)
            {
                prefix = QString("#%1 ").arg(asBuildNumber(), -4);
            }
            if (hours > 0)
            {
                return QString("%1%2h%3")
                        .arg(prefix)
                        .arg(hours)
                        .arg(sec, 7, 'f', 3, '0');
            }
            return QString("%1%2")
                    .arg(prefix)
                    .arg(sec, 7, 'f', 3, '0');
        }
        else
        {
            return QString("#%1 ").arg(asBuildNumber(), -4) + d->dateTime.toString("hh:mm:ss.zzz");
        }
    }
    if (d->flags.testFlag(Impl::HighPerformance))
    {
        return QString("+%1 sec").arg(d->highPerfElapsed, 0, 'f', 3);
    }
    return "";
}

static int parseMonth(String const &shortName)
{
    static char const *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    for (int i = 0; i < 12; ++i)
    {
        if (shortName == months[i])
        {
            return i + 1;
        }
    }
    return 0;
}

Time Time::fromText(String const &text, Time::Format format)
{
    DE_ASSERT(format == ISOFormat ||
                 format == ISODateOnly ||
                 format == FriendlyFormat ||
                 format == CompilerDateTime ||
                 format == HumanDate ||
                 format == UnixLsStyleDateTime);

    if (format == ISOFormat)
    {
        return Time(QDateTime::fromString(text, ISO_FORMAT));
    }
    else if (format == ISODateOnly)
    {
        return Time(QDateTime::fromString(text, "yyyy-MM-dd"));
    }
    else if (format == FriendlyFormat)
    {
        return Time(QDateTime::fromString(text, Qt::TextDate));
    }
    else if (format == CompilerDateTime)
    {
        // Parse the text manually as it is locale-independent.
        QStringList const parts = text.split(" ", QString::SkipEmptyParts);
        if (parts.size() >= 4)
        {
            int day = parts[1].toInt();
            int year = parts[2].toInt();
            int month = parseMonth(parts[0]);
            QDate date(year, month, day);
            QTime time = QTime::fromString(parts[3], "HH:mm:ss");
            return Time(QDateTime(date, time));
        }
    }
    else if (format == UnixLsStyleDateTime)
    {
        // Two options:
        // Jun 2 2016
        // Jun 2 06:30

        QStringList const parts = text.split(" ", QString::SkipEmptyParts);
        if (parts.size() >= 3)
        {
            int month = parseMonth(parts[0]);
            int day = parts[1].toInt();
            int hour = 0;
            int minute = 0;
            int year;
            if (parts[2].contains(QChar(':')))
            {
                year = QDate::currentDate().year();
                hour = parts[2].left(2).toInt();
                minute = parts[2].right(2).toInt();
                return Time(QDateTime(QDate(year, month, day),
                                      QTime(hour, minute, 0)));
            }
            else
            {
                year = parts[2].toInt();
                return Time(QDateTime(QDate(year, month, day), QTime(0, 0, 0)));
            }
        }
    }
    else if (format == HumanDate)
    {
        // Note: Check use of indices below.
        static QStringList const formats({
            "M/d/yy",
            "MM/dd/yy",
            "d.M.yy",
            "dd.MM.yy",

            "MM/dd/yyyy",
            "d.M.yyyy",
            "dd.MM.yyyy",
            "MM.dd.yyyy", // as a fallback...
            "yyyy-MM-dd",

            // Unix "ls" style:
            "MMM d yyyy",
            "MMM d hh:mm",
        });
        String const normText = text.normalizeWhitespace();
        for (int i = 0; i < formats.size(); ++i)
        {
            String const fmt = formats.at(i);
            bool const isShortYear = i < 4;
            QDateTime dt = QDateTime::fromString(normText, fmt);
            if (dt.isValid())
            {
                if (i == 10) // No year specified.
                {
                    dt.setDate(QDate(QDate::currentDate().year(),
                                     dt.date().month(),
                                     dt.date().day()));
                }
                else if (isShortYear && dt.date().year() < 1980)
                {
                    dt.setDate(QDate(dt.date().year() + 100,
                                     dt.date().month(),
                                     dt.date().day())); // Y2K?
                }
                return Time(dt);
            }
        }
        return Time(QDateTime::fromString(normText, Qt::TextDate));
    }
    return Time::invalidTime();
}

/*QDateTime &Time::asDateTime()
{
    DE_ASSERT(d->hasDateTime());
    return d->dateTime;
}

QDateTime const &Time::asDateTime() const
{
    if (!d->hasDateTime() && d->flags.testFlag(Impl::HighPerformance))
    {
        d->dateTime = (highPerfTimer().startedAt() + d->highPerfElapsed).asDateTime();
        d->flags |= Impl::DateTime;
    }
    return d->dateTime;
}*/

Date Time::asDate() const
{
    DE_ASSERT(d->hasDateTime());
    return Date(*this);
}

// Flags for serialization.
static duint8 const HAS_DATETIME  = 0x01;
static duint8 const HAS_HIGH_PERF = 0x02;

void Time::operator >> (Writer &to) const
{
    duint8 flags = (d->flags & Impl::DateTime?        HAS_DATETIME  : 0) |
                   (d->flags & Impl::HighPerformance? HAS_HIGH_PERF : 0);
    to << flags;

    if (d->flags.testFlag(Impl::DateTime))
    {
        Block bytes;
        QDataStream s(&bytes, QIODevice::WriteOnly);
        s.setVersion(QDataStream::Qt_4_8);
        s << d->dateTime;
        to << bytes;
    }

    if (d->flags.testFlag(Impl::HighPerformance))
    {
        to << d->highPerfElapsed;
    }
}

void Time::operator << (Reader &from)
{
    if (from.version() >= DE_PROTOCOL_1_11_0_Time_high_performance)
    {
        /*
         * Starting from build 926, Time can optionally contain a high-performance delta
         * component.
         */

        duint8 flags;
        from >> flags;

        d->flags = 0;

        if (flags & HAS_DATETIME)
        {
            d->flags |= Impl::DateTime;

            Block bytes;
            from >> bytes;
            QDataStream s(bytes);
            s.setVersion(QDataStream::Qt_4_8);
            s >> d->dateTime;
        }

        if (flags & HAS_HIGH_PERF)
        {
            d->flags |= Impl::HighPerformance;
            from >> d->highPerfElapsed;
        }

        if ((flags & HAS_DATETIME) && (flags & HAS_HIGH_PERF))
        {
            // If both are present, the high-performance time should be synced
            // with current high-perf timer.
            if (d->dateTime < highPerfTimer().startedAt().asDateTime())
            {
                // Current high-performance timer was started after this time;
                // we can't represent the time as high performance delta.
                d->flags &= ~Impl::HighPerformance;
            }
            else
            {
                d->highPerfElapsed = highPerfTimer().startedAt().deltaTo(d->dateTime);
            }
        }
    }
    else
    {
        // This serialization only has a QDateTime.
        Block bytes;
        from >> bytes;
        QDataStream s(bytes);
        s.setVersion(QDataStream::Qt_4_8);
        s >> d->dateTime;
        d->flags = Impl::DateTime;
    }
}

TimeSpan Time::highPerformanceTime() const
{
    DE_ASSERT(d->flags & Impl::HighPerformance);
    return d->highPerfElapsed;
}

Time Time::currentHighPerformanceTime() // static
{
    return currentHighPerfDelta;
}

void Time::updateCurrentHighPerformanceTime() // static
{
    currentHighPerfDelta = highPerfTimer().elapsed();
}

std::ostream &operator << (std::ostream &os, Time const &t)
{
    return os << t.asText();
}

} // namespace de
