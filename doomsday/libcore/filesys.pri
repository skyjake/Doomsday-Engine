publicHeaders(root, \
    include/de/ArchiveFeed \
    include/de/ArchiveEntryFile \
    include/de/ByteArrayFile \
    include/de/DirectoryFeed \
    include/de/Feed \
    include/de/File \
    include/de/Folder \
    include/de/FS \
    include/de/FileSystem \
    include/de/LibraryFile \
    include/de/LinkFile \
    include/de/NativeFile \
    include/de/NativePath \
    include/de/PackageFolder \
)

publicHeaders(filesys, \
    include/de/filesys/Node \
    include/de/filesys/archivefeed.h \
    include/de/filesys/archiveentryfile.h \
    include/de/filesys/bytearrayfile.h \
    include/de/filesys/directoryfeed.h \
    include/de/filesys/feed.h \
    include/de/filesys/file.h \
    include/de/filesys/folder.h \
    include/de/filesys/filesystem.h \
    include/de/filesys/libraryfile.h \
    include/de/filesys/linkfile.h \
    include/de/filesys/nativefile.h \
    include/de/filesys/nativepath.h \
    include/de/filesys/node.h \
    include/de/filesys/packagefolder.h \
)

SOURCES += \
    src/filesys/archivefeed.cpp \
    src/filesys/archiveentryfile.cpp \
    src/filesys/bytearrayfile.cpp \
    src/filesys/directoryfeed.cpp \
    src/filesys/feed.cpp \
    src/filesys/file.cpp \
    src/filesys/folder.cpp \
    src/filesys/filesystem.cpp \
    src/filesys/libraryfile.cpp \
    src/filesys/linkfile.cpp \
    src/filesys/nativefile.cpp \
    src/filesys/nativepath.cpp \
    src/filesys/node.cpp \
    src/filesys/packagefolder.cpp
