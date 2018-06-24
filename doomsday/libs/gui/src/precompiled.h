/** @file libgui/src/precompiled.h  Precompiled headers for libgui.
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
#include <vector>

/*
// Qt:
#include <QtCore/qglobal.h>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QList>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <QPainter>
#include <QPoint>
#include <QRect>
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
*/

#endif
