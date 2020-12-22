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

#include "de/animation.h"
#include "de/app.h"
#include "de/archivefeed.h"
#include "de/archivefolder.h"
#include "de/block.h"
#include "de/commandline.h"
#include "de/config.h"
#include "de/directoryfeed.h"
#include "de/dscript.h"
#include "de/escapeparser.h"
#include "de/filelogsink.h"
#include "de/filesys/remotefeedrelay.h"
#include "de/filesystem.h"
#include "de/garbage.h"
#include "de/log.h"
#include "de/logbuffer.h"
#include "de/logfilter.h"
#include "de/loop.h"
#include "de/math.h"
#include "de/metadatabank.h"
#include "de/nativefile.h"
#include "de/packagefeed.h"
#include "de/packageloader.h"
#include "de/record.h"
#include "de/scripting/module.h"
#include "de/taskpool.h"
#include "de/unixinfo.h"
#include "de/version.h"
#include "de/writer.h"
#include "de/ziparchive.h"

#ifdef UNIX
#  include <locale.h>
#endif

#include <the_Foundation/thread.h>

namespace de {

static App *singletonApp;

const char *App::ORG_NAME    = "org.name";
const char *App::ORG_DOMAIN  = "org.domain";
const char *App::APP_NAME    = "name";
const char *App::APP_VERSION = "version";
const char *App::CONFIG_PATH = "config";
const char *App::UNIX_HOME   = "unixHome";

static Value *Function_App_Locate(Context &, const Function::ArgumentValues &args)
{
    std::unique_ptr<DictionaryValue> result(new DictionaryValue);

    const String packageId = args.first()->asText();

    // Local packages.
    if (const File *pkg = PackageLoader::get().select(packageId))
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
    thrd_t mainThread{};
    void (*terminateFunc)(const char *){};

    Record metadata; // Metadata about the application.

    CommandLine cmdLine;

    LogFilter logFilter;
    LogBuffer logBuffer;

    NativePath appPath; // Path of the application executable.
    NativePath cachedBasePath;
    NativePath cachedHomePath;

    Clock clock; // Primary (wall) clock.

    Flags initFlags; // Subsystems (not owned).
    List<System *> systems;
    ScriptSystem scriptSys;
    Record appModule;
    Binder binder;
    FileSystem fs;
    std::unique_ptr<MetadataBank> metaBank;
    std::unique_ptr<NativeFile> basePackFile;

    std::unique_ptr<UnixInfo> unixInfo;

    filesys::RemoteFeedRelay remoteFeedRelay;

    Config *config{};

    StringList packagesToLoadAtInit;
    PackageLoader packageLoader;

    std::unique_ptr<FileLogSink> errorSink; // Optional sink for warnings/errors (set with "-errors").

    Impl(Public *a, const StringList &args)
        : Base(a)
        , cmdLine(args)
    {
        DE_ASSERT(isInitialized_Foundation());

        appPath = cmdLine.startupPath() / cmdLine.at(0);

        metadata.set(APP_NAME,    "Doomsday Engine");
        metadata.set(CONFIG_PATH, "/packs/net.dengine.stdlib/modules/Config.ds");
        metadata.set(UNIX_HOME,   ".doomsday");

        #ifdef UNIX
        {
            // We wish to use U.S. English formatting for time and numbers (in libc).
            setenv("LC_ALL", "en_US.UTF-8", 1);
            setenv("LC_NUMERIC", "C", 1);
            setlocale(LC_ALL, "en_US.UTF-8");
            setlocale(LC_NUMERIC, "C");
        }
        #endif

        packagesToLoadAtInit << "net.dengine.stdlib";

        singletonApp = a;
        mainThread = thrd_current();

        logBuffer.setEntryFilter(&logFilter);

        Clock::setAppClock(&clock);
        Animation::setClock(&clock);
        srand(unsigned(Time().toTime_t()));

        // Built-in systems.
        systems << &fs << &scriptSys;

        // Built-in file interpreters.
//        static LibraryFile::Interpreter intrpLibraryFile;
//        fs.addInterpreter(intrpLibraryFile);
        static ZipArchive::Interpreter  intrpZipArchive;
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
        debug("[App::~Impl] destroying");

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

        Clock::setAppClock(nullptr);
        logBuffer.setOutputFile("");

        TaskPool::deleteThreadPool();

        debug("[App::~Impl] done");
    }

    NativePath defaultNativeModulePath() const
    {
        #ifdef DE_WINDOWS
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

    void initFileSystem()
    {
        Folder::checkDefaultSettings();

        // Executables.
//        Folder &binFolder = fs.makeFolder("/bin");

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
//                binFolder.attach(new DirectoryFeed(appDir));
                fs.makeFolder("/data").attach(new DirectoryFeed(self().nativeBasePath()));
            }
            #elif DE_WINDOWS
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
            catch (const Error &er)
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

    ArchiveFolder &persistPackFolder() const
    {
        return self().homeFolder().locate<ArchiveFolder>("persist.pack");
    }

    void setOfLoadedPackagesChanged()
    {
        // Make sure the package links are up to date.
        fs.root().locate<Folder>("/packs").populate();
    }

    const Record *findAsset(const String &identifier) const
    {
        // Access the package that has this asset via a link.
        const Folder *pkg = fs.root().tryLocate<Folder>(String("/packs/asset.") + identifier + "/.");
        if (!pkg) return nullptr;

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
            DirectoryFeed::changeWorkingDir(d->cmdLine.at(0).fileNamePath() / "..");
        }
    }
    #endif
}

App::~App()
{
    LOG_AS("~App");
    d.reset();
    singletonApp = nullptr;
    Garbage_Recycle();
}

Record &App::metadata()
{
    return d->metadata;
}

const Record &App::metadata() const
{
    return d->metadata;
}

void App::addInitPackage(const String &identifier)
{
    d->packagesToLoadAtInit << identifier;
}

void App::setUnixHomeFolderName(const String &name)
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
        return unixHome.substr(BytePos(1));
    }
    return unixHome;
}

