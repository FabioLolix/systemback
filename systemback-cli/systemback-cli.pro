QT -= gui
QT += core

TARGET = systemback-cli

CONFIG -= app_bundle
CONFIG += console \
          c++11 \
          exceptions_off

TEMPLATE = app

SOURCES += main.cpp \
           systemback-cli.cpp

HEADERS += systemback-cli.hpp

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-unwind-tables \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/systemback

QMAKE_LFLAGS_RELEASE += -s

LIBS += -L../libsystemback \
        -lsystemback \
        -lncursesw
