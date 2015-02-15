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

#ifndef PNLEVENT_HPP
#define PNLEVENT_HPP

#include <QWidget>

class pnlevent : public QWidget
{
    Q_OBJECT

public:
    inline explicit pnlevent(QWidget *prnt) : QWidget(prnt) {}

protected:
    void leaveEvent(QEvent *);

signals:
    void Mouse_Leave();
};

inline void pnlevent::leaveEvent(QEvent *)
{
    emit Mouse_Leave();
}

#endif
