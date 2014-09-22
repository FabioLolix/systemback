QT += core
QT -= gui

TARGET = sbsysupgrade

CONFIG += console \
    c++11 \
    exceptions_off

CONFIG -= app_bundle

TEMPLATE = app

SOURCES += sbsysupgrade.cpp

QMAKE_CXXFLAGS += -fno-rtti \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback
