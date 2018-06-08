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

#include <ctime>

namespace de {

//static const char *ISO_FORMAT = "yyyy-MM-dd hh:mm:ss.zzz";

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
        , sysTime(std::chrono::system_clock::now())
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
            return sysTime != TimePoint();
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
        if ((flags & SysTime) && (earlier.flags & SysTime))
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

    TimePoint systemTime() const
    {
        if (flags & Impl::SysTime)
        {
            return sysTime;
        }
        if (flags & Impl::HighPerformance)
        {
            return (highPerfTimer().startedAt() + highPerfElapsed).d->sysTime;
        }
        return {};
    }

    static Time getQDateTime(const Block &data)
    {
        uint32_t julianDay;
        uint32_t msecs;
        uint32_t utfOffset;

        // This matches Qt's QDataStream format.
        Reader r(data);
        r >> julianDay >> msecs >> utfOffset;

        return Date::fromJulianDayNumber(julianDay).asTime()
                + double(msecs) / 1.0e3;
    }

    static Block putQDateTime(const Time &time)
    {
        Date date(time);
        uint32_t julianDay = date.julianDayNumber();
        uint32_t msecs = (date.seconds() + date.minutes() * 60 + date.hours() * 3600) * 1000;
        uint32_t utfOffset = 0;

        Block bytes;
        Writer w(bytes);
        w << julianDay << msecs << utfOffset;
        return bytes;
    }
};

Time::Time() : d(new Impl)
{}

Time::Time(Time const &other) : d(new Impl(*other.d))
{}

Time::Time(Time &&moved) : d(std::move(moved.d))
{}

Time::Time(int year, int month, int day, int hour, int minute, int second)
    : d(new Impl)
{
    using namespace std;

    tm t{};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = second;
    d->sysTime = chrono::system_clock::from_time_t(mktime(&t));
    d->flags |= Impl::SysTime;
}

Time::Time(const TimePoint &tp) : d(new Impl(tp))
{}

Time::Time(iTime time) : d(new Impl)
{
    using namespace std::chrono;
    d->flags |= Impl::SysTime;
    d->sysTime = system_clock::from_time_t(time.ts.tv_sec) +
                 duration_cast<system_clock::duration>(nanoseconds(time.ts.tv_nsec));
}

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
    return std::chrono::system_clock::to_time_t(d->systemTime());
}

