QT -= gui

TARGET = systemback

CONFIG += c++11 \
          exceptions_off

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY

! equals(QMAKE_HOST.arch, x86_64) {
    DEFINES += _FILE_OFFSET_BITS=64
}

system(./lcheck.sh):exists(libmount.hpp) {
    DEFINES += C_MNT_LIB
}

SOURCES += sblib.cpp

HEADERS += sblib.hpp \
           sblib_global.hpp \
           sbtypedef.hpp

RESOURCES += version.qrc

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-unwind-tables \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,--as-needed

QMAKE_LFLAGS_RELEASE += -s

LIBS += -lmount \
        -lblkid \
        -lparted
