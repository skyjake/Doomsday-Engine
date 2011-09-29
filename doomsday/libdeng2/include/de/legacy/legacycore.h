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

namespace de {

/**
 * Transitional kernel for supporting legacy Dooomsday C code in accessing
 * libdeng2 functionality. The legacy engine needs to construct one of these
 * via the deng2 C API and make sure it gets destroyed at shutdown. The C API
 * can be used to access functionality in LegacyCore.
 */
class LegacyCore
{
public:
    /**
     * Initializes the legacy core.
     *
     * @param argc  Reference to the application's command line argument
     *              count. Note that this may be modified in this method.
     * @param argv  Command line arguments of the application. Note that
     *              this may be modified in the this method.
     */
    LegacyCore(int& argc, char** argv);

    ~LegacyCore();

    /**
     * Starts the libdeng2 kernel. A deng2 event loop is started in a separate
     * thread, where deng2-specific events are handled.
     */
    void start();

    /**
     * Stops the kernel. This is automatically called when the core is
     * destroyed.
     */
    void stop();

private:
    // Private instance data.
    struct Instance;
    Instance* d;
};

} // namespace de

#endif // LIBDENG2_LEGACYCORE_H
