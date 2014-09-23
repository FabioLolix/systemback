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

#ifndef SYSTEMBACK_HPP
#define SYSTEMBACK_HPP
#define QTblWI QTableWidgetItem
#define QLWI QListWidgetItem
#define QTrWI QTreeWidgetItem

#include "../libsystemback/sblib.hpp"
#include <QTableWidgetItem>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QMainWindow>
#include <QTimer>

namespace Ui
{
    class systemback;
}

class systemback : public QMainWindow
{
    Q_OBJECT

public:
    explicit systemback(QWidget *parent = nullptr);
    ~systemback();

protected:
    bool eventFilter(QObject *, QEvent *ev);
    void keyReleaseEvent(QKeyEvent *ev);
    void keyPressEvent(QKeyEvent *ev);
    void closeEvent(QCloseEvent *ev);

private:
    Ui::systemback *ui;

    static cuchar Strgdr{0}, Lvwrkdr{1}, Dpath{2}, Rpnts{3}, Normal{100}, High{147}, Max{200};
    QTimer *utimer, *bttnstimer, *shdltimer, *dlgtimer, *intrrptimer;
    QStr cpoint, points, pname, prun, dialogdev, hash, grub;
    short wgeom[6], sfctr;
    uchar busycnt, dialog, ppipe;
    bool unity, uchkd, nrxth, ickernel, irfsc, utblock, nohmcpy, sstart, cfgupdt, intrrpt;

    QStr guname();
    ushort ss(ushort size);
    bool minside(cQPoint &pos, cQRect &geom);
    void windowmove(ushort nwidth, ushort nheight, bool fxdw = true);
    void setwontop(bool state = true);
    void busy(bool state = true);
    void fontcheck(uchar wdgt);
    void pointupgrade();
    void accesserror();
    void statustart();
    void systemcopy();
    void dialogopen();
    void livewrite();
    void restore();
    void repair();

private slots:
    void apokkeyreleased();
    void schedulertimer();
    void hmpg1released();
    void hmpg2released();
    void emailreleased();
    void buttonstimer();
    void wmaxreleased();
    void wminreleased();
    void chssreleased();
    void hmpg1pressed();
    void hmpg2pressed();
    void emailpressed();
    void wmaxpressed();
    void wminpressed();
    void dialogtimer();
    void chsspressed();
    void dntreleased();
    void printdbgmsg();
    void foutpsttngs();
    void wcreleased();
    void dntpressed();
    void finpsttngs();
    void wreleased();
    void wmaxenter();
    void wmaxleave();
    void wminenter();
    void wminleave();
    void wcpressed();
    void chssenter();
    void chssleave();
    void xcldenter();
    void xcldleave();
    void hmpg1move();
    void hmpg2move();
    void emailmove();
    void interrupt();
    void umntleave();
    void unitimer();
    void wpressed();
    void wdblclck();
    void chssmove();
    void cpyenter();
    void cpyleave();
    void xcldmove();
    void wcenter();
    void wcleave();
    void cpymove();
    void dntmove();
    void foutp10();
    void foutp11();
    void foutp12();
    void foutp13();
    void foutp14();
    void foutp15();
    void benter();
    void bleave();
    void foutp1();
    void foutp2();
    void foutp3();
    void foutp4();
    void foutp5();
    void foutp6();
    void foutp7();
    void foutp8();
    void foutp9();
    void center();
    void cleave();
    void wmove();

