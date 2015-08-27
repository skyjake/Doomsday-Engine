# Build configuration for ncurses.
macx {
    LIBS += -lcurses
}
else:win32 {
    # Curses is not used on Windows.
}
else {
    # Generic Unix.
    !system($$PKG_CONFIG --exists ncurses) {
        error(Missing dependency: ncurses)
    }
    QMAKE_CXXFLAGS += $$system($$PKG_CONFIG --cflags ncurses)
              LIBS += $$system($$PKG_CONFIG --libs   ncurses)
}
