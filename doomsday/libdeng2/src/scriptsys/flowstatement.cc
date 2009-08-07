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

#include "de/FlowStatement"
#include "de/Evaluator"
#include "de/Context"
#include "de/Process"
#include "de/Expression"
#include "de/Value"
#include "de/Writer"
#include "de/Reader"

using namespace de;

#define HAS_ARG     0x80
#define TYPE_MASK   0x7f
 
FlowStatement::FlowStatement() : type_(PASS), arg_(0)
{}
 
FlowStatement::FlowStatement(Type type, Expression* countArgument) 
    : type_(type), arg_(countArgument) 
{}
 
FlowStatement::~FlowStatement()
{
    delete arg_;
}
         
void FlowStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();
    
    switch(type_)
    {
    case PASS:
        context.proceed();
        break;
        
    case CONTINUE:
        context.jumpContinue();
        break;
        
    case BREAK:
        if(arg_)
        {
            context.jumpBreak(dint(eval.evaluate(arg_).asNumber()));
        }
        else
        {
            context.jumpBreak();
        }        
        break;
        
    case RETURN:
        if(arg_)
        {
            eval.evaluate(arg_);
            context.process().finish(eval.popResult());
        }
        else
        {
            context.process().finish();
        }
        break;
        
    case THROW:
        if(arg_)
        {
            throw Error("thrown in script", eval.evaluate(arg_).asText());
        }   
        else
        {
            /// @todo  Rethrow the current error.
            context.proceed();
        }     
    }
}

void FlowStatement::operator >> (Writer& to) const
{
    to << SerialId(FLOW);
    duint8 header = duint8(type_);
    if(arg_)
    {
        header |= HAS_ARG;
    }
    to << header;
    if(arg_)
    {
        to << *arg_;
    }
}

void FlowStatement::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != FLOW)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("FlowStatement::operator <<", "Invalid ID");
    }
    duint8 header;
    from >> header;
    type_ = Type(header & TYPE_MASK);
    if(header & HAS_ARG)
    {
        delete arg_;
        arg_ = 0;
        arg_ = Expression::constructFrom(from);
    }
}
