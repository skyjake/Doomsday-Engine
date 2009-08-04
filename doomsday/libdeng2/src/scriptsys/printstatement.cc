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

#include "de/PrintStatement"
#include "de/Context"
#include "de/ArrayValue"
#include "de/ArrayExpression"
#include "de/Writer"
#include "de/Reader"

#include <sstream>

using namespace de;

PrintStatement::PrintStatement(ArrayExpression* arguments) : arg_(arguments)
{
    if(!arg_)
    {
        arg_ = new ArrayExpression();
    }
}

PrintStatement::~PrintStatement()
{
    delete arg_;
}

void PrintStatement::execute(Context& context) const
{
    ArrayValue& value = context.evaluator().evaluateTo<ArrayValue>(arg_);

    std::ostringstream os;
    bool isFirst = true;
            
    for(ArrayValue::Elements::const_iterator i = value.elements().begin();
        i != value.elements().end(); ++i)
    {
       if(!isFirst)
       {
           os << " ";
       }
       else
       {
           isFirst = false;
       }
       os << (*i)->asText();
    }
    
    /// @todo  Use the standard Doomsday output stream.
    std::cout << os.str() << "\n";
    
    context.proceed();
}

void PrintStatement::operator >> (Writer& to) const
{
    to << SerialId(PRINT) << *arg_;
}

void PrintStatement::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != PRINT)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("PrintStatement::operator <<", "Invalid ID");
    }
    from >> *arg_;
}
