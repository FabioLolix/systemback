/*
 * Copyright(C) 2014-2015, Krisztián Kende <nemh@freemail.hu>
 *
 * This file is part of Systemback.
 *
 * Systemback is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Systemback is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Systemback. If not, see <http://www.gnu.org/licenses/>.
 */

#include "systemback.hpp"
#include <QApplication>
#include <QTranslator>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator *trnsltr(new QTranslator);
    sb::cfgread();

    if(sb::lang == "auto")
    {
        if(QLocale::system().name() != "en_EN") trnsltr->load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(sb::lang != "en_EN")
        trnsltr->load("systemback_" % sb::lang, "/usr/share/systemback/lang");

    if(trnsltr->isEmpty())
        delete trnsltr;
    else
        a.installTranslator(trnsltr);

    if(qgetenv("XAUTHORITY").startsWith("/home/") && getuid() == 0)
    {
        sb::error("\n " % QTranslator::tr("Unsafe X Window authorization!") % "\n\n " % QTranslator::tr("Please do not use 'sudo' command.") % "\n\n");
        return 1;
    }

    systemback w;
    w.show();
    return a.exec();
}
