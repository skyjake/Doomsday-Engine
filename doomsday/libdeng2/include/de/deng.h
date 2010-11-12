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

/**
 * @file deng.h
 *
 * Contains common definitions, constants, and types that will be used by libdeng2. 
 * The deng types should be used instead of the standard C++ types.
 */ 

#ifndef LIBDENG2_H
#define LIBDENG2_H

#include <QtCore/qglobal.h>
#include <QScopedPointer>

#include <typeinfo> // Standard C++ RTTI

// Exporting and importing symbols.
#if defined(LIBDENG2_EXPORTS)
#  define LIBDENG2_API Q_DECL_EXPORT
#else
#  define LIBDENG2_API Q_DECL_IMPORT
#endif

#define LIBDENG2_EXPORT Q_DECL_EXPORT

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define TYPE_NAME(x) (typeid(x).name())

/**
 * Macro for iterating through an STL container.
 *
 * @param Var           Name of the iterator variable.
 * @param ContainerRef  Container.
 * @param IterClass     Class of the iterator.
 */
#define FOR_EACH(Var, ContainerRef, IterClass) \
    for(IterClass Var = ContainerRef.begin(); Var != ContainerRef.end(); ++Var)

/**
 * Macro for iterating through an STL container in reverse.
 *
 * @param Var           Name of the iterator variable.
 * @param ContainerRef  Container.
 * @param IterClass     Class of the iterator.
 */
#define FOR_EACH_REVERSE(Var, ContainerRef, IterClass) \
    for(IterClass Var = ContainerRef.rbegin(); Var != ContainerRef.rend(); ++Var)

/**
 * @namespace de
 *
 * The @c de namespace contains all the classes, functions and other
 * identifiers of libdeng2.
 */
namespace de 
{
    /**
     * @defgroup types Basic Types
     * 
     * Basic data types.
     */
    
    //@{
    /// @ingroup types
    typedef qint8                   dchar;      ///< 8-bit signed integer.
    typedef quint8                  dbyte;      ///< 8-bit unsigned integer.
    typedef quint8                  duchar;     ///< 8-bit unsigned integer.
    typedef dchar                   dint8;      ///< 8-bit signed integer.
    typedef dbyte                   duint8;     ///< 8-bit unsigned integer.
    typedef qint16                  dint16;     ///< 16-bit signed integer.
    typedef quint16                 duint16;    ///< 16-bit unsigned integer.
    typedef dint16                  dshort;     ///< 16-bit signed integer.
    typedef duint16                 dushort;    ///< 16-bit unsigned integer.
    typedef qint32                  dint32;     ///< 32-bit signed integer.
    typedef quint32                 duint32;    ///< 32-bit unsigned integer.
    typedef dint32                  dint;       ///< 32-bit signed integer.
    typedef duint32                 duint;      ///< 32-bit unsigned integer.
    typedef qint64                  dint64;     ///< 64-bit signed integer.
    typedef quint64                 duint64;    ///< 64-bit unsigned integer.
    typedef float                   dfloat;     ///< 32-bit floating point number.
    typedef qreal                   ddouble;    ///< 64-bit floating point number.
    typedef quint64                 dsize;
    //@}
}

#include "Error"
#include "version.h"
#include "math.h"

#endif /* LIBDENG2_H */
