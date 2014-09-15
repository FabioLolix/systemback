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

#ifndef BTTNEVENT_HPP
#define BTTNEVENT_HPP

#include <QPushButton>

class bttnevent : public QPushButton
{
    Q_OBJECT

public:
    explicit bttnevent(QWidget *parent = nullptr) : QPushButton(parent) {}

protected:
    void leaveEvent(QEvent *);

signals:
    void Mouse_Leave();
};

inline void bttnevent::leaveEvent(QEvent *)
{
    emit Mouse_Leave();
}

#endif // BTTNEVENT_HPP
