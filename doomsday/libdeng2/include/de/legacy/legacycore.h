/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LEGACYCORE_H
#define LIBDENG2_LEGACYCORE_H

#include <QCoreApplication>

namespace de {

/**
 * Transitional kernel for supporting legacy Dooomsday C code to access
 * libdeng2 functionality. The legacy engine needs to construct one of
 * these via the deng2 C API and make sure it gets destroyed at shutdown.
 */
class LegacyCore
{
public:
    LegacyCore();

    /**
     * Starts the libdeng2 kernel. A deng2 event loop is sta
     */
    void start();

private:
    QCoreApplication* _app;
};

} // namespace de

#endif // LIBDENG2_LEGACYCORE_H
