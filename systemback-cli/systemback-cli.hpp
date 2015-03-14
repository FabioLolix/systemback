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

#ifndef SYSTEMBACKCLI_HPP
#define SYSTEMBACKCLI_HPP

#include "../libsystemback/sblib.hpp"
#include <QTimer>

class systemback : public QObject
{
    Q_OBJECT

public:
    systemback();
    ~systemback();

public slots:
    void progress(uchar status = Inprog);
    void main();

private:
    enum { Inprog = 0, Start = 1, Stop = 2 };
    QTimer *ptimer;
    QStr pname, cpoint, prun, pbar;
    QChar yn[2];

    uchar storagedir();
    uchar clistart();
    uchar restore();
    bool newrestorepoint();
    void emptycache();
};

#endif
