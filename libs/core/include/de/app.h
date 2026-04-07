/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_APP_H
#define LIBCORE_APP_H

#include "de/libcore.h"
#include "de/clock.h"
#include "de/fileindex.h"
#include "de/package.h"

/**
 * Macro for conveniently accessing the de::App singleton instance.
 */
#define DE_APP   (&de::App::app())

#if defined (DE_ASSERT_IN_MAIN_THREAD)
#  undef DE_ASSERT_IN_MAIN_THREAD
#endif

#define DE_ASSERT_IN_MAIN_THREAD()   DE_ASSERT(de::App::inMainThread())

namespace de {

class Archive;
class ArchiveFolder;
class CommandLine;
class Config;
class Event;
class FileSystem;
class Folder;
class LogBuffer;
class LogFilter;
class MetadataBank;
class Module;
class Path;
class NativePath;
class PackageLoader;
class ScriptSystem;
class System;
class UnixInfo;

namespace filesys { class RemoteFeedRelay; }
namespace game { class Game; }

/**
 * Represents the application and its subsystems. This is the common
 * denominator (and abstract base class) for GUI and non-GUI apps. de::App is
 * not usable on its own; instead you must use one of the derived variants.
 * @ingroup core
 *
 * @see GuiApp, TextApp
 */
class DE_PUBLIC App : DE_OBSERVES(Clock, TimeChange)
{
public:
    enum SubsystemInitFlag {
        DefaultSubsystems     = 0x0,
        //DisablePlugins        = 0x1,
        DisablePersistentData = 0x2
    };
    using SubsystemInitFlags = Flags;

    /// Attempting to access persistent data when it has been disabled at init.
    /// @ingroup errors
    DE_ERROR(PersistentDataNotAvailable);

    /// Asset with given identifier was not found. @ingroup errors
    DE_ERROR(AssetNotFoundError);

    /**
     * Notified when application startup has been fully completed.
     */
    DE_AUDIENCE(StartupComplete, void appStartupCompleted())

    static const char *ORG_NAME;    ///< Name of the author/organization.
    static const char *ORG_DOMAIN;  ///< Network domain of the author/organization.
    static const char *APP_NAME;    ///< Name of the application, as presented to humans.
    static const char *APP_VERSION; ///< Version of the application.
    static const char *CONFIG_PATH;
    static const char *UNIX_HOME;

public:
    /**
     * Construct an App instance. The application will not be fully usable
     * until initSubsystems() has been called -- you should call
     * initSubsystems() as soon as possible after construction. Never throws an
     * exception.
     *
     * @param appFilePath  Path of the application binary.
     * @param args         Arguments.
     */
    App(const StringList &args);

    virtual ~App();

    /**
     * Metadata about the application (META_* as keys).
     */
    Record &metadata();

    /**
     * Returns the application metadata.
     */
    const Record &metadata() const;

    /**
     * Add a new package to be loaded at initialization time. Call this before
     * initSubsystems().
     *
     * @param identifier  Package identifier.
     */
    void addInitPackage(const String &identifier);

    /**
     * Sets the path of the configuration script that will be automatically run if needed
     * during application launch. The script governs the contents of the special
     * persistent Config module. @see Config
     *
     * This method must be called before initSubsystems().
     *
     * @param path  Location of the @em Config.ds script file. The default path of the
     *              script is "/modules/Config.ds".
     */
//    void setConfigScript(const Path &path);

    /**
     * Sets the name of the application. Derived classes should call this from their
     * implementation of setMetadata().
     *
     * @param appName  Application name. Defaults to "Doomsday Engine".
     */
//    void setName(const String &appName);

    /**
     * Sets the Unix-style home folder name. For instance, ".doomsday" could be used.
     *
     * @param name  Name of the (usually hidden) user-specific home data folder.
     */
    void setUnixHomeFolderName(const String &name);

    String unixHomeFolderName() const;

    /**
     * Returns the home folder name without the possible dot in the beginning.
     */
    String unixEtcFolderName() const;

    /**
     * Returns the reverse domain name of the application. This is based on the
     * ORG_DOMAIN metadata and the Unix home folder name.
     */
    String reverseDomainIdentifier() const;

    /**
     * Sets a callback to be called when an uncaught exception occurs.
     */
    void setTerminateFunc(void (*func)(const char *msg));

    /**
     * Finishes App construction by initializing all the application's
     * subsystems. This includes Config and FS. Has to be called manually in
     * the application's initialization routine. An exception will be thrown if
     * initialization cannot be successfully completed.
     *
     * @param flags  How to/which subsystems to initialize.
     */
    virtual void initSubsystems(SubsystemInitFlags subsystemInitflags = DefaultSubsystems);

    /**
     * Adds a system to the application. The order of systems is preserved; the
     * system added last will be notified of time changes last and will receive
     * input events last (if others don't eat them).
     *
     * @param system  System. Ownership kept by caller. The caller is
     *                responsible for making sure the system has been
     *                initialized properly.
     */
    void addSystem(System &system);

    /**
     * Removes a system from the application.
     *
     * @param system  System to remove.
     */
    void removeSystem(System &system);

