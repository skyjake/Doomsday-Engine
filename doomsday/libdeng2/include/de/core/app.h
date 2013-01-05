/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_APP_H
#define LIBDENG2_APP_H

#include "../libdeng2.h"
#include "../CommandLine"
#include "../NativePath"
#include "../LogBuffer"
#include "../FS"
#include "../Module"
#include "../Config"
#include "../UnixInfo"
#include <QApplication>

/**
 * Macro for conveniently accessing the de::App singleton instance.
 */
#define DENG2_APP   (static_cast<de::App *>(qApp))

namespace de {

class Archive;

/**
 * Application whose event loop is protected against uncaught exceptions.
 * Catches the exception and shuts down the app cleanly.
 */
class DENG2_PUBLIC App : public QApplication
{
    Q_OBJECT

public:
    /// The object or resource that was being looked for was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    enum GUIMode {
        GUIDisabled = 0,
        GUIEnabled = 1
    };

    enum SubsystemInitFlag {
        DefaultSubsystems   = 0x0,
        DisablePlugins      = 0x1
    };
    Q_DECLARE_FLAGS(SubsystemInitFlags, SubsystemInitFlag)

public:
    /**
     * Construct an App instance. The application will not be fully usable
     * until initSubsystems() has been called -- you should call
     * initSubsystems() as soon as possible after construction. Never throws
     * an exception.
     *
     * @param argc     Argument count. Note that App holds the reference.
     * @param argv     Arguments.
     * @param guiMode  GUI can be enabled (client) or disabled (server).
     */
    App(int &argc, char **argv, GUIMode guiMode);

    ~App();

    bool notify(QObject *receiver, QEvent *event);

    /**
     * Finishes App construction by initializing all the application's
     * subsystems. This includes Config and FS. Has to be called manually in
     * the application's initialization routine. An exception will be thrown if
     * initialization cannot be successfully completed.
     *
     * @param flags  How to/which subsystems to initialize.
     */
    void initSubsystems(SubsystemInitFlags flags = DefaultSubsystems);

    /**
     * Adds a native module to the set of modules that can be imported in
     * scripts.
     *
     * @param name    Name of the module.
     * @param module  Module namespace. App will observe this for deletion.
     */
    void addNativeModule(String const &name, Record &module);

    static App &app();

    /**
     * Returns the command line used to start the application.
     */
    static CommandLine &commandLine();

    /**
     * Returns the absolute native path of the application executable.
     */
    static NativePath executablePath();

#ifdef MACOSX
    /**
     * Returns the native path of the application bundle contents.
     */
    NativePath nativeAppContentsPath();
#endif

    /**
     * Returns the native path of the data base folder.
     *
     * In libdeng2, the base folder is the location where all the common
     * data files are located, e.g., /usr/share/doomsday on Linux.
     */
    NativePath nativeBasePath();

    /**
     * Returns the native path of where to load binaries (plugins). This
     * is where "/bin" points to.
     */
    NativePath nativePluginBinaryPath();

    /**
     * Returns the native path where user-specific runtime files should be
     * placed (this is where "/home" points to). The user can override the
     * location using the @em -userdir command line option.
     */
    NativePath nativeHomePath();

    /**
     * Returns the archive for storing persistent engine state into. Typically
     * the contents are updated when subsystems are being shut down. When the
     * file system is being shut down, the <code>/home/persist.pack</code>
     * package is written to disk.
     *
     * @return Persistent data archive.
     */
    static Archive &persistentData();

    /**
     * Returns the application's current native working directory.
     */
    static NativePath currentWorkPath();

    /**
     * Changes the application's current native working directory.
     *
     * @param cwd  New working directory for the application.
     *
     * @return @c true, if the current working directory was changed,
     * otherwise @c false.
     */
    static bool setCurrentWorkPath(NativePath const &cwd);

    /**
     * Returns the application's file system.
     */
    static FS &fileSystem();

    /**
     * Returns the root folder of the file system.
     */
    static Folder &rootFolder();

    /**
     * Returns the /home folder.
     */
    static Folder &homeFolder();

    /**
     * Returns the configuration.
     */
    static Config &config();

    /**
     * Returns the Unix system-level configuration preferences.
     */
    static UnixInfo &unixInfo();

    /**
     * Imports a script module that is located on the import path.
     *
     * @param name      Name of the module.
     * @param fromPath  Absolute path of the script doing the importing.
     *
     * @return  The imported module.
     */
    static Record &importModule(String const &name, String const &fromPath = "");

    /**
     * Emits the displayModeChanged() signal.
     *
     * @todo In the future when de::App (or a sub-object owned by it) is
     * responsible for display modes, this should be handled internally and
     * not via this public interface where anybody can call it.
     */
    void notifyDisplayModeChanged();

signals:
    void uncaughtException(QString message);

    /// Emitted when the display mode has changed.
    void displayModeChanged();

private:
    struct Instance;
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(App::SubsystemInitFlags)

} // namespace de

#endif // LIBDENG2_APP_H
