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

#ifndef SBSCHEDULER_HPP
#define SBSCHEDULER_HPP

#include "../libsystemback/sblib.hpp"
#include <QDateTime>
#include <QObject>

class scheduler : public QObject
{
    Q_OBJECT

public:
    scheduler();
    ~scheduler();

    static QDateTime cfglmd;

public slots:
    void main();

private:
    QStr *pfile;

    void newrpnt();
};

#endif
