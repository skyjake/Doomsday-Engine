/** @file precompiled.h  Precompiled headers for Doomsday Server.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef min
#  undef max
#endif

#ifdef __cplusplus

// C++ standard library:
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

// Qt:
#include <QtCore/qglobal.h>
#include <QtAlgorithms>
#include <QBitArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QFlags>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QMutableMapIterator>
#include <QMutex>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QVarLengthArray>
#include <QVector>

// Doomsday SDK:
#include <de/app.h>
#include <de/error.h>
#include <de/log.h>
#include <de/legacy/memory.h>
#include <de/nativepath.h>
#include <de/observers.h>
#include <de/pathtree.h>
#include <de/reader.h>
#include <de/rectangle.h>
#include <de/string.h>
#include <de/vector.h>
#include <de/writer.h>

#endif
