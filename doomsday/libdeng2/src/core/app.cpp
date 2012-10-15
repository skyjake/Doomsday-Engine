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
#include "de/Folder"
#include "de/Log"
#include "de/LogBuffer"
#include "de/Module"
#include "de/math.h"
#include <QDesktopServices>

using namespace de;

App::App(int& argc, char** argv, GUIMode guiMode)
    : QApplication(argc, argv, guiMode == GUIEnabled),
      _cmdLine(argc, argv),
      _config(0)
{
    // This instance of LogBuffer is used globally.
    LogBuffer::setAppBuffer(_logBuffer);
#ifdef DENG2_DEBUG
    _logBuffer.enable(Log::DEBUG);
#endif
    //_logBuffer.enable(Log::TRACE);

    _appPath = applicationFilePath();

    LOG_INFO("Application path: ") << _appPath;

#ifdef MACOSX
    // When the application is started through Finder, we get a special command
    // line argument. The working directory needs to be changed.
    if(_cmdLine.count() >= 2 && _cmdLine.at(1).beginsWith("-psn"))
    {
        DirectoryFeed::changeWorkingDir(_cmdLine.at(0).fileNamePath() + "/..");
    }
#endif
}

String App::nativeBinaryPath()
{
    String path;
#ifdef WIN32
    path = _appPath.fileNameNativePath() / "plugins";
#else
# ifdef MACOSX
    path = _appPath.fileNameNativePath() / "../DengPlugins";
# else
    path = DENG_LIBRARY_DIR;
# endif
    // Also check the system config files.
    _unixInfo.path("libdir", path);
#endif
    return path;
}

String App::nativeBasePath()
{
    int i;
    if((i = _cmdLine.check("-basedir", 1)))
    {
        _cmdLine.makeAbsolutePath(i + 1);
        return _cmdLine.at(i + 1);
    }

    String path;
#ifdef WIN32
    path = _appPath.fileNameNativePath().concatenateNativePath("..");
#else
# ifdef MACOSX
    path = ".";
# else
    path = DENG_BASE_DIR;
# endif
    // Also check the system config files.
    _unixInfo.path("basedir", path);
#endif
    return path;
}

void App::initSubsystems()
{
    // Initialize the built-in folders. This hooks up the default native
    // directories into the appropriate places in the file system.
    // All of these are in read-only mode.
#ifdef MACOSX
    String appDir = _appPath.fileNameNativePath();
    Folder& binFolder = _fs.makeFolder("/bin");
    binFolder.attach(new DirectoryFeed(appDir));
    binFolder.attach(new DirectoryFeed(nativeBinaryPath()));
    _fs.makeFolder("/data").attach(new DirectoryFeed(appDir / "../Resources"));
    _fs.makeFolder("/config").attach(new DirectoryFeed(appDir / "../Resources/config"));
    //fs_->makeFolder("/modules").attach(new DirectoryFeed("Resources/modules"));

#elif WIN32
    _fs.makeFolder("/bin").attach(new DirectoryFeed(nativeBinaryPath()));
    String appDir = _appPath.fileNameNativePath();
    _fs.makeFolder("/data").attach(new DirectoryFeed(appDir.concatenateNativePath("..\\data")));
    _fs.makeFolder("/config").attach(new DirectoryFeed(appDir.concatenateNativePath("..\\data\\config")));
    //fs_->makeFolder("/modules").attach(new DirectoryFeed("data\\modules"));

#else // UNIX
    _fs.makeFolder("/bin").attach(new DirectoryFeed(nativeBinaryPath()));
    String dataDir = nativeBasePath() / "data";
    _fs.makeFolder("/data").attach(new DirectoryFeed(dataDir));
    _fs.makeFolder("/config").attach(new DirectoryFeed(dataDir / "config"));
    //fs_->makeFolder("/modules").attach(new DirectoryFeed("data/modules"));
#endif

    // User's home folder.
#ifdef MACOSX
    String nativeHome = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
    nativeHome = nativeHome / "Library/Application Support/Doomsday Engine/runtime";
#elif WIN32
    /// @todo We still have Snowberry, so use its runtime folder. Really should use local AppData.
    String nativeHome = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    nativeHome = nativeHome.concatenateNativePath("Doomsday Frontend\\runtime");
#else // UNIX
    String nativeHome = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
    nativeHome = nativeHome / ".doomsday/runtime";
#endif
    _fs.makeFolder("/home").attach(new DirectoryFeed(nativeHome,
        DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing));

    // Populate the file system.
    _fs.refresh();

    // The configuration.
    QScopedPointer<Config> conf(new Config("/config/deng.de"));
    conf->read();

    LogBuffer& logBuf = LogBuffer::appBuffer();

    // Update the log buffer max entry count: number of items to hold in memory.
    logBuf.setMaxEntryCount(conf->getui("log.bufferSize"));

    // Set the log output file.
    logBuf.setOutputFile(conf->gets("log.file"));

    // The level of enabled messages.
    logBuf.enable(Log::LogLevel(conf->getui("log.level")));

#if 0 // not yet handled by libdeng2
    // Load the basic plugins.
    loadPlugins();
#endif

    // Successful construction without errors, so drop our guard.
    _config = conf.take();

    LOG_VERBOSE("libdeng2::App ") << LIBDENG2_VERSION << " subsystems initialized.";
}

bool App::notify(QObject* receiver, QEvent* event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(const std::exception& error)
    {
        emit uncaughtException(error.what());
    }
    catch(...)
    {
        emit uncaughtException("de::App caught exception of unknown type.");
    }
    return false;
}

App& App::app()
{
    return *DENG2_APP;
}

CommandLine& App::commandLine()
{
    return DENG2_APP->_cmdLine;
}

String App::executablePath()
{
    return DENG2_APP->_appPath;
}

FS& App::fileSystem()
{
    return DENG2_APP->_fs;
}

Folder& App::rootFolder()
{
    return fileSystem().root();
}

Folder& App::homeFolder()
{
    return rootFolder().locate<Folder>("/home");
}

Config& App::config()
{
    DENG2_ASSERT(DENG2_APP->_config != 0);
    return *DENG2_APP->_config;
}

UnixInfo& App::unixInfo()
{
    return DENG2_APP->_unixInfo;
}

static int sortFilesByModifiedAt(const File* a, const File* b)
{
    return cmp(a->status().modifiedAt, b->status().modifiedAt);
}

Record& App::importModule(const String& name, const String& fromPath)
{
    LOG_AS("App::importModule");

    App& self = app();

    // There are some special modules.
    if(name == "Config")
    {
        return self.config().names();
    }

    // Maybe we already have this module?
    Modules::iterator found = self._modules.find(name);
    if(found != self._modules.end())
    {
        return found->second->names();
    }

    /// @todo  Move this path searching logic to FS.

    // Fall back on the default if the config hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue* importPath = defaultImportPath.get();
    try
    {
        importPath = &config().names()["importPath"].value<ArrayValue>();
    }
    catch(const Record::NotFoundError&)
    {}

    // Search the import path (array of paths).
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, importPath->elements())
    {
        String dir = (*i)->asText();
        String p;
        FS::FoundFiles matching;
        File* found = 0;
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
            Module* module = new Module(*found);
            self._modules[name] = module;
            return module->names();
        }
    }

    throw NotFoundError("App::importModule", "Cannot find module '" + name + "'");
}

void App::notifyDisplayModeChanged()
{
    emit displayModeChanged();
}
