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

#include "de/Animation"
#include "de/App"
#include "de/ArchiveFeed"
#include "de/ArchiveFolder"
#include "de/ArrayValue"
#include "de/Block"
#include "de/CommandLine"
#include "de/Config"
#include "de/DictionaryValue"
#include "de/DirectoryFeed"
#include "de/EscapeParser"
#include "de/FileLogSink"
#include "de/FileSystem"
#include "de/LibraryFile"
#include "de/Log"
#include "de/LogBuffer"
#include "de/LogFilter"
#include "de/Loop"
#include "de/math.h"
#include "de/MetadataBank"
#include "de/Module"
#include "de/NativeFile"
#include "de/NumberValue"
#include "de/PackageFeed"
#include "de/PackageLoader"
#include "de/Record"
#include "de/RemoteFeedRelay"
#include "de/ScriptSystem"
#include "de/StaticLibraryFeed"
#include "de/TextValue"
#include "de/UnixInfo"
#include "de/Version"
#include "de/Writer"
#include "de/ZipArchive"

#ifdef UNIX
#  include <locale.h>
#endif

#include <c_plus/thread.h>

namespace de {

static App *singletonApp;

const char *App::ORG_NAME    = "org.name";
const char *App::ORG_DOMAIN  = "org.domain";
const char *App::APP_NAME    = "name";
const char *App::APP_VERSION = "version";
const char *App::CONFIG_PATH = "config";
const char *App::UNIX_HOME   = "unixHome";

static Value *Function_App_Locate(Context &, Function::ArgumentValues const &args)
{
    std::unique_ptr<DictionaryValue> result(new DictionaryValue);

    String const packageId = args.first()->asText();

    // Local packages.
    if (File const *pkg = PackageLoader::get().select(packageId))
    {
        result->add(new TextValue(packageId), RecordValue::takeRecord(
                        Record::withMembers(
                            "localPath",   pkg->path(),
                            "description", pkg->description()
                        )));
    }

    // Remote packages.
    auto found = App::remoteFeedRelay().locatePackages({packageId});
    for (auto i = found.begin(); i != found.end(); ++i)
    {
        result->add(new TextValue(i->first), RecordValue::takeRecord(
                        Record::withMembers(
                            "repository", i->second.link->address(),
                            "localPath",  i->second.localPath,
                            "remotePath", i->second.remotePath
                        )));
    }

    return result.release();
}

DE_PIMPL(App)
, DE_OBSERVES(PackageLoader, Activity)
{
    iThread *mainThread = nullptr;

    /// Metadata about the application.
    Record metadata;

    CommandLine cmdLine;

    LogFilter logFilter;
    LogBuffer logBuffer;

    /// Path of the application executable.
    NativePath appPath;
//    String unixHomeFolder;

    NativePath cachedBasePath;
    NativePath cachedPluginBinaryPath;
    NativePath cachedHomePath;

    /// Primary (wall) clock.
    Clock clock;

    /// Subsystems (not owned).
    List<System *> systems;

    ScriptSystem scriptSys;
    Record appModule;
    Binder binder;

    FileSystem fs;
    std::unique_ptr<MetadataBank> metaBank;
    std::unique_ptr<NativeFile> basePackFile;

    /// Archive where persistent data should be stored. Written to /home/persist.pack.
    /// The archive is owned by the file system.
    Archive *persistentData;

    std::unique_ptr<UnixInfo> unixInfo;

    filesys::RemoteFeedRelay remoteFeedRelay;

    /// The configuration.
//    Path configPath;
    Config *config;

    StringList packagesToLoadAtInit;
    PackageLoader packageLoader;

    void (*terminateFunc)(char const *);

    /// Optional sink for warnings and errors (set with "-errors").
    std::unique_ptr<FileLogSink> errorSink;

    Impl(Public *a, const StringList &args)
        : Base(a)
        //, appName("Doomsday Engine")
        , cmdLine(args)
//        , unixHomeFolder()
        , persistentData(0)
//        , configPath("/packs/net.dengine.stdlib/modules/Config.ds")
        , config(0)
        , terminateFunc(0)
    {
        appPath = cmdLine.startupPath() / cmdLine.at(0);

        metadata.set(APP_NAME,    "Doomsday Engine");
        metadata.set(CONFIG_PATH, "/packs/net.dengine.stdlib/modules/Config.ds");
        metadata.set(UNIX_HOME,   ".doomsday");

        #ifdef UNIX
        {
            // We wish to use U.S. English formatting for time and numbers (in libc).
            setlocale(LC_ALL, "en_US.UTF-8");
            setlocale(LC_NUMERIC, "C");
        }
        #endif

        // Override the system number formatting.
        //QLocale::setDefault(QLocale("en_US.UTF-8"));

        packagesToLoadAtInit << "net.dengine.stdlib";

        singletonApp = a;
        mainThread = current_Thread();

        logBuffer.setEntryFilter(&logFilter);

        Clock::setAppClock(&clock);
        Animation::setClock(&clock);
        srand(Time().toTime_t());

        // Built-in systems.
        systems << &fs << &scriptSys;

        // Built-in file interpreters.
        static LibraryFile::Interpreter intrpLibraryFile;
        static ZipArchive::Interpreter  intrpZipArchive;
        fs.addInterpreter(intrpLibraryFile);
        fs.addInterpreter(intrpZipArchive);

        // Native App module.
        {
            scriptSys.addNativeModule("App", appModule);
            binder.init(appModule)
                    << DE_FUNC(App_Locate, "locate", "packageId");
        }
    }

    ~Impl()
    {
        metaBank.reset();

        if (errorSink)
        {
            logBuffer.removeSink(*errorSink);
        }

        if (config)
        {
            // Update the log filter in the persistent configuration.
            Record *filter = new Record;
            logFilter.write(*filter);
            config->objectNamespace().add("log.filter", filter);

            delete config;
        }

        Clock::setAppClock(0);
        logBuffer.setOutputFile("");
    }

    NativePath defaultNativeModulePath() const
    {
        #ifdef WIN32
        {
            NativePath appDir = appPath.fileNamePath();
            return appDir / "..\\modules";
        }
        #else
        {
            return self().nativeBasePath() / "modules";
        }
        #endif
    }

    void initFileSystem(bool allowPlugins)
    {
        Folder::checkDefaultSettings();

        // Executables.
        Folder &binFolder = fs.makeFolder("/bin");

        // Initialize the built-in folders. This hooks up the default native
        // directories into the appropriate places in the file system.
        // All of these are in read-only mode.

        if (ZipArchive::recognize(self().nativeBasePath()))
        {
            // As a special case, if the base path points to a resource pack,
            // use the contents of the pack as the root of the file system.
            // The pack itself does not appear in the file system.
            basePackFile.reset(NativeFile::newStandalone(self().nativeBasePath()));
            fs.root().attach(new ArchiveFeed(*basePackFile));
        }
        else
        {
            #ifdef MACOSX
            {
                NativePath appDir = appPath.fileNamePath();
                binFolder.attach(new DirectoryFeed(appDir));
                fs.makeFolder("/data").attach(new DirectoryFeed(self().nativeBasePath()));
            }
            #elif WIN32
            {
                NativePath appDir = appPath.fileNamePath();
                fs.makeFolder("/data").attach(new DirectoryFeed(appDir / "..\\data"));
            }
            #else // UNIX
            {
                if ((self().nativeBasePath() / "data").exists())
                {
                    fs.makeFolder("/data").attach(new DirectoryFeed(self().nativeBasePath() / "data"));
                }
                else
                {
                    fs.makeFolder("/data").attach(new DirectoryFeed(self().nativeBasePath()));
                }
            }
            #endif

            if (defaultNativeModulePath().exists())
            {
                fs.makeFolder("/modules").attach(new DirectoryFeed(defaultNativeModulePath()));
            }
        }

        if (allowPlugins)
        {
#if !defined (DE_STATIC_LINK)
            binFolder.attach(new DirectoryFeed(self().nativePluginBinaryPath()));
#else
            binFolder.attach(new StaticLibraryFeed);
#endif
        }

        // User's home folder.
        fs.makeFolder("/home", FS::DontInheritFeeds).attach(new DirectoryFeed(self().nativeHomePath(),
                DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing | DirectoryFeed::DefaultFlags));

        // Loaded packages (as links).
        fs.makeFolder("/packs").attach(new PackageFeed(packageLoader));

        // Internal data (for runtime use only, thus writable).
        fs.makeFolder("/sys").setMode(File::Write);

        // Metadata for files.
        metaBank.reset(new MetadataBank);

        // Populate the file system (blocking).
        fs.root().populate(Folder::PopulateFullTree);

        // Ensure known subfolders exist:
        // - /home/configs is used by de::Profiles.
        fs.makeFolder("/home/configs");

        packageLoader.audienceForActivity() += this;
    }

    void setLogLevelAccordingToOptions()
    {
        // Override the log message level.
        if (cmdLine.has("-loglevel") || cmdLine.has("-verbose") || cmdLine.has("-v") ||
            cmdLine.has("-vv") || cmdLine.has("-vvv"))
        {
            LogEntry::Level level = LogEntry::Message;
            try
            {
                int pos;
                if ((pos = cmdLine.check("-loglevel", 1)) > 0)
                {
                    level = LogEntry::textToLevel(cmdLine.at(pos + 1));
                }
            }
            catch (Error const &er)
            {
                warning("%s", er.asText().c_str());
            }

            level = LogEntry::Level(level
                                    - cmdLine.has("-verbose")
                                    - cmdLine.has("-v") * (cmdLine.isAliasDefinedFor("-verbose")? 0 : 1)
                                    - cmdLine.has("-vv") * 2
                                    - cmdLine.has("-vvv") * 3);

            if (level < LogEntry::XVerbose)
            {
                // Even more verbosity requested, so enable dev messages, too.
                logFilter.setAllowDev(LogEntry::AllDomains, true);
                level = LogEntry::XVerbose;
            }

            logFilter.setMinLevel(LogEntry::AllDomains, level);
        }

        // Enable developer messages across the board?
        if (cmdLine.has("-devlog"))
        {
            logFilter.setAllowDev(LogEntry::AllDomains, true);
        }
        if (cmdLine.has("-nodevlog"))
        {
            logFilter.setAllowDev(LogEntry::AllDomains, false);
        }
    }

    void checkForErrorDumpFile()
    {
        if (CommandLine::ArgWithParams arg = cmdLine.check("-errors", 1))
        {
            File &errors = self().rootFolder().replaceFile(Path("/home") / arg.params.at(0));
            errorSink.reset(new FileLogSink(errors));
            errorSink->setMode(LogSink::OnlyWarningEntries);
            logBuffer.addSink(*errorSink);
        }
    }

    ArchiveFolder &persistPackFolder()
    {
        return self().homeFolder().locate<ArchiveFolder>("persist.pack");
    }

    void setOfLoadedPackagesChanged()
    {
        // Make sure the package links are up to date.
        fs.root().locate<Folder>("/packs").populate();
    }

    Record const *findAsset(const String &identifier) const
    {
        // Access the package that has this asset via a link.
        Folder const *pkg = fs.root().tryLocate<Folder>(String("/packs/asset.") + identifier + "/.");
        if (!pkg) return 0;

        // Find the record that has this asset's metadata.
        const String ns = "package." + identifier;
        if (pkg->objectNamespace().has(ns))
        {
            return &(*pkg)[ns].valueAsRecord();
        }
        return nullptr;
    }

    DE_PIMPL_AUDIENCE(StartupComplete)
};

DE_AUDIENCE_METHOD(App, StartupComplete)

App::App(const StringList &args)
    : d(new Impl(this, args))
{
    d->unixInfo.reset(new UnixInfo);

    // Global time source for animations.
    Animation::setClock(&d->clock);

    // This instance of LogBuffer is used globally.
    LogBuffer::setAppBuffer(d->logBuffer);

    // Do not flush the log buffer until we've found out where messages should
    // be flushed (Config.log.file).
    d->logBuffer.enableFlushing(false);

    if (d->cmdLine.has("-stdout"))
    {
        // Standard output can be flushed straight away.
        d->logBuffer.enableStandardOutput(true);
        d->logBuffer.enableFlushing(true);
    }

    // The log filter will be read from Config, but until that time we can use
    // the options from the command line.
    d->setLogLevelAccordingToOptions();

    //d->appPath = appFilePath;

    LOG_NOTE("Application path: ") << d->appPath;
    LOG_NOTE("Version: ") << Version::currentBuild().asHumanReadableText();

    #if defined (MACOSX)
    {
        // When the application is started through Finder, we get a special command
        // line argument. The working directory needs to be changed.
        if (d->cmdLine.count() >= 2 && d->cmdLine.at(1).beginsWith("-psn"))
        {
            DirectoryFeed::changeWorkingDir(d->cmdLine.at(0).fileNamePath() + "/..");
        }
    }
    #endif
}

App::~App()
{
    LOG_AS("~App");
    d.reset();
    singletonApp = 0;
}

Record &App::metadata()
{
    return d->metadata;
}

const Record &App::metadata() const
{
    return d->metadata;
}

void App::addInitPackage(String const &identifier)
{
    d->packagesToLoadAtInit << identifier;
}

/*void App::setConfigScript(Path const &path)
{
    d->configPath = path;
}*/

/*
void App::setName(String const &appName)
{
    d->appName = appName;
}
*/

void App::setUnixHomeFolderName(String const &name)
{
    d->metadata.set(UNIX_HOME, name);

    // Reload Unix config files.
    d->unixInfo.reset(new UnixInfo);
}

String App::unixHomeFolderName() const
{
    return d->metadata.gets(UNIX_HOME);
}

String App::unixEtcFolderName() const
{
    String unixHome = d->metadata.gets(UNIX_HOME);
    if (unixHome.beginsWith("."))
    {
        return unixHome.substr(String::BytePos(1));
    }
    return unixHome;
}

void App::setTerminateFunc(void (*func)(char const *))
{
    d->terminateFunc = func;
}

void App::handleUncaughtException(String message)
{
    LOG_CRITICAL(message);

    EscapeParser esc;
    esc.parse(message);

    if (d->terminateFunc) d->terminateFunc(esc.plainText());
}

bool App::processEvent(Event const &ev)
{
    for (System *sys : d->systems)
    {
        if (sys->behavior() & System::ReceivesInputEvents)
        {
            if (sys->processEvent(ev))
                return true;
        }
    }
    return false;
}

void App::timeChanged(Clock const &clock)
{
    for (System *sys : d->systems)
    {
        if (sys->behavior() & System::ObservesTime)
        {
            sys->timeChanged(clock);
        }
    }
}

bool App::inMainThread()
{
    if (!App::appExists())
    {
        // No app even created yet, must be main thread.
        return true;
    }
    return DE_APP->d->mainThread == current_Thread();
}

#if !defined (DE_STATIC_LINK)
NativePath App::nativePluginBinaryPath()
{
    if (!d->cachedPluginBinaryPath.isEmpty()) return d->cachedPluginBinaryPath;

    if (auto const opt = d->cmdLine.check("-libdir", 1))
    {
        d->cmdLine.makeAbsolutePath(opt.pos + 1);
        return (d->cachedPluginBinaryPath = d->cmdLine.at(opt.pos + 1));
    }

    NativePath path;

    #if defined (WIN32)
    {
        path = d->appPath.fileNamePath() / "plugins";
        return (d->cachedPluginBinaryPath = path);
    }
    #else
    {
        #if defined (MACOSX)
        {
            path = d->appPath.fileNamePath() / "../PlugIns/Doomsday";
        }
        #else
        {
            path = d->appPath.fileNamePath() / DE_LIBRARY_DIR;
            if (!path.exists())
            {
                // Try a fallback relative to the executable.
                path = d->appPath.fileNamePath() / "../lib/doomsday";
            }
        }
        #endif

        // Also check the system config files.
        d->unixInfo->path("libdir", path);
        return (d->cachedPluginBinaryPath = path);
    }
    #endif
}
#endif

NativePath App::nativeHomePath()
{
    if (!d->cachedHomePath.isEmpty()) return d->cachedHomePath;

    if (auto const opt = d->cmdLine.check("-userdir", 1))
    {
        d->cmdLine.makeAbsolutePath(opt.pos + 1);
        return (d->cachedHomePath = d->cmdLine.at(opt.pos + 1));
    }

    NativePath nativeHome;

    #if defined (DE_IOS)
    {
        nativeHome = NativePath::homePath();
        nativeHome = nativeHome / "Documents/runtime";
    }
    #elif defined (MACOSX)
    {
        nativeHome = NativePath::homePath();
        nativeHome = nativeHome / "Library/Application Support" / d->metadata.gets(APP_NAME) / "runtime";
    }
    #elif defined (WIN32)
    {
        nativeHome = appDataPath();
        nativeHome = nativeHome / "runtime";
    }
    #else // UNIX
    {
        nativeHome = NativePath::homePath();
        nativeHome = nativeHome / d->unixHomeFolder / "runtime";
    }
    #endif

    return (d->cachedHomePath = nativeHome);
}

Archive const &App::persistentData()
{
    Archive const *persist = DE_APP->d->persistentData;
    if (!persist)
    {
        throw PersistentDataNotAvailable("App::persistentData", "Persistent data is disabled");
    }
    return *persist;
}

Archive &App::mutablePersistentData()
{
    Archive *persist = DE_APP->d->persistentData;
    if (!persist)
    {
        throw PersistentDataNotAvailable("App::mutablePersistentData", "Persistent data is disabled");
    }
    return *persist;
}

bool App::hasPersistentData()
{
    return DE_APP->d->persistentData != 0;
}

ArchiveFolder &App::persistPackFolder()
{
    return DE_APP->d->persistPackFolder();
}

NativePath App::currentWorkPath()
{
    return NativePath::workPath();
}

bool App::setCurrentWorkPath(NativePath const &cwd)
{
    return NativePath::setWorkPath(cwd);
}

NativePath App::nativeBasePath()
{
    if (!d->cachedBasePath.isEmpty()) return d->cachedBasePath;

    int i;
    if ((i = d->cmdLine.check("-basedir", 1)))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedBasePath = d->cmdLine.at(i + 1));
    }

    NativePath path;
    #ifdef WIN32
    {
        path = d->appPath.fileNamePath() / "..";
    }
    #else
    {
        #ifdef MACOSX
        {
            path = d->appPath.fileNamePath() / "../Resources";
            if (!path.exists())
            {
                // Try the built-in base directory (unbundled apps).
                path = d->appPath.fileNamePath() / DE_BASE_DIR;
            }
        }
        #else
        {
            path = d->appPath.fileNamePath() / DE_BASE_DIR;
        }
        #endif

        if (!path.exists())
        {
            // Fall back to using the application binary path, which always exists.
            // We use this instead of the working directory because the basedir is
            // meant for storing read-only data files that may be deployed with
            // the application binary.
            path = d->appPath.fileNamePath();
        }
        // Also check the system config files.
        d->unixInfo->path("basedir", path);
    }
    #endif
    return (d->cachedBasePath = path);
}

