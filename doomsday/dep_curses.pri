# Build configuration for ncurses.
macx {
    LIBS += -lcurses
}
else:win32 {
    # Curses is not used on Windows.
}
else {
    # Generic Unix.
    !system(pkg-config --exists ncurses) {
        error(Missing dependency: ncurses)
    }
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags ncurses)
              LIBS += $$system(pkg-config --libs   ncurses)
}