String App::reverseDomainIdentifier() const
{
    const DotPath domain(metadata().gets(ORG_DOMAIN));
    String rdi;
    for (int i = 0; i < domain.segmentCount(); ++i)
    {
        rdi = rdi.concatenateMember(domain.reverseSegment(i).toLowercaseString());
    }
    return rdi.concatenateMember(unixEtcFolderName());
}

void App::setTerminateFunc(void (*func)(const char *))
{
    d->terminateFunc = func;
}

void App::handleUncaughtException(const String& message)
{
    LOG_CRITICAL(message);

    EscapeParser esc;
    esc.parse(message);

    if (d->terminateFunc) d->terminateFunc(esc.plainText());
}

#if 0
bool App::processEvent(const Event &ev)
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
#endif

void App::timeChanged(const Clock &clock)
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
    return DE_APP->d->mainThread == thrd_current();
}

#if 0
#if !defined (DE_STATIC_LINK)
NativePath App::nativePluginBinaryPath()
{
    if (!d->cachedPluginBinaryPath.isEmpty()) return d->cachedPluginBinaryPath;

    if (const auto opt = d->cmdLine.check("-libdir", 1))
    {
        d->cmdLine.makeAbsolutePath(opt.pos + 1);
        return (d->cachedPluginBinaryPath = d->cmdLine.at(opt.pos + 1));
    }

    NativePath path;

    #if defined (DE_WINDOWS)
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
#endif

NativePath App::nativeHomePath()
{
    if (!d->cachedHomePath.isEmpty()) return d->cachedHomePath;

    if (const auto opt = d->cmdLine.check("-userdir", 1))
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
    #elif defined (DE_WINDOWS)
    {
        nativeHome = appDataPath();
        nativeHome = nativeHome / "runtime";
    }
    #else // UNIX
    {
        nativeHome = NativePath::homePath();
        nativeHome = nativeHome / unixHomeFolderName() / "runtime";
    }
    #endif

    return (d->cachedHomePath = nativeHome);
}

const Archive &App::persistentData()
{
    const Impl *d = DE_APP->d;
    if (d->initFlags & DisablePersistentData)
    {
        throw PersistentDataNotAvailable("App::persistentData", "Persistent data is disabled");
    }
    return d->persistPackFolder().archive();
}

Archive &App::mutablePersistentData()
{
    const Impl *d = DE_APP->d;
    if (d->initFlags & DisablePersistentData)
    {
        throw PersistentDataNotAvailable("App::mutablePersistentData", "Persistent data is disabled");
    }
    return d->persistPackFolder().archive();
}

bool App::hasPersistentData()
{
    return !(DE_APP->d->initFlags & DisablePersistentData);
}

ArchiveFolder &App::persistPackFolder()
{
    return DE_APP->d->persistPackFolder();
}

NativePath App::currentWorkPath()
{
    return NativePath::workPath();
}

NativePath App::tempPath()
{
    NativePath sysTemp;
    #if defined (DE_WINDOWS)
    {
        sysTemp = NativePath::homePath() / "AppData\\Local\\Temp";
    }
    #else
    {
        sysTemp = "/tmp";
    }
    #endif
    return sysTemp / Stringf("%s-%i", app().reverseDomainIdentifier().c_str(), pid_Process(nullptr));
}

NativePath App::cachePath()
{
    NativePath dir;

    #if defined (MACOSX)
    {
        dir = NativePath::homePath() / "Library/Caches" / app().reverseDomainIdentifier();
    }
    #else
    {
        DE_ASSERT_FAIL("App::cachePath() not implemented");
        return NativePath();
    }
    #endif

    // Make sure the directory actually exists.
    if (!dir.exists())
    {
        dir.create();
    }
    return dir;
}

bool App::setCurrentWorkPath(const NativePath &cwd)
{
    return NativePath::setWorkPath(cwd);
}

NativePath App::nativeBasePath()
{
    if (!d->cachedBasePath.isEmpty()) return d->cachedBasePath;

    if (int i = d->cmdLine.check("-basedir", 1))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedBasePath = d->cmdLine.at(i + 1));
    }

    NativePath path;
    #if defined (DE_WINDOWS)
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
    d->initFlags = flags;

    d->initFileSystem();

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
            arch.add("Info", Stringf("# Package for %s's persistent state.\n",
                                     d->metadata.gets(APP_NAME).c_str()));
            File &persistPack = homeFolder().replaceFile("persist.pack");
            Writer(persistPack) << arch;
            persistPack.reinterpret()->as<ArchiveFolder>().populate();
        }
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
    catch (const Error &er)
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
    catch (const Error &er)
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

    LOG_VERBOSE("%s %s subsystems initialized")
        << metadata().gets(APP_NAME) << Version::currentBuild().asHumanReadableText();
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
    DE_NOTIFY(StartupComplete, i)
    {
        i->appStartupCompleted();
    }
}

bool App::appExists()
{
    return singletonApp != nullptr;
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

NativePath App::executableFile()
{
    return DE_APP->d->appPath;
}

NativePath de::App::executableDir()
{
    return DE_APP->d->appPath.fileNamePath();
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
    const Record *info = DE_APP->d->findAsset(identifier);
    if (!info)
    {
        throw AssetNotFoundError("App::asset", "Asset \"" + identifier + "\" does not exist");
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
    DE_ASSERT(DE_APP->d->config != nullptr);
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
    fflush(stdout);
}

void warning(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);

    // TODO: On Windows, also print using the Win32 debug output functions.
}

} // namespace de
