QT += core
QT -= gui

TARGET = sbsysupgrade

CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += sbsysupgrade.cpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback
