publicHeaders(root, \
    include/de/ArchiveEntryFile \
    include/de/ArchiveFeed \
    include/de/ArchiveFolder \
    include/de/ByteArrayFile \
    include/de/DirectoryFeed \
    include/de/Feed \
    include/de/File \
    include/de/FileIndex \
    include/de/Folder \
    include/de/FS \
    include/de/FileSystem \
    include/de/LibraryFile \
    include/de/LinkFile \
    include/de/NativeFile \
    include/de/NativePath \
    include/de/Package \
    include/de/PackageFeed \
    include/de/PackageLoader \
)

publicHeaders(filesys, \
    include/de/filesys/Node \
    include/de/filesys/archiveentryfile.h \
    include/de/filesys/archivefeed.h \
    include/de/filesys/archivefolder.h \
    include/de/filesys/bytearrayfile.h \
    include/de/filesys/directoryfeed.h \
    include/de/filesys/feed.h \
    include/de/filesys/file.h \
    include/de/filesys/fileindex.h \
    include/de/filesys/filesystem.h \
    include/de/filesys/folder.h \
    include/de/filesys/libraryfile.h \
    include/de/filesys/linkfile.h \
    include/de/filesys/nativefile.h \
    include/de/filesys/nativepath.h \
    include/de/filesys/node.h \
    include/de/filesys/package.h \
    include/de/filesys/packagefeed.h \
    include/de/filesys/packageloader.h \
)

SOURCES += \
    src/filesys/archiveentryfile.cpp \
    src/filesys/archivefeed.cpp \
    src/filesys/archivefolder.cpp \
    src/filesys/bytearrayfile.cpp \
    src/filesys/directoryfeed.cpp \
    src/filesys/feed.cpp \
    src/filesys/file.cpp \
    src/filesys/fileindex.cpp \
    src/filesys/filesystem.cpp \
    src/filesys/folder.cpp \
    src/filesys/libraryfile.cpp \
    src/filesys/linkfile.cpp \
    src/filesys/nativefile.cpp \
    src/filesys/nativepath.cpp \
    src/filesys/node.cpp \
    src/filesys/package.cpp \
    src/filesys/packagefeed.cpp \
    src/filesys/packageloader.cpp
