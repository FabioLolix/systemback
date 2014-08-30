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

#ifndef SYSTEMBACKCLI_HPP
#define SYSTEMBACKCLI_HPP

#include "../libsystemback/sblib.hpp"
#include <QTimer>

class systemback : public QObject
{
    Q_OBJECT

public:
    explicit systemback(QObject *parent = nullptr);

public slots:
    void progress();
    void main();

private:
    QTimer *ptimer;
    QStr pname, cpoint, prun, pbar;
    QChar yn[2];

    uchar storagedir();
    uchar clistart();
    uchar restore();
    uchar uinit();
    bool newrestorepoint();
    bool pointdelete();
};

#endif // SYSTEMBACKCLI_HPP
