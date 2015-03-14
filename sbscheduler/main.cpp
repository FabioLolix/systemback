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

#include "sbscheduler.hpp"
#include <QCoreApplication>
#include <QTimer>

QDateTime scheduler::cfglmd;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTrn *tltr(sb::ldtltr());
    if(tltr) a.installTranslator(tltr);
    scheduler s;
    QTimer::singleShot(0, &s, SLOT(main()));
    uchar rv(a.exec());
    if(tltr) delete tltr;
    return rv;
}