void App::initSubsystems(SubsystemInitFlags flags)
{
    bool allowPlugins = !(flags & DisablePlugins);

    d->initFileSystem(allowPlugins);

    // Load the init packages.
    for (const String &pkg : d->packagesToLoadAtInit)
    {
        d->packageLoader.load(pkg);
    }

    if (!(flags & DisablePersistentData))
    {
        // Recreate the persistent state data package, if necessary.
        if (!homeFolder().has("persist.pack") || commandLine().has("-reset"))
        {
            ZipArchive arch;
            arch.add("Info", String::format("# Package for %s's persistent state.\n", d->metadata.gets(APP_NAME).c_str()));
            File &persistPack = homeFolder().replaceFile("persist.pack");
            Writer(persistPack) << arch;
            persistPack.reinterpret()->as<ArchiveFolder>().populate();
        }

        // Load the persistent data.
        d->persistentData = &d->persistPackFolder().archive();
    }

    // The configuration.
    d->config = new Config(d->metadata.gets(CONFIG_PATH));
    d->scriptSys.addNativeModule("Config", d->config->objectNamespace());

    // Whenever the version changes, reset the cached metadata so any updates will be applied.
    if (d->config->read() == Config::DifferentVersion)
    {
        LOG_RES_NOTE("Clearing cached metadata due to version change");
        d->metaBank->clear();

        // Immediately after upgrading, OLD_VERSION is also present in the Version module.
        Version oldVer = d->config->upgradedFromVersion();
        if (oldVer != Version::currentBuild())
        {
            ArrayValue *old = new ArrayValue;
            *old << NumberValue(oldVer.major) << NumberValue(oldVer.minor)
                 << NumberValue(oldVer.patch) << NumberValue(oldVer.build);
            d->scriptSys["Version"].addArray("OLD_VERSION", old).setReadOnly();
        }
    }

    // Set up the log buffer.
    LogBuffer &logBuf = LogBuffer::get();

    // Update the log buffer max entry count: number of items to hold in memory.
    logBuf.setMaxEntryCount(d->config->getui("log.bufferSize", 1000));

    try
    {
        // The -out option can be used to override the configured output file.
        if (CommandLine::ArgWithParams outArg = commandLine().check("-out", 1))
        {
            logBuf.setOutputFile(String("/home") / outArg.params.at(0));
        }
        else if (d->config->has("log"))
        {
            // Set the log output file.
            logBuf.setOutputFile(d->config->gets("log.file"));
        }
    }
    catch (Error const &er)
    {
        LOG_WARNING("Failed to set log output file:\n" + er.asText());
    }

    // Check if a separate error output file is requested.
    d->checkForErrorDumpFile();

    try
    {
        if (d->config->has("log.filter"))
        {
            // The level of enabled messages.
            d->logFilter.read(d->config->subrecord("log.filter"));
        }
    }
    catch (Error const &er)
    {
        LOG_WARNING("Failed to apply log filter:\n" + er.asText());
    }

    // Command line options may override the saved config.
    d->setLogLevelAccordingToOptions();

    LOGDEV_NOTE("Developer log entries enabled");

    // We can start flushing now when the destination is known.
    logBuf.enableFlushing(true);

    // Update the wall clock time.
    Time::updateCurrentHighPerformanceTime();
    d->clock.setTime(Time::currentHighPerformanceTime());

    // Now we can start observing progress of time.
    d->clock.audienceForTimeChange() += this;

    if (allowPlugins)
    {
#if 0 // not yet handled by libcore
        // Load the basic plugins.
        loadPlugins();
#endif
    }

    LOG_VERBOSE("Subsystems initialized");
}

