/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_CONFIG_H
#define LIBCORE_CONFIG_H

#include "de/string.h"
#include "de/path.h"
#include "de/version.h"
#include "de/record.h"
#include "de/scripting/iobject.h"
#include "de/scripting/process.h"

namespace de {

class ArrayValue;

/**
 * Stores the configuration of everything.
 *
 * The application owns a Config. The default configuration is produced by executing the
 * .de scripts in the config directories. The resulting namespace is serialized for
 * storage, and is restored from the serialized version directly before the config
 * scripts are run.
 *
 * The version of the engine is stored in the serialized config namespace. This is for
 * actions needed when upgrading: the config script can check the previous version and
 * apply changes accordingly.
 *
 * In practice, Config is a specialized script namespace stored in a Record. It gets
 * written to the application's persistent data store (persist.pack) using a Refuge. The
 * Config is automatically written persistently before being destroyed.
 *
 * @ingroup core
 */
class DE_PUBLIC Config : public RecordAccessor, public IObject
{
public:
    enum ReadStatus { WasNotRead, SameVersion, DifferentVersion };

public:
    /**
     * Constructs a new configuration.
     *
     * @param path  Name of the configuration file to read.
     */
    Config(const Path &path);

    /// Read configuration from files.
    ReadStatus read();

    /// Writes the configuration to /home.
    void write() const;

    void writeIfModified() const;

    /**
     * Sets the value of a variable, creating the variable if needed.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Value for the variable.
     *
     * @return Variable whose value was set.
     */
    Variable &set(const String &name, bool value);

    /// @copydoc set()
    Variable &set(const String &name, const Value::Text &value);

    /// @copydoc set()
    Variable &set(const String &name, const Value::Number &value);

    /// @copydoc set()
    Variable &set(const String &name, dint value);

    /// @copydoc set()
    Variable &set(const String &name, duint value);

    /**
     * Sets the value of a variable, creating the variable if it doesn't exist.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Array to use as the value of the variable. Ownership taken.
     */
    Variable &set(const String &name, ArrayValue *value);

    /**
     * Returns the old version, when a new installed version has been detected.
     * If no upgrade has occurred, returns the current version.
     */
    Version upgradedFromVersion() const;

    // Implements IObject.
    Record &objectNamespace();
    const Record &objectNamespace() const;

    static Config &get();
    static Variable &get(const String &name);
    static bool exists();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_CONFIG_H */
