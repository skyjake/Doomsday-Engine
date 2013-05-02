HEADERS += \
    include/de/Guard \
    include/de/Lockable \
    include/de/ReadWriteLockable \
    include/de/Waitable 

HEADERS += \
    include/de/concurrency/guard.h \
    include/de/concurrency/lockable.h \
    include/de/concurrency/readwritelockable.h \
    include/de/concurrency/waitable.h 

SOURCES += \
    src/concurrency/guard.cpp \
    src/concurrency/lockable.cpp \
    src/concurrency/readwritelockable.cpp \
    src/concurrency/waitable.cpp 
