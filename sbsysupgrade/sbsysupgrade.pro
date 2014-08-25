QT += core
QT -= gui

TARGET = sbsysupgrade

CONFIG += console \
    c++11

CONFIG -= app_bundle

TEMPLATE = app

SOURCES += sbsysupgrade.cpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback
