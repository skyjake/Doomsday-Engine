/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

namespace de
{
    /**
     * Information about a date.
     *
     * @ingroup types
     */
    class LIBDENG2_API Date
    {
    public:
        /**
         * Constructs a new Date out of the current time.
         */
        Date();        
        
        Date(const Time& time);
        
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
        
        dint microSeconds;
        dint seconds;
        dint minutes;
        dint hours;
        dint month;
        dint year;
        dint weekDay;
        dint dayOfMonth;
        dint dayOfYear;
    };
    
    LIBDENG2_API std::ostream& operator << (std::ostream& os, const Date& date);
}

#endif /* LIBDENG2_DATE_H */
