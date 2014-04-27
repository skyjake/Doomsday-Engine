# Open Asset Import Library
aiOpts   = ""
aiIncDir = ""
aiLibs   = ""

!isEmpty(ASSIMP_DIR) {
    aiIncDir = $$ASSIMP_DIR/include
    aiLibs   = -L$$ASSIMP_DIR/lib -lassimp
    *-g*|*-clang* {
        # Inform the dynamic linker about a custom location.
        QMAKE_LFLAGS += -Wl,-rpath,$$ASSIMP_DIR/lib
    }
}
else:!macx:!win32 {
    # Are the development files installed?
    !system(pkg-config --exists assimp) {
        error(Missing dependency: Open Asset Import Library)
    }

    aiOpts = $$system(pkg-config --cflags assimp)
    aiLibs = $$system(pkg-config --libs assimp)
}
else {
    error(Open Asset Import Library location not defined (ASSIMP_DIR))
}

INCLUDEPATH    += $$aiIncDir
QMAKE_CFLAGS   += $$aiOpts
QMAKE_CXXFLAGS += $$aiOpts
LIBS           += $$aiLibs
