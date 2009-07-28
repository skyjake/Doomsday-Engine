/**
 * @mainpage The Doomsday Engine Project: libdeng2
 *
 * Copyright &copy; 2009 Jaakko Ker&auml;nen <jaakko.keranen@iki.fi>
 *
 * <p>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but <b>without any warranty</b>; without even the implied warranty of
 * <b>merchantability</b> or <b>fitness for a particular purpose</b>.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/
 *
 * <h2>Table of Contents</h2>
 *
 * <h3>Main Modules</h3>
 * - @ref core
 * - @ref fs
 * - @ref net
 *      - @ref protocol
 * - @ref video
 *
 * <h3>Supporting Modules</h3>
 * - @ref types
 * - @ref data
 * - @ref errors
 */

/**
 * @file deng.h
 *
 * Header file that contains common definitions, constants, and types that
 * will be used by libdeng2. The deng types should be used instead of the 
 * standard C++ types.
 */ 

#ifndef LIBDENG2_H
#define LIBDENG2_H

#include <cassert>
#include <iostream>
#include <typeinfo>
#include <memory>

#ifdef WIN32
#   ifdef LIBDENG2_EXPORTS
#       define LIBDENG2_API __declspec(dllexport)
#   else
#       define LIBDENG2_API __declspec(dllimport)
#   endif
#   define LIBDENG2_EXPORT __declspec(dllexport)
    // Disable warnings about non-exported (C++ standard library) base classes.
#   pragma warning(disable: 4275)
#   pragma warning(disable: 4251)
    // Disable warning about using this pointer in initializer list.
#   pragma warning(disable: 4355)
#else
#   define LIBDENG2_API
#   define LIBDENG2_EXPORT
#endif

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define TYPE_NAME(x) (typeid(x).name())

/**
 * @namespace de
 *
 * The @c de namespace contains all the classes, functions and other
 * identifiers of libdeng2.
 */
namespace de 
{
    /**
     * @defgroup types Basic Data Types
     * 
     * Basic data types.
     * 
     * @todo Use native types that are guaranteed to have the right size.
     */
    
    //@{
    /// @ingroup types
	typedef char	    		    dchar;	    ///< 8-bit signed integer.
	typedef unsigned char		    dbyte;	    ///< 8-bit unsigned integer.
	typedef unsigned char		    duchar;     ///< 8-bit unsigned integer.
    typedef dchar                   dint8;      ///< 8-bit signed integer.
    typedef dbyte                   duint8;     ///< 8-bit unsigned integer.
	typedef signed short int	    dint16;     ///< 16-bit signed integer.
	typedef unsigned short int	    duint16;    ///< 16-bit unsigned integer.
	typedef dint16          	    dshort;     ///< 16-bit signed integer.
	typedef duint16	                dushort;    ///< 16-bit unsigned integer.
	typedef signed int	            dint32;	    ///< 32-bit signed integer.
	typedef unsigned long int	    duint32;	///< 32-bit unsigned integer.
	typedef dint32  	            dint;	    ///< 32-bit signed integer.
	typedef duint32	                duint;	    ///< 32-bit unsigned integer.
	typedef signed long long int    dint64;     ///< 64-bit signed integer.
	typedef unsigned long long int  duint64;    ///< 64-bit unsigned integer.
	typedef float				    dfloat;     ///< 32-bit floating point number.
	typedef double				    ddouble;    ///< 64-bit floating point number.
    typedef size_t                  dsize;
    //@}
}

#include <de/Error>
#include "version.h"

#endif /* LIBDENG2_H */
