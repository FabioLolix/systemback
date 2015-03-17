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

#ifndef SYSTEMBACK_HPP
#define SYSTEMBACK_HPP
#define QTblWI QTableWidgetItem
#define QLWI QListWidgetItem
#define QTrWI QTreeWidgetItem
#define cQTrWI const QTreeWidgetItem

#include "../libsystemback/sblib.hpp"
#include <QTableWidgetItem>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QMainWindow>
#include <QTimer>

namespace Ui {
class systemback;
}

class systemback : public QMainWindow
{
    Q_OBJECT

public:
    systemback();
    ~systemback();

protected:
    bool eventFilter(QObject *, QEvent *ev);
    void keyReleaseEvent(QKeyEvent *ev);
    void keyPressEvent(QKeyEvent *ev);
    void closeEvent(QCloseEvent *ev);

private:
    enum { Strgdr = 0, Lvwrkdr = 1, Dpath = 2, Rpnts = 3,
           Normal = 100, High = 147, Max = 200 };

    Ui::systemback *ui;

    struct GRUB
    {
        QStr name;
        bool isEFI;
    };

    GRUB grub;
    QTimer utimer, *shdltimer, *dlgtimer, *intrrptimer;
    QStr cpoint, points, pname, prun, hash;
    ushort dialog;
    short wgeom[4], cpos;
    uchar busycnt, ppipe, sfctr, icnt;
    bool wismax, uchkd, nrxth, ickernel, irblck, utblock, nohmcpy, sstart, cfgupdt, intrrpt;

    QLE *getpoint(uchar num);
    QCB *getppipe(uchar num);
    QStr guname();
    QStr ckname();
    ushort ss(ushort size);
    bool minside(cQPoint &wpos, cQSize &wsize);
    void dialogopen(ushort dlg = 0, cchar *dev = nullptr, schar snum = -1);
    void windowmove(ushort nwidth, ushort nheight, bool fxdw = true);
    void ptxtchange(uchar num, cQStr &txt);
    void ilstupdt(cQStr &dir = nullptr);
    void setwontop(bool state = true);
    void busy(bool state = true);
    void fontcheck(uchar wdgt);
    void pnmchange(uchar num);
    void pointupgrade();
    void emptycache();
    void statustart();
    void systemcopy();
    void livewrite();
    void rmntcheck();
    void stschange();
    void restore();
    void repair();

private slots:
    void apokkeyreleased();
    void schedulertimer();
    void sbttnreleased();
    void hmpg1released();
    void hmpg2released();
    void emailreleased();
    void sbttnpressed();
    void wmaxreleased();
    void wminreleased();
    void hmpg1pressed();
    void hmpg2pressed();
    void emailpressed();
    void wmaxpressed();
    void wminpressed();
    void chsreleased();
    void dialogtimer();
    void dntreleased();
    void printdbgmsg();
    void foutpsttngs();
    void chspressed();
    void wcreleased();
    void dntpressed();
    void finpsttngs();
    void sbttnleave();
    void wreleased();
    void wmaxenter();
    void wmaxleave();
    void wminenter();
    void wminleave();
    void wcpressed();
    void xcldenter();
    void xcldleave();
    void sbttnmove();
    void hmpg1move();
    void hmpg2move();
    void emailmove();
    void umntleave();
    void chsenter();
    void chsleave();
    void unitimer();
    void wpressed();
    void wdblclck();
    void cpyenter();
    void cpyleave();
    void wcenter();
    void wcleave();
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
    void rmove();
    void bmove();