uint64_t Time::millisecondsSinceEpoch() const
{
    const auto dur = d->systemTime().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

Time::TimePoint Time::toTimePoint() const
{
    return d->systemTime();
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
    if (d->flags & Impl::SysTime)
    {
        const Date date{*this};
        return (date.year() - 2011) * 365 + date.dayOfYear();
    }
    return 0;
}

String Time::asText(Format format) const
{
    if (!isValid())
    {
        return "(undefined time)";
    }
    if (d->flags & Impl::SysTime)
    {
        if (format == ISOFormat)
        {
            auto ms = millisecondsSinceEpoch();
            return asText("%F %M") + stringf(".%03u", ms % 1000).c_str();
        }
        else if (format == ISODateOnly)
        {
            return asText("%F");
        }
        else if (format == FriendlyFormat)
        {
            // Wed May 20 03:40:13 1998
            // return asText("%a %b %d %H:%M:%S %Y");

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
            else if (d->flags.testFlag(Impl::SysTime))
            {
                elapsed = highPerfTimer().startedAt().deltaTo(Time(d->sysTime));
            }
            int hours = int(elapsed.asHours());
            ddouble sec = elapsed - hours * 3600.0;
            String prefix;
            if (format == BuildNumberAndSecondsSinceStart)
            {
                prefix = String::format("#%-4d ", asBuildNumber());
            }
            if (hours > 0)
            {
                return String::format("%s%ih%7.3f", prefix.c_str(), hours, sec);
            }
            return String::format("%s%7.3f", prefix.c_str(), sec);
        }
        else
        {
            auto ms = millisecondsSinceEpoch();
            return String::format("#%-4d ", asBuildNumber()) + asText("%H:%M:%S") +
                   stringf(".%03i", ms % 1000).c_str();
        }
    }
    if (d->flags.testFlag(Impl::HighPerformance))
    {
        return String::format("+%.3f sec", ddouble(d->highPerfElapsed));
    }
    return {};
}

String Time::asText(const char *format) const
{
    const time_t time = toTime_t();
    char buf[256];
    strftime(buf, sizeof(buf) - 1, format, std::localtime(&time));
    return buf;
}

static int parseMonth(const String &shortName)
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

Time Time::fromText(const String &text, Time::Format format)
{
    DE_ASSERT(format == ISOFormat ||
              format == ISODateOnly ||
              format == CompilerDateTime ||
              format == HumanDate ||
              format == UnixLsStyleDateTime);

    if (format == ISOFormat)
    {
        int year = 0, month = 0, mday = 0, hour = 0, minute = 0;
        double seconds = 0;
        std::sscanf(text, "%4d-%d-%d %d:%d:%lf", &year, &month, &mday, &hour, &minute, &seconds);
        return Time(year, month, mday, hour, minute, 0) + seconds;
    }
    else if (format == ISODateOnly)
        int year = 0, month = 0, mday = 0;
    {
        std::sscanf(text, "%4d-%d-%d", &year, &month, &mday);
        return Time(year, month, mday, 0, 0, 0);
    }
    else if (format == CompilerDateTime)
    {
        // Parse the text manually as it is locale-independent.
        const StringList parts = filter(text.split(" "), [](const String &s){ return !s.empty(); });
        if (parts.size() >= 4)
        {
            int day = parts[1].toInt();
            int year = parts[2].toInt();
            int month = parseMonth(parts[0]);
            int hour = 0, minute = 0, seconds = 0;
            std::sscanf(parts[3], "%d:%d:%d", &hour, &minute, &seconds);
            return Time(year, month, day, hour, minute, seconds);
        }
    }
    else if (format == UnixLsStyleDateTime)
    {
        // Two options:
        // Jun 2 2016
        // Jun 2 06:30

        const StringList parts = filter(text.split(" "), [](const String &s){ return !s.empty(); });
        if (parts.size() >= 3)
        {
            int month  = parseMonth(parts[0]);
            int day    = parts[1].toInt();
            int hour   = 0;
            int minute = 0;
            int year;
            if (parts[2].contains(":"))
            {
                year   = Date::currentDate().year();
                hour   = parts[2].left(String::BytePos(2)).toInt();
                minute = parts[2].right(String::BytePos(2)).toInt();
                return Time(year, month, day, hour, minute, 0);
            }
            else
            {
                year = parts[2].toInt();
                return Time(year, month, day, 0, 0, 0);
            }
        }
    }
    else if (format == HumanDate)
    {
        // Note: Check use of indices below.
        static const char *formats[] = {
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
        };
        const String normText = text.normalizeWhitespace();
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
    return Date(*this);
}

// Flags for serialization.
static duint8 const HAS_DATETIME  = 0x01;
static duint8 const HAS_HIGH_PERF = 0x02;

void Time::operator>>(Writer &to) const
{
    duint8 flags = (d->flags & Impl::SysTime?         HAS_DATETIME  : 0) |
                   (d->flags & Impl::HighPerformance? HAS_HIGH_PERF : 0);
    to << flags;

    if (d->flags.testFlag(Impl::SysTime))
    {
        // Using Qt's format for backwards compatibility (and it's pretty compact).
        to << Impl::putQDateTime(*this);
    }

    if (d->flags.testFlag(Impl::HighPerformance))
    {
        to << d->highPerfElapsed;
    }
}

void Time::operator<<(Reader &from)
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
            d->flags |= Impl::SysTime;

            Block bytes;
            from >> bytes;
            d->sysTime = Impl::getQDateTime(bytes).d->sysTime;
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
            if (d->sysTime < highPerfTimer().startedAt().toTimePoint())
            {
                // Current high-performance timer was started after this time;
                // we can't represent the time as high performance delta.
                d->flags &= ~Impl::HighPerformance;
            }
            else
            {
                const auto dur = d->sysTime - highPerfTimer().startedAt().toTimePoint();
                d->highPerfElapsed = std::chrono::duration_cast
                        <std::chrono::milliseconds>(dur).count() / 1.0e3;
            }
        }
    }
    else
    {
        // This serialization only has a QDateTime.
        Block bytes;
        from >> bytes;
        d->sysTime = Impl::getQDateTime(bytes).d->sysTime;
        d->flags = Impl::SysTime;
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
