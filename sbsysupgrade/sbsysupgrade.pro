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

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-unwind-tables \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed

QMAKE_LFLAGS_RELEASE += -s

LIBS += -L../libsystemback \
        -lsystemback
