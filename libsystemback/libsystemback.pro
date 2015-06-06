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

QMAKE_CXXFLAGS += -g \
                  -flto \
                  -fno-rtti \
                  -fvisibility=hidden \
                  -fvisibility-inlines-hidden \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                          -Wshadow \
                          -Werror

QMAKE_LFLAGS += -g \
                -Wl,-Bsymbolic-functions \
                -Wl,--as-needed \
                -fuse-ld=gold \
                -flto

LIBS += -lmount \
        -lblkid \
        -lparted
