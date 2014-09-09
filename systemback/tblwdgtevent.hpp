#ifndef TBLWDGTEVENT_HPP
#define TBLWDGTEVENT_HPP

#include <QTableWidget>

class tblwdgtevent : public QTableWidget
{
    Q_OBJECT

public:
    explicit tblwdgtevent(QWidget *parent = nullptr) : QTableWidget(parent) {}

protected:
    void focusInEvent(QFocusEvent *ev);
    void focusOutEvent(QFocusEvent *ev);

signals:
    void Focus_In();
    void Focus_Out();
};

inline void tblwdgtevent::focusInEvent(QFocusEvent *ev)
{
    QTableWidget::focusInEvent(ev);
    emit Focus_In();
}

inline void tblwdgtevent::focusOutEvent(QFocusEvent *ev)
{
    QTableWidget::focusOutEvent(ev);
    emit Focus_Out();
}

#endif // TBLWDGTEVENT_HPP
