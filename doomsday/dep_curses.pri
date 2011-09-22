# Build configuration for ncurses.
macx {
    LIBS += -lcurses
}
else:win32 {
    # Curses is not used on Windows.
}
else {
    # Generic Unix.
    LIBS += -lncurses
}
