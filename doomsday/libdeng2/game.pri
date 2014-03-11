publicHeaders(game, \
    include/de/game/Game \
    include/de/game/MapStateReader \
    include/de/game/SavedSession \
    include/de/game/SavedSessionRepository \
    \
    include/de/game/game.h \
    include/de/game/mapstatereader.h \
    include/de/game/savedsession.h \
    include/de/game/savedsessionrepository.h \
)

SOURCES += \
    src/game/game.cpp \
    src/game/mapstatereader.cpp \
    src/game/savedsession.cpp \
    src/game/savedsessionrepository.cpp
