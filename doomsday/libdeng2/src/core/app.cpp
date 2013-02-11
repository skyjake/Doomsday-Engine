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

#include "de/App"
#include "de/Animation"
#include "de/ArchiveFeed"
#include "de/ArrayValue"
#include "de/Block"
#include "de/DirectoryFeed"
#include "de/Log"
#include "de/LogBuffer"
#include "de/Module"
#include "de/NumberValue"
#include "de/PackageFolder"
#include "de/Record"
#include "de/Version"
#include "de/Version"
#include "de/Writer"
#include "de/ZipArchive"
#include "de/math.h"

#include <QDir>

namespace de {

static App *singletonApp;

DENG2_PIMPL(App), DENG2_OBSERVES(Record, Deletion)
{
    CommandLine cmdLine;

    LogBuffer logBuffer;

    /// Path of the application executable.
    NativePath appPath;

    NativePath cachedBasePath;
    NativePath cachedPluginBinaryPath;
    NativePath cachedHomePath;

    /// Primary (wall) clock.
    Clock clock;

    /// The file system.
    FS fs;

    /// Archive where persistent data should be stored. Written to /home/persist.pack.
    /// The archive is owned by the file system.
    Archive *persistentData;

    UnixInfo unixInfo;

    /// The configuration.
    Config *config;

    /// Built-in special modules. These are constructed by native code and thus not
    /// parsed from any script.
    typedef QMap<String, Record *> NativeModules;
    NativeModules nativeModules;
    Record versionModule; // Version: information about the platform and build

    /// Resident modules.
    typedef QMap<String, Module *> Modules;
    Modules modules;

    void (*terminateFunc)(char const *);

    Instance(Public &a, QStringList args)
        : Private(a), cmdLine(args), persistentData(0), config(0), terminateFunc(0)
    {
        singletonApp = &a;

        Clock::setAppClock(&clock);
    }

    ~Instance()
    {
        DENG2_FOR_EACH(NativeModules, i, nativeModules)
        {
            i.value()->audienceForDeletion -= this;
        }
        delete config;
        Clock::setAppClock(0);
    }

    void recordBeingDeleted(Record &record)
    {
        QMutableMapIterator<String, Record *> iter(nativeModules);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == &record)
            {
                iter.remove();
            }
        }
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
            binFolder.attach(new DirectoryFeed(self.nativePluginBinaryPath()));
        }
        fs.makeFolder("/data").attach(new DirectoryFeed(self.nativeBasePath()));
        fs.makeFolder("/modules").attach(new DirectoryFeed(self.nativeBasePath() / "modules"));

#elif WIN32
        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(self.nativePluginBinaryPath()));
        }
        NativePath appDir = appPath.fileNamePath();
        fs.makeFolder("/data").attach(new DirectoryFeed(appDir / "..\\data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(appDir / "..\\modules"));

#else // UNIX
        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(self.nativePluginBinaryPath()));
        }
        fs.makeFolder("/data").attach(new DirectoryFeed(self.nativeBasePath() / "data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(self.nativeBasePath() / "modules"));
#endif

        // User's home folder.
        fs.makeFolder("/home").attach(new DirectoryFeed(self.nativeHomePath(),
            DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing));

        // Populate the file system.
        fs.refresh();
    }
};

App::App(NativePath const &appFilePath, QStringList args)
    : d(new Instance(*this, args))
{
    // Global time source for animations.
    Animation::setClock(&d->clock);

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
    level = de::max(LogEntry::TRACE,
                    LogEntry::Level(level
                                    - d->cmdLine.has("-verbose")
                                    - d->cmdLine.has("-v")));
    d->logBuffer.enable(level);

    d->appPath = appFilePath;

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

    // Setup the Version module.
    Version ver;
    Record &mod = d->versionModule;
    ArrayValue *num = new ArrayValue;
    *num << NumberValue(ver.major) << NumberValue(ver.minor)
         << NumberValue(ver.patch) << NumberValue(ver.build);
    mod.addArray("VERSION", num).setReadOnly();
    mod.addText("TEXT", ver.asText()).setReadOnly();
    mod.addNumber("BUILD", ver.build).setReadOnly();
    mod.addText("OS", Version::operatingSystem()).setReadOnly();
    mod.addNumber("CPU_BITS", Version::cpuBits()).setReadOnly();
    mod.addBoolean("DEBUG", Version::isDebugBuild()).setReadOnly();
    addNativeModule("Version", mod);
}

App::~App()
{
    LOG_AS("~App");

    delete d;

    singletonApp = 0;
}

void App::setTerminateFunc(void (*func)(char const *))
{
    d->terminateFunc = func;
}

void App::handleUncaughtException(String message)
{
    LOG_CRITICAL(message);

    if(d->terminateFunc) d->terminateFunc(message.toUtf8().constData());
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
    NativePath nativeHome = QDir::homePath();
    nativeHome = nativeHome / "Library/Application Support/Doomsday Engine/runtime";
#elif WIN32
    NativePath nativeHome = appDataPath();
    nativeHome = nativeHome / "runtime";
#else // UNIX
    NativePath nativeHome = QDir::homePath();
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

        homeFolder().populate(Folder::PopulateOnlyThisFolder);
    }            

    d->persistentData = &homeFolder().locate<PackageFolder>("persist.pack").archive();

    // The configuration.
    d->config = new Config("/modules/Config.de");
    addNativeModule("Config", d->config->names());

    d->config->read();

    LogBuffer &logBuf = LogBuffer::appBuffer();

    // Update the log buffer max entry count: number of items to hold in memory.
    logBuf.setMaxEntryCount(d->config->getui("log.bufferSize"));

    try
    {
        // Set the log output file.
        logBuf.setOutputFile(d->config->gets("log.file"));
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to use \"" + d->config->gets("log.file") +
                    "\" as the log output file:\n" + er.asText());
    }

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

    // Update the wall clock time.
    d->clock.setTime(Time());

    LOG_VERBOSE("libdeng2::App %s subsystems initialized.") << Version().asText();
}

void App::addNativeModule(String const &name, Record &module)
{
    d->nativeModules.insert(name, &module);
    module.audienceForDeletion += d;
}

App &App::app()
{
    return *singletonApp;
}

CommandLine &App::commandLine()
{
    return DENG2_APP->d->cmdLine;
}

NativePath App::executablePath()
{
    return DENG2_APP->d->appPath;
}

#ifdef MACOSX
NativePath App::nativeAppContentsPath()
{
    return DENG2_APP->d->appPath/"../..";
}
#endif

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

    // There are some special native modules.
    Instance::NativeModules::const_iterator foundNative = self.d->nativeModules.constFind(name);
    if(foundNative != self.d->nativeModules.constEnd())
    {
        return *foundNative.value();
    }

    // Maybe we already have this module?
    Instance::Modules::iterator found = self.d->modules.find(name);
    if(found != self.d->modules.end())
    {
        return found.value()->names();
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
            self.d->modules.insert(name, module);
            return module->names();
        }
    }

    throw NotFoundError("App::importModule", "Cannot find module '" + name + "'");
}

} // namespace de
