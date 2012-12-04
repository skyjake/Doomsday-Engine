/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_CONFIG_H
#define LIBDENG2_CONFIG_H

#include "../Process"
#include "../String"
#include "../Path"

namespace de
{
    /**
     * Stores the configuration of everything. The application owns a Config.
     * The default configuration is produced by executing the .de scripts
     * in the config directories. The resulting namespace is serialized for 
     * storage, and is restored from the serialized version directly before the
     * config scripts are run.
     *
     * The version of the engine is stored in the serialized config namespace.
     * This is for actions needed when upgrading: the config script can check
     * the previous version and apply changes accordingly.
     */
    class DENG2_PUBLIC Config
    {
    public:
        /// Attempted to get the value of a variable while expecting the wrong type. @ingroup errors
        DENG2_ERROR(ValueTypeError);

    public:
        /**
         * Constructs a new configuration.
         *
         * @param path  Name of the configuration file to read.
         */
        Config(Path const &path);

        /**
         * Destructor. The configuration is automatically saved.
         */
        virtual ~Config();

        /// Read configuration from files.
        void read();
        
        /// Writes the configuration to /home.
        void write();

        /// Returns the value of @a name as a Value.
        Value &get(String const &name);

        /// Returns the value of @a name as an integer.
        dint geti(String const &name);

        /// Returns the value of @a name as a boolean.
        bool getb(String const &name);

        /// Returns the value of @a name as an unsigned integer.
        duint getui(String const &name);

        /// Returns the value of @a name as a double-precision floating point number.
        ddouble getd(String const &name);

        /// Returns the value of @a name as a string.
        String gets(String const &name);

        template <typename ValueType>
        ValueType &getAs(String const &name) {
            ValueType *v = dynamic_cast<ValueType *>(&get(name));
            if(!v)
            {
                throw ValueTypeError("Config::getAs", String("Cannot cast to expected type (") +
                                     DENG2_TYPE_NAME(ValueType) + ")");
            }
            return *v;
        }
    
        /**
         * Returns the configuration namespace.
         */
        Record &names();
        
    private:
        struct Instance;
        Instance *d;
    };
}

#endif /* LIBDENG2_CONFIG_H */
