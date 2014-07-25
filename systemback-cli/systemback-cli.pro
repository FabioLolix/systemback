QT  += core
QT  -= gui

TARGET = systemback-cli

CONFIG  += console
CONFIG  -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    systemback-cli.cpp

HEADERS += systemback-cli.hpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback \
    -lncursesw

RESOURCES += \
    resedit.qrc
