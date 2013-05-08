HEADERS += \
    include/de/Guard \
    include/de/Lockable \
    include/de/ReadWriteLockable \
    include/de/Task \
    include/de/TaskPool \
    include/de/Waitable

HEADERS += \
    include/de/concurrency/guard.h \
    include/de/concurrency/lockable.h \
    include/de/concurrency/readwritelockable.h \
    include/de/concurrency/task.h \
    include/de/concurrency/taskpool.h \
    include/de/concurrency/waitable.h

SOURCES += \
    src/concurrency/guard.cpp \
    src/concurrency/lockable.cpp \
    src/concurrency/readwritelockable.cpp \
    src/concurrency/task.cpp \
    src/concurrency/taskpool.cpp \
    src/concurrency/waitable.cpp
