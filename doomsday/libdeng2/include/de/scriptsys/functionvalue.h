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

#ifndef LIBDENG2_FUNCTIONVALUE_H
#define LIBDENG2_FUNCTIONVALUE_H

#include "../Value"
#include "../Function"

namespace de
{
    /**
     * Holds a reference to a function and provides a way to call the function.
     *
     * @ingroup script
     */
    class FunctionValue : public Value
    {
    public:
        FunctionValue();
        FunctionValue(Function *func);
        ~FunctionValue();
        
        /// Returns the function.
        Function const &function() const { return *_func; }
       
        Value *duplicate() const;
        Text asText() const;
        bool isTrue() const;
        bool isFalse() const;
        dint compare(Value const &value) const;
        void call(Process &process, Value const &arguments) const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);         
        
    private:
        Function *_func;
    };
}

#endif /* LIBDENG2_FUNCTIONVALUE_H */
