QT += core \
      gui \
      widgets

TARGET = systemback

CONFIG += c++11 \
          exceptions_off

TEMPLATE = app

! equals(QMAKE_HOST.arch, x86_64) {
    DEFINES += _FILE_OFFSET_BITS=64
}

SOURCES += main.cpp \
           systemback.cpp

HEADERS += systemback.hpp \
           bttnevent.hpp \
           chckbxevent.hpp \
           lblevent.hpp \
           lndtevent.hpp \
           pnlevent.hpp \
           tblwdgtevent.hpp

FORMS += systemback.ui

RESOURCES += pictures.qrc

QMAKE_CXXFLAGS += -fno-rtti \
                  -fno-unwind-tables \
                  -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/systemback \
                -Wl,--as-needed

QMAKE_LFLAGS_RELEASE += -s

LIBS += -L../libsystemback \
        -lsystemback \
        -lcrypt \
        -lX11
