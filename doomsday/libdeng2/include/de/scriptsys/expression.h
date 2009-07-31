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

#ifndef DEXPRESSION_HH
#define DEXPRESSION_HH

#include <vector>

namespace de
{
	class Evaluator;
	class Value;
    class Object;

/**
 * Base class for expressions.
 */
	class Expression
	{
	public:
		virtual ~Expression();

		virtual void push(Evaluator& evaluator, Object* names = 0) const;
		
		virtual Value* evaluate(Evaluator& evaluator) const = 0;
	};
}

#endif
