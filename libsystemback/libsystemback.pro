QT -= gui

TARGET = systemback

CONFIG += c++11 \
          exceptions_off

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY \
           _FILE_OFFSET_BITS=64

system(./lcheck.sh):exists(libmount.hpp) {
    DEFINES += C_MNT_LIB
}

SOURCES += sblib.cpp

HEADERS += sblib.hpp \
           sblib_global.hpp \
           sbtypedef.hpp \
           bstr.hpp

RESOURCES += version.qrc

QMAKE_CXXFLAGS += -flto \
                  -fno-rtti \
                  -fvisibility=hidden \
                  -fvisibility-inlines-hidden \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                          -Wshadow \
                          -Werror

QMAKE_LFLAGS += -Wl,--as-needed \
                -fuse-ld=gold \
                -flto

QMAKE_LFLAGS_RELEASE += -s

LIBS += -lmount \
        -lblkid \
        -lparted
