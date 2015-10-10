QT -= gui
QT += core

TARGET = sbsysupgrade

CONFIG -= app_bundle
CONFIG += console \
          c++11 \
          exceptions_off

TEMPLATE = app

DEFINES += _FILE_OFFSET_BITS=64

SOURCES += sbsysupgrade.cpp

QMAKE_CXXFLAGS += -g \
                  -fno-rtti \
                  -fvisibility=hidden \
                  -fvisibility-inlines-hidden \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                          -Wshadow \
                          -Werror

QMAKE_LFLAGS += -g \
                -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed \
                -fuse-ld=gold

! equals(QMAKE_CXX, clang++) {
    QMAKE_CXXFLAGS += -flto
    QMAKE_LFLAGS += -flto
}

LIBS += -L../libsystemback \
        -lsystemback