    void on_partitionsettings_currentItemChanged(QTblWI *current, QTblWI *previous);
    void on_livedevices_currentItemChanged(QTblWI *current, QTblWI *previous);
    void on_repairmountpoint_currentTextChanged(const QStr &arg1);
    void on_windowposition_currentIndexChanged(const QStr &arg1);
    void on_includeusers_currentIndexChanged(const QStr &arg1);
    void on_filesystem_currentIndexChanged(const QStr &arg1);
    void on_mountpoint_currentTextChanged(const QStr &arg1);
    void on_excludedlist_currentItemChanged(QLWI *current);
    void on_admins_currentIndexChanged(const QStr &arg1);
    void on_dirchoose_currentItemChanged(QTrWI *current);
    void on_itemslist_currentItemChanged(QTrWI *current);
    void on_adminpassword_textChanged(const QStr &arg1);
    void on_rootpassword1_textChanged(const QStr &arg1);
    void on_livelist_currentItemChanged(QLWI *current);
    void on_autorestoreoptions_clicked(bool checked);
    void on_autorepairoptions_clicked(bool checked);
    void on_password1_textChanged(const QStr &arg1);
    void on_usersettingscopy_stateChanged(int arg1);
    void on_skipfstabrestore_clicked(bool checked);
    void on_livename_textChanged(const QStr &arg1);
    void on_fullname_textChanged(const QStr &arg1);
    void on_username_textChanged(const QStr &arg1);
    void on_hostname_textChanged(const QStr &arg1);
    void on_point10_textChanged(const QStr &arg1);
    void on_point11_textChanged(const QStr &arg1);
    void on_point12_textChanged(const QStr &arg1);
    void on_point13_textChanged(const QStr &arg1);
    void on_point14_textChanged(const QStr &arg1);
    void on_point15_textChanged(const QStr &arg1);
    void on_skipfstabrepair_clicked(bool checked);
    void on_userdatainclude_clicked(bool checked);
    void on_point1_textChanged(const QStr &arg1);
    void on_point2_textChanged(const QStr &arg1);
    void on_point3_textChanged(const QStr &arg1);
    void on_point4_textChanged(const QStr &arg1);
    void on_point5_textChanged(const QStr &arg1);
    void on_point6_textChanged(const QStr &arg1);
    void on_point7_textChanged(const QStr &arg1);
    void on_point8_textChanged(const QStr &arg1);
    void on_point9_textChanged(const QStr &arg1);
    void on_dirchoose_itemExpanded(QTrWI *item);
    void on_configurationfilesrestore_clicked();
    void on_itemslist_itemExpanded(QTrWI *item);
    void on_silentmode_clicked(bool checked);
    void on_repairpartitionrefresh_clicked();
    void on_format_clicked(bool checked);
    void on_livedevicesrefresh_clicked();
    void on_liveworkdirbutton_clicked();
    void on_rootpassword2_textChanged();
    void on_partitionrefresh2_clicked();
    void on_partitionrefresh3_clicked();
    void on_functionmenunext_clicked();
    void on_functionmenuback_clicked();
    void on_storagedirbutton_clicked();
    void on_passwordinputok_clicked();
    void on_partitionrefresh_clicked();
    void on_dirchoosecancel_clicked();
    void on_changepartition_clicked();
    void on_newrestorepoint_clicked();
    void on_partitiondelete_clicked();
    void on_schedulerstart_clicked();
    void on_livecreatemenu_clicked();
    void on_livecreateback_clicked();
    void on_schedulerstate_clicked();
    void on_pointhighlight_clicked();
    void on_livewritestart_clicked();
    void on_schedulerlater_clicked();
    void on_systemupgrade_clicked();
    void on_systemrestore_clicked();
    void on_password2_textChanged();
    void on_livecreatenew_clicked();
    void on_schedulerback_clicked();
    void on_dialogcancel_clicked();
    void on_schedulemenu_clicked();
    void on_pointexclude_clicked();
    void on_systemrepair_clicked();
    void on_umountdelete_clicked();
    void on_newpartition_clicked();
    void on_startcancel_clicked();
    void on_restoremenu_clicked();
    void on_installmenu_clicked();
    void on_excludemenu_clicked();
    void on_restoreback_clicked();
    void on_installback_clicked();
    void on_excludeback_clicked();
    void on_licensemenu_clicked();
    void on_licenseback_clicked();
    void on_pointpipe10_clicked();
    void on_pointpipe11_clicked();
    void on_pointpipe12_clicked();
    void on_pointpipe13_clicked();
    void on_pointpipe14_clicked();
    void on_pointpipe15_clicked();
    void on_pointrename_clicked();
    void on_dirchooseok_clicked();
    void on_fullrestore_clicked();
    void on_restorenext_clicked();
    void on_installnext_clicked();
    void on_repairmount_clicked();
    void on_liveexclude_clicked();
    void on_pointdelete_clicked();
    void on_liveconvert_clicked();
    void on_repairmenu_clicked();
    void on_repairback_clicked();
    void on_pointpipe1_clicked();
    void on_pointpipe2_clicked();
    void on_pointpipe3_clicked();
    void on_pointpipe4_clicked();
    void on_pointpipe5_clicked();
    void on_pointpipe6_clicked();
    void on_pointpipe7_clicked();
    void on_pointpipe8_clicked();
    void on_pointpipe9_clicked();
    void on_livedelete_clicked();
    void on_fullrepair_clicked();
    void on_grubrepair_clicked();
    void on_repairnext_clicked();
    void on_removeitem_clicked();
    void on_minutedown_clicked();
    void on_seconddown_clicked();
    void on_dirrefresh_clicked();
    void on_pnumber10_clicked();
    void on_aboutmenu_clicked();
    void on_aboutback_clicked();
    void on_interrupt_clicked();
    void on_pnumber3_clicked();
    void on_pnumber4_clicked();
    void on_pnumber5_clicked();
    void on_pnumber6_clicked();
    void on_pnumber7_clicked();
    void on_pnumber8_clicked();
    void on_pnumber9_clicked();
    void on_copymenu_clicked();
    void on_copyback_clicked();
    void on_dialogok_clicked();
    void on_copynext_clicked();
    void on_hourdown_clicked();
    void on_minuteup_clicked();
    void on_secondup_clicked();
    void on_additem_clicked();
    void on_daydown_clicked();
    void on_hourup_clicked();
    void on_dayup_clicked();
    void on_umount_clicked();
};

#endif // SYSTEMBACK_HPP