    void on_partitionsettings_currentItemChanged(QTblWI *crrnt, QTblWI *prvs);
    void on_livedevices_currentItemChanged(QTblWI *crrnt, QTblWI *prvs);
    void on_grubreinstallrestore_currentIndexChanged(const QStr &arg1);
    void on_grubreinstallrepair_currentIndexChanged(const QStr &arg1);
    void on_grubinstallcopy_currentIndexChanged(const QStr &arg1);
    void on_repairpartition_currentIndexChanged(const QStr &arg1);
    void on_repairmountpoint_currentTextChanged(const QStr &arg1);
    void on_windowposition_currentIndexChanged(const QStr &arg1);
    void on_includeusers_currentIndexChanged(const QStr &arg1);
    void on_filesystem_currentIndexChanged(const QStr &arg1);
    void on_languages_currentIndexChanged(const QStr &arg1);
    void on_mountpoint_currentTextChanged(const QStr &arg1);
    void on_excludedlist_currentItemChanged(QLWI *crrnt);
    void on_styles_currentIndexChanged(const QStr &arg1);
    void on_admins_currentIndexChanged(const QStr &arg1);
    void on_adminpassword_textChanged(const QStr &arg1);
    void on_rootpassword1_textChanged(const QStr &arg1);
    void on_users_currentIndexChanged(const QStr &arg1);
    void on_dirchoose_currentItemChanged(QTrWI *crrnt);
    void on_itemslist_currentItemChanged(QTrWI *crrnt);
    void on_livelist_currentItemChanged(QLWI *crrnt);
    void on_password1_textChanged(const QStr &arg1);
    void on_usersettingscopy_stateChanged(int arg1);
    void on_autorestoreoptions_clicked(bool chckd);
    void on_incrementaldisable_clicked(bool chckd);
    void on_livename_textChanged(const QStr &arg1);
    void on_fullname_textChanged(const QStr &arg1);
    void on_username_textChanged(const QStr &arg1);
    void on_hostname_textChanged(const QStr &arg1);
    void on_autorepairoptions_clicked(bool chckd);
    void on_point10_textChanged(const QStr &arg1);
    void on_point11_textChanged(const QStr &arg1);
    void on_point12_textChanged(const QStr &arg1);
    void on_point13_textChanged(const QStr &arg1);
    void on_point14_textChanged(const QStr &arg1);
    void on_point15_textChanged(const QStr &arg1);
    void on_skipfstabrestore_clicked(bool chckd);
    void on_languageoverride_clicked(bool chckd);
    void on_schedulerdisable_clicked(bool chckd);
    void on_cachemptydisable_clicked(bool chckd);
    void on_point1_textChanged(const QStr &arg1);
    void on_point2_textChanged(const QStr &arg1);
    void on_point3_textChanged(const QStr &arg1);
    void on_point4_textChanged(const QStr &arg1);
    void on_point5_textChanged(const QStr &arg1);
    void on_point6_textChanged(const QStr &arg1);
    void on_point7_textChanged(const QStr &arg1);
    void on_point8_textChanged(const QStr &arg1);
    void on_point9_textChanged(const QStr &arg1);
    void on_skipfstabrepair_clicked(bool chckd);
    void on_userdatainclude_clicked(bool chckd);
    void on_usexzcompressor_clicked(bool chckd);
    void on_dirchoose_itemExpanded(QTrWI *item);
    void on_configurationfilesrestore_clicked();
    void on_itemslist_itemExpanded(QTrWI *item);
    void on_styleoverride_clicked(bool chckd);
    void on_autoisocreate_clicked(bool chckd);
    void on_repairpartitionrefresh_clicked();
    void on_alwaysontop_clicked(bool chckd);
    void on_silentmode_clicked(bool chckd);
    void on_livedevicesrefresh_clicked();
    void on_liveworkdirbutton_clicked();
    void on_rootpassword2_textChanged();
    void on_partitionrefresh2_clicked();
    void on_partitionrefresh3_clicked();
    void on_format_clicked(bool chckd);
    void on_functionmenunext_clicked();
    void on_functionmenuback_clicked();
    void on_storagedirbutton_clicked();
    void on_partitionrefresh_clicked();
    void on_fullname_editingFinished();
    void on_schedulerrefresh_clicked();
    void on_passwordinputok_clicked();
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
    void on_unmountdelete_clicked();
    void on_dialogcancel_clicked();
    void on_schedulemenu_clicked();
    void on_pointexclude_clicked();
    void on_systemrepair_clicked();
    void on_newpartition_clicked();
    void on_settingsmenu_clicked();
    void on_settingsback_clicked();
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
    void on_scalingdown_clicked();
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
    void on_scalingup_clicked();
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
    void on_unmount_clicked();
    void on_adduser_clicked();
    void on_hourup_clicked();
    void on_dayup_clicked();
};

#endif