void App::addSystem(System &system)
{
    d->systems.removeAll(&system);
    d->systems.append(&system);
}

void App::removeSystem(System &system)
{
    d->systems.removeAll(&system);
}

void App::notifyStartupComplete()
{
    DE_FOR_AUDIENCE2(StartupComplete, i)
    {
        i->appStartupCompleted();
    }
}

bool App::appExists()
{
    return singletonApp != 0;
}

App &App::app()
{
    DE_ASSERT(appExists());
    return *singletonApp;
}

LogFilter &App::logFilter()
{
    return DE_APP->d->logFilter;
}

CommandLine &App::commandLine()
{
    return DE_APP->d->cmdLine;
}

NativePath App::executablePath()
{
    return DE_APP->d->appPath;
}

#ifdef MACOSX
NativePath App::nativeAppContentsPath()
{
    return DE_APP->d->appPath/"../..";
}
#endif

FileSystem &App::fileSystem()
{
    return DE_APP->d->fs;
}

MetadataBank &App::metadataBank()
{
    DE_ASSERT(DE_APP->d->metaBank);
    return *DE_APP->d->metaBank;
}

PackageLoader &App::packageLoader()
{
    return DE_APP->d->packageLoader;
}

int App::findInPackages(const String &partialPath, FS::FoundFiles &files)
{
    return App::fileSystem().nameIndex().findPartialPathInPackageOrder(partialPath, files);
}

