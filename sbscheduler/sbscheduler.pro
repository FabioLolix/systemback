QT -= gui
QT += core

TARGET = sbscheduler

CONFIG -= app_bundle
CONFIG += console \
          c++11 \
          exceptions_off

TEMPLATE = app

DEFINES += _FILE_OFFSET_BITS=64

SOURCES += main.cpp \
           sbscheduler.cpp

HEADERS += sbscheduler.hpp

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra \
                          -Wshadow \
                          -Winline \
                          -Werror

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed

QMAKE_LFLAGS_RELEASE += -s

LIBS += -L../libsystemback \
        -lsystemback

INCLUDEPATH = ../libsystemback
