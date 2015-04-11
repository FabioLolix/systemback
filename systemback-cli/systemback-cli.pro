QT -= gui
QT += core

TARGET = systemback-cli

CONFIG -= app_bundle
CONFIG += console \
          c++11 \
          exceptions_off

TEMPLATE = app

DEFINES += _FILE_OFFSET_BITS=64

SOURCES += main.cpp \
           systemback-cli.cpp

HEADERS += systemback-cli.hpp

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                          -Wshadow \
                          -Werror

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed

QMAKE_LFLAGS_RELEASE += -s

LIBS += -L../libsystemback \
        -lsystemback \
        -lncursesw
