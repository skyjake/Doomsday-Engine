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

#ifndef LIBDENG2_ERROR_H
#define LIBDENG2_ERROR_H

#include <string>
#include <stdexcept>
#include <de/deng.h>

namespace de
{
	class PUBLIC_API Error : public std::runtime_error
	{
	public:
		Error(const std::string& where, const std::string& message)
			: std::runtime_error(std::string("(") + where + ") " + message) {}
	};
}
	
/**
 * Macro for defining an exception class that belongs to a parent group of
 * exceptions.  This should be used so that whoever uses the class
 * that throws an exception is able to choose the level of generality
 * of the caught errors.
 */
#define DEFINE_SUB_ERROR(Parent, Name) \
	class PUBLIC_API Name : public Parent \
	{ \
	public: \
		Name(const std::string& message) \
			: Parent("-", message) {} \
		Name(const std::string& where, const std::string& message) \
			: Parent(where, message) {}							   \
	}

/**
 * Define a top-level exception class.
 */
#define DEFINE_ERROR(Name) DEFINE_SUB_ERROR(de::Error, Name)

#endif /* LIBDENG2_ERROR_H */
