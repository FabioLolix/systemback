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

#include "../libsystemback/sblib.hpp"
#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTranslator *trnsltr(new QTranslator);
    sb::cfgread();

    if(sb::lang == "auto")
    {
        if(! QLocale::system().name().startsWith("en")) trnsltr->load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(! sb::lang.startsWith("en"))
        trnsltr->load("systemback_" % sb::lang, "/usr/share/systemback/lang");

    if(trnsltr->isEmpty())
        delete trnsltr;
    else
        a.installTranslator(trnsltr);

    sb::supgrade({QTranslator::tr("An error occurred while upgrading the system!"), QTranslator::tr("Restart upgrade ...")});
}
