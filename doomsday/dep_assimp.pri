# Open Asset Import Library
!macx:!win32 {
    # Are the development files installed?
    !system(pkg-config --exists assimp) {
        error(Missing dependency: Open Asset Import Library)
    }
    
    opts = $$system(pkg-config --cflags assimp)
    QMAKE_CFLAGS   += $$opts
    QMAKE_CXXFLAGS += $$opts
    LIBS += $$system(pkg-config --libs assimp)
}
else {
    isEmpty(ASSIMP_DIR): error(Open Asset Import Library location not defined (ASSIMP_DIR))

}

