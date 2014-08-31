/********************************************************************

 Copyright(C) 2014, Krisztián Kende <nemh@freemail.hu>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#ifndef SBTYPEDEF_HPP
#define SBTYPEDEF_HPP

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
typedef unsigned long long ullong;
typedef long long llong;
typedef const unsigned char cuchar;
typedef const char cchar;
typedef signed char schar;

#endif // SBTYPEDEF_HPP
