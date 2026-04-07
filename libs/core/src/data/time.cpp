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

#include "de/time.h"

#include "de/date.h"
#include "de/highperformancetimer.h"
#include "de/reader.h"
#include "de/cstring.h"
#include "de/thread.h"
#include "de/writer.h"

#include <atomic>
#include <ctime>
#include <sstream>

namespace de {

static HighPerformanceTimer &highPerfTimer()
{
    static HighPerformanceTimer hpt;
    return hpt;
}

static std::atomic<double> currentHighPerfDelta;

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
        SysTime         = 0x1u,
        HighPerformance = 0x2u
    };

    Flags     flags;
    TimePoint sysTime;
    Span      highPerfElapsed;

    Impl()
        : flags(SysTime | HighPerformance)
        , sysTime(std::chrono::system_clock::now())
        , highPerfElapsed(highPerfTimer().elapsed())
    {}

    Impl(const TimePoint &tp) : flags(tp != TimePoint() ? SysTime : 0), sysTime(tp) {}

    Impl(int) {} // invalid time

    Impl(const Span &span) : flags(HighPerformance), highPerfElapsed(span) {}

    Impl(const Impl &other)
        : flags(other.flags)
        , sysTime(other.sysTime)
        , highPerfElapsed(other.highPerfElapsed)
    {}

    Impl &operator=(const Impl &other)
    {
        flags = other.flags;
        sysTime = other.sysTime;
        highPerfElapsed = other.highPerfElapsed;
        return *this;
    }

    bool isValid() const
    {
        if (flags & SysTime)
        {
            return sysTime != TimePoint();
        }
        return (flags & HighPerformance) != 0;
    }

    bool isLessThan(const Impl &other) const
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
        return systemTime() < other.systemTime();
    }

    bool isEqualTo(const Impl &other) const
    {
        if ((flags & HighPerformance) && (other.flags & HighPerformance))
        {
            return highPerfElapsed == other.highPerfElapsed;
        }
        if ((flags & SysTime) && (other.flags & SysTime))
        {
            return sysTime == other.sysTime;
        }
        return systemTime() == other.systemTime();
    }

    void add(const Span &delta)
    {
        if (flags & SysTime)
        {
            sysTime += std::chrono::milliseconds(delta.asMilliSeconds());
        }
        if (flags & HighPerformance)
        {
            highPerfElapsed += delta;
        }
    }

    Span delta(const Impl &earlier) const
    {
        if ((flags & HighPerformance) && (earlier.flags & HighPerformance))
        {
            return highPerfElapsed - earlier.highPerfElapsed;
        }
        else //if ((flags & SysTime) && (earlier.flags & SysTime))
        {
            //return earlier.dateTime.msecsTo(dateTime) / 1000.0;
            using namespace std::chrono;
            return duration_cast<milliseconds>(systemTime() - earlier.systemTime()).count() / 1.0e3;
        }
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

    struct QDateTimeFormat
    {
        uint32_t julianDay;
        uint32_t msecs;
        uint8_t timezone;
    };

    static Time getQDateTime(const Block &data)
    {
        // This matches Qt's QDataStream format.
        QDateTimeFormat fmt;
        Reader r(data);
        r >> fmt.julianDay >> fmt.msecs >> fmt.timezone;

        return Date::fromJulianDayNumber(fmt.julianDay).asTime() + double(fmt.msecs) / 1.0e3;
    }

    static Block putQDateTime(const Time &time)
    {
        Date date(time);
        QDateTimeFormat fmt;
        fmt.julianDay = date.julianDayNumber();
        fmt.msecs = uint32_t((date.seconds() + date.minutes() * 60 + date.hours() * 3600) * 1000);
        fmt.timezone = 0;

        Block bytes;
        Writer w(bytes);
        w << fmt.julianDay << fmt.msecs << fmt.timezone;
        return bytes;
    }
};

Time::Time() : d(new Impl)
{}

Time::Time(const Time &other) : d(new Impl(*other.d))
{}

Time::Time(Time &&moved) : d(std::move(moved.d))
{}

Time::Time(int year, int month, int day, int hour, int minute, int second)
    : d(new Impl(0 /* init as invalid */))
{
    using namespace std;

    tm t{};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = second;
    t.tm_isdst = -1; // determine if Daylight Saving Time was in effect then
    d->sysTime = chrono::system_clock::from_time_t(mktime(&t));
    d->flags |= Impl::SysTime;
}

Time Time::fromHighPerformanceDelta(const Span &highPerformanceDelta)
{
    Time t = invalidTime();
    t.d->flags |= Impl::HighPerformance;
    t.d->highPerfElapsed = highPerformanceDelta;
    return t;
}

Time::Time(const TimePoint &tp) : d(new Impl(tp))
{}

Time::Time(const iTime &time) : d(new Impl(0 /* init as invalid */))
{
    using namespace std::chrono;
    d->flags |= Impl::SysTime;
    d->sysTime = system_clock::from_time_t(time.ts.tv_sec) +
                 duration_cast<system_clock::duration>(nanoseconds(time.ts.tv_nsec));
}

Time Time::invalidTime()
{
    return Time(TimePoint());
}

Time &Time::operator = (const Time &other)
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

