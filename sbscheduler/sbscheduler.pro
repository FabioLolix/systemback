QT += core
QT -= gui

TARGET = sbscheduler

CONFIG += console \
    c++11

CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    sbscheduler.cpp

HEADERS += sbscheduler.hpp

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback

INCLUDEPATH = ../libsystemback
