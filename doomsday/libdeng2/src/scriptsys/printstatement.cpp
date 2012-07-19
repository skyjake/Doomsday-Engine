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
#include "de/Log"

#include <QTextStream>

using namespace de;

PrintStatement::PrintStatement(ArrayExpression* arguments) : _arg(arguments)
{
    if(!_arg)
    {
        _arg = new ArrayExpression();
    }
}

PrintStatement::~PrintStatement()
{
    delete _arg;
}

void PrintStatement::execute(Context& context) const
{
    ArrayValue& value = context.evaluator().evaluateTo<ArrayValue>(_arg);

    String msg;
    QTextStream os(&msg);
    bool isFirst = true;
            
    DENG2_FOR_EACH_i(value.elements(), ArrayValue::Elements::const_iterator)
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
    
    LOG_MSG("") << msg;
    
    context.proceed();
}

void PrintStatement::operator >> (Writer& to) const
{
    to << SerialId(PRINT) << *_arg;
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
    from >> *_arg;
}
