/*
 * The Doomsday Engine Project -- Hawthorn
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

#ifndef DMETHODSTATEMENT_HH
#define DMETHODSTATEMENT_HH

#include "dstatement.hh"
#include "dmethod.hh"
#include "dcompound.hh"
#include "ddictionaryexpression.hh"

#include <list>
#include <string>

namespace de
{
    class Expression;
    
/**
 * MethodStatement creates a new method when it is executed.
 */
    class MethodStatement : public Statement
    {
    public:
        MethodStatement(const std::string& path);
        
        ~MethodStatement();

        const std::string& path() { return path_; }

        void addArgument(const std::string& argName, Expression* defaultValue = 0);

        /// Returns the statement compound of the method.
        Compound& compound() { return compound_; }
        
        void execute(Context& context) const;
        
    private:
        std::string path_;
        Compound compound_;

        Method::Arguments arguments_;
        
        /// Expression that evaluates into the default values of the method.
        DictionaryExpression defaults_;
    };
}

#endif /* DMETHODSTATEMENT_HH */
