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

#ifndef PNLEVENT_HPP
#define PNLEVENT_HPP

#include <QWidget>

class pnlevent : public QWidget
{
    Q_OBJECT

public:
    explicit pnlevent(QWidget *parent = nullptr) : QWidget(parent) {}

protected:
    inline void leaveEvent(QEvent *);

signals:
    void Mouse_Leave();
};

inline void pnlevent::leaveEvent(QEvent *)
{
    emit Mouse_Leave();
}

#endif // PNLEVENT_HPP
