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

#include "../deng.h"

#include <string>
#include <ctime>

namespace de
{
    class Date;
    
    /**
     * The Time class represents a single time measurement. It represents
     * one absolute point in time (since the epoch).  Instances of Time should be
     * used wherever time needs to be measured, calculated or stored.
     *
     * @ingroup types
     */
    class LIBDENG2_API Time
    {   
    public: 
        /**
         * The difference between to points of time.
         *
         * @ingroup types
         */ 
        class LIBDENG2_API Delta
        {
        public:
            /**
             * Constructs a time delta.
             *
             * @param seconds  Length of the delta.
             */
            Delta(ddouble seconds = 0) : seconds_(seconds) {}
            
            /**
             * Conversion to the numeric type (floating-point seconds).
             */
            operator const ddouble() const { return seconds_; }
            
            bool operator < (const ddouble& d) const {
                return seconds_ < d;
            }
            
            bool operator > (const ddouble& d) const {
                return seconds_ > d;
            }

            Delta operator + (const ddouble& d) const;
            
            Delta operator - (const ddouble& d) const;
            
            /**
             * Convert the delta to milliseconds.  
             *
             * @return  Milliseconds.
             */
            duint64 asMilliSeconds() const;
            
            /**
             * Suspend execution for a period of time.
             */
            void sleep() const;
            
        private:
            ddouble seconds_;
        };
        
    public:
        /** 
         * Constructor initializes the time to the current time.
         */
        Time();
        
        Time(const time_t& t, dint m = 0) : time_(t), micro_(m) {}

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
         *
         * @return  Delta.
         */
        Delta since() const { return deltaTo(Time()); }
        
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
        std::string asText() const;
        
        /**
         * Converts the time into a Date.
         */
        Date asDate() const;

    public:
        static void sleep(const Time::Delta& delta) { delta.sleep(); }

    private:
        time_t time_;
        dint micro_;

        friend class Date;
    };
    
    LIBDENG2_API std::ostream& operator << (std::ostream& os, const Time& t);
}

#endif /* LIBDENG2_TIME_H */
