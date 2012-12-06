/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/App"
#include "de/ArrayValue"
#include "de/DirectoryFeed"
#include "de/PackageFolder"
#include "de/Log"
#include "de/LogBuffer"
#include "de/Module"
#include "de/Version"
#include "de/Block"
#include "de/ZipArchive"
#include "de/ArchiveFeed"
#include "de/Writer"
#include "de/math.h"

#include <QDesktopServices>
#include <QDir>

namespace de {

struct App::Instance
{
    App &app;

    CommandLine cmdLine;

    LogBuffer logBuffer;

    /// Path of the application executable.
    NativePath appPath;

    NativePath cachedBasePath;
    NativePath cachedPluginBinaryPath;
    NativePath cachedHomePath;

    /// The file system.
    FS fs;

    /// Archive where persistent data should be stored. Written to /home/persist.pack.
    /// The archive is owned by the file system.
    Archive *persistentData;

    UnixInfo unixInfo;

    /// The configuration.
    Config *config;

    /// Resident modules.
    typedef std::map<String, Module *> Modules;
    Modules modules;

    Instance(App &a, QStringList args) : app(a), cmdLine(args), persistentData(0), config(0)
    {}

    ~Instance()
    {
        delete config;
    }

    void initFileSystem(bool allowPlugins)
    {
        Folder &binFolder = fs.makeFolder("/bin");

        // Initialize the built-in folders. This hooks up the default native
        // directories into the appropriate places in the file system.
        // All of these are in read-only mode.
#ifdef MACOSX
        NativePath appDir = appPath.fileNamePath();
        binFolder.attach(new DirectoryFeed(appDir));
        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(app.nativePluginBinaryPath()));
        }
        fs.makeFolder("/data").attach(new DirectoryFeed(app.nativeBasePath()));
        fs.makeFolder("/modules").attach(new DirectoryFeed(app.nativeBasePath() / "modules"));

#elif WIN32
        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(app.nativePluginBinaryPath()));
        }
        NativePath appDir = appPath.fileNamePath();
        fs.makeFolder("/data").attach(new DirectoryFeed(appDir / "..\\data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(appDir / "..\\modules"));

#else // UNIX
        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(app.nativePluginBinaryPath()));
        }
        fs.makeFolder("/data").attach(new DirectoryFeed(app.nativeBasePath() / "data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(app.nativeBasePath() / "modules"));
#endif

        // User's home folder.
        fs.makeFolder("/home").attach(new DirectoryFeed(app.nativeHomePath(),
            DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing));

        // Populate the file system.
        fs.refresh();
    }
};

App::App(int &argc, char **argv, GUIMode guiMode)
    : QApplication(argc, argv, guiMode == GUIEnabled),
      d(new Instance(*this, arguments()))
{
    // This instance of LogBuffer is used globally.
    LogBuffer::setAppBuffer(d->logBuffer);

    // Do not flush the log buffer until we've found out where messages should
    // be flushed (Config.log.file).
    d->logBuffer.enableFlushing(false);

    // Set the log message level.
#ifdef DENG2_DEBUG
    LogEntry::Level level = LogEntry::DEBUG;
#else
    LogEntry::Level level = LogEntry::MESSAGE;
#endif
    try
    {
        int pos;
        if((pos = d->cmdLine.check("-loglevel", 1)) > 0)
        {
            level = LogEntry::textToLevel(d->cmdLine.at(pos + 1));
        }
    }
    catch(Error const &er)
    {
        qWarning("%s", er.asText().toAscii().constData());
    }
    // Aliases have not been defined at this point.
    level = qMax(LogEntry::TRACE, LogEntry::Level(level - d->cmdLine.has("-verbose") - d->cmdLine.has("-v")));
    d->logBuffer.enable(level);

    d->appPath = applicationFilePath();

    LOG_INFO("Application path: ") << d->appPath;
    LOG_INFO("Enabled log entry level: ") << LogEntry::levelToText(level);

#ifdef MACOSX
    // When the application is started through Finder, we get a special command
    // line argument. The working directory needs to be changed.
    if(d->cmdLine.count() >= 2 && d->cmdLine.at(1).beginsWith("-psn"))
    {
        DirectoryFeed::changeWorkingDir(d->cmdLine.at(0).fileNamePath() + "/..");
    }
#endif
}

App::~App()
{
    LOG_AS("~App");

    delete d;
}

NativePath App::nativePluginBinaryPath()
{
    if(!d->cachedPluginBinaryPath.isEmpty()) return d->cachedPluginBinaryPath;

    NativePath path;
#ifdef WIN32
    path = d->appPath.fileNamePath() / "plugins";
#else
# ifdef MACOSX
    path = d->appPath.fileNamePath() / "../DengPlugins";
# else
    path = DENG_LIBRARY_DIR;
# endif
    // Also check the system config files.
    d->unixInfo.path("libdir", path);
#endif
    return (d->cachedPluginBinaryPath = path);
}

NativePath App::nativeHomePath()
{
    if(!d->cachedHomePath.isEmpty()) return d->cachedHomePath;

    int i;
    if((i = d->cmdLine.check("-userdir", 1)))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedHomePath = d->cmdLine.at(i + 1));
    }

#ifdef MACOSX
    NativePath nativeHome = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
    nativeHome = nativeHome / "Library/Application Support/Doomsday Engine/runtime";
#elif WIN32
    NativePath nativeHome = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    nativeHome = nativeHome / "runtime";
#else // UNIX
    NativePath nativeHome = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
    nativeHome = nativeHome / ".doomsday/runtime";
#endif
    return (d->cachedHomePath = nativeHome);
}

