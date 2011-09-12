# Build configuration for ncurses.
macx {
    LIBS += -lcurses
}
else:win32 {
}
else {
    # Generic Unix.
    LIBS += -lncurses
}