bool App::assetExists(const String &identifier)
{
    return DE_APP->d->findAsset(identifier) != 0;
}

Package::Asset App::asset(const String &identifier)
{
    Record const *info = DE_APP->d->findAsset(identifier);
    if (!info)
    {
        throw AssetNotFoundError("App::asset", "Asset \"" + identifier +
                                 "\" does not exist");
    }
    return *info;
}

ScriptSystem &App::scriptSystem()
{
    return DE_APP->d->scriptSys;
}

bool App::configExists()
{
    return DE_APP->d->config != nullptr;
}

Folder &App::rootFolder()
{
    return fileSystem().root();
}

Folder &App::homeFolder()
{
    return rootFolder().locate<Folder>("home");
}

filesys::RemoteFeedRelay &App::remoteFeedRelay()
{
    return DE_APP->d->remoteFeedRelay;
}

Config &App::config()
{
    DE_ASSERT(DE_APP->d->config != 0);
    return *DE_APP->d->config;
}

Variable &App::config(const String &name)
{
    return config()[name];
}

String App::apiUrl() // static
{
    String u = Config::get().gets("apiUrl");
    if (!u.beginsWith("http")) u = "http://" + u;
    if (!u.endsWith("/")) u += "/";
    return u;
}

UnixInfo &App::unixInfo()
{
    return *DE_APP->d->unixInfo;
}

void debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void warning(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);

    // TODO: On Windows, also print using the Win32 debug output functions.
}

} // namespace de