Archive &App::persistentData()
{
    DENG2_ASSERT(DENG2_APP->d->persistentData != 0);

    return *DENG2_APP->d->persistentData;
}

NativePath App::currentWorkPath()
{
    return NativePath::workPath();
}

bool App::setCurrentWorkPath(NativePath const &cwd)
{
    return QDir::setCurrent(cwd);
}

NativePath App::nativeBasePath()
{
    if(!d->cachedBasePath.isEmpty()) return d->cachedBasePath;

    int i;
    if((i = d->cmdLine.check("-basedir", 1)))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedBasePath = d->cmdLine.at(i + 1));
    }

    NativePath path;
#ifdef WIN32
    path = d->appPath.fileNamePath() / "..";
#else
# ifdef MACOSX
    path = d->appPath.fileNamePath() / "../Resources";
# else
    path = DENG_BASE_DIR;
# endif
    // Also check the system config files.
    d->unixInfo.path("basedir", path);
#endif
    return (d->cachedBasePath = path);
}

void App::initSubsystems(SubsystemInitFlags flags)
{
    bool allowPlugins = !flags.testFlag(DisablePlugins);

    d->initFileSystem(allowPlugins);

    if(!homeFolder().has("persist.pack"))
    {
        // Recreate the persistent state data package.
        ZipArchive arch;
        arch.add("Info", String("# Package for Doomsday's persistent state.\n").toUtf8());
        Writer(homeFolder().newFile("persist.pack")) << arch;

        homeFolder().populate(Folder::PopulateJustThisFolder);
    }            

    d->persistentData = &homeFolder().locate<PackageFolder>("persist.pack").archive();

    // The configuration.
    d->config = new Config("/modules/Config.de");
    d->config->read();

    LogBuffer &logBuf = LogBuffer::appBuffer();

    // Update the log buffer max entry count: number of items to hold in memory.
    logBuf.setMaxEntryCount(d->config->getui("log.bufferSize"));

    // Set the log output file.
    logBuf.setOutputFile(d->config->gets("log.file"));

    // The level of enabled messages.
    /**
     * @todo We are presently controlling the log levels depending on build
     * configuration, so ignore what the config says.
     */
    //logBuf.enable(Log::LogLevel(conf->getui("log.level")));

    // We can start flushing now when the destination is known.
    logBuf.enableFlushing(true);

    if(allowPlugins)
    {
#if 0 // not yet handled by libdeng2
        // Load the basic plugins.
        loadPlugins();
#endif
    }

    LOG_VERBOSE("libdeng2::App %s subsystems initialized.") << Version().asText();
}

bool App::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(std::exception const &error)
    {
        emit uncaughtException(error.what());
    }
    catch(...)
    {
        emit uncaughtException("de::App caught exception of unknown type.");
    }
    return false;
}

App &App::app()
{
    return *DENG2_APP;
}

CommandLine &App::commandLine()
{
    return DENG2_APP->d->cmdLine;
}

NativePath App::executablePath()
{
    return DENG2_APP->d->appPath;
}

NativePath App::nativeAppContentsPath()
{
    return DENG2_APP->d->appPath/"../..";
}

FS &App::fileSystem()
{
    return DENG2_APP->d->fs;
}

Folder &App::rootFolder()
{
    return fileSystem().root();
}

Folder &App::homeFolder()
{
    return rootFolder().locate<Folder>("home");
}

Config &App::config()
{
    DENG2_ASSERT(DENG2_APP->d->config != 0);
    return *DENG2_APP->d->config;
}

UnixInfo &App::unixInfo()
{
    return DENG2_APP->d->unixInfo;
}

static int sortFilesByModifiedAt(File const *a, File const *b)
{
    return cmp(a->status().modifiedAt, b->status().modifiedAt);
}

Record &App::importModule(String const &name, String const &fromPath)
{
    LOG_AS("App::importModule");

    App &self = app();

    // There are some special modules.
    if(name == "Config")
    {
        return self.config().names();
    }

    // Maybe we already have this module?
    Instance::Modules::iterator found = self.d->modules.find(name);
    if(found != self.d->modules.end())
    {
        return found->second->names();
    }

    /// @todo  Move this path searching logic to FS.

    // Fall back on the default if the config hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue *importPath = defaultImportPath.get();
    try
    {
        importPath = &config().names()["importPath"].value<ArrayValue>();
    }
    catch(Record::NotFoundError const &)
    {}

    // Search the import path (array of paths).
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, importPath->elements())
    {
        String dir = (*i)->asText();
        String p;
        FS::FoundFiles matching;
        File *found = 0;
        if(dir.empty())
        {
            if(!fromPath.empty())
            {
                // Try the local folder.
                p = fromPath.fileNamePath() / name;
            }
            else
            {
                continue;
            }
        }
        else if(dir == "*")
        {
            fileSystem().findAll(name + ".de", matching);
            if(matching.empty())
            {
                continue;
            }
            matching.sort(sortFilesByModifiedAt);
            found = matching.back();
            LOG_VERBOSE("Chose ") << found->path() << " out of " << dint(matching.size()) << " candidates (latest modified).";
        }
        else
        {
            p = dir / name;
        }
        if(!found)
        {
            found = rootFolder().tryLocateFile(p + ".de");
        }
        if(found)
        {
            Module *module = new Module(*found);
            self.d->modules[name] = module;
            return module->names();
        }
    }

    throw NotFoundError("App::importModule", "Cannot find module '" + name + "'");
}

void App::notifyDisplayModeChanged()
{
    emit displayModeChanged();
}

} // namespace de
