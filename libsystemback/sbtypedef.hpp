#ifndef SBTYPEDEF_HPP
#define SBTYPEDEF_HPP
#define _FILE_OFFSET_BITS 64
#define isfile(path) QFileInfo(path).isFile()
#define isdir(path) QFileInfo(path).isDir()
#define islink(path) QFileInfo(path).isSymLink()
#define fsize(path) QFileInfo(path).size()

#include <QTextStream>
#include <QStringList>

typedef QTextStream QTS;
typedef const QStringList cQSL;
typedef QStringList QSL;
typedef const QList<short> cQSIL;
typedef QList<short> QSIL;
typedef const QString cQStr;
typedef QString QStr;
typedef const QChar cQChar;
typedef const QRect cQRect;
typedef const QPoint cQPoint;
typedef const unsigned long long cullong;
typedef unsigned long long ullong;
typedef long long llong;
typedef const unsigned short cushort;
typedef const short cshort;
typedef const unsigned char cuchar;
typedef const char cchar;
typedef signed char schar;
typedef const bool cbool;

#endif // SBTYPEDEF_HPP