    void notifyStartupComplete();

    /**
     * Determines if an instance of App currently exists.
     */
    static bool appExists();

    static App &app();

    static LogFilter &logFilter();

    /**
     * Returns the command line used to start the application.
     */
    static CommandLine &commandLine();

    /**
     * Returns the absolute native path of the application executable.
     */
    static NativePath executableFile();

    /**
     * Returns the absolute native path of the directory where the application executable is located.
     */
    static NativePath executableDir();

#ifdef MACOSX
    /**
     * Returns the native path of the application bundle contents.
     */
    NativePath nativeAppContentsPath();
#endif

    /**
     * Returns the native path of the data base folder.
     *
     * The base folder is the location where all the common data files are located, e.g.,
     * /usr/share/doomsday on Linux.
     */
    NativePath nativeBasePath();

#if 0
#if !defined (DE_STATIC_LINK)
    /**
     * Returns the native path of where to load binaries (plugins). This
     * is where "/bin" points to.
     */
    NativePath nativePluginBinaryPath();
#endif
#endif

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
    static const Archive &persistentData();

    /**
     * Returns the persistent data as a mutable archive. Accessing the entries in
     * a mutable archive will automatically update their timestamps.
     *
     * @return Persistent data archive (allowing changes).
     */
    static Archive &mutablePersistentData();

    static bool hasPersistentData();

    static ArchiveFolder &persistPackFolder();

    /**
     * Returns the application's current native working directory.
     */
    static NativePath currentWorkPath();

    /**
     * Returns a directory where temporary files can be written.
     */
    static NativePath tempPath();

    /**
     * Returns a native directory for caching non-user-specific (native) files.
     */
    static NativePath cachePath();

    /**
     * Changes the application's current native working directory.
     *
     * @param cwd  New working directory for the application.
     *
     * @return @c true, if the current working directory was changed,
     * otherwise @c false.
     */
    static bool setCurrentWorkPath(const NativePath &cwd);

    /**
     * Returns the application's file system.
     */
    static FileSystem &fileSystem();

    /**
     * Returns the application's metadata cache.
     */
    static MetadataBank &metadataBank();

    /**
     * Returns the root folder of the file system.
     */
    static Folder &rootFolder();

    /**
     * Returns the /home folder.
     */
    static Folder &homeFolder();

    /**
     * Returns the application's package loader.
     */
    static PackageLoader &packageLoader();

    /**
     * Returns the remote feed relay that manages connections to remote file repositories.
     */
    static filesys::RemoteFeedRelay &remoteFeedRelay();

    /**
     * Convenience method for finding all files matching a certain name or partial path
     * from all loaded packages.
     *
     * @param partialPath  File name or partial path.
     * @param files        Resulting list of found files.
     *
     * @return Number of files found.
     */
    static int findInPackages(const String &partialPath, FileIndex::FoundFiles &files);

    /**
     * Checks if an asset exists.
     *
     * @param identifier  Identifier.
     *
     * @return @c true, if assetInfo() can be called.
     */
    static bool assetExists(const String &identifier);

    /**
     * Retrieves the namespace of an asset.
     *
     * @param identifier  Identifier.
     *
     * @return Asset namespace accessor.
     */
    static Package::Asset asset(const String &identifier);

    /**
     * Returns the application's script system.
     */
    static ScriptSystem &scriptSystem();

    static bool configExists();

    /**
     * Returns the configuration.
     */
    static Config &config();

    /**
     * Returns a configuration variable.
     *
     * @param name  Name of the variable.
     *
     * @return Variable.
     */
    static Variable &config(const String &name);

    /**
     * Returns the web API URL. Always includes the protocol and ends with a slash.
     * @return Configured web API URL.
     */
    static String apiUrl();

    /**
     * Returns the Unix system-level configuration preferences.
     */
    static UnixInfo &unixInfo();

    /**
     * Requests engine shutdown by calling the specified termination callback
     * (see setTerminateFunc()). Called when an exception is caught at the
     * de::App level, at which point there is no way to gracefully handle it
     * and the application has to be shut down.
     *
     * This should not be called directly. From C++ code, one should throw an
     * exception in unrecoverable error situations. From C code, one should
     * call the App_FatalError() function.
     *
     * @param message  Error message to be shown to the user.
     */
    void handleUncaughtException(const String& message);

    /*
     * Events received from the operating system should be passed here; the
     * application will make sure all subsystems get a chance to process them.
     */
//    virtual bool processEvent(const Event &);

    /**
     * Informs all the subsystems about advancement of time. Subsystems will be
     * notified in the order they were added with addSystem(). This will be
     * automatically called by the application clock when time changes.
     */
    void timeChanged(const Clock &);

public:
    /**
     * Determines if the currently executing thread is the application's main
     * thread.
     */
    static bool inMainThread();

protected:
    /**
     * Returns the native path of the directory where the application can store
     * user-specific data. This is usually not the same as the user's native
     * home folder.
     *
     * @return Application data path.
     */
    virtual NativePath appDataPath() const = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_APP_H
