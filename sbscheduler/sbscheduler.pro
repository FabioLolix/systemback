QT  += core
QT  -= gui

TARGET = sbscheduler

CONFIG  += console
CONFIG  -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    sbscheduler.cpp

HEADERS += sbscheduler.hpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback

INCLUDEPATH = ../libsystemback
