/********************************************************************

 Copyright(C) 2014-2015, Krisztián Kende <nemh@freemail.hu>

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

#include "systemback-cli.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTranslator>
#include <QLocale>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTranslator trnsltr;
    sb::cfgread();

    if(sb::lang == "auto")
    {
        if(! QLocale::system().name().startsWith("en")) trnsltr.load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(! sb::lang.startsWith("en"))
        trnsltr.load("systemback_" % sb::lang, "/usr/share/systemback/lang");

    if(! trnsltr.isEmpty()) a.installTranslator(&trnsltr);
    systemback c;
    QTimer::singleShot(0, &c, SLOT(main()));
    return a.exec();
}