bool Time::operator < (const Time &t) const
{
    return d->isLessThan(*t.d);
}

bool Time::operator == (const Time &t) const
{
    return d->isEqualTo(*t.d);
}

Time Time::operator + (const Span &span) const
{
    Time result = *this;
    result += span;
    return result;
}

Time &Time::operator += (const Span &span)
{
    d->add(span);
    return *this;
}

TimeSpan Time::operator - (const Time &earlierTime) const
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
            return asText("%F %T") + stringf(".%03u", ms % 1000).c_str();
        }
        else if (format == ISODateOnly)
        {
            return asText("%F");
        }
        else if (format == FriendlyFormat)
        {
            // Wed May 20 03:40:13 1998
            // return asText("%a %b %d %H:%M:%S %Y");

            const Date today = Date::currentDate();
            const Date date = asDate();

            // Is it today?
            if (date.isSameDay(today))
            {
                return asText("%H:%M");
            }
            else if (date.year() == today.year())
            {
                return asText("%b %d %H:%M");
            }
            else
            {
                return asText("%Y %b %d");
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
                prefix = Stringf("#%-4d ", asBuildNumber());
            }
            if (hours > 0)
            {
                return Stringf("%s%ih%7.3f", prefix.c_str(), hours, sec);
            }
            return Stringf("%s%7.3f", prefix.c_str(), sec);
        }
        else
        {
            auto ms = millisecondsSinceEpoch();
            return Stringf("#%-4d ", asBuildNumber()) + asText("%H:%M:%S") +
                   stringf(".%03i", ms % 1000).c_str();
        }
    }
    if (d->flags.testFlag(Impl::HighPerformance))
    {
        return Stringf("+%.3f sec", ddouble(d->highPerfElapsed));
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
    static const char *months[] = {
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

Time Time::parse(const String &text, const char *format) // static
{
    const Date currentDate = Date::currentDate();

    int year = 0, month = 0, day = 0, hour = 0, minute = 0;

    const CString fmt{format};
    const char *pos = fmt.begin();
    const char *end = fmt.end();
    std::istringstream is(text.strip().c_str());
    while (pos != end)
    {
        if (!iCmpStrN(pos, "yyyy", 4))
        {
            pos += 4;
            is >> year;
        }
        else if (!iCmpStrN(pos, "yy", 2))
        {
            pos += 2;
            is >> year;
            year += (year >= 70 ? 1900 : 2000);
        }
        else if (!iCmpStrN(pos, "MMM", 3))
        {
            char name[4]{};
            pos += 3;
            is >> name[0] >> name[1] >> name[2];
            if (is.fail()) return Time::invalidTime();
            month = parseMonth(name);
            if (!month) return Time::invalidTime();
        }
        else if (!iCmpStrN(pos, "MM", 2))
        {
            pos += 2;
            is >> month;
        }
        else if (!iCmpStrN(pos, "M", 1))
        {
            pos += 1;
            is >> month;
        }
        else if (!iCmpStrN(pos, "dd", 2))
        {
            pos += 2;
            is >> day;
        }
        else if (!iCmpStrN(pos, "d", 1))
        {
            pos += 1;
            is >> day;
        }
        else if (!iCmpStrN(pos, "hh", 2))
        {
            pos += 2;
            is >> hour;
        }
        else if (!iCmpStrN(pos, "hh", 2))
        {
            pos += 2;
            is >> hour;
        }
        else if (!iCmpStrN(pos, "mm", 2))
        {
            pos += 2;
            is >> hour;
        }
        else
        {
            // Any character.
            char ch = 0;
            is >> ch;
            if (*pos != ch) return Time::invalidTime();
            pos++;
        }
        if (is.fail()) return Time::invalidTime();
    }

    // Assume missing date values.
    if (year == 0)
    {
        year = currentDate.year();
    }
    if (month == 0)
    {
        month = 1;
    }
    if (day == 0)
    {
        day = 1;
    }

    // In the valid range?
    if (month < 1 || month > 12) return Time::invalidTime();
    if (day   < 1 || day   > 31) return Time::invalidTime();

    return Time(year, month, day, hour, minute, 0);
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
    {
        int year = 0, month = 0, mday = 0;
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
                hour   = parts[2].left(BytePos(2)).toInt();
                minute = parts[2].right(BytePos(2)).toInt();
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
        const String normText = text.normalizeWhitespace();
        for (const char *fmt : {
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
                 "MMM d hh:mm",
                 "MMM d yyyy",
             })
        {
            const Time parsed = parse(normText, fmt);
            if (parsed.isValid())
            {
                return parsed;
            }
        }
    }
    return Time::invalidTime();
}

Date Time::asDate() const
{
    return Date(*this);
}

// Flags for serialization.
static const duint8 HAS_DATETIME  = 0x01;
static const duint8 HAS_HIGH_PERF = 0x02;

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
    return fromHighPerformanceDelta(currentHighPerfDelta.load());
}

void Time::updateCurrentHighPerformanceTime() // static
{
    currentHighPerfDelta.store(highPerfTimer().elapsed());
}

Time::Span Time::currentHighPerformanceDelta()
{
    return currentHighPerfDelta.load();
}

std::ostream &operator<<(std::ostream &os, const Time &t)
{
    return os << t.asText();
}

} // namespace de
