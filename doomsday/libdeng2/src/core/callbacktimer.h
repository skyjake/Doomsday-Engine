/**
 * @file callbacktimer.h
 * Internal helper class for making callbacks to C code.
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG2_CALLBACKTIMER_H
#define LIBDENG2_CALLBACKTIMER_H

#include <QTimer>

namespace de {
namespace internal {

/**
 * Helper for making timed callbacks to C code.
 */
class CallbackTimer : public QTimer
{
    Q_OBJECT

public:
    explicit CallbackTimer(void (*func)(void), QObject *parent = 0);
    
public slots:
    void callbackAndDeleteLater();
    
private:
    void (*_func)(void);
};

} // internal
} // de

#endif // LIBDENG2_CALLBACKTIMER_H
