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

#include "ui_systemback.h"
#include "systemback.hpp"
#include <QDesktopWidget>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDateTime>
#include <sys/utsname.h>
#include <sys/swap.h>
#include <X11/Xlib.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>

#ifdef KeyRelease
#undef KeyRelease
#endif

#ifdef KeyPress
#undef KeyPress
#endif

#ifdef False
#undef False
#endif

#ifdef True
#undef True
#endif

ushort lblevent::MouseX, lblevent::MouseY;

systemback::systemback() : QMainWindow(nullptr, Qt::FramelessWindowHint), ui(new Ui::systemback)
{
    cfgupdt = sb::style != "auto" && [this] {
            if(QStyleFactory::keys().contains(sb::style))
            {
                qApp->setStyle(QStyleFactory::create(sb::style));
                return false;
            }

            sb::style = "auto";
            return true;
        }();

    schar snum(qApp->desktop()->screenNumber(this));
    shdltimer = dlgtimer = intrptimer = nullptr, wismax = nrxth = false;
    ui->setupUi(this);
    ui->dialogpanel->move(0, 0);
    for(QWdt *wdgt : QWL{ui->statuspanel, ui->scalingbuttonspanel, ui->buttonspanel, ui->resizepanel}) wdgt->hide();
    for(QWdt *wdgt : QWL{ui->dialogpanel, ui->windowmaximize, ui->windowminimize, ui->windowclose}) wdgt->setBackgroundRole(QPalette::Foreground);
    for(QWdt *wdgt : QWL{ui->function3, ui->windowbutton3, ui->windowmaximize, ui->windowminimize, ui->windowclose}) wdgt->setForegroundRole(QPalette::Base);
    ui->subdialogpanel->setBackgroundRole(QPalette::Background);
    ui->buttonspanel->setBackgroundRole(QPalette::Highlight);
    connect(ui->function3, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
    connect(ui->function3, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
    connect(ui->function3, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
    connect(ui->windowbutton3, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
    connect(ui->windowbutton3, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
    connect(ui->buttonspanel, SIGNAL(Mouse_Move()), this, SLOT(bmove()));
    connect(ui->buttonspanel, SIGNAL(Mouse_Leave()), this, SLOT(bleave()));
    connect(ui->windowminimize, SIGNAL(Mouse_Enter()), this, SLOT(wminenter()));
    connect(ui->windowminimize, SIGNAL(Mouse_Leave()), this, SLOT(wminleave()));
    connect(ui->windowminimize, SIGNAL(Mouse_Pressed()), this, SLOT(wminpressed()));
    connect(ui->windowminimize, SIGNAL(Mouse_Move()), this, SLOT(bmove()));
    connect(ui->windowminimize, SIGNAL(Mouse_Released()), this, SLOT(wminreleased()));
    connect(ui->windowclose, SIGNAL(Mouse_Enter()), this, SLOT(wcenter()));
    connect(ui->windowclose, SIGNAL(Mouse_Leave()), this, SLOT(wcleave()));
    connect(ui->windowclose, SIGNAL(Mouse_Pressed()), this, SLOT(wcpressed()));
    connect(ui->windowclose, SIGNAL(Mouse_Move()), this, SLOT(bmove()));
    connect(ui->windowclose, SIGNAL(Mouse_Released()), this, SLOT(wcreleased()));

    dialog = getuid() + getgid() > 0 ? 305 : [this] {
            if(qApp->arguments().count() == 2 && qApp->arguments().value(1) == "schedule")
                sstart = true;
            else if(! sb::lock(sb::Sblock))
                return 300;
            else if(sb::lock(sb::Dpkglock))
                sstart = false;
            else
                return 301;

            return 0;
        }();

    {
        QFont fnt;
        if(! sb::like(fnt.family(), {"_Ubuntu_", "_FreeSans_"})) fnt.setFamily(QFontDatabase().families().contains("Ubuntu") ? "Ubuntu" : "FreeSans");
        if(fnt.weight() != QFont::Normal) fnt.setWeight(QFont::Normal);
        if(fnt.bold()) fnt.setBold(false);
        if(fnt.italic()) fnt.setItalic(false);
        if(fnt.overline()) fnt.setOverline(false);
        if(fnt.strikeOut()) fnt.setStrikeOut(false);
        if(fnt.underline()) fnt.setUnderline(false);
        if(fnt != font()) setFont(fnt);

        if(! sb::like(sb::wsclng, {"_auto_", "_1_"}) || fontInfo().pixelSize() != 15)
        {
            sfctr = sb::wsclng == "auto" ? fontInfo().pixelSize() > 28 ? Max : fontInfo().pixelSize() > 21 ? High : Normal : sb::wsclng == "2" ? Max : sb::wsclng == "1.5" ? High : Normal;
            while(sfctr > Normal && (qApp->desktop()->screenGeometry(snum).width() - ss(30) < ss(698) || qApp->desktop()->screenGeometry(snum).height() - ss(30) < ss(465))) sfctr = sfctr == Max ? High : Normal;
            fnt.setPixelSize(ss(15));
            setFont(fnt);
            fnt.setPixelSize(ss(27));
            ui->buttonspanel->setFont(fnt);
            fnt.setPixelSize(ss(17));
            fnt.setBold(true);
            ui->passwordtitletext->setFont(fnt);

            if(sfctr > Normal)
            {
                for(QWdt *wdgt : findChildren<QWdt *>()) wdgt->setGeometry(ss(wdgt->x()), ss(wdgt->y()), ss(wdgt->width()), ss(wdgt->height()));
                for(QPB *pbtn : findChildren<QPB *>()) pbtn->setIconSize(QSize(ss(pbtn->iconSize().width()), ss(pbtn->iconSize().height())));

                if(! sstart && dialog == 0)
                {
                    {
                        ushort nsize[2]{ss(10), ss(20)};

                        for(QTblW *tblw : findChildren<QTblW *>())
                        {
                            tblw->horizontalHeader()->setMinimumSectionSize(nsize[0]);
                            tblw->verticalHeader()->setDefaultSectionSize(nsize[1]);
                        }
                    }

                    { QSize nsize(ss(112), ss(32));
                    for(QCbB *cmbx : findChildren<QCbB *>()) cmbx->setMinimumSize(nsize); }
                    QStyleOption optn;
                    optn.init(ui->pointpipe1);
                    QStr nsize(QStr::number(ss(ui->pointpipe1->style()->subElementRect(QStyle::SE_CheckBoxClickRect, &optn).width())));
                    for(QCB *ckbx : findChildren<QCB *>()) ckbx->setStyleSheet("QCB::indicator{width:" % nsize % "px; height:" % nsize % "px;}");
                    optn.init(ui->pnumber3);
                    nsize = QStr::number(ss(ui->pnumber3->style()->subElementRect(QStyle::SE_RadioButtonClickRect, &optn).width()));
                    for(QRadioButton *rbtn : findChildren<QRadioButton *>()) rbtn->setStyleSheet("QRadioButton::indicator{width:" % nsize % "px; height:" % nsize % "px;}");
                }
            }
        }
        else
            sfctr = Normal;
    }

    if(dialog > 0)
    {
        for(QWdt *wdgt : QWL{ui->mainpanel, ui->passwordpanel, ui->schedulerpanel}) wdgt->hide();
        dialogopen(dialog, nullptr, snum);
    }
    else
    {
        intrrpt = irblck = utblck = false, prun.type = prun.pnts = ppipe = busycnt = 0;
        ui->dialogpanel->hide();
        ui->statuspanel->move(0, 0);
        ui->statuspanel->setBackgroundRole(QPalette::Foreground);
        ui->substatuspanel->setBackgroundRole(QPalette::Background);
        for(QWdt *wdgt : QWL{ui->function2, ui->function4, ui->windowbutton2, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Base);
        ui->interrupt->setStyleSheet("QPushButton:enabled {color: red}");
        connect(ui->function2, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
        connect(ui->function2, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
        connect(ui->function2, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
        connect(ui->function4, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
        connect(ui->function4, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
        connect(ui->function4, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
        connect(ui->windowbutton2, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
        connect(ui->windowbutton2, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
        connect(ui->windowbutton4, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
        connect(ui->windowbutton4, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));

        if(! sstart)
        {
            icnt = 0, cpos = -1, nohmcpy[1] = uchkd = false;
            for(QWdt *wdgt : QWL{ui->restorepanel, ui->copypanel, ui->installpanel, ui->livecreatepanel, ui->repairpanel, ui->excludepanel, ui->schedulepanel, ui->aboutpanel, ui->licensepanel, ui->settingspanel, ui->choosepanel, ui->functionmenu2, ui->storagedirbutton, ui->fullnamepipe, ui->usernamepipe, ui->usernameerror, ui->passwordpipe, ui->passworderror, ui->rootpasswordpipe, ui->rootpassworderror, ui->hostnamepipe, ui->hostnameerror}) wdgt->hide();
            ui->storagedir->resize(ss(236), ss(28));
            ui->installpanel->move(ui->sbpanel->pos());
            ui->mainpanel->setBackgroundRole(QPalette::Foreground);
            for(QWdt *wdgt : QWL{ui->sbpanel, ui->installpanel, ui->subscalingbuttonspanel}) wdgt->setBackgroundRole(QPalette::Background);
            for(QWdt *wdgt : QWL{ui->function1, ui->scalingbutton, ui->windowbutton1}) wdgt->setForegroundRole(QPalette::Base);
            for(QWdt *wdgt : QWL{ui->scalingfactor, ui->storagedirarea}) wdgt->setBackgroundRole(QPalette::Base);
            ui->scalingbuttonspanel->setBackgroundRole(QPalette::Highlight);
            ui->scalingbuttonspanel->move(0, 0);

            if(sb::wsclng == "auto")
                ui->scalingdown->setDisabled(true);
            else if(sb::wsclng == "2")
            {
                ui->scalingfactor->setText("x2");
                ui->scalingup->setDisabled(true);
            }
            else
                ui->scalingfactor->setText('x' % sb::wsclng);

            connect(ui->windowmaximize, SIGNAL(Mouse_Enter()), this, SLOT(wmaxenter()));
            connect(ui->windowmaximize, SIGNAL(Mouse_Leave()), this, SLOT(wmaxleave()));
            connect(ui->windowmaximize, SIGNAL(Mouse_Pressed()), this, SLOT(wmaxpressed()));
            connect(ui->windowmaximize, SIGNAL(Mouse_Move()), this, SLOT(bmove()));
            connect(ui->windowmaximize, SIGNAL(Mouse_Released()), this, SLOT(wmaxreleased()));
            connect(ui->function1, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
            connect(ui->function1, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
            connect(ui->function1, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
            connect(ui->function1, SIGNAL(Mouse_DblClick()), this, SLOT(wdblclck()));
            connect(ui->scalingbutton, SIGNAL(Mouse_Pressed()), this, SLOT(sbttnpressed()));
            connect(ui->scalingbutton, SIGNAL(Mouse_Released()), this, SLOT(sbttnreleased()));
            connect(ui->scalingbutton, SIGNAL(Mouse_Move()), this, SLOT(sbttnmove()));
            connect(ui->scalingbuttonspanel, SIGNAL(Mouse_Leave()), this, SLOT(sbttnleave()));
            connect(ui->windowbutton1, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
            connect(ui->windowbutton1, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
            connect(ui->chooseresize, SIGNAL(Mouse_Enter()), this, SLOT(chsenter()));
            connect(ui->chooseresize, SIGNAL(Mouse_Leave()), this, SLOT(chsleave()));
            connect(ui->chooseresize, SIGNAL(Mouse_Pressed()), this, SLOT(chspressed()));
            connect(ui->chooseresize, SIGNAL(Mouse_Released()), this, SLOT(chsreleased()));
            connect(ui->chooseresize, SIGNAL(Mouse_Move()), this, SLOT(rmove()));
            connect(ui->copyresize, SIGNAL(Mouse_Enter()), this, SLOT(cpyenter()));
            connect(ui->copyresize, SIGNAL(Mouse_Leave()), this, SLOT(cpyleave()));
            connect(ui->copyresize, SIGNAL(Mouse_Pressed()), this, SLOT(chspressed()));
            connect(ui->copyresize, SIGNAL(Mouse_Released()), this, SLOT(chsreleased()));
            connect(ui->copyresize, SIGNAL(Mouse_Move()), this, SLOT(rmove()));
            connect(ui->excluderesize, SIGNAL(Mouse_Enter()), this, SLOT(xcldenter()));
            connect(ui->excluderesize, SIGNAL(Mouse_Leave()), this, SLOT(xcldleave()));
            connect(ui->excluderesize, SIGNAL(Mouse_Pressed()), this, SLOT(chspressed()));
            connect(ui->excluderesize, SIGNAL(Mouse_Released()), this, SLOT(chsreleased()));
            connect(ui->excluderesize, SIGNAL(Mouse_Move()), this, SLOT(rmove()));
            connect(ui->homepage1, SIGNAL(Mouse_Pressed()), this, SLOT(hmpg1pressed()));
            connect(ui->homepage1, SIGNAL(Mouse_Released()), this, SLOT(hmpg1released()));
            connect(ui->homepage1, SIGNAL(Mouse_Move()), this, SLOT(hmpg1move()));
            connect(ui->homepage2, SIGNAL(Mouse_Pressed()), this, SLOT(hmpg2pressed()));
            connect(ui->homepage2, SIGNAL(Mouse_Released()), this, SLOT(hmpg2released()));
            connect(ui->homepage2, SIGNAL(Mouse_Move()), this, SLOT(hmpg2move()));
            connect(ui->email, SIGNAL(Mouse_Pressed()), this, SLOT(emailpressed()));
            connect(ui->email, SIGNAL(Mouse_Released()), this, SLOT(emailreleased()));
            connect(ui->email, SIGNAL(Mouse_Move()), this, SLOT(emailmove()));
            connect(ui->donate, SIGNAL(Mouse_Pressed()), this, SLOT(dntpressed()));
            connect(ui->donate, SIGNAL(Mouse_Released()), this, SLOT(dntreleased()));
            connect(ui->donate, SIGNAL(Mouse_Move()), this, SLOT(dntmove()));
            connect(ui->point1, SIGNAL(Focus_Out()), this, SLOT(foutp1()));
            connect(ui->point2, SIGNAL(Focus_Out()), this, SLOT(foutp2()));
            connect(ui->point3, SIGNAL(Focus_Out()), this, SLOT(foutp3()));
            connect(ui->point4, SIGNAL(Focus_Out()), this, SLOT(foutp4()));
            connect(ui->point5, SIGNAL(Focus_Out()), this, SLOT(foutp5()));
            connect(ui->point6, SIGNAL(Focus_Out()), this, SLOT(foutp6()));
            connect(ui->point7, SIGNAL(Focus_Out()), this, SLOT(foutp7()));
            connect(ui->point8, SIGNAL(Focus_Out()), this, SLOT(foutp8()));
            connect(ui->point9, SIGNAL(Focus_Out()), this, SLOT(foutp9()));
            connect(ui->point10, SIGNAL(Focus_Out()), this, SLOT(foutp10()));
            connect(ui->point11, SIGNAL(Focus_Out()), this, SLOT(foutp11()));
            connect(ui->point12, SIGNAL(Focus_Out()), this, SLOT(foutp12()));
            connect(ui->point13, SIGNAL(Focus_Out()), this, SLOT(foutp13()));
            connect(ui->point14, SIGNAL(Focus_Out()), this, SLOT(foutp14()));
            connect(ui->point15, SIGNAL(Focus_Out()), this, SLOT(foutp15()));
            connect(ui->partitionsettings, SIGNAL(Focus_In()), this, SLOT(finpsttngs()));
            connect(ui->partitionsettings, SIGNAL(Focus_Out()), this, SLOT(foutpsttngs()));
            connect(ui->usersettingscopy, SIGNAL(Mouse_Enter()), this, SLOT(center()));
            connect(ui->usersettingscopy, SIGNAL(Mouse_Leave()), this, SLOT(cleave()));
            connect(ui->unmountdelete, SIGNAL(Mouse_Leave()), this, SLOT(umntleave()));

            if(! (sislive = sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs")))
                ui->livename->setText("auto");
            else if(sb::isdir("/.systemback"))
                on_installmenu_clicked();
        }

        if(qApp->arguments().count() == 3 && qApp->arguments().value(1) == "authorization" && (ui->sbpanel->isVisibleTo(ui->mainpanel) || ! sb::like(sb::fload("/proc/self/mounts"), {"* / overlay *","* / overlayfs *", "* / aufs *", "* / unionfs *", "* / fuse.unionfs-fuse *"})))
        {
            for(QWdt *wdgt : QWL{ui->mainpanel, ui->schedulerpanel, ui->adminpasswordpipe, ui->adminpassworderror}) wdgt->hide();
            ui->passwordpanel->move(0, 0);
            ui->adminstext->resize(fontMetrics().width(ui->adminstext->text()) + ss(7), ui->adminstext->height());
            ui->admins->move(ui->adminstext->x() + ui->adminstext->width(), ui->admins->y());
            ui->admins->setMaximumWidth(ui->passwordpanel->width() - ui->admins->x() - ss(8));
            ui->adminpasswordtext->resize(fontMetrics().width(ui->adminpasswordtext->text()) + ss(7), ui->adminpasswordtext->height());
            ui->adminpassword->move(ui->adminpasswordtext->x() + ui->adminpasswordtext->width(), ui->adminpassword->y());
            ui->adminpassword->resize(ss(336) - ui->adminpassword->x(), ui->adminpassword->height());

            {
                QFile file("/etc/group");

                if(file.open(QIODevice::ReadOnly))
                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());

                        if(cline.startsWith("sudo:"))
                            for(cQStr &usr : sb::right(cline, - sb::rinstr(cline, ":")).split(','))
                                if(! usr.isEmpty() && ui->admins->findText(usr) == -1) ui->admins->addItem(usr);
                    }
            }

            if(ui->admins->count() == 0)
                ui->admins->addItem("root");
            else if(ui->admins->findText(qApp->arguments().value(2)) != -1)
                ui->admins->setCurrentIndex(ui->admins->findText(qApp->arguments().value(2)));

            setFixedSize((wgeom[2] = ss(376)), (wgeom[3] = ss(224)));
            move((wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() / 2 - ss(188)), (wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() / 2 - ss(112)));
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        }
        else
        {
            ui->passwordpanel->hide();
            busy();
            QTimer::singleShot(0, this, SLOT(unitimer()));

            if(sstart)
            {
                ui->mainpanel->hide();
                ui->schedulerpanel->move(0, 0);
                ui->schedulerpanel->setBackgroundRole(QPalette::Foreground);
                ui->subschedulerpanel->setBackgroundRole(QPalette::Background);
                ui->function4->setText("Systemback " % tr("scheduler"));

                if(sb::schdlr[0] == "topleft")
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + ss(30), wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + ss(30);
                else if(sb::schdlr[0] == "center")
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() / 2 - ss(201), wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() / 2 - ss(80);
                else if(sb::schdlr[0] == "bottomleft")
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + ss(30), wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() - ss(191);
                else if(sb::schdlr[0] == "bottomright")
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() - ss(432), wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() - ss(191);
                else
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() - ss(432), wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + ss(30);

                setFixedSize((wgeom[2] = ss(402)), (wgeom[3] = ss(161)));
                move(wgeom[0], wgeom[1]);
                QTimer::singleShot(0, this, SLOT(schedulertimer()));
                setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
            }
            else
            {
                ui->schedulerpanel->hide();
                setFixedSize((wgeom[2] = ss(698)), (wgeom[3] = ss(465)));
                move((wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() / 2 - ss(349)), (wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() / 2 - ss(232)));
            }
        }
    }

    if(sb::waot == sb::True && ! windowFlags().testFlag(Qt::WindowStaysOnTopHint)) setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    Display *dsply(XOpenDisplay(nullptr));
    Atom atm(XInternAtom(dsply, "_MOTIF_WM_HINTS", 0));
    ulong hnts[]{3, 44, 0, 0, 0};
    XChangeProperty(dsply, winId(), atm, atm, 32, PropModeReplace, (uchar *)&hnts, 5);
    XFlush(dsply);
    XCloseDisplay(dsply);
    installEventFilter(this);
}

systemback::~systemback()
{
    if(cfgupdt) sb::cfgwrite();

    if(! nrxth)
    {
        QBA xauth(qgetenv("XAUTHORITY"));
        if(xauth.startsWith("/tmp/sbXauthority-")) QFile::remove(xauth);
    }

    for(QTimer *tmr : {shdltimer, dlgtimer, intrptimer})
        if(tmr) delete tmr;

    delete ui;
}

void systemback::closeEvent(QCloseEvent *ev)
{
    if(ui->statuspanel->isVisible() && ! (sstart && sb::ThrdKill) && prun.type != 11) ev->ignore();
}

void systemback::unitimer()
{
    if(! utblck)
    {
        utblck = true;

        if(! utimer.isActive())
        {
            switch(sb::pnumber) {
            case 3:
                ui->pnumber3->setChecked(true);
                on_pnumber3_clicked();
                break;
            case 4:
                ui->pnumber4->setChecked(true);
                on_pnumber4_clicked();
                break;
            case 6:
                ui->pnumber6->setChecked(true);
                on_pnumber6_clicked();
                break;
            case 7:
                ui->pnumber7->setChecked(true);
                on_pnumber7_clicked();
                break;
            case 8:
                ui->pnumber8->setChecked(true);
                on_pnumber8_clicked();
                break;
            case 9:
                ui->pnumber9->setChecked(true);
                on_pnumber9_clicked();
                break;
            case 10:
                ui->pnumber10->setChecked(true);
                on_pnumber10_clicked();
            }

            ui->resizepanel->move(ui->sbpanel->pos());

            if(! sstart)
            {
                ui->storagedir->setText(sb::sdir[0]);
                ui->storagedir->setToolTip(sb::sdir[0]);
                ui->liveworkdir->setText(sb::sdir[2]);
                ui->liveworkdir->setToolTip(sb::sdir[2]);
                for(QLE *ldt : QList<QLE *>{ui->storagedir, ui->liveworkdir}) ldt->setCursorPosition(0);

                for(QWdt *wdgt : QWL{ui->restorepanel, ui->copypanel, ui->livecreatepanel, ui->repairpanel, ui->excludepanel, ui->schedulepanel, ui->aboutpanel, ui->licensepanel, ui->settingspanel, ui->choosepanel})
                {
                    wdgt->move(ui->sbpanel->pos());
                    wdgt->setBackgroundRole(QPalette::Background);
                }

                for(QWdt *wdgt : QWL{ui->liveworkdirarea, ui->schedulerday, ui->schedulerhour, ui->schedulerminute, ui->schedulersecond}) wdgt->setBackgroundRole(QPalette::Base);
                ui->partitiondelete->setStyleSheet("QPushButton:enabled {color: red}");
                { QPalette pal(ui->license->palette());
                pal.setBrush(QPalette::Base, pal.background());
                ui->license->setPalette(pal); }
                ui->partitionsettings->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Label"), tr("Current mount point"), tr("New mount point"), tr("Filesystem"), tr("Format")});
                for(uchar a(7) ; a < 11 ; ++a) ui->partitionsettings->setColumnHidden(a, true);
                { QFont fnt;
                fnt.setPixelSize(ss(14));
                for(QTblW *wdgt : QList<QTblW *>{ui->partitionsettings, ui->livedevices}) wdgt->horizontalHeader()->setFont(fnt); }
                for(uchar a : {0, 1, 5, 6}) ui->partitionsettings->horizontalHeader()->setSectionResizeMode(a, QHeaderView::Fixed);
                ui->livedevices->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Device"), tr("Format")});
                for(uchar a : {0, 1, 3}) ui->livedevices->horizontalHeader()->setSectionResizeMode(a, QHeaderView::Fixed);
                for(QWdt *wdgt : QWL{ui->livenamepipe, ui->livenameerror, ui->partitionsettingspanel2, ui->partitionsettingspanel3, ui->grubreinstallrestoredisable, ui->grubreinstallrepairdisable, ui->usersettingscopy, ui->repaircover}) wdgt->hide();
                for(QCbB *cmbx : QCbBL{ui->grubreinstallrestoredisable, ui->grubinstallcopydisable, ui->grubreinstallrepairdisable}) cmbx->addItem(tr("Disabled"));
                if(sb::schdle[0] == sb::True) ui->schedulerstate->click();
                if(sb::schdle[5] == sb::True) ui->silentmode->setChecked(true);
                ui->windowposition->addItems({tr("Top left"), tr("Top right"), tr("Center"), tr("Bottom left"), tr("Bottom right")});
                if(sb::schdlr[0] != "topleft") ui->windowposition->setCurrentIndex(ui->windowposition->findText(sb::schdlr[0] == "topright" ? tr("Top right") : sb::schdlr[0] == "center" ? tr("Center") : sb::schdlr[0] == "bottomleft" ? tr("Bottom left") : tr("Bottom right")));
                ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
                ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
                ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
                ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));

                auto lodsbl([this](bool chkd = false) {
                        ui->languageoverride->setDisabled(true);

                        if(chkd || sb::lang != "auto")
                        {
                            sb::lang = "auto";
                            if(! cfgupdt) cfgupdt = true;
                        }
                    });

                if(sb::isdir("/usr/share/systemback/lang"))
                {
                    {
                        QSL lst("English (common)");
                        lst.reserve(13);

                        for(cQStr &item : QDir("/usr/share/systemback/lang").entryList(QDir::Files))
                        {
                            QStr lcode(sb::left(sb::right(item, -11), -3));

                            cchar *lname(lcode == "ar_EG" ? "المصرية العربية"
                                       : lcode == "ca_ES" ? "Català"
                                       : lcode == "cs" ? "Čeština"
                                       : lcode == "en_GB" ? "English (United Kingdom)"
                                       : lcode == "es" ? "Español"
                                       : lcode == "fi" ? "Suomi"
                                       : lcode == "fr" ? "Français"
                                       : lcode == "gl_ES" ? "Galego"
                                       : lcode == "hu" ? "Magyar"
                                       : lcode == "id" ? "Bahasa Indonesia"
                                       : lcode == "pt_BR" ? "Português (Brasil)"
                                       : lcode == "ro" ? "Română"
                                       : lcode == "tr" ? "Türkçe"
                                       : lcode == "zh_CN" ? "中文（简体）" : nullptr);

                            if(lname) lst.append(lname);
                        }

                        if(lst.count() == 1)
                            lodsbl();
                        else
                        {
                            lst.sort();

                            if(sb::lang == "auto")
                                ui->languages->addItems(lst);
                            else
                            {
                                schar indx(sb::lang == "id_ID" ? 0
                                         : sb::lang == "ar_EG" ? lst.indexOf("المصرية العربية")
                                         : sb::lang == "ca_ES" ? lst.indexOf("Català")
                                         : sb::lang == "cs_CS" ? lst.indexOf("Čeština")
                                         : sb::lang == "en_EN" ? lst.indexOf("English (common)")
                                         : sb::lang == "en_GB" ? lst.indexOf("English (United Kingdom)")
                                         : sb::lang == "es_ES" ? lst.indexOf("Español")
                                         : sb::lang == "fi_FI" ? lst.indexOf("Suomi")
                                         : sb::lang == "fr_FR" ? lst.indexOf("Français")
                                         : sb::lang == "gl_ES" ? lst.indexOf("Galego")
                                         : sb::lang == "hu_HU" ? lst.indexOf("Magyar")
                                         : sb::lang == "pt_BR" ? lst.indexOf("Português (Brasil)")
                                         : sb::lang == "ro_RO" ? lst.indexOf("Română")
                                         : sb::lang == "tr_TR" ? lst.indexOf("Türkçe")
                                         : sb::lang == "zh_CN" ? lst.indexOf("中文（简体）") : -1);

                                if(indx == -1)
                                    lodsbl(true);
                                else
                                {
                                    ui->languageoverride->setChecked(true);
                                    ui->languages->setEnabled(true);
                                    ui->languages->addItems(lst);
                                    if(indx > 0) ui->languages->setCurrentIndex(indx);
                                }
                            }
                        }
                    }

                    for(QLabel *lbl : findChildren<QLabel *>())
                        if(lbl->alignment() == (Qt::AlignLeft | Qt::AlignVCenter) && lbl->text().isRightToLeft()) lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                }
                else
                    lodsbl();

                { QSL kys(QStyleFactory::keys());
                kys.sort();
                ui->styles->addItems(kys); }

                if(sb::style != "auto")
                {
                    ui->styleoverride->setChecked(true);
                    ui->styles->setCurrentIndex(ui->styles->findText(sb::style));
                    ui->styles->setEnabled(true);
                }

                if(sb::waot == sb::True) ui->alwaysontop->setChecked(true);
                if(sb::incrmtl == sb::False) ui->incrementaldisable->setChecked(true);
                if(sb::ecache == sb::False) ui->cachemptydisable->setChecked(true);
                if(sb::xzcmpr == sb::True) ui->usexzcompressor->setChecked(true);
                if(sb::autoiso == sb::True) ui->autoisocreate->setChecked(true);

                if(sb::schdlr[1] != "false")
                {
                    if(sb::schdlr[1] == "everyone")
                    {
                        ui->schedulerdisable->click();
                        ui->schedulerusers->setText(tr("Everyone"));
                    }
                    else
                    {
                        ui->schedulerdisable->setChecked(true);
                        QSL susr(sb::right(sb::schdlr[1], -1).split(','));
                        if(! susr.contains("root")) ui->users->addItem("root");
                        QFile file("/etc/passwd");

                        if(file.open(QIODevice::ReadOnly))
                            while(! file.atEnd())
                            {
                                QStr usr(file.readLine().trimmed());
                                if(usr.contains(":/home/") && ! susr.contains((usr = sb::left(usr, sb::instr(usr, ":") -1))) && sb::isdir("/home/" % usr)) ui->users->addItem(usr);
                            }

                        if(ui->users->count() > 0)
                            for(QWdt *wdgt : QWL{ui->users, ui->adduser}) wdgt->setEnabled(true);

                        ui->schedulerusers->setText(sb::right(sb::schdlr[1], -1));
                        ui->schedulerrefresh->setEnabled(true);
                    }

                    ui->schedulerusers->setCursorPosition(0);
                }

                ui->includeuserstext->resize(fontMetrics().width(ui->includeuserstext->text()) + ss(7), ui->includeuserstext->height());
                ui->includeusers->move(ui->includeuserstext->x() + ui->includeuserstext->width(), ui->includeusers->y());
                ui->includeusers->setMaximumWidth(width() - ui->includeusers->x() - ss(8));
                ui->grubreinstallrestoretext->resize(fontMetrics().width(ui->grubreinstallrestoretext->text()) + ss(7), ui->grubreinstallrestoretext->height());
                ui->grubreinstallrestore->move(ui->grubreinstallrestoretext->x() + ui->grubreinstallrestoretext->width(), ui->grubreinstallrestore->y());
                ui->grubreinstallrestoredisable->move(ui->grubreinstallrestore->pos());
                ui->grubinstalltext->resize(fontMetrics().width(ui->grubinstalltext->text()) + ss(7), ui->grubinstalltext->height());
                ui->grubinstallcopy->move(ui->grubinstalltext->x() + ui->grubinstalltext->width(), ui->grubinstallcopy->y());
                ui->grubinstallcopydisable->move(ui->grubinstallcopy->pos());
                ui->grubreinstallrepairtext->resize(fontMetrics().width(ui->grubreinstallrepairtext->text()) + ss(7), ui->grubreinstallrepairtext->height());
                ui->grubreinstallrepair->move(ui->grubreinstallrepairtext->x() + ui->grubreinstallrepairtext->width(), ui->grubreinstallrepair->y());
                ui->grubreinstallrepairdisable->move(ui->grubreinstallrepair->pos());
                ui->schedulerstatetext->resize(fontMetrics().width(ui->schedulerstatetext->text()) + ss(7), ui->schedulerstatetext->height());
                ui->schedulerstate->move(ui->schedulerstatetext->x() + ui->schedulerstatetext->width(), ui->schedulerstate->y());
                ui->schedulersecondtext->resize(fontMetrics().width(ui->schedulersecondtext->text()) + ss(7), ui->schedulersecondtext->height());
                ui->schedulersecondpanel->move(ui->schedulersecondtext->x() + ui->schedulersecondtext->width(), ui->schedulersecondpanel->y());
                ui->windowpositiontext->resize(fontMetrics().width(ui->windowpositiontext->text()) + ss(7), ui->windowpositiontext->height());
                ui->windowposition->move(ui->windowpositiontext->x() + ui->windowpositiontext->width(), ui->windowposition->y());
                ui->homepage1->resize(fontMetrics().width(ui->homepage1->text()) + ss(7), ui->homepage1->height());
                ui->homepage2->resize(fontMetrics().width(ui->homepage2->text()) + ss(7), ui->homepage2->height());
                ui->email->resize(fontMetrics().width(ui->email->text()) + ss(7), ui->email->height());
                ui->donate->resize(fontMetrics().width(ui->donate->text()) + ss(7), ui->donate->height());
                ui->filesystemwarning->resize(ui->filesystemwarning->fontMetrics().width(ui->filesystemwarning->text()) + ss(7), ui->filesystemwarning->height());
                ui->format->resize(fontMetrics().width(ui->format->text()) + ss(28), ui->format->height());
                ui->format->move((ui->partitionsettingspanel1->width() - ui->format->width()) / 2, ui->format->y());
                ui->fullrestore->resize(fontMetrics().width(ui->fullrestore->text()) + ss(28), ui->fullrestore->height());
                ui->systemrestore->resize(fontMetrics().width(ui->systemrestore->text()) + ss(28), ui->systemrestore->height());
                ui->configurationfilesrestore->resize(fontMetrics().width(ui->configurationfilesrestore->text()) + ss(28), ui->configurationfilesrestore->height());
                ui->keepfiles->resize(fontMetrics().width(ui->keepfiles->text()) + ss(28), ui->keepfiles->height());
                ui->autorestoreoptions->resize(fontMetrics().width(ui->autorestoreoptions->text()) + ss(28), ui->autorestoreoptions->height());
                ui->skipfstabrestore->resize(fontMetrics().width(ui->skipfstabrestore->text()) + ss(28), ui->skipfstabrestore->height());
                ui->autorepairoptions->resize(fontMetrics().width(ui->autorepairoptions->text()) + ss(28), ui->autorepairoptions->height());
                ui->skipfstabrepair->resize(fontMetrics().width(ui->skipfstabrepair->text()) + ss(28), ui->skipfstabrepair->height());
                ui->userdatafilescopy->resize(fontMetrics().width(ui->userdatafilescopy->text()) + ss(28), ui->userdatafilescopy->height());
                ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration and data files")) + ss(28), ui->usersettingscopy->height());
                ui->usersettingscopy->setCheckState(Qt::PartiallyChecked);
                ui->userdatainclude->resize(fontMetrics().width(ui->userdatainclude->text()) + ss(28), ui->userdatainclude->height());
                ui->systemrepair->resize(fontMetrics().width(ui->systemrepair->text()) + ss(28), ui->systemrepair->height());
                ui->fullrepair->resize(fontMetrics().width(ui->fullrepair->text()) + ss(28), ui->fullrepair->height());
                ui->grubrepair->resize(fontMetrics().width(ui->grubrepair->text()) + ss(28), ui->grubrepair->height());
                ui->pointexclude->resize(fontMetrics().width(ui->pointexclude->text()) + ss(28), ui->pointexclude->height());
                ui->liveexclude->resize(fontMetrics().width(ui->liveexclude->text()) + ss(28), ui->liveexclude->height());
                ui->silentmode->resize(fontMetrics().width(ui->silentmode->text()) + ss(28), ui->silentmode->height());
                ui->languageoverride->resize(fontMetrics().width(ui->languageoverride->text()) + ss(28), ui->languageoverride->height());
                ui->languages->move(ui->languageoverride->x() + ui->languageoverride->width() + ss(3), ui->languages->y());
                ui->languages->setMaximumWidth(width() - ui->languages->x() - ss(8));
                ui->styleoverride->resize(fontMetrics().width(ui->styleoverride->text()) + ss(28), ui->styleoverride->height());
                ui->styles->move(ui->styleoverride->x() + ui->styleoverride->width() + ss(3), ui->styles->y());
                ui->styles->setMaximumWidth(width() - ui->styles->x() - ss(8));
                ui->alwaysontop->resize(fontMetrics().width(ui->alwaysontop->text()) + ss(28), ui->alwaysontop->height());
                ui->incrementaldisable->resize(fontMetrics().width(ui->incrementaldisable->text()) + ss(28), ui->incrementaldisable->height());
                ui->cachemptydisable->resize(fontMetrics().width(ui->cachemptydisable->text()) + ss(28), ui->cachemptydisable->height());
                ui->usexzcompressor->resize(fontMetrics().width(ui->usexzcompressor->text()) + ss(28), ui->usexzcompressor->height());
                ui->autoisocreate->resize(fontMetrics().width(ui->autoisocreate->text()) + ss(28), ui->autoisocreate->height());
                ui->schedulerdisable->resize(fontMetrics().width(ui->schedulerdisable->text()) + ss(28), ui->schedulerdisable->height());
                ui->filesystem->addItems({"ext4", "ext3", "ext2"});

                for(cQStr &fs : {"btrfs", "reiserfs", "jfs", "xfs"})
                    if(sb::execsrch("mkfs." % fs)) ui->filesystem->addItem(fs);

                ui->systembackversion->setText(sb::appver());
                ui->repairmountpoint->addItems({nullptr, "/mnt", "/mnt/home", "/mnt/boot"});
#ifdef __amd64__
                if(sb::isdir("/sys/firmware/efi")) goto isefi;

                {
                    QStr ckernel(ckname());

                    if(sb::isfile("/lib/modules/" % ckernel % "/modules.builtin"))
                    {
                        QFile file("/lib/modules/" % ckernel % "/modules.builtin");

                        if(file.open(QIODevice::ReadOnly))
                            while(! file.atEnd())
                                if(file.readLine().trimmed().endsWith("efivars.ko")) goto noefi;
                    }

                    if(sb::isfile("/proc/modules") && ! sb::fload("/proc/modules").contains("efivars ") && sb::isfile("/lib/modules/" % ckernel % "/modules.order"))
                    {
                        QFile file("/lib/modules/" % ckernel % "/modules.order");

                        if(file.open(QIODevice::ReadOnly))
                            while(! file.atEnd())
                            {
                                QBA cline(file.readLine().trimmed());
                                if(cline.endsWith("efivars.ko") && sb::isfile("/lib/modules/" % ckernel % '/' % cline) && sb::exec("modprobe efivars", nullptr, sb::Silent) == 0 && sb::isdir("/sys/firmware/efi")) goto isefi;
                            }
                    }
                }

                goto noefi;
            isefi:
                grub.name = "efi-amd64-bin", grub.isEFI = true;
                ui->repairmountpoint->addItem("/mnt/boot/efi");
                ui->grubinstallcopy->hide();
                for(QCbB *cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItems({"EFI", tr("Disabled")});
                ui->grubinstallcopydisable->adjustSize();
                ui->efiwarning->move(ui->grubinstallcopydisable->x() + ui->grubinstallcopydisable->width() + ss(5), ui->grubinstallcopydisable->y() - ss(4));
                ui->efiwarning->resize(ui->copypanel->width() - ui->efiwarning->x() - ss(8), ui->efiwarning->height());
                ui->efiwarning->setForegroundRole(QPalette::Highlight);
                goto next_1;
            noefi:
#endif
                grub.name = "pc-bin", grub.isEFI = false;
                for(QWdt *wdgt : QWL{ui->grubinstallcopydisable, ui->efiwarning}) wdgt->hide();
#ifdef __amd64__
            next_1:
#endif
                ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"});
                ui->repairmountpoint->setCurrentIndex(1);

                nohmcpy[0] = sb::isfile("/etc/fstab") && [] {
                        QFile file("/etc/fstab");

                        if(file.open(QIODevice::ReadOnly))
                        {
                            QSL incl{"* /home *", "*\t/home *", "* /home\t*", "*\t/home\t*", "* /home/ *", "*\t/home/ *", "* /home/\t*", "*\t/home/\t*"};

                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed());
                                if(! cline.startsWith('#') && sb::like(cline, incl)) return true;
                            }
                        }

                        return false;
                    }();

                on_partitionrefresh_clicked();
                on_livedevicesrefresh_clicked();
                on_pointexclude_clicked();
                ui->storagedir->resize(ss(210), ss(28));
                ui->storagedirbutton->show();
                for(QWdt *wdgt : QWL{ui->repairmenu, ui->aboutmenu, ui->settingsmenu, ui->pnumber3, ui->pnumber4, ui->pnumber5, ui->pnumber6, ui->pnumber7, ui->pnumber8, ui->pnumber9, ui->pnumber10}) wdgt->setEnabled(true);

                if(ui->installpanel->isHidden())
                {
                    if(! sislive) ickernel = [this] {
                            QStr ckernel(ckname()), fend[]{"order", "builtin"};

                            for(uchar a(0) ; a < 2 ; ++a)
                                if(sb::isfile("/lib/modules/" % ckernel % "/modules." % fend[a]))
                                {
                                    QFile file("/lib/modules/" % ckernel % "/modules." % fend[a]);

                                    if(file.open(QIODevice::ReadOnly))
                                    {
                                        QSL incl{"*aufs.ko_", "*overlay.ko_", "*overlayfs.ko_", "*unionfs.ko_"};

                                        while(! file.atEnd())
                                            if(sb::like(file.readLine().trimmed(), incl)) return true;
                                    }
                                }

                            return sb::execsrch("unionfs-fuse");
                        }();

                    for(QWdt *wdgt : QWL{ui->copymenu, ui->installmenu, ui->systemupgrade, ui->excludemenu, ui->schedulemenu}) wdgt->setEnabled(true);
                    pname = tr("Currently running system");
                }
            }

            pntupgrade();
            if(sstart) ui->schedulerstart->setEnabled(true);
            busy(false);
            connect(&utimer, SIGNAL(timeout()), this, SLOT(unitimer()));
            utimer.start(500);
        }
        else if(ui->statuspanel->isVisible())
        {
            if(prun.type > 0)
            {
                ui->processrun->setText(prun.txt % [this] {
                        switch(++prun.pnts) {
                        case 1:
                            return " .  ";
                        case 2:
                            return " .. ";
                        case 3:
                            return " ...";
                        default:
                            prun.pnts = 0;
                            return "    ";
                        }
                    }());

                switch(prun.type) {
                case 2 ... 6:
                case 8 ... 10:
                case 15:
                case 19 ... 21:
                {
                    if(irblck)
                    {
                        if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);
                    }
                    else if(! ui->interrupt->isEnabled())
                        ui->interrupt->setEnabled(true);

                    schar cperc(sb::Progress);

                    if(cperc != -1)
                    {
                        if(ui->progressbar->maximum() == 0) ui->progressbar->setMaximum(100);

                        if(cperc < 100)
                        {
                            if(ui->progressbar->value() < cperc)
                                ui->progressbar->setValue(cperc);
                            else if(sb::like(99, {cperc, ui->progressbar->value()}, true))
                                ui->progressbar->setValue(100);
                        }
                        else if(ui->progressbar->value() < 100)
                            ui->progressbar->setValue(100);
                    }
                    else if(ui->progressbar->maximum() == 100)
                        ui->progressbar->setMaximum(0);

                    break;
                }
                case 18:
                {
                    if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                    schar cperc(sb::Progress);

                    if(cperc != -1)
                    {
                        if(ui->progressbar->maximum() == 0) ui->progressbar->setMaximum(100);
                        if(cperc < 101 && ui->progressbar->value() < cperc) ui->progressbar->setValue(cperc);
                    }
                    else if(ui->progressbar->maximum() == 100)
                        ui->progressbar->setMaximum(0);

                    break;
                }
                default:
                    if(! irblck && sb::like(prun.type, {12, 14, 16, 17}))
                    {
                        if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                    }
                    else if(ui->interrupt->isEnabled())
                        ui->interrupt->setDisabled(true);

                    if(ui->progressbar->maximum() == 100) ui->progressbar->setMaximum(0);
                }
            }
        }
        else
        {
            if(! sstart)
            {
                auto acserr([this] {
                        pntupgrade();

                        if(ui->dialogquestion->isVisible())
                            on_dialogcancel_clicked();
                        else if(ui->restorepanel->isVisible())
                            on_restoreback_clicked();
                        else if(ui->copypanel->isVisible())
                        {
                            on_copyback_clicked();
                            if(ui->function1->text() != "Systemback") on_installback_clicked();
                        }
                        else if(ui->installpanel->isVisible())
                            on_installback_clicked();
                        else if(ui->repairpanel->isVisible())
                            on_repairback_clicked();
                    });

                if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write))
                {
                    if(! ui->storagedirarea->styleSheet().isEmpty())
                    {
                        for(QWdt *wdgt : QWL{ui->storagedirarea, ui->storagedir}) wdgt->setStyleSheet(nullptr);
                        fontcheck(Strgdr);
                        pntupgrade();
                    }

                    if(ppipe == 0 && pname == tr("Currently running system") && ! ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setEnabled(true);
                    schar num(-1);

                    for(QLE *ldt : ui->points->findChildren<QLE *>())
                    {
                        ++num;

                        if(ldt->isEnabled() && ! sb::isdir(sb::sdir[1] % [num]() -> QStr {
                                switch(num) {
                                case 9:
                                    return "/S10_";
                                case 10 ... 14:
                                    return "/H0" % QStr::number(num - 9) % '_';
                                default:
                                    return "/S0" % QStr::number(num + 1) % '_';
                                }
                            }() % sb::pnames[num]))
                        {
                            acserr();
                            break;
                        }
                    }
                }
                else
                {
                    if(ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) acserr();
                    if(ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setDisabled(true);

                    if(ui->storagedirarea->styleSheet().isEmpty())
                    {
                        for(QWdt *wdgt : QWL{ui->storagedirarea, ui->storagedir}) wdgt->setStyleSheet("background-color: rgb(255, 103, 103)");
                        fontcheck(Strgdr);
                    }
                }

                if(ui->installpanel->isVisible())
                {
                    if(ui->installmenu->isEnabled() && ui->fullnamepipe->isVisible() && ui->usernamepipe->isVisible() && ui->hostnamepipe->isVisible() && ui->passwordpipe->isVisible() && (ui->rootpassword1->text().isEmpty() || ui->rootpasswordpipe->isVisible()) && ! ui->installnext->isEnabled()) ui->installnext->setEnabled(true);
                }
                else if(ui->livecreatepanel->isVisible())
                {
                    if(ui->livenameerror->isVisible() || ui->liveworkdir->text().isEmpty())
                    {
                        if(ui->livecreatenew->isEnabled()) ui->livecreatenew->setDisabled(true);
                    }
                    else if(ui->livenamepipe->isVisible() || ui->livename->text() == "auto")
                    {
                        if(sb::isdir(sb::sdir[2]) && sb::access(sb::sdir[2], sb::Write))
                        {
                            if(! ui->liveworkdirarea->styleSheet().isEmpty())
                            {
                                for(QWdt *wdgt : QWL{ui->liveworkdirarea, ui->liveworkdir}) wdgt->setStyleSheet(nullptr);
                                fontcheck(Lvwrkdr);
                            }

                            if(ickernel && ! ui->livecreatenew->isEnabled()) ui->livecreatenew->setEnabled(true);

                            if(! ui->livelist->isEnabled())
                            {
                                ui->livelist->setEnabled(true);
                                on_livecreatemenu_clicked();
                            }
                        }
                        else
                        {
                            if(ui->liveworkdirarea->styleSheet().isEmpty())
                            {
                                for(QWdt *wdgt : QWL{ui->liveworkdirarea, ui->liveworkdir}) wdgt->setStyleSheet("background-color: rgb(255, 103, 103)");
                                fontcheck(Lvwrkdr);
                            }

                            for(QWdt *wdgt : QWL{ui->livecreatenew, ui->livedelete, ui->liveconvert, ui->livewritestart})
                                if(wdgt->isEnabled()) wdgt->setDisabled(true);

                            if(ui->livelist->isEnabled())
                            {
                                if(ui->livelist->count() > 0) ui->livelist->clear();
                                ui->livelist->setDisabled(true);
                            }
                        }
                    }
                }
                else if(ui->repairpanel->isVisible())
                    rmntcheck();
            }

            if(prun.type > 0)
            {
                prun.type = 0;
                prun.txt.clear();
                ui->processrun->clear();
                if(prun.pnts > 0) prun.pnts = 0;
                if(sb::Progress != -1) sb::Progress = -1;
                if(ui->progressbar->maximum() == 0) ui->progressbar->setMaximum(100);
                if(ui->progressbar->value() > 0) ui->progressbar->setValue(0);
                if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);
            }
        }

        if(ui->buttonspanel->isVisible() && ! minside(ui->buttonspanel->pos(), ui->buttonspanel->size())) ui->buttonspanel->hide();
        utblck = false;
    }
}

inline ushort systemback::ss(ushort size)
{
    switch(sfctr) {
    case Max:
        return size * 2;
    case High:
        switch(size) {
        case 0:
            return 0;
        case 1:
            return 2;
        default:
            ushort ssize((size * High + 50) / 100);

            switch(size) {
            case 154:
            case 201:
            case 402:
            case 506:
            case 698:
                return ssize + 1;
            default:
                return ssize;
            }
        }
    default:
        return size;
    }
}

inline QLE *systemback::getpoint(uchar num)
{
    schar cnum(-1);

    for(QLE *ldt : ui->points->findChildren<QLE *>())
        if(++cnum == num) return ldt;

    return nullptr;
}

inline QCB *systemback::getppipe(uchar num)
{
    schar cnum(-1);

    for(QCB *ckbx : ui->sbpanel->findChildren<QCB *>())
        if(++cnum == num) return ckbx;

    return nullptr;
}

void systemback::fontcheck(uchar wdgt)
{
    switch(wdgt) {
    case Strgdr:
        if(ui->storagedir->font().pixelSize() != ss(15))
        {
            for(QWdt *wdgt : QWL{ui->storagedirarea, ui->storagedir}) wdgt->setFont(font());
            QFont fnt;
            fnt.setPixelSize(ss(15));
            fnt.setBold(true);
            ui->storagedirbutton->setFont(fnt);
        }

        break;
    case Lvwrkdr:
        if(ui->liveworkdir->font().pixelSize() != ss(15))
        {
            for(QWdt *wdgt : QWL{ui->liveworkdirarea, ui->liveworkdir}) wdgt->setFont(font());
            QFont fnt;
            fnt.setPixelSize(ss(15));
            fnt.setBold(true);
            ui->liveworkdirbutton->setFont(fnt);
        }

        break;
    case Dpath:
        if(ui->dirpath->font().pixelSize() != ss(15)) ui->dirpath->setFont(font());
        break;
    case Rpnts:
        for(QLE *ldt : ui->points->findChildren<QLE *>())
            if(ldt->font().pixelSize() != ss(15)) ldt->setFont(font());
    }
}

void systemback::busy(bool state)
{
    state ? ++busycnt : --busycnt;

    switch(busycnt) {
    case 0:
        if(qApp->overrideCursor()->shape() == Qt::WaitCursor) qApp->restoreOverrideCursor();
        break;
    case 1:
        if(! qApp->overrideCursor()) qApp->setOverrideCursor(Qt::WaitCursor);
    }
}

inline bool systemback::minside(cQPoint &wpos, cQSize &wsize)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
    short wxy[2];

    if(wismax)
    {
        schar snum(qApp->desktop()->screenNumber(this));
        short scrxy(qApp->desktop()->availableGeometry(snum).x());
        wxy[0] = scrxy > 0 && x() == 0 ? scrxy : x(), wxy[1] = (scrxy = qApp->desktop()->availableGeometry(snum).y()) > 0 && y() == 0 ? scrxy : y();
    }
    else
        wxy[0] = x(), wxy[1] = y();
#endif

    return QCursor::pos().x() >
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
            wxy[0]
#else
            x()
#endif
            + wpos.x() && QCursor::pos().y() >
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
            wxy[1]
#else
            y()
#endif
            + wpos.y() && QCursor::pos().x() <
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
            wxy[0]
#else
            x()
#endif
            + wpos.x() + wsize.width() && QCursor::pos().y() <
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
            wxy[1]
#else
            y()
#endif
            + wpos.y() + wsize.height();
}

QStr systemback::guname()
{
    if(! uchkd && (ui->admins->count() == 0 || ui->admins->currentText() == "root"))
    {
        uchkd = true;
        QSL usrs;

        {
            QFile file("/etc/passwd");

            if(file.open(QIODevice::ReadOnly))
            {
                QSL incl{"*:x:1000:10*", "*:x:1001:10*", "*:x:1002:10*", "*:x:1003:10*", "*:x:1004:10*", "*:x:1005:10*", "*:x:1006:10*", "*:x:1007:10*", "*:x:1008:10*", "*:x:1009:10*", "*:x:1010:10*", "*:x:1011:10*", "*:x:1012:10*", "*:x:1013:10*", "*:x:1014:10*", "*:x:1015:10*"};

                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());
                    if(sb::like(usr, incl)) usrs.append(sb::left(usr, sb::instr(usr, ":") -1));
                }
            }
        }

        if(usrs.count() > 0)
        {
            QStr uname;

            for(cQStr &usr : usrs)
                if(sb::isdir("/home/" % usr))
                {
                    uname = usr;
                    break;
                }

            if(uname.isEmpty()) uname = usrs.at(0);
            if(ui->admins->findText(uname) == -1) ui->admins->addItem(uname);
            if(ui->admins->count() > 1) ui->admins->setCurrentIndex(ui->admins->findText(uname));
        }
        else if(ui->admins->currentText().isNull())
            ui->admins->addItem("root");
    }

    return ui->admins->currentText();
}

QStr systemback::ckname()
{
    utsname snfo;
    uname(&snfo);
    return snfo.release;
}

void systemback::pset(uchar type, cchar *tend)
{
    prun.txt = [type, &tend]() -> QStr {
            switch(type) {
            case 1:
                return sb::ecache == sb::True ? tr("Emptying cache") : tr("Flushing filesystem buffers");
            case 2:
                return tr("Restoring the full system");
            case 3:
                return tr("Restoring the system files");
            case 4:
                return tr("Restoring user(s) configuration files");
            case 5:
                return tr("Repairing the system files");
            case 6:
                return tr("Repairing the full system");
            case 7:
                return tr("Repairing the GRUB 2");
            case 8:
                return tr("Copying the system");
            case 9:
                return tr("Installing the system");
            case 10:
                return tr("Writing Live image to target device");
            case 11:
                return tr("Upgrading the system");
            case 12:
                return tr("Deleting incomplete restore point");
            case 13:
                return tr("Interrupting the current process");
            case 14:
                return tr("Deleting old restore point") % ' ' % tend;
            case 15:
                return tr("Creating restore point");
            case 16:
                return tr("Deleting restore point") % ' ' % tend;
            case 21:
                return tr("Converting Live system image") % '\n' % tr("process") % tend;
            default:
                return tr("Creating Live system") % '\n' % tr("process") % tend % (type < 20 && sb::autoiso == sb::True ? "+1" : nullptr);
            }
        }();

    prun.type = type;
}

void systemback::emptycache()
{
    pset(1);
    sb::fssync();
    if(sb::ecache == sb::True) sb::crtfile("/proc/sys/vm/drop_caches", "3");
}

void systemback::stschange()
{
    schar snum(qApp->desktop()->screenNumber(this));

    if(wismax)
    {
        wismax = false;
        setGeometry(wgeom[0], wgeom[1], wgeom[2], wgeom[3]);
        setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60));
    }
    else
    {
        wismax = true;
        setMaximumSize(qApp->desktop()->availableGeometry(snum).size());
        setGeometry(qApp->desktop()->availableGeometry(snum));

        if(ui->copypanel->isVisible())
            for(uchar a(2) ; a < 5 ; ++a) ui->partitionsettings->resizeColumnToContents(a);
    }
}

void systemback::schedulertimer()
{
    if(ui->schedulernumber->text().isEmpty())
    {
        ui->schedulernumber->setText(QStr::number(sb::schdle[4]) % 's');
        connect((shdltimer = new QTimer), SIGNAL(timeout()), this, SLOT(schedulertimer()));
        shdltimer->start(1000);
    }
    else if(ui->schedulernumber->text() == "1s")
        on_schedulerstart_clicked();
    else
        ui->schedulernumber->setText(QStr::number(sb::left(ui->schedulernumber->text(), - 1).toUShort() - 1) % 's');
}

void systemback::dialogtimer()
{
    ui->dialognumber->setText(QStr::number(sb::left(ui->dialognumber->text(), -1).toUShort() - 1) % "s");
    if(ui->dialognumber->text() == "0s" && sb::like(ui->dialogok->text(), {'_' % tr("Reboot") % '_', '_' % tr("X restart") % '_'})) on_dialogok_clicked();
}

void systemback::wpressed()
{
    if(! wismax)
    {
        if(qApp->overrideCursor()) qApp->restoreOverrideCursor();
        qApp->setOverrideCursor(Qt::SizeAllCursor);
    }
}

void systemback::wreleased()
{
    if(qApp->overrideCursor() && qApp->overrideCursor()->shape() == Qt::SizeAllCursor)
    {
        qApp->restoreOverrideCursor();
        if(busycnt > 0) qApp->setOverrideCursor(Qt::WaitCursor);
    }

    if(! wismax)
    {
        schar snum(qApp->desktop()->screenNumber(this));
        short scrxy(qApp->desktop()->screenGeometry(snum).x());

        if(x() < scrxy)
            wgeom[0] = scrxy + ss(30);
        else
        {
            short scrw(qApp->desktop()->screenGeometry(snum).width());
            if(x() > scrxy + scrw - width()) wgeom[0] = scrxy + scrw - width() - ss(30);
        }

        if(y() < (scrxy = qApp->desktop()->screenGeometry(snum).y()))
            wgeom[1] = scrxy + ss(30);
        else
        {
            short scrh(qApp->desktop()->screenGeometry(snum).height());
            if(y() > scrxy + scrh - height()) wgeom[1] = scrxy + scrh - height() - ss(30);
        }

        if(pos() != QPoint(wgeom[0], wgeom[1])) move(wgeom[0], wgeom[1]);
    }
}

void systemback::wdblclck()
{
    if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->choosepanel->isVisible()) stschange();
}

void systemback::benter()
{
    if(qApp->mouseButtons() == Qt::NoButton)
    {
        if(ui->statuspanel->isVisible())
        {
            if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
            {
                ui->windowmaximize->hide();
                ui->windowminimize->move(ss(2), ss(2));
            }

            if(ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->hide();
            if(ui->buttonspanel->width() != ss(48)) ui->buttonspanel->resize(ui->buttonspanel->height(), ui->buttonspanel->height());
        }
        else if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->choosepanel->isVisible())
        {
            if(! ui->windowmaximize->isVisibleTo(ui->buttonspanel))
            {
                ui->windowmaximize->show();
                ui->windowminimize->move(ss(47), ss(2));
            }

            if(wismax)
            {
                if(ui->windowmaximize->text() == "□") ui->windowmaximize->setText("▭");
            }
            else if(ui->windowmaximize->text() == "▭")
                ui->windowmaximize->setText("□");

            wmaxleave();
            if(ui->windowclose->x() != ss(92)) ui->windowclose->move(ss(92), ss(2));
            if(! ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->show();
            if(ui->buttonspanel->width() != ss(138)) ui->buttonspanel->resize(ss(138), ui->buttonspanel->height());
        }
        else
        {
            if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
            {
                ui->windowmaximize->hide();
                ui->windowminimize->move(ss(2), ss(2));
            }

            if(ui->windowclose->x() != ss(47)) ui->windowclose->move(ss(47), ss(2));
            if(! ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->show();
            if(ui->buttonspanel->width() != ss(93)) ui->buttonspanel->resize(ss(93), ui->buttonspanel->height());
        }

        wminleave();
        if(ui->buttonspanel->x() != width() - ui->buttonspanel->width()) ui->buttonspanel->move(width() - ui->buttonspanel->width(), 0);
        ui->buttonspanel->show();
    }
}

void systemback::bmove()
{
    if(minside(ui->buttonspanel->pos(), ui->buttonspanel->size()))
    {
        if(ui->windowmaximize->isVisible())
        {
            if(minside(ui->buttonspanel->pos() + ui->windowmaximize->pos(), ui->windowmaximize->size()))
            {
                if(ui->windowmaximize->backgroundRole() == QPalette::Foreground)
                {
                    ui->windowmaximize->setBackgroundRole(QPalette::Background);
                    ui->windowmaximize->setForegroundRole(QPalette::Text);
                }
            }
            else if(ui->windowmaximize->backgroundRole() == QPalette::Background)
            {
                ui->windowmaximize->setBackgroundRole(QPalette::Foreground);
                ui->windowmaximize->setForegroundRole(QPalette::Base);
            }
        }

        if(minside(ui->buttonspanel->pos() + ui->windowminimize->pos(), ui->windowminimize->size()))
        {
            if(ui->windowminimize->backgroundRole() == QPalette::Foreground)
            {
                ui->windowminimize->setBackgroundRole(QPalette::Background);
                ui->windowminimize->setForegroundRole(QPalette::Text);
            }
        }
        else if(ui->windowminimize->backgroundRole() == QPalette::Background)
        {
            ui->windowminimize->setBackgroundRole(QPalette::Foreground);
            ui->windowminimize->setForegroundRole(QPalette::Base);
        }

        if(ui->windowclose->isVisible())
        {
            if(minside(ui->buttonspanel->pos() + ui->windowclose->pos(), ui->windowclose->size()))
            {
                if(ui->windowclose->backgroundRole() == QPalette::Foreground)
                {
                    ui->windowclose->setBackgroundRole(QPalette::Background);
                    ui->windowclose->setForegroundRole(QPalette::Text);
                }
            }
            else if(ui->windowclose->backgroundRole() == QPalette::Background)
            {
                ui->windowclose->setBackgroundRole(QPalette::Foreground);
                ui->windowclose->setForegroundRole(QPalette::Base);
            }
        }

        if(ui->buttonspanel->isHidden() && minside(ui->windowbutton1->pos(), ui->windowbutton1->size())) ui->buttonspanel->show();
    }
    else if(ui->buttonspanel->isVisible())
        ui->buttonspanel->hide();
}

void systemback::bleave()
{
    if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
}

void systemback::wmaxenter()
{
     ui->windowmaximize->setBackgroundRole(QPalette::Background);
     ui->windowmaximize->setForegroundRole(QPalette::Text);
}

void systemback::wmaxleave()
{
    if(ui->windowmaximize->backgroundRole() == QPalette::Background)
    {
        ui->windowmaximize->setBackgroundRole(QPalette::Foreground);
        ui->windowmaximize->setForegroundRole(QPalette::Base);
    }
}

void systemback::wmaxpressed()
{
    ui->windowmaximize->setForegroundRole(QPalette::Highlight);
}

void systemback::wmaxreleased()
{
    if(ui->buttonspanel->isVisible() && ui->windowmaximize->foregroundRole() == QPalette::Highlight)
    {
        stschange();
        if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
    }
}

void systemback::wminenter()
{
    ui->windowminimize->setBackgroundRole(QPalette::Background);
    ui->windowminimize->setForegroundRole(QPalette::Text);
}

void systemback::wminleave()
{
    if(ui->windowminimize->backgroundRole() == QPalette::Background)
    {
        ui->windowminimize->setBackgroundRole(QPalette::Foreground);
        ui->windowminimize->setForegroundRole(QPalette::Base);
    }
}

void systemback::wminpressed()
{
    ui->windowminimize->setForegroundRole(QPalette::Highlight);
}

void systemback::wminreleased()
{
    if(ui->buttonspanel->isVisible() && ui->windowminimize->foregroundRole() == QPalette::Highlight)
    {
        Display *dsply(XOpenDisplay(nullptr));
        XWindowAttributes attr;
        XGetWindowAttributes(dsply, winId(), &attr);
        XIconifyWindow(dsply, winId(), XScreenNumberOfScreen(attr.screen));
        XFlush(dsply);
        XCloseDisplay(dsply);
    }
}

void systemback::wcenter()
{
    ui->windowclose->setBackgroundRole(QPalette::Background);
    ui->windowclose->setForegroundRole(QPalette::Text);
}

void systemback::wcleave()
{
    if(ui->windowclose->backgroundRole() == QPalette::Background)
    {
        ui->windowclose->setBackgroundRole(QPalette::Foreground);
        ui->windowclose->setForegroundRole(QPalette::Base);
    }
}

void systemback::wcpressed()
{
    ui->windowclose->setForegroundRole(QPalette::Highlight);
}

void systemback::wcreleased()
{
    if(ui->buttonspanel->isVisible() && ui->windowclose->foregroundRole() == QPalette::Highlight) close();
}

void systemback::chsenter()
{
    if(! wismax)
    {
        if(ui->chooseresize->cursor().shape() == Qt::ArrowCursor) ui->chooseresize->setCursor(Qt::PointingHandCursor);
        if(ui->chooseresize->width() == ss(10)) ui->chooseresize->setGeometry(ui->chooseresize->x() - ss(20), ui->chooseresize->y() - ss(20), ss(30), ss(30));
    }
    else if(ui->chooseresize->cursor().shape() == Qt::PointingHandCursor)
        ui->chooseresize->setCursor(Qt::ArrowCursor);
}

void systemback::chsleave()
{
    if(ui->chooseresize->width() == ss(30) && (! qApp->overrideCursor() || qApp->overrideCursor()->shape() != Qt::SizeFDiagCursor)) ui->chooseresize->setGeometry(ui->chooseresize->x() + ss(20), ui->chooseresize->y() + ss(20), ss(10), ss(10));
}

void systemback::chspressed()
{
    if(! wismax)
    {
        if(qApp->overrideCursor()) qApp->restoreOverrideCursor();
        qApp->setOverrideCursor(Qt::SizeFDiagCursor);
    }
}

void systemback::chsreleased()
{
    if(qApp->overrideCursor() && qApp->overrideCursor()->shape() == Qt::SizeFDiagCursor)
    {
        qApp->restoreOverrideCursor();
        if(busycnt > 0) qApp->setOverrideCursor(Qt::WaitCursor);
    }
}

void systemback::cpyenter()
{
    if(! wismax)
    {
        if(ui->copyresize->cursor().shape() == Qt::ArrowCursor) ui->copyresize->setCursor(Qt::PointingHandCursor);
        if(ui->copyresize->width() == ss(10)) ui->copyresize->setGeometry(ui->copyresize->x() - ss(20), ui->copyresize->y() - ss(20), ss(30), ss(30));
    }
    else if(ui->copyresize->cursor().shape() == Qt::PointingHandCursor)
        ui->copyresize->setCursor(Qt::ArrowCursor);
}

void systemback::cpyleave()
{
    if(ui->copyresize->width() == ss(30) && (! qApp->overrideCursor() || qApp->overrideCursor()->shape() != Qt::SizeFDiagCursor)) ui->copyresize->setGeometry(ui->copyresize->x() + ss(20), ui->copyresize->y() + ss(20), ss(10), ss(10));
}

void systemback::xcldenter()
{
    if(! wismax)
    {
        if(ui->excluderesize->cursor().shape() == Qt::ArrowCursor) ui->excluderesize->setCursor(Qt::PointingHandCursor);
        if(ui->excluderesize->width() == ss(10)) ui->excluderesize->setGeometry(ui->excluderesize->x() - ss(20), ui->excluderesize->y() - ss(20), ss(30), ss(30));
    }
    else if(ui->excluderesize->cursor().shape() == Qt::PointingHandCursor)
        ui->excluderesize->setCursor(Qt::ArrowCursor);
}

void systemback::xcldleave()
{
    if(ui->excluderesize->width() == ss(30) && (! qApp->overrideCursor() || qApp->overrideCursor()->shape() != Qt::SizeFDiagCursor)) ui->excluderesize->setGeometry(ui->excluderesize->x() + ss(20), ui->excluderesize->y() + ss(20), ss(10), ss(10));
}

void systemback::sbttnpressed()
{
    ui->scalingbutton->setForegroundRole(QPalette::Highlight);
}

void systemback::sbttnreleased()
{
    if(ui->scalingbutton->foregroundRole() == QPalette::Highlight) ui->scalingbuttonspanel->show();
}

void systemback::sbttnmove()
{
    if(minside({0, 0}, ui->scalingbutton->size()))
    {
        if(ui->scalingbutton->foregroundRole() == QPalette::Base) ui->scalingbutton->setForegroundRole(QPalette::Highlight);
    }
    else if(ui->scalingbutton->foregroundRole() == QPalette::Highlight)
        ui->scalingbutton->setForegroundRole(QPalette::Base);
}

void systemback::sbttnleave()
{
    QStr nsclng(ui->scalingfactor->text() == "auto" ? "auto" : sb::right(ui->scalingfactor->text(), -1));

    if(sb::wsclng == nsclng)
    {
        ui->scalingbutton->setForegroundRole(QPalette::Base);
        ui->scalingbuttonspanel->hide();
    }
    else
    {
        sb::wsclng = nsclng, nrxth = true;
        sb::cfgwrite();
        if(cfgupdt) cfgupdt = false;
        sb::unlock(sb::Sblock);
        sb::exec("systemback", nullptr, sb::Bckgrnd);
        close();
    }
}

void systemback::hmpg1pressed()
{
    ui->homepage1->setForegroundRole(QPalette::Highlight);
}

void systemback::hmpg1released()
{
    if(ui->homepage1->foregroundRole() == QPalette::Highlight)
    {
        ui->homepage1->setForegroundRole(QPalette::Text);
        sb::exec("su -c \"xdg-open https://sourceforge.net/projects/systemback &\" " % guname(), nullptr, sb::Bckgrnd);
    }
}

void systemback::hmpg1move()
{
    if(minside(ui->aboutpanel->pos() + ui->homepage1->pos(), ui->homepage1->size()))
    {
        if(ui->homepage1->foregroundRole() == QPalette::Text) ui->homepage1->setForegroundRole(QPalette::Highlight);
    }
    else if(ui->homepage1->foregroundRole() == QPalette::Highlight)
        ui->homepage1->setForegroundRole(QPalette::Text);
}

void systemback::hmpg2pressed()
{
    ui->homepage2->setForegroundRole(QPalette::Highlight);
}

void systemback::hmpg2released()
{
    if(ui->homepage2->foregroundRole() == QPalette::Highlight)
    {
        ui->homepage2->setForegroundRole(QPalette::Text);
        sb::exec("su -c \"xdg-open https://launchpad.net/systemback &\" " % guname(), nullptr, sb::Bckgrnd);
    }
}

void systemback::hmpg2move()
{
    if(minside(ui->aboutpanel->pos() + ui->homepage2->pos(), ui->homepage2->size()))
    {
        if(ui->homepage2->foregroundRole() == QPalette::Text) ui->homepage2->setForegroundRole(QPalette::Highlight);
    }
    else if(ui->homepage2->foregroundRole() == QPalette::Highlight)
        ui->homepage2->setForegroundRole(QPalette::Text);
}

void systemback::emailpressed()
{
    ui->email->setForegroundRole(QPalette::Highlight);
}

void systemback::emailreleased()
{
    if(ui->email->foregroundRole() == QPalette::Highlight)
    {
        ui->email->setForegroundRole(QPalette::Text);
        sb::exec("su -c \"xdg-email nemh@freemail.hu &\" " % guname(), nullptr, sb::Bckgrnd);
    }
}

void systemback::emailmove()
{
    if(minside(ui->aboutpanel->pos() + ui->email->pos(), ui->email->size()))
    {
        if(ui->email->foregroundRole() == QPalette::Text) ui->email->setForegroundRole(QPalette::Highlight);
    }
    else if(ui->email->foregroundRole() == QPalette::Highlight)
        ui->email->setForegroundRole(QPalette::Text);
}

void systemback::dntpressed()
{
    ui->donate->setForegroundRole(QPalette::Highlight);
}

void systemback::dntreleased()
{
    if(ui->donate->foregroundRole() == QPalette::Highlight)
    {
        ui->donate->setForegroundRole(QPalette::Text);
        sb::exec("su -c \"xdg-open 'https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZQ668BBR7UCEQ' &\" " % guname(), nullptr, sb::Bckgrnd);
    }
}

void systemback::dntmove()
{
    if(minside(ui->aboutpanel->pos() + ui->donate->pos(), ui->donate->size()))
    {
        if(ui->donate->foregroundRole() == QPalette::Text) ui->donate->setForegroundRole(QPalette::Highlight);
    }
    else if(ui->donate->foregroundRole() == QPalette::Highlight)
        ui->donate->setForegroundRole(QPalette::Text);
}

void systemback::foutp1()
{
    if(ui->point1->text().isEmpty()) ui->point1->setText(sb::pnames[0]);
}

void systemback::foutp2()
{
    if(ui->point2->text().isEmpty()) ui->point2->setText(sb::pnames[1]);
}

void systemback::foutp3()
{
    if(ui->point3->text().isEmpty()) ui->point3->setText(sb::pnames[2]);
}

void systemback::foutp4()
{
    if(ui->point4->text().isEmpty()) ui->point4->setText(sb::pnames[3]);
}

void systemback::foutp5()
{
    if(ui->point5->text().isEmpty()) ui->point5->setText(sb::pnames[4]);
}

void systemback::foutp6()
{
    if(ui->point6->text().isEmpty()) ui->point6->setText(sb::pnames[5]);
}

void systemback::foutp7()
{
    if(ui->point7->text().isEmpty()) ui->point7->setText(sb::pnames[6]);
}

void systemback::foutp8()
{
    if(ui->point8->text().isEmpty()) ui->point8->setText(sb::pnames[7]);
}

void systemback::foutp9()
{
    if(ui->point9->text().isEmpty()) ui->point9->setText(sb::pnames[8]);
}

void systemback::foutp10()
{
    if(ui->point10->text().isEmpty()) ui->point10->setText(sb::pnames[9]);
}

void systemback::foutp11()
{
    if(ui->point11->text().isEmpty()) ui->point11->setText(sb::pnames[10]);
}

void systemback::foutp12()
{
    if(ui->point12->text().isEmpty()) ui->point12->setText(sb::pnames[11]);
}

void systemback::foutp13()
{
    if(ui->point13->text().isEmpty()) ui->point13->setText(sb::pnames[12]);
}

void systemback::foutp14()
{
    if(ui->point14->text().isEmpty()) ui->point14->setText(sb::pnames[13]);
}

void systemback::foutp15()
{
    if(ui->point15->text().isEmpty()) ui->point15->setText(sb::pnames[14]);
}

void systemback::finpsttngs()
{
    if(ui->partitionsettingspanel2->isVisible())
        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() == QPalette().color(QPalette::Inactive, QPalette::Highlight) ; ++a)
        {
            ui->partitionsettings->item(a, 0)->setBackground(QPalette().highlight());
            ui->partitionsettings->item(a, 0)->setForeground(QPalette().highlightedText());
        }
}

void systemback::foutpsttngs()
{
    if(ui->partitionsettingspanel2->isVisibleTo(ui->copypanel))
        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() == QPalette().highlight() ; ++a)
        {
            ui->partitionsettings->item(a, 0)->setBackground(QPalette().color(QPalette::Inactive, QPalette::Highlight));
            ui->partitionsettings->item(a, 0)->setForeground(QPalette().color(QPalette::Inactive, QPalette::HighlightedText));
        }
}

void systemback::center()
{
    if(ui->usersettingscopy->checkState() == Qt::PartiallyChecked) ui->usersettingscopy->setText(tr("Transfer user configuration and data files"));
}

void systemback::cleave()
{
    if(ui->usersettingscopy->checkState() == Qt::PartiallyChecked && ui->usersettingscopy->text() == tr("Transfer user configuration and data files")) ui->usersettingscopy->setText(tr("Transfer user configuration files"));
}

void systemback::umntleave()
{
    if(! ui->unmountdelete->isEnabled() && ui->unmountdelete->text() == tr("! Delete !") && (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text().isEmpty() || ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "btrfs")) ui->unmountdelete->setEnabled(true);
}

void systemback::on_usersettingscopy_stateChanged(int arg1)
{
    if(ppipe == 0 && ui->copypanel->isVisible()) ui->usersettingscopy->setText(arg1 == 1 ? tr("Transfer user configuration files") : tr("Transfer user configuration and data files"));
}

void systemback::pntupgrade()
{
    sb::pupgrade();
    schar num(-1);

    for(QLE *ldt : ui->points->findChildren<QLE *>())
        if(! sb::pnames[++num].isEmpty())
        {
            if(! ldt->isEnabled())
            {
                ldt->setEnabled(true);

                switch(num) {
                case 2:
                    if(sb::pnumber == 3 && ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                case 3 ... 8:
                    if(sb::pnumber < num + 2 && ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                case 9:
                    if(ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                }

                ldt->setText(sb::pnames[num]);
            }
            else if(ldt->text() != sb::pnames[num])
                ldt->setText(sb::pnames[num]);
        }
        else if(ldt->isEnabled())
        {
            ldt->setDisabled(true);
            if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);

            switch(num) {
            case 3 ... 9:
                if(sb::pnumber < num + 1)
                {
                    if(ldt->text() != tr("not used")) ldt->setText(tr("not used"));
                }
                else if(ldt->text() != tr("empty"))
                    ldt->setText(tr("empty"));

                break;
            default:
                if(ldt->text() != tr("empty")) ldt->setText(tr("empty"));
            }
        }

    if(! sstart) on_pointpipe1_clicked();
    fontcheck(Rpnts);
}

void systemback::statustart()
{
    if(sstart && dialog != 304)
    {
        ui->schedulerpanel->hide();
        setwontop(false);
    }
    else
    {
        if(ui->mainpanel->isVisible())
            ui->mainpanel->hide();
        else if(ui->dialogpanel->isVisible())
        {
            ui->dialogpanel->hide();
            setwontop(false);
        }

        windowmove(ui->statuspanel->width(), ui->statuspanel->height());
    }

    ui->statuspanel->show();
    ui->progressbar->setMaximum(0);
    ui->focusfix->setFocus();
    repaint();
}

void systemback::restore()
{
    statustart();
    uchar mthd(ui->fullrestore->isChecked() ? 1 : ui->systemrestore->isChecked() ? 2 : ui->keepfiles->isChecked() ? ui->includeusers->currentIndex() == 0 ? 3 : 4 : ui->includeusers->currentIndex() == 0 ? 5 : 6);
    pset(mthd > 2 ? 4 : mthd + 1);
    bool fcmp(sb::isfile("/etc/fstab") && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") && sb::fload("/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab")), sfstab(fcmp ? ui->autorestoreoptions->isChecked() : ui->skipfstabrestore->isChecked());
    auto exit([this] { intrrpt = false; });
    if(intrrpt) return exit();

    if(sb::srestore(mthd, ui->includeusers->currentText(), sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, sfstab))
    {
        if(intrrpt) return exit();
        sb::Progress = -1;

        if(mthd < 3)
        {
            if(ui->grubreinstallrestore->isVisibleTo(ui->restorepanel) && (! ui->grubreinstallrestore->isEnabled() || ui->grubreinstallrestore->currentText() != tr("Disabled")))
            {
                sb::exec("update-grub");
                if(intrrpt) return exit();

                if(! fcmp || (! ui->autorestoreoptions->isChecked() && ui->grubreinstallrestore->currentText() != "Auto"))
                {
                    if(sb::exec("grub-install --force " % (grub.isEFI ? nullptr : ui->autorestoreoptions->isChecked() || ui->grubreinstallrestore->currentText() == "Auto" ? sb::gdetect() : ui->grubreinstallrestore->currentText())) > 0) dialog = 308;
                    if(intrrpt) return exit();
                }
            }

            sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        if(dialog != 308) dialog = [mthd, this] {
                switch(mthd) {
                case 1:
                    return 205;
                case 2:
                    return 204;
                default:
                    return ui->keepfiles->isChecked() ? 201 : 200;
                }
            }();
    }
    else
        dialog = 338;

    if(intrrpt) return exit();
    dialogopen();
}

void systemback::repair()
{
    statustart();
    uchar mthd(ui->systemrepair->isChecked() ? 2 : ui->fullrepair->isChecked() ? 1 : 0);
    pset(7 - mthd);
    auto exit([this] { intrrpt = false; });

    if(mthd == 0)
    {
        QSL mlst{"dev", "dev/pts", "proc", "sys"};
        for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/mnt/" % bpath);
        dialog = sb::exec("chroot /mnt sh -c \"update-grub ; grub-install --force " % (grub.isEFI ? nullptr : ui->grubreinstallrepair->currentText() == "Auto" ? sb::gdetect("/mnt") : ui->grubreinstallrepair->currentText()) % '\"') == 0 ? 208 : 317;
        for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
        if(intrrpt) return exit();
    }
    else
    {
        bool fcmp(sb::isfile("/mnt/etc/fstab") && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") && sb::fload("/mnt/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab")), sfstab(fcmp ? ui->autorepairoptions->isChecked() : ui->skipfstabrepair->isChecked()), rv;
        if(intrrpt) return exit();

        if(ppipe == 0)
        {
            if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
            {
                QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                QDir().mkdir("/.systembacklivepoint");
            }

            if(! sb::mount(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs", "/.systembacklivepoint", "loop")) return dialogopen(333);
            if(intrrpt) return exit();
            rv = sb::srestore(mthd, nullptr, "/.systembacklivepoint", "/mnt", sfstab);
            sb::umount("/.systembacklivepoint");
            QDir().rmdir("/.systembacklivepoint");
        }
        else
            rv = sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, "/mnt", sfstab);

        if(intrrpt) return exit();

        if(rv)
        {
            if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel) && (! ui->grubreinstallrepair->isEnabled() || ui->grubreinstallrepair->currentText() != tr("Disabled")))
            {
                QSL mlst{"dev", "dev/pts", "proc", "sys"};
                for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/mnt/" % bpath);
                sb::exec("chroot /mnt update-grub");
                if(intrrpt) return exit();
                if((! fcmp || (! ui->autorepairoptions->isChecked() && ui->grubreinstallrepair->currentText() != "Auto")) && sb::exec("chroot /mnt grub-install --force " % (grub.isEFI ? nullptr : ui->autorepairoptions->isChecked() || ui->grubreinstallrepair->currentText() == "Auto" ? sb::gdetect("/mnt") : ui->grubreinstallrepair->currentText())) > 0) dialog = ui->fullrepair->isChecked() ? 309 : 303;
                for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
                if(intrrpt) return exit();
            }

            emptycache();

            if(sb::like(dialog, {101, 102}))
            {
                if(ppipe == 1 && sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
                dialog = ui->fullrepair->isChecked() ? 202 : 203;
            }
        }
        else
            dialog = 339;
    }

    dialogopen();
}

void systemback::systemcopy()
{
    statustart();
    pset(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 8 : 9);

    auto error([this](ushort dlg = 0, cchar *dev = nullptr) {
            if(! intrrpt && (dlg == 0 || ! sb::like(dlg, {307, 313, 314, 316, 329, 330, 331, 332}))) dlg = [this] {
                    if(sb::dfree("/.sbsystemcopy") > 104857600 && (! sb::isdir("/.sbsystemcopy/home") || sb::dfree("/.sbsystemcopy/home") > 104857600) && (! sb::isdir("/.sbsystemcopy/boot") || sb::dfree("/.sbsystemcopy/boot") > 52428800) && (! sb::isdir("/.sbsystemcopy/boot/efi") || sb::dfree("/.sbsystemcopy/boot/efi") > 10485760))
                    {
                        irblck = true;
                        if(! sb::ThrdDbg.isEmpty()) prntdbgmsg();
                        return ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 319 : 320;
                    }
                    else
                        return ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 306 : 315;
                }();

            {
                QStr mnts(sb::fload("/proc/self/mounts", true));
                QTS in(&mnts, QIODevice::ReadOnly);
                QSL incl{"* /.sbsystemcopy*", "* /.sbmountpoints*", "* /.systembacklivepoint *"};

                while(! in.atEnd())
                {
                    QStr cline(in.readLine());
                    if(sb::like(cline, incl)) sb::umount(cline.split(' ').at(1));
                }
            }

            if(sb::isdir("/.sbmountpoints"))
            {
                for(cQStr &item : QDir("/.sbmountpoints").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) QDir().rmdir("/.sbmountpoints/" % item);
                QDir().rmdir("/.sbmountpoints");
            }

            QDir().rmdir("/.sbsystemcopy");
            if(sb::isdir("/.systembacklivepoint")) QDir().rmdir("/.systembacklivepoint");

            if(intrrpt)
                intrrpt = false;
            else
            {
                if(irblck) irblck = false;
                dialogopen(dlg, dev);
            }
        });

    if(! sb::isdir("/.sbsystemcopy") && ! QDir().mkdir("/.sbsystemcopy")) return error();

    {
        QSL msort;
        msort.reserve(ui->partitionsettings->rowCount());

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! nmpt.isEmpty() && (! nohmcpy[1] || nmpt != "/home"))
                msort.append(nmpt % '\n' % (ui->partitionsettings->item(a, 6)->text() == "x" ? ui->partitionsettings->item(a, 5)->text() : ui->partitionsettings->item(a, 5)->text() == "btrfs" ? "-b" : "-") % '\n' % ui->partitionsettings->item(a, 0)->text());
        }

        msort.sort();
        QSL ckd;
        ckd.reserve(msort.count());

        for(cQStr &vals : msort)
        {
            QSL cval(vals.split('\n'));
            cQStr &mpoint(cval.at(0)), &fstype(cval.at(1)), &part(cval.at(2));
            if(! ckd.contains(part) && sb::mcheck(part) && (! grub.isEFI || mpoint != "/boot/efi" || fstype.length() > 2)) sb::umount(part);
            if(intrrpt) return error();
            sb::fssync();
            if(intrrpt) return error();

            if(fstype.length() > 2)
            {
                QStr lbl("SB@" % (mpoint.startsWith('/') ? sb::right(mpoint, -1) : mpoint));

                uchar rv(fstype == "swap" ? sb::exec("mkswap -L " % lbl % ' ' % part)
                       : fstype == "jfs" ? sb::exec("mkfs.jfs -qL " % lbl % ' ' % part)
                       : fstype == "reiserfs" ? sb::exec("mkfs.reiserfs -ql " % lbl % ' ' % part)
                       : fstype == "xfs" ? sb::exec("mkfs.xfs -fL " % lbl % ' ' % part)
                       : fstype == "vfat" ? sb::setpflag(part, "boot") ? sb::exec("mkfs.vfat -F 32 -n " % lbl.toUpper() % ' ' % part) : 255
                       : fstype == "btrfs" ? (ckd.contains(part) ? 0 : sb::exec("mkfs.btrfs -fL " % lbl % ' ' % part)) == 0 ? 0 : sb::exec("mkfs.btrfs -L " % lbl % ' ' % part)
                       : sb::exec("mkfs." % fstype % " -FL " % lbl % ' ' % part));

                if(intrrpt) return error();
                if(rv > 0) return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 316 : 330, chr(part));
            }

            if(mpoint != "SWAP")
            {
                if(! sb::isdir("/.sbsystemcopy" % mpoint))
                {
                    QStr path("/.sbsystemcopy");

                    for(cQStr &cpath : mpoint.split('/'))
                        if(! sb::isdir(path.append('/' % cpath)) && ! QDir().mkdir(path))
                        {
                            QFile::rename(path, path % '_' % sb::rndstr());
                            if(! QDir().mkdir(path)) return error();
                        }
                }

                if(intrrpt) return error();

                if(fstype.length() == 2 || fstype == "btrfs")
                {
                    QStr mpt("/.sbmountpoints");

                    for(uchar a(0) ; a < 4 ; ++a)
                        switch(a) {
                        case 0 ... 1:
                            if(! sb::isdir(mpt))
                            {
                                if(sb::exist(mpt)) QFile::remove(mpt);
                                if(! QDir().mkdir(mpt)) return error();
                            }

                            if(a == 0)
                                mpt.append('/' % sb::right(part, - sb::rinstr(part, "/")));
                            else if(! ckd.contains(part))
                            {
                                if(sb::mount(part, mpt))
                                    ckd.append(part);
                                else
                                    return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 313 : 329, chr(part));
                            }

                            break;
                        case 2 ... 3:
                            sb::exec("btrfs subvolume create " % mpt % "/@" % sb::right(mpoint, -1));
                            if(intrrpt) return error();

                            if(sb::mount(part, "/.sbsystemcopy" % mpoint, "noatime,subvol=@" % sb::right(mpoint, -1)))
                                ++a;
                            else if(a == 3 || ! QFile::rename(mpt % "/@" % sb::right(mpoint, -1), mpt % "/@" % sb::right(mpoint, -1) % '_' % sb::rndstr()))
                                return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 313 : 329, chr(part));
                        }
                }
                else if(! sb::mount(part, "/.sbsystemcopy" % mpoint))
                    return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 313 : 329, chr(part));
            }

            if(intrrpt) return error();
        }
    }

    if(ppipe == 0)
    {
        if(pname == tr("Currently running system"))
        {
            if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
            {
                if(! sb::scopy([this] {
                        switch(ui->usersettingscopy->checkState()) {
                        case Qt::Unchecked:
                            return 5;
                        case Qt::PartiallyChecked:
                            return 3;
                        default:
                            return 4;
                        }
                    }(), guname(), nullptr)) return error();
            }
            else if(! sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, nullptr) || (sb::schdle[0] == sb::True && ! sb::cfgwrite("/.sbsystemcopy/etc/systemback.conf")))
                return error();
        }
        else
        {
            if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
            {
                QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                if(! QDir().mkdir("/.systembacklivepoint") || intrrpt) return error();
            }

            QStr mdev(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs");
            if(! sb::mount(mdev, "/.systembacklivepoint", "loop")) return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 331 : 332);
            if(intrrpt) return error();

            if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
            {
                if(! sb::scopy([this] {
                        switch(ui->usersettingscopy->checkState()) {
                        case Qt::Unchecked:
                            return 5;
                        case Qt::PartiallyChecked:
                            return 3;
                        default:
                            return 4;
                        }
                    }(), guname(), "/.systembacklivepoint")) return error();
            }
            else
            {
                if(! sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, "/.systembacklivepoint")) return error();
                QBA cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));
                if(cfg.contains("enabled=true") && ! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", cfg.replace("enabled=true", "enabled=false"))) return error();
            }

            sb::umount("/.systembacklivepoint");
            QDir().rmdir("/.systembacklivepoint");
        }
    }
    else if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
    {
        if(! sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname)) return error();
        QBA cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));
        if(cfg.contains("enabled=true") && ! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", cfg.replace("enabled=true", "enabled=false"))) return error();
    }
    else if(! sb::scopy(ui->usersettingscopy->isChecked() ? 4 : 5, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname))
        return error();

    if(intrrpt) return error();
    sb::Progress = -1;

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        if(sb::exec("chroot /.sbsystemcopy sh -c \"for rmuser in $(grep :\\$6\\$* /etc/shadow | cut -d : -f 1) ; do [ $rmuser = " % guname() % " ] || [ $rmuser = root ] || userdel $rmuser ; done\"") > 0) return error();
        QStr nfile;

        if(guname() != "root")
        {
            QStr nuname(ui->username->text());

            if(guname() != nuname)
            {
                if(sb::isdir("/.sbsystemcopy/home/" % guname()) && ((sb::exist("/.sbsystemcopy/home/" % nuname) && ! QFile::rename("/.sbsystemcopy/home/" % nuname, "/.sbsystemcopy/home/" % nuname % '_' % sb::rndstr())) || ! QFile::rename("/.sbsystemcopy/home/" % guname(), "/.sbsystemcopy/home/" % nuname))) return error();

                if(sb::isfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"))
                {
                    QStr cnt(sb::fload("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"));
                    if(cnt.contains("file:///home/" % guname() % '/') && ! sb::crtfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks", cnt.replace("file:///home/" % guname() % '/', "file:///home/" % nuname % '/'))) return error();
                }

                if(sb::isfile("/.sbsystemcopy/etc/subuid") && sb::isfile("/.sbsystemcopy/etc/subgid"))
                {
                    {
                        QFile file("/.sbsystemcopy/etc/subuid");
                        if(! file.open(QIODevice::ReadOnly)) return error();

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) return error();
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subuid", nfile)) return error();
                    nfile.clear();

                    {
                        QFile file("/.sbsystemcopy/etc/subgid");
                        if(! file.open(QIODevice::ReadOnly)) return error();

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) return error();
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subgid", nfile)) return error();
                    nfile.clear();
                }
            }

            for(cQStr &cfname : {"/.sbsystemcopy/etc/group", "/.sbsystemcopy/etc/gshadow"})
            {
                QFile file(cfname);
                if(! file.open(QIODevice::ReadOnly)) return error();

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(nline.startsWith("sudo:") && ui->rootpassword1->text().isEmpty() && ! sb::right(nline, - sb::rinstr(nline, ":")).split(',').contains(guname()))
                        nline.append((nline.endsWith(':') ? nullptr : ",") % nuname);
                    else if(guname() != nuname)
                    {
                        if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                        nline.replace(':' % guname() % ',', ':' % nuname % ',');
                        nline.replace(',' % guname() % ',', ',' % nuname % ',');
                        if(sb::like(nline, {"*:" % guname() % '_', "*," % guname() % '_'})) nline.replace(nline.length() - guname().length(), guname().length(), nuname);
                    }

                    nfile.append(nline % '\n');
                    if(intrrpt) return error();
                }

                if(! sb::crtfile(cfname , nfile)) return error();
                nfile.clear();
                qApp->processEvents();
            }

            QFile file("/.sbsystemcopy/etc/passwd");
            if(! file.open(QIODevice::ReadOnly)) return error();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                nfile.append((! cline.startsWith(guname() % ':') ? cline : [&] {
                            QSL uslst(cline.split(':'));
                            QStr nline;

                            for(uchar a(0) ; a < uslst.count() ; ++a)
                                nline.append([&, a]() -> QStr {
                                        switch(a) {
                                        case 0:
                                            return nuname;
                                        case 4:
                                            return ui->fullname->text() % ",,,";
                                        case 5:
                                            return "/home/" % nuname;
                                        default:
                                            return uslst.at(a);
                                        }
                                    }() % ':');

                            return sb::left(nline, -1);
                    }()) % '\n');

                if(intrrpt) return error();
            }

            if(! sb::crtfile("/.sbsystemcopy/etc/passwd", nfile)) return error();
            nfile.clear();
        }

        {
            QFile file("/.sbsystemcopy/etc/shadow");
            if(! file.open(QIODevice::ReadOnly)) return error();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                nfile.append((cline.startsWith(guname() % ':') ? [&] {
                        QSL uslst(cline.split(':'));
                        QStr nline;

                        for(uchar a(0) ; a < uslst.count() ; ++a)
                            nline.append([&, a]() -> QStr {
                                    switch(a) {
                                    case 0:
                                        return guname() == "root" ? "root" : ui->username->text();
                                    case 1:
                                        return QStr(crypt(chr(ui->password1->text()), chr(("$6$" % sb::rndstr(16)))));
                                    default:
                                        return uslst.at(a);
                                    }
                                }() % ':');

                        return sb::left(nline, -1);
                    }() : cline.startsWith("root:") ? [&] {
                        QSL uslst(cline.split(':'));
                        QStr nline;

                        for(uchar a(0) ; a < uslst.count() ; ++a)
                            nline.append([&, a]() -> QStr {
                                    switch(a) {
                                    case 1:
                                        return ui->rootpassword1->text().isEmpty() ? "!" : QStr(crypt(chr(ui->rootpassword1->text()), chr(("$6$" % sb::rndstr(16)))));
                                    default:
                                        return uslst.at(a);
                                    }
                                }() % ':');

                        return sb::left(nline, -1);
                    }() : cline) % '\n');

                if(intrrpt) return error();
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/shadow", nfile)) return error();
        nfile.clear();

        {
            QFile file("/.sbsystemcopy/etc/hostname");
            if(! file.open(QIODevice::ReadOnly)) return error();
            QStr ohname(file.readLine().trimmed()), nhname(ui->hostname->text());
            file.close();

            if(ohname != nhname)
            {
                if(! sb::crtfile("/.sbsystemcopy/etc/hostname", nhname % '\n')) return error();
                file.setFileName("/.sbsystemcopy/etc/hosts");
                if(! file.open(QIODevice::ReadOnly)) return error();

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    nline.replace('\t' % ohname % '\t', '\t' % nhname % '\t');
                    if(nline.endsWith('\t' % ohname)) nline.replace(nline.length() - ohname.length(), ohname.length(), nhname);
                    nfile.append(nline % '\n');
                    if(intrrpt) return error();
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/hosts", nfile)) return error();
                nfile.clear();
            }
        }

        for(uchar a(0) ; a < 5 ; ++a)
        {
            QStr fpath("/.sbsystemcopy/etc/" % [a]() -> QStr {
                    switch(a) {
                    case 0:
                        return "lightdm/lightdm.conf";
                    case 1:
                        return "kde4/kdm/kdmrc";
                    case 2:
                        return "sddm.conf";
                    case 3:
                        return "gdm3/daemon.conf";
                    default:
                        return "mdm/mdm.conf";
                    }
                }());

            if(sb::isfile(fpath))
            {
                QFile file(fpath);
                if(! file.open(QIODevice::ReadOnly)) return error();
                uchar mdfd(0);

                QSL incl([a]() -> QSL {
                        switch(a) {
                        case 0:
                            return {"_autologin-user=*"};
                        case 1:
                            return {"_AutoLoginUser=*"};
                        case 2:
                            return {"_User=*", "_HideUsers=*"};
                        default:
                            return {"_AutomaticLogin=*", "_TimedLogin=*"};
                        }
                    }());

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(sb::like(nline, incl))
                        if(! nline.endsWith('='))
                        {
                            bool algn(nline.endsWith('=' % guname()) && (a != 2 || ! nline.startsWith("HideUsers=")));
                            nline = sb::left(nline, sb::instr(nline, "="));

                            if(algn)
                            {
                                nline.append(ui->username->text());
                                if(mdfd == 0) ++mdfd;
                            }
                            else
                                switch(a) {
                                case 1:
                                    if(mdfd < 2) mdfd = 2;
                                    break;
                                case 3 ... 4:
                                    if(mdfd < 4) mdfd = nline.at(0) == 'A' ? mdfd == 3 ? 4 : 2 : mdfd == 2 ? 4 : 3;
                                    break;
                                default:
                                    if(mdfd == 0) ++mdfd;
                                }
                        }

                    nfile.append(nline % '\n');
                    if(intrrpt) return error();
                }

                if(mdfd > 0)
                {
                    switch(a) {
                    case 1:
                        if(mdfd == 2) nfile.replace("AutoLoginEnabled=true", "AutoLoginEnabled=false");
                        break;
                    case 3 ... 4:
                        if(mdfd > 1)
                        {
                            if(sb::like(mdfd, {2, 4})) nfile.replace("AutomaticLoginEnable=true", "AutomaticLoginEnable=false");
                            if(sb::like(mdfd, {3, 4})) nfile.replace("TimedLoginEnable=true", "TimedLoginEnable=false");
                        }
                    }

                    if(! sb::crtfile(fpath, nfile)) return error();
                }

                nfile.clear();
                qApp->processEvents();
            }
        }

        QBA mid(sb::rndstr(16).toUtf8().toHex() % '\n');
        if(intrrpt || ! sb::crtfile("/.sbsystemcopy/etc/machine-id", mid) || (sb::isdir("/.sbsystemcopy/var/lib/dbus") && ! QFile::setPermissions("/.sbsystemcopy/etc/machine-id", QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther) && ! sb::crtfile("/.sbsystemcopy/var/lib/dbus/machine-id", mid))) return error();
    }

    {
        QStr fstabtxt("# /etc/fstab: static file system information.\n#\n# Use 'blkid' to print the universally unique identifier for a\n# device; this may be used with UUID= as a more robust way to name devices\n# that works even if disks are added and removed. See fstab(5).\n#\n# <file system> <mount point>   <type>  <options>       <dump>  <pass>\n");

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! nmpt.isEmpty())
            {
                if(nohmcpy[1] && nmpt == "/home")
                {
                    QFile file("/etc/fstab");
                    if(! file.open(QIODevice::ReadOnly)) return error();
                    QSL incl{"* /home *", "*\t/home *", "* /home\t*", "*\t/home\t*", "* /home/ *", "*\t/home/ *", "* /home/\t*", "*\t/home/\t*"};

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());

                        if(! cline.startsWith('#') && sb::like(cline, incl))
                        {
                            fstabtxt.append("# /home\n" % cline % '\n');
                            break;
                        }
                    }
                }
                else
                {
                    QStr uuid(sb::ruuid(ui->partitionsettings->item(a, 0)->text())), nfs(ui->partitionsettings->item(a, 5)->text());

                    fstabtxt.append("# " % (nmpt == "SWAP" ? QStr("SWAP\nUUID=" % uuid % "   none   swap   sw   0   0")
                                : nmpt % "\nUUID=" % uuid % "   " % nmpt % "   " % nfs % "   noatime"
                                    % (nmpt == "/" ? QStr(sb::like(nfs, {"_ext4_", "_ext3_", "_ext2_", "_jfs_", "_xfs_"}) ? ",errors=remount-ro" : nfs == "reiserfs" ? ",notail" : ",subvol=@") % "   0   1"
                                        : (nfs == "reiserfs" ? ",notail" : nfs == "btrfs" ? QStr(",subvol=@" % sb::right(nmpt, -1)) : nullptr) % "   0   2")) % '\n');
                }
            }

            if(intrrpt) return error();
        }

        if(sb::isfile("/etc/fstab"))
        {
            QFile file("/etc/fstab");
            if(! file.open(QIODevice::ReadOnly)) return error();
            QSL incl{"*/dev/cdrom*", "*/dev/sr*"};

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());
                if(! cline.startsWith('#') && sb::like(cline, incl)) fstabtxt.append("# cdrom\n" % cline % '\n');
                if(intrrpt) return error();
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/fstab", fstabtxt)) return error();
    }

    {
        QStr cfpath(ppipe == 0 ? "/etc/crypttab" : QStr(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/crypttab"));

        if(sb::isfile(cfpath))
        {
            QFile file(cfpath);
            if(! file.open(QIODevice::ReadOnly)) return error();

            while(! file.atEnd())
            {
                QBA cline(file.readLine().trimmed());

                if(! cline.startsWith('#') && cline.contains("UUID="))
                {
                    if(intrrpt || ! sb::crtfile("/.sbsystemcopy/etc/mtab") || sb::exec("chroot /.sbsystemcopy update-initramfs -tck all") > 0) return error();
                    break;
                }
            }
        }
    }

    if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(intrrpt) return error();
        { QSL mlst{"dev", "dev/pts", "proc", "sys"};
        for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/.sbsystemcopy/" % bpath); }

        if(ui->grubinstallcopy->currentText() == tr("Disabled"))
            sb::exec("chroot /.sbsystemcopy update-grub");
        else if(sb::exec("chroot /.sbsystemcopy sh -c \"update-grub ; grub-install --force " % (grub.isEFI ? nullptr : ui->grubinstallcopy->currentText() == "Auto" ? sb::gdetect("/.sbsystemcopy") : ui->grubinstallcopy->currentText()) % '\"') > 0)
            return error(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 307 : 314);
    }

    if(intrrpt) return error();
    pset(1);

    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);
        QSL incl{"* /.sbsystemcopy*", "* /.sbmountpoints*"};

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, incl)) sb::umount(cline.split(' ').at(1));
        }
    }

    if(sb::isdir("/.sbmountpoints"))
    {
        for(cQStr &item : QDir("/.sbmountpoints").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) QDir().rmdir("/.sbmountpoints/" % item);
        QDir().rmdir("/.sbmountpoints");
    }

    QDir().rmdir("/.sbsystemcopy");
    sb::fssync();
    if(sb::ecache == sb::True) sb::crtfile("/proc/sys/vm/drop_caches", "3");
    dialogopen(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 206 : 209);
}

void systemback::livewrite()
{
    statustart();
    pset(10);
    QStr ldev(ui->livedevices->item(ui->livedevices->currentRow(), 0)->text());
    bool ismmc(ldev.contains("mmc"));

    auto error([&, ismmc](ushort dlg = 335) {
            if(sb::isdir("/.sblivesystemwrite"))
            {
                if(sb::mcheck("/.sblivesystemwrite/sblive")) sb::umount("/.sblivesystemwrite/sblive");
                QDir().rmdir("/.sblivesystemwrite/sblive");

                if(sb::isdir("/.sblivesystemwrite/sbroot"))
                {
                    if(sb::mcheck("/.sblivesystemwrite/sbroot")) sb::umount("/.sblivesystemwrite/sbroot");
                    QDir().rmdir("/.sblivesystemwrite/sbroot");
                }

                QDir().rmdir("/.sblivesystemwrite");
            }

            if(intrrpt)
                intrrpt = false;
            else
                dialogopen(dlg, sb::like(dlg, {336, 337}) ? chr((ldev % (ismmc ? "p" : nullptr) % '1')) : nullptr);
        });

    if(! sb::exist(ldev))
        return error(337);
    else if(sb::mcheck(ldev))
    {
        for(cQStr &sitem : QDir("/dev").entryList(QDir::System))
        {
            QStr item("/dev/" % sitem);

            if(item.length() > (ismmc ? 12 : 8) && item.startsWith(ldev))
                while(sb::mcheck(item)) sb::umount(item);

            if(intrrpt) return error();
        }

        if(sb::mcheck(ldev)) sb::umount(ldev);
        sb::fssync();
        if(intrrpt) return error();
    }

    if(! sb::mkptable(ldev) || intrrpt) return error(337);
    ullong size(sb::fsize(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive"));
    QStr lrdir;

    if(size < 4294967295)
    {
        if(! sb::mkpart(ldev) || intrrpt) return error(337);
        lrdir = "sblive";
    }
    else if(! sb::mkpart(ldev, 1048576, 104857600) || ! sb::mkpart(ldev)
            || intrrpt || sb::exec("mkfs.ext2 -FL SBROOT " % ldev % (ismmc ? "p" : nullptr) % '2') > 0
            || intrrpt)
        return error(337);
    else
        lrdir = "sbroot";

    if(sb::exec("mkfs.vfat -F 32 -n SBLIVE " % ldev % (ismmc ? "p" : nullptr) % '1') > 0 || intrrpt) return error(337);

    if(sb::exec("dd if=/usr/lib/syslinux/" % QStr(sb::isfile("/usr/lib/syslinux/mbr.bin") ? nullptr : "mbr/") % "mbr.bin of=" % ldev % " conv=notrunc bs=440 count=1") > 0 || ! sb::setpflag(ldev % (ismmc ? "p" : nullptr) % '1', "boot") || ! sb::setpflag(ldev % (ismmc ? "p" : nullptr) % '1', "lba")
        || intrrpt || (sb::exist("/.sblivesystemwrite") && (((sb::mcheck("/.sblivesystemwrite/sblive") && ! sb::umount("/.sblivesystemwrite/sblive")) || (sb::mcheck("/.sblivesystemwrite/sbroot") && ! sb::umount("/.sblivesystemwrite/sbroot"))) || ! sb::remove("/.sblivesystemwrite")))
        || intrrpt || ! QDir().mkdir("/.sblivesystemwrite") || ! QDir().mkdir("/.sblivesystemwrite/sblive")
        || intrrpt) return error();

    if(! sb::mount(ldev % (ismmc ? "p" : nullptr) % '1', "/.sblivesystemwrite/sblive") || intrrpt) return error(336);

    if(lrdir == "sbroot")
    {
        if(! QDir().mkdir("/.sblivesystemwrite/sbroot")) return error();
        if(! sb::mount(ldev % (ismmc ? "p" : nullptr) % '2', "/.sblivesystemwrite/sbroot") || intrrpt) return error(336);
    }

    if(sb::dfree("/.sblivesystemwrite/" % lrdir) < size + 52428800) return error(321);
    sb::ThrdStr[0] = "/.sblivesystemwrite", sb::ThrdLng[0] = size;

    if(lrdir == "sblive")
    {
        if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --no-same-owner --no-same-permissions", nullptr, sb::Prgrss) > 0) return error(322);
    }
    else if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --exclude=casper/filesystem.squashfs --exclude=live/filesystem.squashfs --no-same-owner --no-same-permissions") > 0 || sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sbroot --exclude=.disk --exclude=boot --exclude=EFI --exclude=syslinux --exclude=casper/initrd.gz --exclude=casper/vmlinuz --exclude=live/initrd.gz --exclude=live/vmlinuz --no-same-owner --no-same-permissions", nullptr, sb::Prgrss) > 0)
        return error(322);

    pset(1);
    if(sb::exec("syslinux -ifd syslinux " % ldev % (ismmc ? "p" : nullptr) % '1') > 0) return error();
    sb::fssync();
    if(sb::ecache == sb::True) sb::crtfile("/proc/sys/vm/drop_caches", "3");
    sb::umount("/.sblivesystemwrite/sblive");
    QDir().rmdir("/.sblivesystemwrite/sblive");

    if(lrdir == "sbroot")
    {
        sb::umount("/.sblivesystemwrite/sbroot");
        QDir().rmdir("/.sblivesystemwrite/sbroot");
    }

    QDir().rmdir("/.sblivesystemwrite");
    dialogopen(210);
}

void systemback::dialogopen(ushort dlg, cchar *dev, schar snum)
{
    if(ui->dialognumber->isVisibleTo(ui->dialogpanel)) ui->dialognumber->hide();

    if(dlg == 0)
        dlg = dialog;
    else if(dialog != dlg)
        dialog = dlg;

    if(dlg < 200)
    {
        for(QWdt *wdgt : QWL{ui->dialoginfo, ui->dialogerror})
            if(wdgt->isVisibleTo(ui->dialogpanel)) wdgt->hide();

        for(QWdt *wdgt : QWL{ui->dialogquestion, ui->dialogcancel})
            if(! wdgt->isVisibleTo(ui->dialogpanel)) wdgt->show();

        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));

        ui->dialogtext->setText([this, dlg]() -> QStr {
                switch(dlg) {
                case 100:
                    return tr("Restore the system files to the following restore point:") % "<p><b>" % pname;
                case 101:
                    return tr("Repair the system files with the following restore point:") % "<p><b>" % pname;
                case 102:
                    return tr("Repair the complete system with the following restore point:") % "<p><b>" % pname;
                case 103:
                    return tr("Restore the complete user(s) configuration files to the following restore point:") % "<p><b>" % pname;
                case 104:
                    return tr("Restore the user(s) configuration files to the following restore point:") % "<p><b>" % pname;
                case 105:
                    return tr("Copy the system, using the following restore point:") % "<p><b>" % pname;
                case 106:
                    return tr("Install the system, using the following restore point:") % "<p><b>" % pname;
                case 107:
                    return tr("Restore complete system to the following restore point:") % "<p><b>" % pname;
                case 108:
                    return tr("Format the %1, and write the following Live system image:").arg(" <b>" % ui->livedevices->item(ui->livedevices->currentRow(), 0)->text() % "</b>") % "<p><b>" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % "</b>";
                default:
                    return tr("Repair the GRUB 2 bootloader.");
                }
            }());
    }
    else
    {
        for(QWdt *wdgt : QWL{ui->dialogquestion, ui->dialogcancel})
            if(wdgt->isVisibleTo(ui->dialogpanel)) wdgt->hide();

        if(dlg < 300)
        {
            if(ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->hide();
            if(! ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->show();
            bool cntd(false);

            ui->dialogtext->setText([&, dlg]() -> QStr {
                    switch(dlg) {
                    case 200:
                        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                        for(QWdt *wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("User(s) configuration files full restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds.");
                    case 201:
                        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                        for(QWdt *wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("User(s) configuration files restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds.");
                    case 202:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("Full system repair is completed.");
                    case 203:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("System repair is completed.");
                    case 204:
                        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                        for(QWdt *wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("System files restoration are completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds.");
                    case 205:
                        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                        for(QWdt *wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("Full system restoration is completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds.");
                    case 206:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("System copy is completed.");
                    case 207:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("Live system creation is completed.") % "<p>" % tr("The created .sblive file can be written to pendrive.");
                    case 208:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("GRUB 2 repair is completed.");
                    case 209:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("System install is completed.");
                    default:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("Live system image write is completed.");
                    }
                }());

            if(cntd)
            {
                connect((dlgtimer = new QTimer), SIGNAL(timeout()), this, SLOT(dialogtimer()));
                dlgtimer->start(1000);
            }
        }
        else
        {
            if(ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->hide();
            if(! ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->show();
            if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");

            ui->dialogtext->setText([&, dlg]() -> QStr {
                    switch(dlg) {
                    case 300:
                        return tr("Another systemback process is currently running, please wait until it finishes.");
                    case 301:
                        return tr("Unable to get exclusive lock!") % "<p>" % tr("First, close all package manager.");
                    case 302:
                        return tr("The specified name contain(s) unsupported character(s)!") % "<p>" % tr("Please enter a new name.");
                    case 303:
                        return tr("System files repair are completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)");
                    case 304:
                        return tr("Restore point creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process.");
                    case 305:
                        return tr("Root privileges are required for running Systemback!");
                    case 306:
                        return tr("System copy is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to copy the system. The copied system will not function properly.");
                    case 307:
                        return tr("System copy is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader.");
                    case 308:
                        return tr("System restoration is aborted!") % "<p>" % tr("An error occurred while reinstalling GRUB.");
                    case 309:
                        return tr("Full system repair is completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)");
                    case 310:
                        return tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating file system image.");
                    case 311:
                        return tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating container file.");
                    case 312:
                        return tr("Live system creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process.");
                    case 313:
                        return tr("System copy is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev;
                    case 314:
                        return tr("System install is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader.");
                    case 315:
                        return tr("System installation is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to install the system. The installed system will not function properly.");
                    case 316:
                        return tr("System copy is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev;
                    case 317:
                        return tr("An error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)");
                    case 318:
                        return tr("Restore point creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 319:
                        return tr("System copying is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 320:
                        return tr("System installation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 321:
                        return tr("Live write is aborted!") % "<p>" % tr("The selected device does not have enough space to write the Live system.");
                    case 322:
                        return tr("Live write is aborted!") % "<p>" % tr("An error occurred while unpacking Live system files.");
                    case 323:
                        return tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while renaming essential Live files.");
                    case 324:
                        return tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while creating .iso image.");
                    case 325:
                        return tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while reading .sblive image.");
                    case 326:
                        return tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating new initramfs image.");
                    case 327:
                        return tr("Live system creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 328:
                        return tr("Restore point deletion is aborted!") % "<p>" % tr("An error occurred while during the process.");
                    case 329:
                        return tr("System installation is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev;
                    case 330:
                        return tr("System installation is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev;
                    case 331:
                        return tr("System copy is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 332:
                        return tr("System installation is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 333:
                        return tr("System repair is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 334:
                        return tr("Live conversion is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 335:
                        return tr("Live write is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 336:
                        return tr("Live write is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev;
                    case 337:
                        return tr("Live write is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev;
                    case 338:
                        return tr("System restoration is aborted!") % "<p>" % tr("There is not enough free space.");
                    default:
                        return tr("System repair is aborted!") % "<p>" % tr("There is not enough free space.");
                    }
                }());
        }
    }

    for(QWdt *wdgt : QWL{ui->mainpanel, ui->statuspanel, ui->schedulerpanel})
        if(wdgt->isVisible()) wdgt->hide();

    if(ui->dialogpanel->isHidden()) ui->dialogpanel->show();
    ui->dialogok->setFocus();

    if(width() != ui->dialogpanel->width())
    {
        if(utimer.isActive() && ! sstart)
            windowmove(ui->dialogpanel->width(), ui->dialogpanel->height());
        else
        {
            if(sstart && ! sb::like(dialog, {300, 301, 305}) && ! ui->function3->text().contains(' ')) ui->function3->setText("Systemback " % tr("scheduler"));
            setFixedSize((wgeom[2] = ui->dialogpanel->width()), (wgeom[3] = ui->dialogpanel->height()));
            move((wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() / 2 - ss(253)), (wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() / 2 - ss(100)));
        }
    }

    repaint();
    setwontop();
}

void systemback::setwontop(bool state)
{
    if(sb::waot == sb::False)
    {
        Display *dsply(XOpenDisplay(nullptr));
        XEvent ev;
        ev.xclient.type = ClientMessage,
        ev.xclient.message_type = XInternAtom(dsply, "_NET_WM_STATE", 0),
        ev.xclient.display = dsply,
        ev.xclient.window = winId(),
        ev.xclient.format = 32,
        ev.xclient.data.l[0] = state ? 1 : 0,
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_ABOVE", 0),
        XSendEvent(dsply, XDefaultRootWindow(dsply), 0, SubstructureRedirectMask | SubstructureNotifyMask, &ev),
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_STAYS_ON_TOP", 0),
        XSendEvent(dsply, XDefaultRootWindow(dsply), 0, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XCloseDisplay(dsply);
    }
}

void systemback::windowmove(ushort nwidth, ushort nheight, bool fxdw)
{
    if(wismax)
    {
        wismax = false;
        setGeometry(wgeom[0], wgeom[1], wgeom[2], wgeom[3]);
    }

    if(size() != QSize((wgeom[2] = nwidth), (wgeom[3] = nheight)))
    {
        ui->resizepanel->show();
        repaint();
        wgeom[0] = x() + (width() - wgeom[2]) / 2;
        schar snum(qApp->desktop()->screenNumber(this));
        short scrxy(qApp->desktop()->screenGeometry(snum).x());

        if(wgeom[0] < scrxy + ss(30))
            wgeom[0] = scrxy + ss(30);
        else
        {
            short rghtlmt(scrxy + qApp->desktop()->screenGeometry(snum).width() - wgeom[2] - ss(30));
            if(wgeom[0] > rghtlmt) wgeom[0]= rghtlmt;
        }

        wgeom[1] = y() + (height() - wgeom[3]) / 2;

        if(wgeom[1] < (scrxy = qApp->desktop()->screenGeometry(snum).y()) + ss(30))
            wgeom[1] = scrxy + ss(30);
        else
        {
            short bttmlmt(scrxy + qApp->desktop()->screenGeometry(snum).height() - wgeom[3] - ss(30));
            if(wgeom[1] > bttmlmt) wgeom[1] = bttmlmt;
        }

        setMinimumSize(0, 0);
        setMaximumSize(qApp->desktop()->availableGeometry(snum).size());
        setGeometry(wgeom[0], wgeom[1], wgeom[2], wgeom[3]);
        if(fxdw) setFixedSize(wgeom[2], wgeom[3]);
        ui->resizepanel->hide();
        qApp->processEvents();
    }
    else if(fxdw && minimumSize() != maximumSize())
        setFixedSize(wgeom[2], wgeom[3]);
}

void systemback::wmove()
{
    if(! wismax) move(QCursor::pos().x() - lblevent::MouseX, QCursor::pos().y() - lblevent::MouseY);
}

void systemback::rmove()
{
    if(! wismax && size() != QSize((wgeom[2] = QCursor::pos().x() - x() + ss(31) - lblevent::MouseX), (wgeom[3] = QCursor::pos().y() - y() + ss(31) - lblevent::MouseY))) resize(wgeom[2], wgeom[3]);
}

void systemback::on_functionmenunext_clicked()
{
    ui->functionmenu1->hide();
    ui->functionmenu2->show();
    ui->functionmenunext->setDisabled(true);
    ui->functionmenuback->setEnabled(true);
    ui->functionmenuback->setFocus();
}

void systemback::on_functionmenuback_clicked()
{
    ui->functionmenu2->hide();
    ui->functionmenu1->show();
    ui->functionmenuback->setDisabled(true);
    ui->functionmenunext->setEnabled(true);
    ui->functionmenunext->setFocus();
}

bool systemback::eventFilter(QObject *, QEvent *ev)
{
    switch(ev->type()) {
    case QEvent::WindowActivate:
        if(ui->function3->foregroundRole() == QPalette::Dark)
        {
            for(QWdt *wdgt : QWL{ui->scalingbutton, ui->function1, ui->windowbutton1, ui->function2, ui->windowbutton2, ui->function3, ui->windowbutton3, ui->function4, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Base);
            goto gcheck;
        }

        return false;
    case QEvent::WindowDeactivate:
        if(ui->function3->foregroundRole() == QPalette::Base)
        {
            for(QWdt *wdgt : QWL{ui->scalingbutton, ui->function1, ui->windowbutton1, ui->function2, ui->windowbutton2, ui->function3, ui->windowbutton3, ui->function4, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Dark);

            if(ui->copypanel->isVisible())
            {
                if(ui->partitionsettings->hasFocus() && ui->partitionsettings->currentRow() == -1) ui->copyback->setFocus();
            }
            else if(ui->livecreatepanel->isVisible())
            {
                if((ui->livelist->hasFocus() && ui->livelist->currentRow() == -1) || (ui->livedevices->hasFocus() && ui->livedevices->currentRow() == -1)) ui->livecreateback->setFocus();
            }
            else if(ui->excludepanel->isVisible())
            {
                if((ui->itemslist->hasFocus() && ! ui->itemslist->currentItem()) || (ui->excludedlist->hasFocus() && ui->excludedlist->currentRow() == -1)) ui->excludeback->setFocus();
            }
            else if(ui->choosepanel->isVisible() && ui->dirchoose->hasFocus() && ! ui->dirchoose->currentItem())
                ui->dirchoosecancel->setFocus();

            goto gcheck;
        }

        return false;
    case QEvent::Resize:
        if(wismax)
        {
            schar snum(qApp->desktop()->screenNumber(this));

            if(geometry() != qApp->desktop()->availableGeometry(snum))
            {
                setGeometry(qApp->desktop()->availableGeometry(snum));
                return true;
            }
        }

        ui->mainpanel->resize(size());
        ui->function1->resize(width(), ui->function1->height());
        ui->border->resize(width(), ui->border->height());
        ui->windowbutton1->move(width() - ui->windowbutton1->height(), 0);
        ui->resizepanel->resize(width() - ss(1), height() - ss(24));

        if(ui->choosepanel->isVisible())
        {
            ui->choosepanel->resize(width() - (sfctr == High ? 4 : ss(2)), height() - ss(25));
            ui->dirpath->resize(ui->choosepanel->width() - ss(40), ui->dirpath->height());
            ui->dirrefresh->move(ui->choosepanel->width() - ui->dirrefresh->width(), 0);
            ui->dirchoose->resize(ui->choosepanel->width(), ui->choosepanel->height() - ss(80));
            ui->dirchooseok->move(ui->choosepanel->width() - ss(120), ui->choosepanel->height() - ss(40));
            ui->dirchoosecancel->move(ui->choosepanel->width() - ss(240), ui->choosepanel->height() - ss(40));
            ui->filesystemwarning->move(ui->filesystemwarning->x(), ui->choosepanel->height() - ss(41));
            ui->chooseresize->move(ui->choosepanel->width() - ui->chooseresize->width(), ui->choosepanel->height() - ui->chooseresize->height());
        }
        else if(ui->copypanel->isVisible())
        {
            ui->copypanel->resize(width() - (sfctr == High ? 4 : ss(2)), height() - ss(25));
            ui->partitionsettingstext->resize(ui->copypanel->width(), ui->partitionsettingstext->height());
            ui->partitionsettings->resize(ui->copypanel->width() - ss(152), ui->copypanel->height() - ss(200));
            ui->partitionsettingspanel1->move(ui->partitionsettings->x() + ui->partitionsettings->width(), ui->partitionsettingspanel1->y());
            for(QWdt *wdgt : QWL{ui->partitionsettingspanel2, ui->partitionsettingspanel3}) wdgt->move(ui->partitionsettingspanel1->pos());
            ui->partitionoptionstext->setGeometry(0, ui->copypanel->height() - ss(160), ui->copypanel->width(), ui->partitionoptionstext->height());
            ui->userdatafilescopy->move(ui->userdatafilescopy->x(), ui->copypanel->height() - ss(122) - (sfctr == High ? 1 : 2));
            ui->usersettingscopy->move(ui->usersettingscopy->x(), ui->userdatafilescopy->y());
            ui->grubinstalltext->move(ui->grubinstalltext->x(), ui->copypanel->height() - ss(96));
            ui->grubinstallcopy->move(ui->grubinstallcopy->x(), ui->grubinstalltext->y());
            ui->grubinstallcopydisable->move(ui->grubinstallcopy->pos());
            ui->efiwarning->setGeometry(ui->efiwarning->x(), ui->grubinstalltext->y() - ss(4), ui->copypanel->width() - ui->efiwarning->x() - ss(8), ui->efiwarning->height());
            ui->copyback->move(ui->copyback->x(), ui->copypanel->height() - ss(48));
            ui->copynext->move(ui->copypanel->width() - ss(152), ui->copyback->y());
            ui->copyresize->move(ui->copypanel->width() - ui->copyresize->width(), ui->copypanel->height() - ui->copyresize->height());
            ui->copycover->resize(ui->copypanel->width() - ss(10), ui->copypanel->height() - ss(10));
        }
        else if(ui->excludepanel->isVisible())
        {
            ui->excludepanel->resize(width() - (sfctr == High ? 4 : ss(2)), height() - ss(25));
            ui->itemstext->resize(ui->excludepanel->width() / 2 - ss(44) + (sfctr == High ? 1 : 0), ui->itemstext->height());
            ui->excludedtext->setGeometry(ui->excludepanel->width() / 2 + ss(36), ui->excludedtext->y(), ui->itemstext->width(), ui->excludedtext->height());
            ui->itemslist->resize(ui->itemstext->width(), ui->excludepanel->height() - ss(160));
            ui->excludedlist->setGeometry(ui->excludepanel->width() / 2 + ss(36), ui->excludedlist->y(), ui->itemslist->width(), ui->itemslist->height());
            ui->additem->move(ui->excludepanel->width() / 2 - ss(24), ui->itemslist->height() / 2 + ss(36));
            ui->removeitem->move(ui->additem->x(), ui->itemslist->height() / 2 + ss(108));
            ui->excludeback->move(ui->excludeback->x(), ui->excludepanel->height() - ss(48));
            ui->kendektext->move(ui->excludepanel->width() - ss(306), ui->excludepanel->height() - ss(24));
            ui->excluderesize->move(ui->excludepanel->width() - ui->excluderesize->width(), ui->excludepanel->height() - ui->excluderesize->height());
        }

        if(! qApp->overrideCursor() || qApp->overrideCursor()->shape() != Qt::SizeFDiagCursor) repaint();

        if(! wismax)
        {
            if(wgeom[2] != width()) wgeom[2] = width();
            if(wgeom[3] != height()) wgeom[3] = height();
            goto bcheck;
        }

        return false;
    case QEvent::Move:
        if(wismax)
        {
            schar snum(qApp->desktop()->screenNumber(this));

            if(geometry() != qApp->desktop()->availableGeometry(snum))
            {
                setGeometry(qApp->desktop()->availableGeometry(snum));
                return true;
            }
        }
        else
        {
            if(wgeom[0] != x()) wgeom[0] = x();
            if(wgeom[1] != y()) wgeom[1] = y();
            goto bcheck;
        }

        return false;
    case QEvent::WindowStateChange:
    {
        QEvent nev(isMinimized() ? QEvent::WindowDeactivate : QEvent::WindowActivate);
        qApp->sendEvent(this, &nev);
    }
    default:
        return false;
    }

gcheck:
    if(! wismax)
    {
        schar snum(qApp->desktop()->screenNumber(this));
        short scrxy(qApp->desktop()->screenGeometry(snum).x());

        if(x() < scrxy && x() > scrxy - width())
            wgeom[0] = scrxy + ss(30);
        else
        {
            short scrw(qApp->desktop()->screenGeometry(snum).width());
            if(x() > scrxy + scrw - width() && x() < scrxy + scrw + width()) wgeom[0] = scrxy + scrw - width() - ss(30);
        }

        if(y() < (scrxy = qApp->desktop()->screenGeometry(snum).y()) && y() > scrxy - height())
            wgeom[1] = scrxy + ss(30);
        else
        {
            short scrh(qApp->desktop()->screenGeometry(snum).height());
            if(y() > scrxy + scrh - height() && y() < scrxy + scrh + height()) wgeom[1] = scrxy + scrh - height() - ss(30);
        }

        if(pos() != QPoint(wgeom[0], wgeom[1]))
        {
            move(wgeom[0], wgeom[1]);
            return false;
        }
    }

bcheck:
    if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
    return false;
}

void systemback::keyPressEvent(QKeyEvent *ev)
{
    if(! qApp->overrideCursor())
        switch(ev->key()) {
        case Qt::Key_Escape:
            if(ui->passwordpanel->isVisible()) close();
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
            qApp->sendEvent(qApp->focusObject(), &press);

            if(ui->sbpanel->isVisible())
            {
                if(ui->pointrename->isEnabled())
                {
                    schar num(-1);

                    for(QLE *ldt : ui->points->findChildren<QLE *>())
                    {
                        ++num;

                        if(ldt->hasFocus() && getppipe(num)->isChecked())
                        {
                            on_pointrename_clicked();
                            break;
                        }
                    }
                }
            }
            else if(ui->dirchoose->hasFocus())
                ui->dirchoose->currentItem()->setExpanded(! ui->dirchoose->currentItem()->isExpanded());
            else if(ui->repairmountpoint->hasFocus())
            {
                if(ui->repairmount->isEnabled())
                {
                    on_repairmount_clicked();
                    if(! ui->repairmountpoint->hasFocus()) ui->repairmountpoint->setFocus();
                }
            }
            else if(ui->itemslist->hasFocus())
            {
                if(ui->itemslist->currentItem()->isSelected())
                {
                    if(ui->itemslist->currentItem()->childCount() == 0)
                        on_additem_clicked();
                    else if(ui->itemslist->currentItem()->isExpanded())
                        ui->itemslist->currentItem()->setExpanded(false);
                    else
                        ui->itemslist->currentItem()->setExpanded(true);
                }
            }
            else if(ui->excludedlist->hasFocus())
                on_removeitem_clicked();
            else if(ui->copypanel->isVisible())
            {
                if(ui->mountpoint->hasFocus())
                {
                    if(ui->changepartition->isEnabled()) on_changepartition_clicked();
                }
                else if(ui->partitionsize->hasFocus() && ui->newpartition->isEnabled())
                    on_newpartition_clicked();
            }
            else if(ui->installpanel->isVisible())
            {
                if(ui->fullname->hasFocus())
                {
                    if(ui->fullnamepipe->isVisible()) ui->username->setFocus();
                }
                else if(ui->username->hasFocus())
                {
                    if(ui->usernamepipe->isVisible()) ui->password1->setFocus();
                }
                else if(ui->password1->hasFocus())
                {
                    if(ui->password2->isEnabled()) ui->password2->setFocus();
                }
                else if(ui->password2->hasFocus())
                {
                    if(ui->passwordpipe->isVisible()) ui->rootpassword1->setFocus();
                }
                else if(ui->rootpassword1->hasFocus())
                    ui->rootpassword2->isEnabled() ? ui->rootpassword2->setFocus() : ui->hostname->setFocus();
                else if(ui->rootpassword2->hasFocus())
                {
                    if(ui->rootpasswordpipe->isVisible()) ui->hostname->setFocus();
                }
                else if(ui->hostname->hasFocus() && ui->installnext->isEnabled())
                    ui->installnext->setFocus();
            }

            break;
        }
        case Qt::Key_F5:
            if(ui->sbpanel->isVisible())
            {
                schar num(-1);

                for(QLE *ldt : ui->points->findChildren<QLE *>())
                {
                    ++num;

                    if(ldt->hasFocus())
                    {
                        if(ldt->text() != sb::pnames[num]) ldt->setText(sb::pnames[num]);
                        break;
                    }
                }
            }
            else if(ui->partitionsettings->hasFocus() || ui->mountpoint->hasFocus() || ui->partitionsize->hasFocus())
                on_partitionrefresh2_clicked();
            else if(ui->livecreatepanel->isVisible())
            {
                if(ui->livename->hasFocus())
                {
                    if(ui->livename->text() != "auto") ui->livename->setText("auto");
                }
                else if(ui->livedevices->hasFocus())
                    on_livedevicesrefresh_clicked();
                else if(ui->livelist->hasFocus())
                {
                    on_livecreatemenu_clicked();
                    ui->livecreateback->setFocus();
                }
            }
            else if(ui->dirchoose->hasFocus())
                on_dirrefresh_clicked();
            else if(ui->repairmountpoint->hasFocus())
                on_repairpartitionrefresh_clicked();
            else if(ui->itemslist->hasFocus())
                ilstupdt();

            break;
        case Qt::Key_Delete:
            if(ui->partitionsettings->hasFocus())
            {
                if(ui->unmountdelete->isEnabled() && ui->unmountdelete->text() == tr("Unmount")) on_unmountdelete_clicked();
            }
            else if(ui->livelist->hasFocus())
            {
                if(ui->livedelete->isEnabled()) on_livedelete_clicked();
            }
            else if(ui->itemslist->hasFocus())
                on_additem_clicked();
            else if(ui->excludedlist->hasFocus())
                on_removeitem_clicked();
        }
}

void systemback::keyReleaseEvent(QKeyEvent *ev)
{
    if(! qApp->overrideCursor())
        switch(ev->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
            qApp->sendEvent(qApp->focusObject(), &release);
        }
}

void systemback::on_admins_currentIndexChanged(cQStr &arg1)
{
    ui->admins->resize(fontMetrics().width(arg1) + ss(30), ui->admins->height());
    if(! hash.isEmpty()) hash.clear();

    {
        QFile file("/etc/shadow");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(arg1 % ':'))
                {
                    hash = sb::mid(cline, arg1.length() + 2, sb::instr(cline, ":", arg1.length() + 2) - arg1.length() - 2);
                    break;
                }
            }
    }

    if(ui->adminpassword->text().length() > 0) ui->adminpassword->clear();

    if(! hash.isEmpty() && QStr(crypt("", chr(hash))) == hash)
    {
        ui->adminpasswordpipe->show();
        for(QWdt *wdgt : QWL{ui->adminpassword, ui->admins}) wdgt->setDisabled(true);
        ui->passwordinputok->setEnabled(true);
    }
}

void systemback::on_adminpassword_textChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? (icnt = 0) : ++icnt);

    if(arg1.isEmpty())
    {
        if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
    }
    else if(! hash.isEmpty() && QStr(crypt(chr(arg1), chr(hash))) == hash)
    {
        sb::delay(300);

        if(ccnt == icnt)
        {
            if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
            ui->adminpasswordpipe->show();
            for(QWdt *wdgt : QWL{ui->adminpassword, ui->admins}) wdgt->setDisabled(true);
            ui->passwordinputok->setEnabled(true);
            ui->passwordinputok->setFocus();
        }
    }
    else if(ui->adminpassworderror->isHidden())
        ui->adminpassworderror->show();
}

void systemback::on_startcancel_clicked()
{
    close();
}

void systemback::on_passwordinputok_clicked()
{
    busy();
    QTimer::singleShot(0, this, SLOT(unitimer()));
    ui->passwordpanel->hide();
    ui->mainpanel->show();
    ui->sbpanel->isVisible() ? ui->functionmenunext->setFocus() : ui->fullname->setFocus();
    windowmove(ss(698), ss(465));
    setwontop(false);
}

void systemback::on_schedulerstart_clicked()
{
    delete shdltimer;
    shdltimer = nullptr;
    ui->function2->setText("Systemback " % tr("scheduler"));
    on_newrestorepoint_clicked();
}

void systemback::on_dialogcancel_clicked()
{
    if(dialog != 108)
    {
        if(dlgtimer)
        {
            delete dlgtimer;
            dlgtimer = nullptr;
            if(ui->dialognumber->text() != "30s") ui->dialognumber->setText("30s");
        }

        if(! ui->sbpanel->isVisibleTo(ui->mainpanel))
        {
            if(ui->restorepanel->isVisibleTo(ui->mainpanel))
                ui->restorepanel->hide();
            else if(ui->copypanel->isVisibleTo(ui->mainpanel))
                ui->copypanel->hide();
            else if(ui->livecreatepanel->isVisibleTo(ui->mainpanel))
                ui->livecreatepanel->hide();
            else if(ui->repairpanel->isVisibleTo(ui->mainpanel))
                ui->repairpanel->hide();

            ui->sbpanel->show();
            ui->function1->setText("Systemback");
        }

        for(QCB *ckbx : ui->sbpanel->findChildren<QCB *>())
            if(ckbx->isChecked())
            {
                ckbx->click();
                break;
            }
    }

    ui->dialogpanel->hide();
    ui->mainpanel->show();
    ui->functionmenunext->setFocus();
    windowmove(ss(698), ss(465));
    setwontop(false);
}

void systemback::pnmchange(uchar num)
{
    if(sb::pnumber != num)
    {
        sb::pnumber = num;
        if(! cfgupdt) cfgupdt = true;
    }

    uchar cnum(0);

    for(QLE *ldt : ui->points->findChildren<QLE *>())
        switch(++cnum) {
        case 1 ... 2:
            break;
        case 3:
            if(ldt->isEnabled())
                switch(num) {
                case 3:
                    if(ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                default:
                    if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);
                }

            break;
        case 11:
            goto end;
        default:
            if(ldt->isEnabled())
            {
                if(cnum < num)
                {
                    if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);
                }
                else if(ldt->styleSheet().isEmpty())
                    ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
            }
            else if(cnum <= num)
            {
                if(ldt->text() == tr("not used")) ldt->setText(tr("empty"));
            }
            else if(ldt->text() == tr("empty"))
                ldt->setText(tr("not used"));
        }

end:
    fontcheck(Rpnts);
}

void systemback::on_pnumber3_clicked()
{
    pnmchange(3);
}

void systemback::on_pnumber4_clicked()
{
    pnmchange(4);
}

void systemback::on_pnumber5_clicked()
{
    pnmchange(5);
}

void systemback::on_pnumber6_clicked()
{
    pnmchange(6);
}

void systemback::on_pnumber7_clicked()
{
    pnmchange(7);
}

void systemback::on_pnumber8_clicked()
{
    pnmchange(8);
}

void systemback::on_pnumber9_clicked()
{
    pnmchange(9);
}

void systemback::on_pnumber10_clicked()
{
    pnmchange(10);
}

inline void systemback::ptxtchange(uchar num, cQStr &txt)
{
    QLE *ldt(getpoint(num));
    QCB *ckbx(getppipe(num));

    if(ldt->isEnabled())
    {
        if(txt.isEmpty())
        {
            if(ckbx->isChecked()) ckbx->click();
            ckbx->setDisabled(true);
        }
        else if(sb::like(txt, {"* *", "*/*"}))
        {
            uchar cpos(ldt->cursorPosition() - 1);
            ldt->setText(QStr(txt).replace(cpos, 1, nullptr));
            ldt->setCursorPosition(cpos);
        }
        else
        {
            if(! ckbx->isEnabled()) ckbx->setEnabled(true);
            if(! ldt->hasFocus()) ldt->setCursorPosition(0);

            if(txt != sb::pnames[num])
            {
                if(! ckbx->isChecked()) ckbx->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ckbx->isChecked())
                ckbx->click();
        }
    }
    else if(ckbx->isEnabled())
    {
        if(ckbx->isChecked()) ckbx->click();
        ckbx->setDisabled(true);
    }
}

void systemback::on_point1_textChanged(cQStr &arg1)
{
    ptxtchange(0, arg1);
}

void systemback::on_point2_textChanged(cQStr &arg1)
{
    ptxtchange(1, arg1);
}

void systemback::on_point3_textChanged(cQStr &arg1)
{
    ptxtchange(2, arg1);
}

void systemback::on_point4_textChanged(cQStr &arg1)
{
    ptxtchange(3, arg1);
}

void systemback::on_point5_textChanged(cQStr &arg1)
{
    ptxtchange(4, arg1);
}

void systemback::on_point6_textChanged(cQStr &arg1)
{
    ptxtchange(5, arg1);
}

void systemback::on_point7_textChanged(cQStr &arg1)
{
    ptxtchange(6, arg1);
}

void systemback::on_point8_textChanged(cQStr &arg1)
{
    ptxtchange(7, arg1);
}

void systemback::on_point9_textChanged(cQStr &arg1)
{
    ptxtchange(8, arg1);
}

void systemback::on_point10_textChanged(cQStr &arg1)
{
    ptxtchange(9, arg1);
}

void systemback::on_point11_textChanged(cQStr &arg1)
{
    ptxtchange(10, arg1);
}

void systemback::on_point12_textChanged(cQStr &arg1)
{
    ptxtchange(11, arg1);
}

void systemback::on_point13_textChanged(cQStr &arg1)
{
    ptxtchange(12, arg1);
}

void systemback::on_point14_textChanged(cQStr &arg1)
{
    ptxtchange(13, arg1);
}

void systemback::on_point15_textChanged(cQStr &arg1)
{
    ptxtchange(14, arg1);
}

void systemback::on_restoremenu_clicked()
{
    if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
    {
        if(ui->grubreinstallrestoredisable->isVisibleTo(ui->restorepanel))
        {
            ui->grubreinstallrestoredisable->hide();
            ui->grubreinstallrestore->show();
        }
    }
    else if(ui->grubreinstallrestore->isVisibleTo(ui->restorepanel))
    {
        ui->grubreinstallrestore->hide();
        ui->grubreinstallrestoredisable->show();
    }

    if(ui->includeusers->count() > 0) ui->includeusers->clear();
    ui->includeusers->addItems({tr("Everyone"), "root"});
    if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
    ui->sbpanel->hide();
    ui->restorepanel->show();
    ui->function1->setText(tr("System restore"));
    ui->restoreback->setFocus();
    repaint();
    QFile file("/etc/passwd");

    if(file.open(QIODevice::ReadOnly))
        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ui->includeusers->addItem(usr);
        }
}

void systemback::on_copymenu_clicked()
{
    if(! grub.isEFI || ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(ppipe == 0 ? sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisibleTo(ui->copypanel))
            {
                ui->grubinstallcopydisable->hide();
                ui->grubinstallcopy->show();
            }
        }
        else if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
        {
            ui->grubinstallcopy->hide();
            ui->grubinstallcopydisable->show();
        }
    }

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        ui->usersettingscopy->hide();
        ui->userdatafilescopy->show();
    }

    if(ppipe == 1)
    {
        ui->userdatafilescopy->setDisabled(true);
        if(ui->userdatafilescopy->isChecked()) ui->userdatafilescopy->setChecked(false);
    }
    else if(! ui->userdatafilescopy->isEnabled())
        ui->userdatafilescopy->setEnabled(true);

    ui->sbpanel->hide();
    ui->copypanel->show();
    ui->function1->setText(tr("System copy"));
    ui->copyback->setFocus();
    short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
    if(nwidth > ss(698)) windowmove(nwidth < ss(850) ? nwidth : ss(850), ss(465), false);
    setMinimumSize(ss(698), ss(465));
    { schar snum(qApp->desktop()->screenNumber(this));
    setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60)); }

    if(ui->partitionsettings->currentItem())
    {
        if(sb::mcheck("/.sbsystemcopy/") || sb::mcheck("/.sblivesystemwrite/"))
            on_partitionrefresh_clicked();
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && ! ui->mountpoint->isEnabled())
            ui->mountpoint->setEnabled(true);
    }
}

void systemback::on_installmenu_clicked()
{
    ui->sbpanel->hide();
    ui->installpanel->show();
    ui->function1->setText(tr("System install"));
    ui->fullname->setFocus();
    repaint();
}

void systemback::on_livecreatemenu_clicked()
{
    if(ui->livelist->count() > 0) ui->livelist->clear();

    for(QWdt *wdgt : QWL{ui->livedelete, ui->liveconvert, ui->livewritestart})
        if(wdgt->isEnabled()) wdgt->setDisabled(true);

    if(ui->sbpanel->isVisible())
    {
        ui->sbpanel->hide();
        ui->livecreatepanel->show();
        ui->function1->setText(tr("Live system create"));
        ui->livecreateback->setFocus();
        repaint();
    }

    if(sb::isdir(sb::sdir[2]))
        for(cQStr &item : QDir(sb::sdir[2]).entryList(QDir::Files | QDir::Hidden))
            if(item.endsWith(".sblive") && ! item.contains(' ') && ! sb::islink(sb::sdir[2] % '/' % item) && sb::fsize(sb::sdir[2] % '/' % item) > 0)
            {
                QLWI *lwi(new QLWI(sb::left(item, -7) % " (" % QStr::number(qRound64(sb::fsize(sb::sdir[2] % '/' % item) * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB, " % (sb::stype(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") == sb::Isfile && sb::fsize(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") > 0 ? "sblive+iso" : "sblive") % ')'));
                ui->livelist->addItem(lwi);
            }
}

void systemback::on_repairmenu_clicked()
{
    if(ppipe == 1 || pname == tr("Live image"))
    {
        if(! ui->systemrepair->isEnabled())
            for(QWdt *wdgt : QWL{ui->systemrepair, ui->fullrepair}) wdgt->setEnabled(true);

        rmntcheck();
    }
    else if(ui->systemrepair->isEnabled())
    {
        for(QWdt *wdgt : QWL{ui->systemrepair, ui->fullrepair}) wdgt->setDisabled(true);
        if(! ui->grubrepair->isChecked()) ui->grubrepair->click();
    }

    on_repairmountpoint_currentTextChanged(ui->repairmountpoint->currentText());
    ui->sbpanel->hide();
    ui->repairpanel->show();
    ui->function1->setText(tr("System repair"));
    ui->repairback->setFocus();
    repaint();
}

void systemback::on_systemupgrade_clicked()
{
    statustart();
    pset(11);
    QDateTime ofdate(QFileInfo("/usr/bin/systemback").lastModified());
    sb::unlock(sb::Dpkglock);
    sb::exec("xterm +sb -bg grey85 -fg grey25 -fa a -fs 9 -geometry 80x24+80+70 -n \"System upgrade\" -T \"System upgrade\" -cr grey40 -selbg grey86 -bw 0 -bc -bcf 500 -bcn 500 -e sbsysupgrade");

    if(isVisible())
    {
        if(ofdate != QFileInfo("/usr/bin/systemback").lastModified())
        {
            nrxth = true;
            sb::unlock(sb::Sblock);
            sb::exec("systemback", nullptr, sb::Bckgrnd);
            close();
        }
        else if(sb::lock(sb::Dpkglock))
        {
            ui->statuspanel->hide();
            ui->mainpanel->show();
            ui->functionmenunext->setFocus();
            windowmove(ss(698), ss(465));
        }
        else
        {
            utimer.stop();
            dialogopen(301);
        }
    }
}

void systemback::on_excludemenu_clicked()
{
    ui->sbpanel->hide();
    ui->excludepanel->show();
    ui->function1->setText(tr("Exclude"));
    ui->excludeback->setFocus();
    repaint();
    schar snum(qApp->desktop()->screenNumber(this));
    setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60));
}

void systemback::on_schedulemenu_clicked()
{
    ui->sbpanel->hide();
    ui->schedulepanel->show();
    ui->function1->setText(tr("Schedule"));
    ui->schedulerback->setFocus();
    repaint();
}

void systemback::on_aboutmenu_clicked()
{
    ui->sbpanel->hide();
    ui->aboutpanel->show();
    ui->function1->setText(tr("About"));
    ui->aboutback->setFocus();
    repaint();
}

void systemback::on_settingsmenu_clicked()
{
    ui->sbpanel->hide();
    ui->settingspanel->show();
    ui->function1->setText(tr("Settings"));
    ui->settingsback->setFocus();
    repaint();
}

void systemback::on_partitionrefresh_clicked()
{
    busy();
    if(! ui->copycover->isVisibleTo(ui->copypanel)) ui->copycover->show();
    if(ui->copynext->isEnabled()) ui->copynext->setDisabled(true);
    if(ui->mountpoint->count() > 0) ui->mountpoint->clear();
    ui->mountpoint->addItems({nullptr, "/", "/home", "/boot"});

    if(grub.isEFI)
    {
        ui->mountpoint->addItem("/boot/efi");
        if(! ui->efiwarning->isVisibleTo(ui->copypanel)) ui->efiwarning->show();

        if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
        {
            ui->grubinstallcopy->hide();
            ui->grubinstallcopydisable->show();
        }
    }

    ui->mountpoint->addItems({"/tmp", "/usr", "/var", "/srv", "/opt", "/usr/local", "SWAP"});

    if(ui->mountpoint->isEnabled())
    {
        ui->mountpoint->setDisabled(true);
        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
    }

    if(ui->format->isEnabled())
    {
        ui->format->setDisabled(true);
        if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
    }

    if(ui->unmountdelete->text() == tr("! Delete !"))
    {
        ui->unmountdelete->setText(tr("Unmount"));
        ui->unmountdelete->setStyleSheet(nullptr);
    }

    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

    if(nohmcpy[1])
    {
        nohmcpy[1] = false;
        if(ppipe == 0) ui->userdatafilescopy->setEnabled(true);
    }

    if(ui->partitionsettings->rowCount() > 0) ui->partitionsettings->clearContents();
    if(ui->repairpartition->count() > 0) ui->repairpartition->clear();

    if(! wismax)
        for(uchar a(2) ; a < 5 ; ++a) ui->partitionsettings->resizeColumnToContents(a);

    QSL plst;
    sb::readprttns(plst);

    if(! grub.isEFI)
    {
        if(ui->grubinstallcopy->count() > 0)
            for(QCbB *cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->clear();

        for(QCbB *cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItems({"Auto", tr("Disabled")});

        for(cQStr &dts : plst)
        {
            QStr path(dts.split('\n').at(0));

            if(sb::like(path.length(), {8, 12}))
                for(QCbB *cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItem(path);
        }
    }

    schar sn(-1);
    QBA mnts[]{sb::fload("/proc/self/mounts"), sb::fload("/proc/swaps")};

    for(cQStr &cdts : plst)
    {
        QSL dts(cdts.split('\n'));
        cQStr &path(dts.at(0)), &type(dts.at(2));
        ullong bsize(dts.at(1).toULongLong());

        if(sb::like(path.length(), {8, 12}))
        {
            ++sn;
            ui->partitionsettings->setRowCount(sn + 1);
            if(sn > 0) ui->partitionsettings->setRowHeight(sn, ss(25));
            QTblWI *dev(new QTblWI(path));
            dev->setTextAlignment(Qt::AlignBottom);
            ui->partitionsettings->setItem(sn, 0, dev);
            QTblWI *size(new QTblWI);
            size->setText(bsize < 1073741824 ? QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB" : bsize < 1073741824000 ? QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");
            size->setTextAlignment(Qt::AlignRight | Qt::AlignBottom);
            ui->partitionsettings->setItem(sn, 1, size);
            QTblWI *empty(new QTblWI);
            ui->partitionsettings->setItem(sn, 2, empty);
            for(uchar a(3) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, empty->clone());
            QTblWI *tp(new QTblWI(type));
            ui->partitionsettings->setItem(sn, 8, tp);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 10, lngth);
            QFont fnt;
            fnt.setWeight(QFont::DemiBold);
            for(QTblWI *twi : {dev, size}) twi->setFont(fnt);
        }
        else
        {
            switch(type.toUShort()) {
            case sb::Extended:
            {
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);
                size->setText(bsize < 1073741824 ? QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB" : bsize < 1073741824000 ? QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");
                size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->partitionsettings->setItem(sn, 1, size);
                QTblWI *empty(new QTblWI);
                ui->partitionsettings->setItem(sn, 2, empty);
                for(uchar a(3) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, empty->clone());
                QFont fnt;
                fnt.setWeight(QFont::DemiBold);
                fnt.setItalic(true);
                for(QTblWI *twi : {dev, size}) twi->setFont(fnt);
                break;
            }
            case sb::Primary:
            case sb::Logical:
            {
                cQStr &uuid(dts.at(6));

                if(! grub.isEFI)
                    for(QCbB *cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItem(path);

                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);
                size->setText(bsize < 1073741824 ? QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB" : bsize < 1073741824000 ? QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");
                size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->partitionsettings->setItem(sn, 1, size);
                QTblWI *lbl(new QTblWI(dts.at(5)));
                lbl->setTextAlignment(Qt::AlignCenter);
                if(! dts.at(5).isEmpty()) lbl->setToolTip(dts.at(5));
                ui->partitionsettings->setItem(sn, 2, lbl);
                QTblWI *mpt(new QTblWI);

                if(uuid.isEmpty())
                {
                    if(QStr('\n' % mnts[0]).contains('\n' % path % ' '))
                    {
                        QStr mnt(sb::right(mnts[0], - sb::instr(mnts[0], path % ' ')));
                        short spc(sb::instr(mnt, " "));
                        mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                    }
                }
                else if(QStr('\n' % mnts[0]).contains('\n' % path % ' '))
                    mpt->setText(QStr('\n' % mnts[0]).count('\n' % path % ' ') > 1 || QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' ') ? tr("Multiple mount points") : [&] {
                            QStr mnt(sb::right(mnts[0], - sb::instr(mnts[0], path % ' ')));
                            short spc(sb::instr(mnt, " "));
                            return sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " ");
                        }());
                else if(QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                    mpt->setText(QStr('\n' % mnts[0]).count("\n/dev/disk/by-uuid/" % uuid % ' ') > 1 ? tr("Multiple mount points") : [&] {
                            QStr mnt(sb::right(mnts[0], - sb::instr(mnts[0], "/dev/disk/by-uuid/" % uuid % ' ')));
                            short spc(sb::instr(mnt, " "));
                            return sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " ");
                        }());
                else if(QStr('\n' % mnts[1]).contains('\n' % path % ' '))
                    mpt->setText("SWAP");

                mpt->text().isEmpty() ? ui->repairpartition->addItem(path) : mpt->setToolTip(mpt->text());
                ui->partitionsettings->setItem(sn, 3, mpt);
                QTblWI *empty(new QTblWI);
                ui->partitionsettings->setItem(sn, 4, empty);
                QTblWI *fs(new QTblWI(dts.at(4)));
                ui->partitionsettings->setItem(sn, 5, fs);
                QTblWI *frmt(new QTblWI("-"));
                for(QTblWI *wdi : QList<QTblWI *>{mpt, empty, fs, frmt}) wdi->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 6, frmt);
                ui->partitionsettings->setItem(sn, 7, fs->clone());
                break;
            }
            case sb::Freespace:
            case sb::Emptyspace:
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);
                size->setText(bsize < 1073741824 ? QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB" : bsize < 1073741824000 ? QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");
                size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->partitionsettings->setItem(sn, 1, size);
                QTblWI *empty(new QTblWI);
                ui->partitionsettings->setItem(sn, 2, empty);
                for(uchar a(3) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, empty->clone());
                QFont fnt;
                fnt.setItalic(true);
                for(QTblWI *twi : {dev, size}) twi->setFont(fnt);
                break;
            }

            QTblWI *tp(new QTblWI(type));
            ui->partitionsettings->setItem(sn, 8, tp);
            QTblWI *start(new QTblWI(dts.at(3)));
            ui->partitionsettings->setItem(sn, 9, start);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 10, lngth);
        }
    }

    for(uchar a(0) ; a < 7 ; ++a)
        if(wismax || a < 2 || a > 4) ui->partitionsettings->resizeColumnToContents(a);

    if(ui->copypanel->isVisible() && ! ui->copyback->hasFocus()) ui->copyback->setFocus();
    ui->copycover->hide();
    busy(false);
}

void systemback::on_partitionrefresh2_clicked()
{
    on_partitionrefresh_clicked();

    if(! ui->partitionsettingspanel1->isVisibleTo(ui->copypanel))
    {
        ui->partitionsettingspanel2->isVisibleTo(ui->copypanel) ? ui->partitionsettingspanel2->hide() : ui->partitionsettingspanel3->hide();
        ui->partitionsettingspanel1->show();
    }
}

void systemback::on_partitionrefresh3_clicked()
{
    on_partitionrefresh2_clicked();
}

void systemback::on_unmountdelete_clicked()
{
    busy();
    ui->copycover->show();

    if(ui->unmountdelete->text() == tr("Unmount"))
    {
        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() != "SWAP")
        {
            QStr mnts(sb::fload("/proc/self/mounts", true));
            QTS in(&mnts, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(in.readLine()), mpt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().replace(" ", "\\040"));
                if(sb::like(cline, {"* " % mpt % " *", "* " % mpt % "/*"})) sb::umount(cline.split(' ').at(1));
            }

            mnts = sb::fload("/proc/self/mounts");

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            {
                QStr mpt(ui->partitionsettings->item(a, 3)->text());

                if(! mpt.isEmpty() && ! sb::like(mpt, {"_SWAP_", '_' % tr("Multiple mount points") % '_'}) && ! mnts.contains(' ' % mpt.replace(" ", "\\040") % ' '))
                {
                    ui->partitionsettings->item(a, 3)->setText(nullptr);
                    ui->partitionsettings->item(a, 3)->setToolTip(nullptr);
                }
            }

            sb::fssync();
        }
        else if(! QStr('\n' % sb::fload("/proc/swaps")).contains('\n' % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text() % ' ') || swapoff(chr(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text())) == 0)
        {
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->setText(nullptr);
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->setToolTip(nullptr);
        }

        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().isEmpty())
        {
            ui->unmountdelete->setText(tr("! Delete !"));
            ui->unmountdelete->setStyleSheet("QPushButton:enabled {color: red}");
            if(minside(ui->copypanel->pos() + ui->partitionsettingspanel1->pos() + ui->unmountdelete->pos(), ui->unmountdelete->size())) ui->unmountdelete->setDisabled(true);
            if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
            for(QWdt *wdgt : QWL{ui->filesystem, ui->format}) wdgt->setEnabled(true);
        }

        ui->copycover->hide();
    }
    else
    {
        sb::delpart(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text());
        on_partitionrefresh2_clicked();
    }

    busy(false);
}

void systemback::on_unmount_clicked()
{
    busy();
    ui->copycover->show();
    bool umntd(true);

    {
        QStr mnts[2]{sb::fload("/proc/self/mounts", true)};

        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
        {
            QStr mpt(ui->partitionsettings->item(a, 3)->text());

            if(! mpt.isEmpty())
            {
                if(mpt == "SWAP")
                    swapoff(chr(ui->partitionsettings->item(a, 0)->text()));
                else
                {
                    QTS in(&mnts[0], QIODevice::ReadOnly);
                    QSL incl{"* " % mpt.replace(" ", "\\040") % " *", "* " % mpt % "/*"};

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        if(sb::like(cline, incl)) sb::umount(cline.split(' ').at(1));
                    }
                }
            }
        }

        mnts[0] = sb::fload("/proc/self/mounts"), mnts[1] = sb::fload("/proc/swaps");

        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
        {
            QStr mpt(ui->partitionsettings->item(a, 3)->text());

            if(! mpt.isEmpty())
            {
                if((mpt == "SWAP" && ! QStr('\n' % mnts[1]).contains('\n' % ui->partitionsettings->item(a, 0)->text() % ' ')) || (mpt != "SWAP" && ! mnts[0].contains(' ' % mpt.replace(" ", "\\040") % ' ')))
                {
                    ui->partitionsettings->item(a, 3)->setText(nullptr);
                    ui->partitionsettings->item(a, 3)->setToolTip(nullptr);
                }
                else if(umntd)
                    umntd = false;
            }
        }
    }

    sb::fssync();

    if(umntd)
    {
        ui->unmount->setDisabled(true);
        ui->partitiondelete->setEnabled(true);
    }

    ui->copycover->hide();
    busy(false);
}

void systemback::on_restoreback_clicked()
{
    ui->restorepanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenunext->setFocus();
    repaint();
}

void systemback::on_copyback_clicked()
{
    if(ui->copycover->isHidden())
    {
        windowmove(ss(698), ss(465));
        ui->copypanel->hide();

        if(ui->function1->text() == tr("System copy"))
        {
            ui->sbpanel->show();
            ui->function1->setText("Systemback");
            ui->functionmenunext->setFocus();
        }
        else
        {
            ui->installpanel->show();
            ui->fullname->setFocus();
        }

        ui->partitionsettings->resizeColumnToContents(6);
    }
}

void systemback::on_installback_clicked()
{
    ui->installpanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenunext->setFocus();
    repaint();
}

void systemback::on_livecreateback_clicked()
{
    if(ui->livecreatecover->isHidden())
    {
        ui->livecreatepanel->hide();
        ui->sbpanel->show();
        ui->function1->setText("Systemback");
        ui->functionmenunext->setFocus();
        repaint();
    }
}

void systemback::on_repairback_clicked()
{
    if(ui->repaircover->isHidden())
    {
        ui->repairpanel->hide();
        ui->sbpanel->show();
        ui->function1->setText("Systemback");
        ui->functionmenunext->setFocus();
        repaint();
    }
}

void systemback::on_excludeback_clicked()
{
    if(ui->copycover->isHidden())
    {
        windowmove(ss(698), ss(465));
        ui->excludepanel->hide();
        ui->sbpanel->show();
        ui->function1->setText("Systemback");
        ui->functionmenunext->setFocus();
        repaint();
    }
}

void systemback::on_schedulerback_clicked()
{
    ui->schedulepanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenuback->setFocus();
    repaint();
}

void systemback::on_aboutback_clicked()
{
    ui->aboutpanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenuback->setFocus();
    repaint();
}

void systemback::on_licenseback_clicked()
{
    ui->licensepanel->hide();
    ui->aboutpanel->show();
    ui->function1->setText(tr("About"));
    ui->aboutback->setFocus();
    repaint();
}

void systemback::on_licensemenu_clicked()
{
    ui->aboutpanel->hide();
    ui->licensepanel->show();
    ui->function1->setText(tr("License"));
    ui->licenseback->setFocus();
    repaint();

    if(! ui->license->isEnabled())
    {
        busy();
        ui->license->setText(sb::fload("/usr/share/common-licenses/GPL-3"));
        ui->license->setEnabled(true);
        busy(false);
    }
}

void systemback::on_settingsback_clicked()
{
    ui->settingspanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenuback->setFocus();
    repaint();
}

void systemback::on_pointpipe1_clicked()
{
    if(ppipe > 0) ppipe = 0;
    bool rnmenbl(false);
    schar num(-1);

    for(QCB *ckbx : ui->sbpanel->findChildren<QCB *>())
    {
        ++num;

        if(ckbx->isChecked())
        {
            if(++ppipe == 1) cpoint = [num]() -> QStr {
                    switch(num) {
                    case 9:
                        return "S10";
                    case 10 ... 14:
                        return "H0" % QStr::number(num - 9);
                    default:
                        return "S0" % QStr::number(num + 1);
                    }
                }(), pname = sb::pnames[num];

            if(! rnmenbl && getpoint(num)->text() != sb::pnames[num]) rnmenbl = true;
        }
        else
        {
            QLE *ldt(getpoint(num));
            if(ldt->isEnabled() && ldt->text() != sb::pnames[num] && ! ldt->text().isEmpty()) ldt->setText(sb::pnames[num]);
        }
    }

    if(ppipe == 0)
    {
        if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);

        if(ui->storagedirbutton->isHidden())
        {
            ui->storagedir->resize(ss(210), ui->storagedir->height());
            ui->storagedirbutton->show();
            for(QWdt *wdgt : QWL{ui->pointrename, ui->pointdelete}) wdgt->setDisabled(true);
        }

        if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);

        pname = [this]() -> QStr {
                if(! sislive)
                {
                    for(QWdt *wdgt : QWL{ui->newrestorepoint, ui->livecreatemenu})
                        if(! wdgt->isEnabled()) wdgt->setEnabled(true);

                    return tr("Currently running system");
                }
                else if(sb::isdir("/.systemback"))
                {
                    if(! ui->copymenu->isEnabled())
                        for(QWdt *wdgt : QWL{ui->copymenu, ui->installmenu}) wdgt->setEnabled(true);

                    return tr("Live image");
                }

                return nullptr;
            }();
    }
    else
    {
        if(! rnmenbl && ui->pointrename->isEnabled()) ui->pointrename->setDisabled(true);

        if(ppipe == 1)
        {
            if(ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setDisabled(true);

            if(ui->storagedirbutton->isVisible())
            {
                ui->storagedirbutton->hide();
                ui->storagedir->resize(ss(236), ui->storagedir->height());
                ui->pointdelete->setEnabled(true);
            }

            if(ui->pointpipe11->isChecked() || ui->pointpipe12->isChecked() || ui->pointpipe13->isChecked() || ui->pointpipe14->isChecked() || ui->pointpipe15->isChecked())
            {
                if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
            }
            else if(! ui->point15->isEnabled() && ! ui->pointhighlight->isEnabled())
                ui->pointhighlight->setEnabled(true);

            if(! ui->copymenu->isEnabled())
                for(QWdt *wdgt : QWL{ui->copymenu, ui->installmenu}) wdgt->setEnabled(true);

            if(! sislive && ! ui->restoremenu->isEnabled()) ui->restoremenu->setEnabled(true);
            if(ui->livecreatemenu->isEnabled()) ui->livecreatemenu->setDisabled(true);
            if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);
        }
        else
        {
            if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);
            for(QWdt *wdgt : QWL{ui->copymenu, ui->installmenu, ui->repairmenu}) wdgt->setDisabled(true);
            if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        }
    }
}

void systemback::on_pointpipe2_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe3_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe4_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe5_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe6_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe7_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe8_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe9_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe10_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe11_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe12_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe13_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe14_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe15_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_livedevicesrefresh_clicked()
{
    busy();
    if(! ui->livecreatecover->isVisibleTo(ui->livecreatepanel)) ui->livecreatecover->show();
    if(ui->livedevices->rowCount() > 0) ui->livedevices->clearContents();
    QSL dlst;
    sb::readlvdevs(dlst);
    schar sn(-1);

    for(cQStr &cdts : dlst)
    {
        ++sn;
        ui->livedevices->setRowCount(sn + 1);
        QSL dts(cdts.split('\n'));
        QTblWI *dev(new QTblWI(dts.at(0)));
        ui->livedevices->setItem(sn, 0, dev);
        QTblWI *size(new QTblWI);
        ullong bsize(dts.at(2).toULongLong());
        size->setText(bsize < 1073741824 ? QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB" : bsize < 1073741824000 ? QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->livedevices->setItem(sn, 1, size);
        QTblWI *name(new QTblWI(dts.at(1)));
        name->setToolTip(dts.at(1));
        ui->livedevices->setItem(sn, 2, name);
        QTblWI *format(new QTblWI("-"));
        for(QTblWI *wdi : QList<QTblWI *>{name, format}) wdi->setTextAlignment(Qt::AlignCenter);
        ui->livedevices->setItem(sn, 3, format);
    }

    for(uchar a(0) ; a < 4 ; ++a) ui->livedevices->resizeColumnToContents(a);
    if(ui->livedevices->columnWidth(0) + ui->livedevices->columnWidth(1) + ui->livedevices->columnWidth(2) + ui->livedevices->columnWidth(3) > ui->livedevices->contentsRect().width()) ui->livedevices->setColumnWidth(2, ui->livedevices->contentsRect().width() - ui->livedevices->columnWidth(0) - ui->livedevices->columnWidth(1) - ui->livedevices->columnWidth(3));
    if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);
    if(ui->livecreatepanel->isVisible() && ! ui->livecreateback->hasFocus()) ui->livecreateback->setFocus();
    ui->livecreatecover->hide();
    busy(false);
}

void systemback::ilstupdt(cQStr &dir)
{
    if(dir.isEmpty())
    {
        busy();

        if(! ui->excludecover->isVisibleTo(ui->excludepanel))
        {
            ui->excludecover->show();
            if(ui->itemslist->topLevelItemCount() > 0) ui->itemslist->clear();
            if(ui->additem->isEnabled()) ui->additem->setDisabled(true);
        }

        ilstupdt("/root");
        QFile file("/etc/passwd");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ilstupdt("/home/" % usr);
            }

        ui->itemslist->sortItems(0, Qt::AscendingOrder);
        if(ui->excludepanel->isVisible() && ! ui->excludeback->hasFocus()) ui->excludeback->setFocus();
        ui->excludecover->hide();
        busy(false);
    }
    else
        for(cQStr &item : QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
            if((ui->pointexclude->isChecked() ? item.startsWith('.') && ! sb::like(item, {"_.gvfs_", "_.Xauthority_", "_.ICEauthority_"}) : ! item.startsWith('.')) && ui->excludedlist->findItems(item, Qt::MatchExactly).isEmpty())
            {
                if(ui->itemslist->findItems(item, Qt::MatchExactly).isEmpty())
                {
                    QTrWI *twi(new QTrWI);
                    twi->setText(0, item);

                    if(sb::access(dir % '/' % item) && sb::stype(dir % '/' % item) == sb::Isdir)
                    {
                        twi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                        ui->itemslist->addTopLevelItem(twi);
                        QSL sdlst(QDir(dir % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                        for(cQStr &sitem : sdlst)
                            if(ui->excludedlist->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                            {
                                QTrWI *ctwi(new QTrWI);
                                ctwi->setText(0, sitem);
                                twi->addChild(ctwi);
                            }
                    }
                    else
                        ui->itemslist->addTopLevelItem(twi);
                }
                else if(sb::access(dir % '/' % item) && sb::stype(dir % '/' % item) == sb::Isdir)
                {
                    QTrWI *ctwi(ui->itemslist->findItems(item, Qt::MatchExactly).at(0));
                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(":pictures/dir.png"));
                    QSL sdlst(QDir(dir % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)), itmlst;
                    for(ushort a(0) ; a < ctwi->childCount() ; ++a) itmlst.append(ctwi->child(a)->text(0));

                    for(cQStr &sitem : sdlst)
                    {
                        if(ui->excludedlist->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                        {
                            for(cQStr &citem : itmlst)
                                if(citem == sitem) goto next;

                            QTrWI *sctwi(new QTrWI);
                            sctwi->setText(0, sitem);
                            ctwi->addChild(sctwi);
                        }

                    next:;
                    }
                }
            }
}

void systemback::on_pointexclude_clicked()
{
    busy();
    if(! ui->excludecover->isVisibleTo(ui->excludepanel)) ui->excludecover->show();
    if(ui->itemslist->topLevelItemCount() > 0) ui->itemslist->clear();
    if(ui->excludedlist->count() > 0) ui->excludedlist->clear();

    if(ui->additem->isEnabled())
        ui->additem->setDisabled(true);
    else if(ui->removeitem->isEnabled())
        ui->removeitem->setDisabled(true);

    {
        QFile file("/etc/systemback.excludes");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QBA cline(file.readLine().trimmed());

                if(cline.startsWith('.'))
                {
                    if(ui->pointexclude->isChecked()) ui->excludedlist->addItem(cline);
                }
                else if(ui->liveexclude->isChecked())
                    ui->excludedlist->addItem(cline);
            }
    }

    ilstupdt();
    busy(false);
}

void systemback::on_liveexclude_clicked()
{
    on_pointexclude_clicked();
}

void systemback::on_dialogok_clicked()
{
    if(ui->dialogok->text() == "OK")
    {
        if(dialog == 308)
            dialogopen(ui->fullrestore->isChecked() ? 205 : 204);
        else if(dialog == 304)
        {
            statustart();

            for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                if(item.startsWith(".S00_"))
                {
                    pset(12);

                    if(sb::remove(sb::sdir[1] % '/' % item))
                    {
                        emptycache();

                        if(sstart)
                        {
                            sb::crtfile(sb::sdir[1] % "/.sbschedule");
                            sb::ThrdKill = true;
                            close();
                        }
                        else
                        {
                            ui->statuspanel->hide();
                            ui->mainpanel->show();
                            ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
                            windowmove(ss(698), ss(465));
                        }
                    }
                    else
                    {
                        if(sstart) sb::crtfile(sb::sdir[1] % "/.sbschedule");

                        if(intrrpt)
                            intrrpt = false;
                        else
                            dialogopen(328);
                    }

                    return;
                }

            if(sstart)
            {
                sb::crtfile(sb::sdir[1] % "/.sbschedule");
                sb::ThrdKill = true;
                close();
            }
            else
                on_dialogcancel_clicked();
        }
        else if(! utimer.isActive() || sstart)
            close();
        else if(sb::like(dialog, {207, 210, 302, 306, 310, 311, 312, 313, 315, 316, 319, 320, 321, 322, 323, 324, 325, 326, 327, 329, 330, 331, 332, 333, 334, 335, 336, 337}))
        {
            ui->dialogpanel->hide();
            ui->mainpanel->show();

            if(ui->copypanel->isVisible())
            {
                ui->copyback->setFocus();
                short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
                if(nwidth > ss(698)) windowmove(nwidth < ss(850) ? nwidth : ss(850), ss(465), false);
            }
            else
            {
                if(ui->sbpanel->isVisible())
                    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
                else if(ui->livecreatepanel->isVisible())
                    ui->livecreateback->setFocus();

                windowmove(ss(698), ss(465));
            }

            setwontop(false);
        }
        else if(dialog == 209)
        {
            if(sislive)
                close();
            else
                on_dialogcancel_clicked();
        }
        else
            on_dialogcancel_clicked();
    }
    else if(ui->dialogok->text() == tr("Start"))
        switch(dialog) {
        case 100:
        case 103 ... 104:
        case 107:
            return restore();
        case 101 ... 102:
        case 109:
            return repair();
        case 105 ... 106:
            return systemcopy();
        case 108:
            livewrite();
        }
    else if(ui->dialogok->text() == tr("Reboot"))
    {
        sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", nullptr, sb::Bckgrnd);
        close();
    }
    else if(ui->dialogok->text() == tr("X restart"))
    {
        DIR *dir(opendir("/proc"));
        dirent *ent;
        QSL dd{"_._", "_.._"};

        while((ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! sb::like(iname, dd) && ent->d_type == DT_DIR && sb::isnum(iname) && sb::islink("/proc/" % iname % "/exe") && QFile::readLink("/proc/" % iname % "/exe").endsWith("/Xorg"))
            {
                closedir(dir);
                kill(iname.toInt(), SIGTERM);
            }
        }

        close();
    }
}

void systemback::on_pointhighlight_clicked()
{
    busy();
    QFile::rename(sb::sdir[1] % '/' % cpoint % '_' % pname, sb::sdir[1] % "/H05_" % pname);
    pntupgrade();
    busy(false);
}

void systemback::on_pointrename_clicked()
{
    busy();
    if(dialog == 302) dialog = 0;
    schar num(-1);

    for(QCB *ckbx : ui->sbpanel->findChildren<QCB *>())
    {
        ++num;

        if(ckbx->isChecked())
        {
            QLE *ldt(getpoint(num));

            if(ldt->text() != sb::pnames[num])
            {
                QStr ppath([num]() -> QStr {
                        switch(num) {
                        case 9:
                            return "/S10_";
                        case 10 ... 14:
                            return "/H0" % QStr::number(num - 9) % '_';
                        default:
                            return "/S0" % QStr::number(num + 1) % '_';
                        }
                    }());

                if(QFile::rename(sb::sdir[1] % ppath % sb::pnames[num], sb::sdir[1] % ppath % ldt->text()))
                    ckbx->click();
                else if(dialog != 302)
                    dialog = 302;
            }
        }
    }

    pntupgrade();
    busy(false);
    if(dialog == 302) dialogopen();
}

void systemback::on_autorestoreoptions_clicked(bool chckd)
{
    for(QWdt *wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setDisabled(chckd);
}

void systemback::on_autorepairoptions_clicked(bool chckd)
{
    if(chckd)
    {
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(ui->grubreinstallrepair->isEnabled())
            for(QWdt *wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setDisabled(true);
    }
    else
    {
        if(! ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setEnabled(true);

        if(! ui->grubreinstallrepair->isEnabled())
            for(QWdt *wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setEnabled(true);
    }
}

void systemback::on_storagedirbutton_clicked()
{
    for(QWdt *wdgt : QWL{ui->sbpanel, ui->scalingbutton}) wdgt->hide();
    ui->choosepanel->show();
    ui->function1->setText(tr("Storage directory"));
    ui->dirchooseok->setFocus();
    windowmove(ss(642), ss(481), false);
    setMinimumSize(ss(642), ss(481));
    { schar snum(qApp->desktop()->screenNumber(this));
    setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60)); }
    on_dirrefresh_clicked();
}

void systemback::on_liveworkdirbutton_clicked()
{
    for(QWdt *wdgt : QWL{ui->livecreatepanel, ui->scalingbutton}) wdgt->hide();
    ui->choosepanel->show();
    ui->function1->setText(tr("Working directory"));
    ui->dirchooseok->setFocus();
    windowmove(ss(642), ss(481), false);
    setMinimumSize(ss(642), ss(481));
    { schar snum(qApp->desktop()->screenNumber(this));
    setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60)); }
    on_dirrefresh_clicked();
}

void systemback::on_dirrefresh_clicked()
{
    busy();
    if(ui->dirchoose->topLevelItemCount() > 0) ui->dirchoose->clear();
    QStr pwdrs(sb::fload("/etc/passwd"));
    QSL excl{"bin", "boot", "cdrom", "dev", "etc", "lib", "lib32", "lib64", "opt", "proc", "root", "run", "sbin", "selinux", "srv", "sys", "tmp", "usr", "var"};

    for(cQStr &item : QDir("/").entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
    {
        QTrWI *twi(new QTrWI);
        twi->setText(0, item);
        QStr cpath(QDir('/' % item).canonicalPath());

        if(excl.contains(item) || excl.contains(sb::right(cpath, -1)) || pwdrs.contains(':' % cpath % ':') || ! sb::islnxfs('/' % item))
        {
            twi->setTextColor(0, Qt::red);
            twi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
            ui->dirchoose->addTopLevelItem(twi);
        }
        else
        {
            if(ui->function1->text() == tr("Storage directory") && sb::isfile('/' % item % "/Systemback/.sbschedule"))
            {
                twi->setTextColor(0, Qt::green);
                twi->setIcon(0, QIcon(QPixmap(":pictures/isdir.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
            }

            ui->dirchoose->addTopLevelItem(twi);

            if(item == "home")
            {
                ui->dirchoose->setCurrentItem(twi);
                twi->setSelected(true);
            }

            for(cQStr &sitem : QDir('/' % item).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
            {
                QTrWI *ctwi(new QTrWI);
                ctwi->setText(0, sitem);

                if(excl.contains(sb::right((cpath = QDir('/' % item % '/' % sitem).canonicalPath()), -1)) || pwdrs.contains(':' % cpath % ':') || (item == "home" && pwdrs.contains(":/home/" % sitem % ":")))
                {
                    ctwi->setTextColor(0, Qt::red);
                    ctwi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                }

                twi->addChild(ctwi);
            }
        }
    }

    if(! ui->dirchoose->currentItem() && ui->dirpath->text() != "/")
    {
        ui->dirpath->setText("/");

        if(ui->dirpath->styleSheet().isEmpty())
        {
            ui->dirpath->setStyleSheet("color: red");
            ui->dirchooseok->setDisabled(true);
            fontcheck(Dpath);
        }

        ui->dirchoosecancel->setFocus();
    }

    busy(false);
}

void systemback::on_dirchoose_currentItemChanged(QTrWI *crrnt)
{
    if(crrnt)
    {
        cQTrWI *twi(crrnt);
        QStr path('/' % crrnt->text(0));
        while(twi->parent()) path.prepend('/' % (twi = twi->parent())->text(0));

        if(sb::isdir(path))
        {
            ui->dirpath->setText(path);

            if(crrnt->textColor(0) == Qt::red)
            {
                if(ui->dirpath->styleSheet().isEmpty())
                {
                    ui->dirpath->setStyleSheet("color: red");
                    ui->dirchooseok->setDisabled(true);
                    fontcheck(Dpath);
                }
            }
            else if(! ui->dirpath->styleSheet().isEmpty())
            {
                ui->dirpath->setStyleSheet(nullptr);
                ui->dirchooseok->setEnabled(true);
                fontcheck(Dpath);
            }
        }
        else
        {
            crrnt->setDisabled(true);

            if(crrnt->isSelected())
            {
                crrnt->setSelected(false);
                ui->dirchoosecancel->setFocus();

                if(ui->dirpath->text() != "/")
                {
                    ui->dirpath->setText("/");

                    if(ui->dirpath->styleSheet().isEmpty())
                    {
                        ui->dirpath->setStyleSheet("color: red");
                        ui->dirchooseok->setDisabled(true);
                        fontcheck(Dpath);
                    }
                }
            }
        }
    }
}

void systemback::on_dirchoose_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent)
    {
        item->setBackgroundColor(0, Qt::transparent);
        busy();
        cQTrWI *twi(item);
        QStr path('/' % twi->text(0));
        while(twi->parent()) path.prepend('/' % (twi = twi->parent())->text(0));
        QStr pwdrs(sb::fload("/etc/passwd"));

        if(sb::isdir(path))
            for(ushort a(0) ; a < item->childCount() ; ++a)
            {
                QTrWI *ctwi(item->child(a));

                if(ctwi->textColor(0) != Qt::red)
                {
                    QStr iname(ctwi->text(0));

                    if(! sb::isdir(path % '/' % iname))
                        ctwi->setDisabled(true);
                    else if(iname == "Systemback" || pwdrs.contains(':' % QDir(path % '/' % iname).canonicalPath() % ':') || ! sb::islnxfs(path % '/' % iname))
                    {
                        ctwi->setTextColor(0, Qt::red);
                        ctwi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    }
                    else
                    {
                        if(ui->function1->text() == tr("Storage directory") && sb::isfile(path % '/' % iname % '/' % "/Systemback/.sbschedule"))
                        {
                            ctwi->setTextColor(0, Qt::green);
                            ctwi->setIcon(0, QIcon(QPixmap(":pictures/isdir.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                        }

                        for(cQStr &cdir : QDir(path % '/' % iname).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                        {
                            QTrWI *sctwi(new QTrWI);
                            sctwi->setText(0, cdir);
                            ctwi->addChild(sctwi);
                        }
                    }
                }
            }
        else
        {
            item->setDisabled(true);

            if(item->isSelected())
            {
                item->setSelected(false);
                ui->dirchoosecancel->setFocus();

                if(ui->dirpath->text() != "/")
                {
                    ui->dirpath->setText("/");

                    if(ui->dirpath->styleSheet().isEmpty())
                    {
                        ui->dirpath->setStyleSheet("color: red");
                        ui->dirchooseok->setDisabled(true);
                        fontcheck(Dpath);
                    }
                }
            }
        }

        busy(false);
    }
}

void systemback::on_dirchoosecancel_clicked()
{
    ui->choosepanel->hide();
    ui->scalingbutton->show();

    if(ui->function1->text() == tr("Storage directory"))
    {
        ui->sbpanel->show();
        ui->function1->setText("Systemback");
        ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
    }
    else
    {
        ui->livecreatepanel->show();
        ui->function1->setText(tr("Live system create"));
        ui->livecreateback->setFocus();
    }

    windowmove(ss(698), ss(465));
    ui->dirchoose->clear();
}

void systemback::on_dirchooseok_clicked()
{
    if(sb::isdir(ui->dirpath->text()))
    {
        if(ui->function1->text() == tr("Storage directory"))
        {
            if(sb::sdir[0] != ui->dirpath->text())
            {
                QSL dlst(QDir(sb::sdir[1]).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                if(dlst.count() == 0)
                    QDir().rmdir(sb::sdir[1]);
                else if(dlst.count() == 1 && sb::isfile(sb::sdir[1] % "/.sbschedule"))
                    sb::remove(sb::sdir[1]);

                sb::sdir[0] = ui->dirpath->text(), sb::sdir[1] = sb::sdir[0] % "/Systemback", sb::ismpnt = ! sb::issmfs(chr(sb::sdir[0]), sb::sdir[0].count('/') == 1 ? "/" : chr(sb::left(sb::sdir[0], sb::rinstr(sb::sdir[0], "/") - 1)));
                if(! cfgupdt) cfgupdt = true;
                ui->storagedir->setText(sb::sdir[0]);
                ui->storagedir->setToolTip(sb::sdir[0]);
                ui->storagedir->setCursorPosition(0);
                pntupgrade();
            }

            if(! sb::isdir(sb::sdir[1]) && ! QDir().mkdir(sb::sdir[1]))
            {
                QFile::rename(sb::sdir[1], sb::sdir[1] % '_' % sb::rndstr());
                QDir().mkdir(sb::sdir[1]);
            }

            if(! sb::isfile(sb::sdir[1] % "/.sbschedule")) sb::crtfile(sb::sdir[1] % "/.sbschedule");
            ui->choosepanel->hide();
            ui->sbpanel->show();
            ui->function1->setText("Systemback");
            ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
            repaint();
        }
        else
        {
            ui->choosepanel->hide();
            ui->livecreatepanel->show();
            ui->function1->setText(tr("Live system create"));
            ui->livecreateback->setFocus();
            repaint();

            if(sb::sdir[2] != ui->dirpath->text())
            {
                sb::sdir[2] = ui->dirpath->text();
                if(! cfgupdt) cfgupdt = true;
                ui->liveworkdir->setText(sb::sdir[2]);
                ui->liveworkdir->setToolTip(sb::sdir[2]);
                ui->liveworkdir->setCursorPosition(0);
                on_livecreatemenu_clicked();
            }
        }

        ui->scalingbutton->show();
        windowmove(ss(698), ss(465));
        ui->dirchoose->clear();
    }
    else
    {
        delete ui->dirchoose->currentItem();
        ui->dirchoose->setCurrentItem(nullptr);
        ui->dirchoosecancel->setFocus();
        ui->dirpath->setText("/");
        ui->dirpath->setStyleSheet("color: red");
        ui->dirchooseok->setDisabled(true);
        fontcheck(Dpath);
    }
}

void systemback::on_fullrestore_clicked()
{
    if(ui->keepfiles->isEnabled())
    {
        for(QWdt *wdgt : QWL{ui->keepfiles, ui->includeusers}) wdgt->setDisabled(true);
        ui->autorestoreoptions->setEnabled(true);

        if(! ui->autorestoreoptions->isChecked())
            for(QWdt *wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setEnabled(true);
    }

    if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
}

void systemback::on_systemrestore_clicked()
{
    on_fullrestore_clicked();
}

void systemback::on_configurationfilesrestore_clicked()
{
    if(! ui->keepfiles->isEnabled())
    {
        for(QWdt *wdgt : QWL{ui->keepfiles, ui->includeusers}) wdgt->setEnabled(true);
        ui->autorestoreoptions->setDisabled(true);

        if(! ui->autorestoreoptions->isChecked())
            for(QWdt *wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setDisabled(true);
    }

    on_includeusers_currentIndexChanged(ui->includeusers->currentText());
}

void systemback::on_includeusers_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty())
    {
        ui->includeusers->resize(fontMetrics().width(arg1) + ss(30), ui->includeusers->height());

        if(ui->includeusers->currentIndex() < 2 || (ui->includeusers->currentIndex() > 1 && sb::isdir(sb::sdir[1] % '/' % cpoint % '_' % pname % "/home/" % arg1)))
        {
            if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
        }
        else if(ui->restorenext->isEnabled())
            ui->restorenext->setDisabled(true);
    }
}

void systemback::on_restorenext_clicked()
{
    dialogopen(ui->fullrestore->isChecked() ? 107 : ui->systemrestore->isChecked() ? 100 : ui->keepfiles->isChecked() ? 104 : 103);
}

void systemback::on_livelist_currentItemChanged(QLWI *crrnt)
{
    if(crrnt)
    {
        if(sb::isfile(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".sblive"))
        {
            if(! ui->livedelete->isEnabled()) ui->livedelete->setEnabled(true);
            ullong isize(sb::fsize(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".sblive"));

            if(isize > 0 && isize < 4294967295 && isize * 2 + 104857600 < sb::dfree(sb::sdir[2]) && ! sb::exist(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".iso"))
            {
                if(! ui->liveconvert->isEnabled()) ui->liveconvert->setEnabled(true);
            }
            else if(ui->liveconvert->isEnabled())
                ui->liveconvert->setDisabled(true);

            if(ui->livedevices->currentItem() && isize > 0)
            {
                if(! ui->livewritestart->isEnabled()) ui->livewritestart->setEnabled(true);
            }
            else if(ui->livewritestart->isEnabled())
                ui->livewritestart->setDisabled(true);
        }
        else
        {
            delete crrnt;
            ui->livelist->setCurrentItem(nullptr);

            for(QWdt *wdgt : QWL{ui->livedelete, ui->liveconvert, ui->livewritestart})
                if(wdgt->isEnabled()) wdgt->setDisabled(true);

            ui->livecreateback->setFocus();
        }
    }
}

void systemback::on_livedelete_clicked()
{
    busy();
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));
    for(cQStr &ext : {".sblive", ".iso"}) sb::remove(path % ext);
    on_livecreatemenu_clicked();
    busy(false);
}

void systemback::on_livedevices_currentItemChanged(QTblWI *crrnt, QTblWI *prvs)
{
    if(crrnt && (! prvs || crrnt->row() != prvs->row()))
    {
        ui->livedevices->item(crrnt->row(), 3)->setText("x");
        if(prvs) ui->livedevices->item(prvs->row(), 3)->setText("-");
        if(ui->livelist->currentItem() && ! ui->livewritestart->isEnabled()) ui->livewritestart->setEnabled(true);
    }
}

void systemback::rmntcheck()
{
    auto grnst([this](bool enable = true) {
            if(enable)
            {
                if(ui->grubreinstallrepairdisable->isVisibleTo(ui->repairpanel))
                {
                    ui->grubreinstallrepairdisable->hide();
                    ui->grubreinstallrepair->show();
                }
            }
            else if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
            {
                ui->grubreinstallrepair->hide();
                ui->grubreinstallrepairdisable->show();
            }
        });

    if(sb::issmfs("/", "/mnt"))
    {
        grnst(false);
        if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
    }
    else if(! ui->grubrepair->isChecked())
    {
        grnst(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && [this] {
                switch(ppipe) {
                case 0:
                    if(sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) return true;
                    break;
                case 1:
                    if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")) return true;
                }

                return false;
            }());

        if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
    }
    else if(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && sb::execsrch("update-grub2", "/mnt") && sb::isfile("/mnt/var/lib/dpkg/info/grub-" % grub.name % ".list"))
    {
        grnst();
        if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
    }
    else
    {
        grnst(false);
        if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
    }
 }

void systemback::on_systemrepair_clicked()
{
    if(ui->grubrepair->isChecked())
    {
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(ui->autorepairoptions->isEnabled())
        {
            ui->autorepairoptions->setDisabled(true);
            for(QWdt *wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setEnabled(true);
        }
    }
    else
    {
        if(! ui->autorepairoptions->isEnabled())
        {
            ui->autorepairoptions->setEnabled(true);
            on_autorepairoptions_clicked(ui->autorepairoptions->isChecked());
        }

        if(ui->grubreinstallrepair->findText(tr("Disabled")) == -1) ui->grubreinstallrepair->addItem(tr("Disabled"));
    }

    rmntcheck();
}

void systemback::on_fullrepair_clicked()
{
    on_systemrepair_clicked();
}

void systemback::on_grubrepair_clicked()
{
    on_systemrepair_clicked();
    if(ui->grubreinstallrepair->currentText() == tr("Disabled")) ui->grubreinstallrepair->setCurrentIndex(0);
    ui->grubreinstallrepair->removeItem(ui->grubreinstallrepair->findText(tr("Disabled")));
}

void systemback::on_repairnext_clicked()
{
    dialogopen(ui->systemrepair->isChecked() ? 101 : ui->fullrepair->isChecked() ? 102 : 109);
}

void systemback::on_skipfstabrestore_clicked(bool chckd)
{
    if(chckd && ! sb::isfile("/etc/fstab")) ui->skipfstabrestore->setChecked(false);
}

void systemback::on_skipfstabrepair_clicked(bool chckd)
{
    if(chckd && ! sb::isfile("/mnt/etc/fstab")) ui->skipfstabrepair->setChecked(false);
}

void systemback::on_installnext_clicked()
{
    if(! grub.isEFI || ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(ppipe == 0 ? sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisibleTo(ui->copypanel))
            {
                ui->grubinstallcopydisable->hide();
                ui->grubinstallcopy->show();
            }
        }
        else if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
        {
            ui->grubinstallcopy->hide();
            ui->grubinstallcopydisable->show();
        }
    }

    if(ppipe == 1)
    {
        if(ui->usersettingscopy->isTristate())
        {
            ui->usersettingscopy->setTristate(false);
            if(! ui->usersettingscopy->isChecked()) ui->usersettingscopy->setChecked(true);
            if(ui->usersettingscopy->text() == tr("Transfer user configuration and data files")) ui->usersettingscopy->setText(tr("Transfer user configuration files"));
            ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration files")) + ss(28), ui->usersettingscopy->height());
        }
    }
    else if(! ui->usersettingscopy->isTristate())
    {
        ui->usersettingscopy->setTristate(true);
        ui->usersettingscopy->setCheckState(Qt::PartiallyChecked);
        ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration and data files")) + ss(28), ui->usersettingscopy->height());
    }

    if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
    {
        ui->userdatafilescopy->hide();
        ui->usersettingscopy->show();
    }

    ui->installpanel->hide();
    ui->copypanel->show();
    ui->copyback->setFocus();
    short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
    if(nwidth > ss(698)) windowmove(nwidth < ss(850) ? nwidth : ss(850), ss(465), false);
    setMinimumSize(ss(698), ss(465));
    { schar snum(qApp->desktop()->screenNumber(this));
    setMaximumSize(qApp->desktop()->availableGeometry(snum).width() - ss(60), qApp->desktop()->availableGeometry(snum).height() - ss(60)); }
    if(ui->mountpoint->currentText().startsWith("/home/")) ui->mountpoint->setCurrentText(nullptr);

    if(ui->partitionsettings->currentItem())
    {
        if(sb::mcheck("/.sbsystemcopy/") || sb::mcheck("/.sblivesystemwrite/"))
        {
            ui->copyback->setDisabled(true);
            on_partitionrefresh_clicked();
            ui->copyback->setEnabled(true);
        }
        else
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && ui->mountpoint->isEnabled())
            {
                if(ui->mountpoint->currentIndex() > 0) ui->mountpoint->setCurrentIndex(0);
                ui->mountpoint->setDisabled(true);
            }

            if(nohmcpy[1])
            {
                nohmcpy[1] = false;

                for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                    if(ui->partitionsettings->item(a, 3)->text() == "/home" && ! ui->partitionsettings->item(a, 4)->text().isEmpty())
                    {
                        ui->partitionsettings->item(a, 4)->setText(nullptr);
                        return ui->mountpoint->addItem("/home");
                    }
            }
        }
    }
}

void systemback::on_partitionsettings_currentItemChanged(QTblWI *crrnt, QTblWI *prvs)
{
    if(crrnt && (! prvs || crrnt->row() != prvs->row()))
    {
        if(ui->partitionsettingspanel2->isVisible())
            for(ushort a(prvs->row() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
            {
                ui->partitionsettings->item(a, 0)->setBackground(QBrush());
                ui->partitionsettings->item(a, 0)->setForeground(QBrush());
            }

        uchar type(ui->partitionsettings->item(crrnt->row(), 8)->text().toUShort()), pcnt(0);

        switch(type) {
        case sb::MSDOS:
        case sb::GPT:
        case sb::Clear:
        case sb::Extended:
        {
            if(ui->partitionsettingspanel2->isHidden())
            {
                ui->partitionsettingspanel1->isVisible() ? ui->partitionsettingspanel1->hide() : ui->partitionsettingspanel3->hide();
                ui->partitionsettingspanel2->show();
            }

            bool mntd(false), mntcheck(false);
            QStr dev(ui->partitionsettings->item(crrnt->row(), 0)->text());

            for(ushort a(crrnt->row() + 1) ; a < ui->partitionsettings->rowCount() && ((type == sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(sb::left(dev, dev.contains("mmc") ? 12 : 8)) && sb::like(ui->partitionsettings->item(a, 8)->text().toInt(), {sb::Logical, sb::Emptyspace})) || (type != sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(dev))) ; ++a)
            {
                ui->partitionsettings->item(a, 0)->setBackground(QPalette().highlight());
                ui->partitionsettings->item(a, 0)->setForeground(QPalette().highlightedText());

                if(! mntcheck)
                {
                    QStr mpt(ui->partitionsettings->item(a, 3)->text());

                    if(! mpt.isEmpty())
                    {
                        if(! mntd) mntd = true;

                        if(((ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) && sb::sdir[0].startsWith(mpt)) || sb::like(mpt, {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
                            mntcheck = true;
                        else if(sb::isfile("/etc/fstab"))
                        {
                            QFile file("/etc/fstab");

                            if(file.open(QIODevice::ReadOnly))
                                while(! file.atEnd())
                                {
                                    QBA cline(file.readLine().trimmed().replace('\t', ' '));

                                    if(! cline.startsWith('#') && sb::like(cline.replace("\\040", " "), {"* " % mpt % " *", "* " % mpt % "/ *"}))
                                    {
                                        mntcheck = true;
                                        break;
                                    }
                                }
                        }
                    }
                }
            }

            if(mntd)
            {
                if(ui->partitiondelete->isEnabled()) ui->partitiondelete->setDisabled(true);

                if(mntcheck)
                {
                    if(ui->unmount->isEnabled()) ui->unmount->setDisabled(true);
                }
                else if(! ui->unmount->isEnabled())
                    ui->unmount->setEnabled(true);
            }
            else
            {
                if(! ui->partitiondelete->isEnabled()) ui->partitiondelete->setEnabled(true);
                if(ui->unmount->isEnabled()) ui->unmount->setDisabled(true);
            }

            break;
        }
        case sb::Freespace:
        {
            QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().contains("mmc") ? 12 : 8));

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() && pcnt < 4 ; ++a)
                if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                    switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                    case sb::GPT:
                        pcnt = 5;
                        break;
                    case sb::Primary:
                    case sb::Extended:
                        ++pcnt;
                    }
        }
        case sb::Emptyspace:
            if(ui->partitionsettingspanel3->isHidden())
            {
                ui->partitionsettingspanel1->isVisible() ? ui->partitionsettingspanel1->hide() : ui->partitionsettingspanel2->hide();
                ui->partitionsettingspanel3->show();
            }

            if(pcnt == 4)
            {
                if(ui->newpartition->isEnabled()) ui->newpartition->setDisabled(true);
            }
            else if(! ui->newpartition->isEnabled())
                ui->newpartition->setEnabled(true);

            ui->partitionsize->setMaximum((ui->partitionsettings->item(crrnt->row(), 10)->text().toULongLong() * 10 / 1048576 + 5) / 10);
            ui->partitionsize->setValue(ui->partitionsize->maximum());
            break;
        default:
            if(ui->partitionsettingspanel1->isHidden())
            {
                ui->partitionsettingspanel2->isVisible() ? ui->partitionsettingspanel2->hide() : ui->partitionsettingspanel3->hide();
                ui->partitionsettingspanel1->show();
            }

            if(ui->partitionsettings->item(crrnt->row(), 3)->text().isEmpty())
            {
                if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);

                if(ui->unmountdelete->text() == tr("Unmount"))
                {
                    ui->unmountdelete->setText(tr("! Delete !"));
                    ui->unmountdelete->setStyleSheet("QPushButton:enabled {color: red}");
                }

                if(! ui->partitionsettings->item(crrnt->row(), 4)->text().isEmpty() && ui->partitionsettings->item(crrnt->row(), 5)->text() == "btrfs")
                {
                    if(ui->format->isEnabled())
                    {
                        ui->format->setDisabled(true);
                        if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                    }

                    if(ui->filesystem->currentText() != "btrfs") ui->filesystem->setCurrentIndex(ui->filesystem->findText("btrfs"));
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->partitionsettings->item(crrnt->row(), 6)->text() == "-")
                    {
                        if(ui->format->isChecked()) ui->format->setChecked(false);
                    }
                    else if(! ui->format->isChecked())
                        ui->format->setChecked(true);
                }
                else
                {
                    if(! ui->filesystem->isEnabled())
                    {
                        ui->filesystem->setEnabled(true);
                        if(! ui->format->isEnabled()) ui->format->setEnabled(true);
                    }

                    if(! ui->format->isChecked()) ui->format->setChecked(true);
                    if(! ui->unmountdelete->isEnabled()) ui->unmountdelete->setEnabled(true);
                }

                if(ui->mountpoint->currentIndex() == 0)
                {
                    if(! ui->mountpoint->currentText().isEmpty()) ui->mountpoint->setCurrentText(nullptr);
                }
                else
                    ui->mountpoint->setCurrentIndex(0);
            }
            else
            {
                if(ui->format->isEnabled())
                {
                    ui->format->setDisabled(true);
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }

                if(ui->unmountdelete->text() == tr("! Delete !"))
                {
                    ui->unmountdelete->setText(tr("Unmount"));
                    ui->unmountdelete->setStyleSheet(nullptr);
                }

                QStr mpt(ui->partitionsettings->item(crrnt->row(), 3)->text());

                if(grub.isEFI && mpt == "/boot/efi")
                {
                    if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->mountpoint->currentText() != "/boot/efi")
                    {
                        if(ui->mountpoint->currentIndex() > 0)
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                }
                else if(mpt == "SWAP")
                {
                    for(QWdt *wdgt : QWL{ui->mountpoint, ui->unmountdelete})
                        if(! wdgt->isEnabled()) wdgt->setEnabled(true);

                    if(ui->mountpoint->currentText() != "SWAP")
                    {
                        if(ui->mountpoint->currentIndex() > 0)
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                    else if(ui->partitionsettings->item(crrnt->row(), 4)->text() == "SWAP")
                    {
                        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                    }
                    else if(! ui->changepartition->isEnabled())
                        ui->changepartition->setEnabled(true);
                }
                else if(nohmcpy[0] && ui->userdatafilescopy->isVisible() && mpt == "/home")
                {
                    if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->mountpoint->currentText() != "/home")
                    {
                        if(ui->mountpoint->currentIndex() > 0)
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                }
                else
                {
                    if(ui->mountpoint->isEnabled()) ui->mountpoint->setDisabled(true);

                    if(ui->mountpoint->currentIndex() > 0)
                    {
                        ui->mountpoint->setCurrentIndex(0);
                        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                    }
                    else if(! ui->mountpoint->currentText().isEmpty())
                        ui->mountpoint->setCurrentText(nullptr);

                    if([&mpt] {
                            if(sb::sdir[0].startsWith(mpt) || sb::like(mpt, {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
                                return true;
                            else if(sb::isfile("/etc/fstab"))
                            {
                                QFile file("/etc/fstab");

                                if(file.open(QIODevice::ReadOnly))
                                    while(! file.atEnd())
                                    {
                                        QBA cline(file.readLine().trimmed().replace('\t', ' '));
                                        if(! cline.startsWith('#') && sb::like(cline.replace("\\040", " "), {"* " % mpt % " *", "* " % mpt % "/ *"})) return true;
                                    }
                            }

                            return false;
                        }())
                    {
                        if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);
                    }
                    else if(! ui->unmountdelete->isEnabled())
                        ui->unmountdelete->setEnabled(true);
                }

                if(! ui->format->isChecked()) ui->format->setChecked(true);
            }
        }
    }
}

void systemback::on_changepartition_clicked()
{
    busy();
    QStr ompt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text()), mpt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text());

    if(! ompt.isEmpty())
    {
        if(mpt.isEmpty() && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() == "btrfs")
        {
            ui->partitionsettings->setRowCount(ui->partitionsettings->rowCount() + 1);

            for(short a(ui->partitionsettings->rowCount() - 1) ; a > ui->partitionsettings->currentRow() - 1 ; --a)
                for(uchar b(0) ; b < 11 ; ++b)
                {
                    QTblWI *item(ui->partitionsettings->item(a, b));
                    ui->partitionsettings->setItem(a + 1, b, item ? item->clone() : nullptr);
                }
        }
        else if(ompt == "/")
        {
            ui->copynext->setDisabled(true);
            ui->mountpoint->addItem("/");
        }
        else if(grub.isEFI && ompt == "/boot/efi")
        {
            if(ui->grubinstallcopy->isVisible())
            {
                ui->grubinstallcopy->hide();
                ui->grubinstallcopydisable->show();
            }

            ui->mountpoint->addItem("/boot/efi");
            ui->efiwarning->show();
        }
        else if(sb::like(ompt, {"_/home_", "_/boot_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_"}))
            ui->mountpoint->addItem(ompt);
    }

    if(mpt.isEmpty())
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText(ui->mountpoint->currentText());

        if(ui->mountpoint->currentText() == "/boot/efi")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "vfat") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("vfat");

            if(grub.isEFI)
            {
                if(ppipe == 0 ? sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
                {
                    if(ui->grubinstallcopydisable->isVisible())
                    {
                        ui->grubinstallcopydisable->hide();
                        ui->grubinstallcopy->show();
                    }
                }
                else if(ui->grubinstallcopy->isVisible())
                {
                    ui->grubinstallcopy->hide();
                    ui->grubinstallcopydisable->show();
                }

                ui->efiwarning->hide();
            }
        }
        else if(ui->mountpoint->currentText() != "SWAP")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != ui->filesystem->currentText()) ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText(ui->filesystem->currentText());
            if(ui->mountpoint->currentText() == "/") ui->copynext->setEnabled(true);
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "swap")
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("swap");

        if(ui->format->isChecked())
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "x")
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("-");

        if(ui->filesystem->isEnabled() && ui->filesystem->currentText() == "btrfs")
        {
            ui->filesystem->setDisabled(true);
            ui->format->setDisabled(true);
        }
    }
    else if(grub.isEFI && mpt == "/boot/efi")
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/boot/efi");
        ui->efiwarning->hide();

        if(ppipe == 0 ? sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisible())
            {
                ui->grubinstallcopydisable->hide();
                ui->grubinstallcopy->show();
            }
        }
        else if(ui->grubinstallcopy->isVisible())
        {
            ui->grubinstallcopy->hide();
            ui->grubinstallcopydisable->show();
        }
    }
    else if(nohmcpy[0] && mpt == "/home")
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/home");
        ui->userdatafilescopy->setDisabled(true);
        nohmcpy[1] = true;
    }
    else
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("SWAP");

    if(ui->mountpoint->currentIndex() > 0 && ui->mountpoint->currentText() != "SWAP") ui->mountpoint->removeItem(ui->mountpoint->currentIndex());

    if(ui->mountpoint->currentIndex() > 0)
        ui->mountpoint->setCurrentIndex(0);
    else if(! ui->mountpoint->currentText().isEmpty())
        ui->mountpoint->setCurrentText(nullptr);

    ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setToolTip(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text());
    busy(false);
}

void systemback::on_filesystem_currentIndexChanged(cQStr &arg1)
{
    if(! ui->format->isChecked() && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != arg1) ui->format->setChecked(true);
}

void systemback::on_format_clicked(bool chckd)
{
    if(! chckd)
    {
        if(ui->mountpoint->currentText() == "/boot/efi")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != "vfat") ui->format->setChecked(true);
        }
        else if(ui->mountpoint->currentText() == "SWAP")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != "swap") ui->format->setChecked(true);
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != ui->filesystem->currentText())
            ui->format->setChecked(true);
    }
}

void systemback::on_mountpoint_currentTextChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? (icnt = 0) : ++icnt);

    if(ui->mountpoint->isEnabled())
    {
        if(! arg1.isEmpty() && (! sb::like(arg1, {"_/*", "_S_", "_SW_", "_SWA_", "_SWAP_"}) || sb::like(arg1, {"* *", "*//*"})))
            ui->mountpoint->setCurrentText(sb::left(arg1, -1));
        else
        {
            if(sb::like(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text(), {"_/boot/efi_", "_/home_", "_SWAP_"}))
            {
                if(ui->format->isEnabled())
                {
                    ui->format->setDisabled(true);
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }
            }
            else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text().isEmpty() || ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "btrfs")
            {
                if(sb::like(arg1, {"_/boot/efi_", "_SWAP_"}))
                {
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }
                else if(! ui->filesystem->isEnabled())
                    ui->filesystem->setEnabled(true);

                if(! ui->format->isEnabled()) ui->format->setEnabled(true);
                if(! ui->format->isChecked()) ui->format->setChecked(true);
            }
            else if(sb::like(arg1, {"_/boot/efi_", "_SWAP_"}))
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                return;
            }

            if(arg1.isEmpty() || (arg1.length() > 1 && arg1.endsWith('/')) || arg1 == ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text() || (ui->usersettingscopy->isVisible() && arg1.startsWith("/home/")) || (arg1 != "/boot/efi" && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong() < 268435456) || [&] {
                    if((grub.isEFI && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/boot/efi" && arg1 != "/boot/efi") || (nohmcpy[0] && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && arg1 != "/home") || (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "SWAP" && arg1 != "SWAP"))
                        return true;
                    else if(arg1 != "SWAP")
                        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                            if(ui->partitionsettings->item(a, 4)->text() == arg1) return true;

                    return false;
                }())
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
            }
            else if(! sb::like(arg1, {"_/_", "_/home_", "_/boot_", "_/boot/efi_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_", "_SWAP_"}))
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                sb::delay(300);

                if(ccnt == icnt)
                {
                    QTemporaryDir tdir("/tmp/" % QStr(arg1 % '_' % sb::rndstr()).replace('/', '_'));
                    if(tdir.isValid()) ui->changepartition->setEnabled(true);
                }
            }
            else if(! ui->changepartition->isEnabled())
                ui->changepartition->setEnabled(true);
        }
    }
}

void systemback::on_copynext_clicked()
{
    dialogopen(ui->function1->text() == tr("System copy") ? 105 : 106);
}

void systemback::on_repairpartitionrefresh_clicked()
{
    busy();
    ui->repaircover->show();

    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(cline.contains(" /mnt")) sb::umount(cline.split(' ').at(1));
        }
    }

    sb::fssync();
    ui->repairmountpoint->clear();
    ui->repairmountpoint->addItems({nullptr, "/mnt", "/mnt/home", "/mnt/boot"});
    if(grub.isEFI) ui->repairmountpoint->addItem("/mnt/boot/efi");
    ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"});
    ui->repairmountpoint->setCurrentIndex(1);
    rmntcheck();
    on_partitionrefresh2_clicked();
    on_repairmountpoint_currentTextChanged("/mnt");
    ui->repaircover->hide();
    if(ui->repairpanel->isVisible() && ! ui->repairback->hasFocus()) ui->repairback->setFocus();
    busy(false);
}

void systemback::on_repairpartition_currentIndexChanged(cQStr &arg1)
{
    ui->repairpartition->resize(fontMetrics().width(arg1) + ss(30), ui->repairpartition->height());
    ui->repairpartition->move(ui->repairmountpoint->x() - ui->repairpartition->width() - ss(8), ui->repairpartition->y());
}

void systemback::on_repairmountpoint_currentTextChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? (icnt = 0) : ++icnt);

    if(! arg1.isEmpty() && (! sb::like(arg1, {"_/_", "_/m_", "_/mn_", "_/mnt_", "_/mnt/*"}) || sb::like(arg1, {"* *", "*//*"})))
        ui->repairmountpoint->setCurrentText(sb::left(arg1, -1));
    else if(! arg1.startsWith("/mnt") || arg1.endsWith('/') || (arg1.length() > 5 && sb::issmfs("/", "/mnt")) || sb::mcheck(arg1 % '/'))
    {
        if(ui->repairmount->isEnabled()) ui->repairmount->setDisabled(true);
    }
    else if(! sb::like(arg1, {"_/mnt_", "_/mnt/home_", "_/mnt/boot_", "_/mnt/boot/efi_", "_/mnt/usr_", "_/mnt/usr/local_", "_/mnt/var_", "_/mnt/opt_"}))
    {
        if(ui->repairmount->isEnabled()) ui->repairmount->setDisabled(true);
        sb::delay(300);

        if(ccnt == icnt)
        {
            QTemporaryDir tdir("/tmp/" % QStr(arg1 % '_' % sb::rndstr()).replace('/', '_'));
            if(tdir.isValid()) ui->repairmount->setEnabled(true);
        }
    }
    else if(! ui->repairmount->isEnabled())
        ui->repairmount->setEnabled(true);
}

void systemback::on_repairmount_clicked()
{
    if(! ui->repairmount->text().isEmpty())
    {
        busy();
        ui->repaircover->show();

        {
            QStr path;

            for(cQStr &cpath : sb::right(ui->repairmountpoint->currentText(), -5).split('/'))
                if(! sb::isdir("/mnt/" % path.append('/' % cpath)) && ! QDir().mkdir("/mnt" % path))
                {
                    QFile::rename("/mnt" % path, "/mnt" % path % '_' % sb::rndstr());
                    QDir().mkdir("/mnt" % path);
                }
        }

        if(sb::mount(ui->repairpartition->currentText(), ui->repairmountpoint->currentText()))
        {
            on_partitionrefresh2_clicked();

            if(ui->repairmountpoint->currentIndex() > 0)
            {
                ui->repairmountpoint->removeItem(ui->repairmountpoint->currentIndex());
                if(ui->repairmountpoint->currentIndex() > 0) ui->repairmountpoint->setCurrentIndex(0);
            }
            else if(! ui->repairmountpoint->currentText().isEmpty())
                ui->repairmountpoint->setCurrentText(nullptr);
        }
        else
        {
            ui->repairmount->setText(nullptr);
            ui->repairmount->setIcon(QIcon(":pictures/error.png"));
        }

        ui->repaircover->hide();
        busy(false);

        if(ui->repairmount->text().isEmpty())
        {
            ui->repairmount->setEnabled(true);
            sb::delay(500);
            ui->repairmount->setIcon(QIcon());
            ui->repairmount->setText(tr("Mount"));
        }
    }

    if(! ui->repairback->hasFocus() && ! ui->repairmountpoint->hasFocus()) ui->repairback->setFocus();
}

void systemback::on_livename_textChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? (icnt = 0) : ++icnt);

    if(cpos > -1)
    {
        ui->livename->setCursorPosition(cpos);
        cpos = -1;
    }

    if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if(ctr == '/' || ((ctr < 'a' || ctr > 'z') && (ctr < 'A' || ctr > 'Z') && ! ctr.isDigit() && ! ctr.isPunct())) return true;
            }

            return false;
        }() || arg1.toUtf8().length() > 32 || arg1.toLower().endsWith(".iso"))

        ui->livename->setText(QStr(arg1).replace((cpos = ui->livename->cursorPosition() - 1), 1, nullptr));
    else
    {
        if(ui->livenamepipe->isVisible()) ui->livenamepipe->hide();

        if(arg1 == "auto")
        {
            QFont fnt;
            fnt.setItalic(true);
            ui->livename->setFont(fnt);
            if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
        }
        else
        {
            if(ui->livecreatenew->isEnabled()) ui->livecreatenew->setDisabled(true);

            if(ui->livename->fontInfo().italic())
            {
                QFont fnt;
                ui->livename->setFont(fnt);
            }

            if(! arg1.isEmpty())
            {
                sb::delay(300);

                if(ccnt == icnt)
                {
                    QTemporaryDir tdir("/tmp/" % arg1 % '_' % sb::rndstr());

                    if(tdir.isValid())
                    {
                        if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
                        ui->livenamepipe->show();
                    }
                    else if(ui->livenameerror->isHidden())
                        ui->livenameerror->show();
                }
            }
            else if(ui->livenameerror->isHidden())
                ui->livenameerror->show();
        }
    }
}

void systemback::on_itemslist_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent)
    {
        item->setBackgroundColor(0, Qt::transparent);
        busy();
        cQTrWI *twi(item);
        QStr path('/' % twi->text(0));
        while(twi->parent()) path.prepend('/' % (twi = twi->parent())->text(0));

        auto itmxpnd([&](cQSL &path) {
                for(ushort a(0) ; a < item->childCount() ; ++a)
                {
                    QTrWI *ctwi(item->child(a));
                    QStr iname(ctwi->text(0));

                    if(sb::stype(path.at(0) % path.at(1) % '/' % iname) == sb::Isdir)
                    {
                        if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                        QSL itmlst;
                        itmlst.reserve(ctwi->childCount());
                        for(ushort b(0) ; b < ctwi->childCount() ; ++b) itmlst.append(ctwi->child(b)->text(0));

                        for(cQStr &siname : QDir(path.at(0) % path.at(1) % '/' % iname).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                        {
                            if(ui->excludedlist->findItems(sb::right(path.at(1), -1) % '/' % iname % '/' % siname, Qt::MatchExactly).isEmpty())
                            {
                                for(ushort b(0) ; b < itmlst.count() ; ++b)
                                    if(itmlst.at(b) == siname)
                                    {
                                        itmlst.removeAt(b);
                                        goto next;
                                    }

                                QTrWI *sctwi(new QTrWI);
                                sctwi->setText(0, siname);
                                ctwi->addChild(sctwi);
                            }

                        next:;
                        }
                    }

                    ctwi->sortChildren(0, Qt::AscendingOrder);
                }
            });

        if(sb::stype("/root" % path) == sb::Isdir) itmxpnd({"/root", path});
        QFile file("/etc/passwd");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::stype("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)) % path) == sb::Isdir) itmxpnd({"/home/" % usr, path});
            }

        busy(false);
    }
}

void systemback::on_itemslist_currentItemChanged(QTrWI *crrnt)
{
    if(crrnt && ! ui->additem->isEnabled())
    {
        ui->additem->setEnabled(true);

        if(ui->removeitem->isEnabled())
        {
            ui->excludedlist->setCurrentItem(nullptr);
            ui->removeitem->setDisabled(true);
        }
    }
}

void systemback::on_excludedlist_currentItemChanged(QLWI *crrnt)
{
    if(crrnt && ! ui->removeitem->isEnabled())
    {
        ui->removeitem->setEnabled(true);

        if(ui->additem->isEnabled())
        {
            ui->itemslist->setCurrentItem(nullptr);
            ui->additem->setDisabled(true);
        }
    }
}

void systemback::on_additem_clicked()
{
    busy();
    ui->excludecover->show();
    cQTrWI *twi(ui->itemslist->currentItem());
    QStr path(twi->text(0));
    while(twi->parent()) path.prepend((twi = twi->parent())->text(0) % '/');
    QFile file("/etc/systemback.excludes");

    if(file.open(QIODevice::Append))
    {
        file.write(QStr(path % '\n').toUtf8());
        file.flush();
        ui->excludedlist->addItem(path);
        delete ui->itemslist->currentItem();
        ui->itemslist->setCurrentItem(nullptr);
        ui->additem->setDisabled(true);
        ui->excludeback->setFocus();
    }

    ui->excludecover->hide();
    busy(false);
}

void systemback::on_removeitem_clicked()
{
    busy();
    ui->excludecover->show();
    QFile file("/etc/systemback.excludes");

    if(file.open(QIODevice::ReadOnly))
    {
        QStr excdlst;

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
            if(cline != ui->excludedlist->currentItem()->text()) excdlst.append(cline % '\n');
        }

        file.close();

        if(sb::crtfile("/etc/systemback.excludes", excdlst))
        {
            delete ui->excludedlist->currentItem();
            ui->excludedlist->setCurrentItem(nullptr);
            ui->removeitem->setDisabled(true);
            ilstupdt();
            ui->excludeback->setFocus();
        }
    }

    ui->excludecover->hide();
    busy(false);
}

void systemback::on_fullname_textChanged(cQStr &arg1)
{
    if(cpos > -1)
    {
        ui->fullname->setCursorPosition(cpos);
        cpos = -1;
    }

    if(arg1.isEmpty())
    {
        if(ui->fullnamepipe->isVisible())
            ui->fullnamepipe->hide();
        else if(ui->installnext->isEnabled())
            ui->installnext->setDisabled(true);
    }
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if(ctr == ':' || ctr == ',' || ctr == '=' || ! (ctr.isLetterOrNumber() || ctr.isPrint())) return true;
            }

            return false;
        }() || sb::like(arg1, {"_ *", "*  *", "*ß*"}))

        ui->fullname->setText(QStr(arg1).replace((cpos = ui->fullname->cursorPosition() - 1), 1, nullptr));
    else if(arg1.at(0).isLower())
    {
        cpos = ui->fullname->cursorPosition();
        ui->fullname->setText(arg1.at(0).toUpper() % sb::right(arg1, -1));
    }
    else
    {
        for(cQStr &word : arg1.split(' '))
            if(! word.isEmpty() && word.at(0).isLower())
            {
                cpos = ui->fullname->cursorPosition();
                ui->fullname->setText(QStr(arg1).replace(' ' % word.at(0) % sb::right(word, -1), ' ' % word.at(0).toUpper() % sb::right(word, -1)));
                return;
            }

        if(ui->fullnamepipe->isHidden()) ui->fullnamepipe->show();
    }
}

void systemback::on_fullname_editingFinished()
{
    if(ui->fullname->text().endsWith(' ')) ui->fullname->setText(ui->fullname->text().trimmed());
}

void systemback::on_username_textChanged(cQStr &arg1)
{
    if(cpos > -1)
    {
        ui->username->setCursorPosition(cpos);
        cpos = -1;
    }

    if(arg1.isEmpty())
    {
        if(ui->usernameerror->isVisible())
            ui->usernameerror->hide();
        else if(ui->usernamepipe->isVisible())
        {
            ui->usernamepipe->hide();
            if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
        }
    }
    else if(arg1 == "root")
    {
        if(ui->usernameerror->isHidden())
        {
            if(ui->usernamepipe->isVisible())
            {
                ui->usernamepipe->hide();
                if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
            }

            ui->usernameerror->show();
        }
    }
    else if(arg1 != arg1.toLower())
        ui->username->setText(arg1.toLower());
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if((ctr < 'a' || ctr > 'z') && ctr != '.' && ctr != '_' && ctr != '@' && (a == 0 || (! ctr.isDigit() && ctr != '-' && ctr != '$'))) return true;
            }

            return false;
        }() || (arg1.contains('$') && (arg1.count('$') > 1 || ! arg1.endsWith('$'))))

        ui->username->setText(QStr(arg1).replace((cpos = ui->username->cursorPosition() - 1), 1, nullptr));
    else if(ui->usernamepipe->isHidden())
    {
        if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
        ui->usernamepipe->show();
    }
}

void systemback::on_hostname_textChanged(cQStr &arg1)
{
    if(cpos > -1)
    {
        ui->hostname->setCursorPosition(cpos);
        cpos = -1;
    }

    if(arg1.isEmpty())
    {
        if(ui->hostnameerror ->isVisible())
            ui->hostnameerror->hide();
        else if(ui->hostnamepipe->isVisible())
        {
            ui->hostnamepipe->hide();
            if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
        }
    }
    else if(arg1.length() > 1 && sb::like(arg1, {"*._", "*-_"}) && ! sb::like(arg1, {"*..*", "*--*", "*.-*", "*-.*"}))
    {
        if(ui->hostnameerror->isHidden())
        {
            if(ui->hostnamepipe->isVisible())
            {
                ui->hostnamepipe->hide();
                if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
            }

            ui->hostnameerror->show();
        }
    }
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if((ctr < 'a' || ctr > 'z') && (ctr < 'A' || ctr > 'Z') && ! ctr.isDigit() && (a == 0 || (ctr != '-' && ctr != '.'))) return true;
            }

            return false;
        }() || (arg1.length() > 1 && sb::like(arg1, {"*..*", "*--*", "*.-*", "*-.*"})))

        ui->hostname->setText(QStr(arg1).replace((cpos = ui->hostname->cursorPosition() - 1), 1, nullptr));
    else if(ui->hostnamepipe->isHidden())
    {
        if(ui->hostnameerror->isVisible()) ui->hostnameerror->hide();
        ui->hostnamepipe->show();
    }
}

void systemback::on_password1_textChanged(cQStr &arg1)
{
    if(ui->passwordpipe->isVisible())
    {
        ui->passwordpipe->hide();
        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }

    if(arg1.isEmpty())
    {
        if(! ui->password2->text().isEmpty()) ui->password2->clear();
        if(ui->password2->isEnabled()) ui->password2->setDisabled(true);
        if(ui->passworderror->isVisible()) ui->passworderror->hide();
    }
    else
    {
        if(! ui->password2->isEnabled()) ui->password2->setEnabled(true);

        if(ui->password2->text().isEmpty())
        {
            if(ui->passworderror->isVisible()) ui->passworderror->hide();
        }
        else if(arg1 == ui->password2->text())
        {
            if(ui->passworderror->isVisible()) ui->passworderror->hide();
            ui->passwordpipe->show();
        }
        else if(ui->passworderror->isHidden())
            ui->passworderror->show();
    }
}

void systemback::on_password2_textChanged()
{
    on_password1_textChanged(ui->password1->text());
}

void systemback::on_rootpassword1_textChanged(cQStr &arg1)
{
    if(ui->rootpasswordpipe->isVisible())
    {
        ui->rootpasswordpipe->hide();
        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }

    if(arg1.isEmpty())
    {
        if(! ui->rootpassword2->text().isEmpty()) ui->rootpassword2->clear();
        if(ui->rootpassword2->isEnabled()) ui->rootpassword2->setDisabled(true);
        if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
    }
    else
    {
        if(! ui->rootpassword2->isEnabled()) ui->rootpassword2->setEnabled(true);

        if(ui->rootpassword2->text().isEmpty())
        {
            if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
        }
        else if(arg1 == ui->rootpassword2->text())
        {
            if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
            ui->rootpasswordpipe->show();
        }
        else if(ui->rootpassworderror->isHidden())
            ui->rootpassworderror->show();
    }
}

void systemback::on_rootpassword2_textChanged()
{
    on_rootpassword1_textChanged(ui->rootpassword1->text());
}

void systemback::on_schedulerstate_clicked()
{
    if(ui->schedulerstate->isChecked())
    {
        if(sb::schdle[0] == sb::False)
        {
            sb::schdle[0] = sb::True;
            if(! cfgupdt) cfgupdt = true;
            if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        ui->schedulerstate->setText(tr("Enabled"));
        if(sb::schdle[1] > 0) ui->daydown->setEnabled(true);
        if(sb::schdle[1] < 7) ui->dayup->setEnabled(true);
        if(sb::schdle[2] > 0) ui->hourdown->setEnabled(true);
        if(sb::schdle[2] < 23) ui->hourup->setEnabled(true);
        if(sb::schdle[3] > 0 && (sb::schdle[1] > 0 || sb::schdle[2] > 0 || sb::schdle[3] > 30)) ui->minutedown->setEnabled(true);
        if(sb::schdle[3] < 59) ui->minuteup->setEnabled(true);
        ui->silentmode->setEnabled(true);

        if(sb::schdle[5] == sb::False)
        {
            if(sb::schdle[4] > 10) ui->seconddown->setEnabled(true);
            if(sb::schdle[4] < 99) ui->secondup->setEnabled(true);
            ui->windowposition->setEnabled(true);
        }
    }
    else
    {
        sb::schdle[0] = sb::False;
        if(! cfgupdt) cfgupdt = true;
        ui->schedulerstate->setText(tr("Disabled"));
        ui->silentmode->setDisabled(true);
        if(ui->windowposition->isEnabled()) ui->windowposition->setDisabled(true);

        for(QPB *pbtn : ui->schedulerdayhrminpanel->findChildren<QPB *>())
            if(pbtn->isEnabled()) pbtn->setDisabled(true);

        for(QWdt *wdgt : QWL{ui->secondup, ui->seconddown})
            if(wdgt->isEnabled()) wdgt->setDisabled(true);
    }
}

void systemback::on_silentmode_clicked(bool chckd)
{
    if(! chckd)
    {
        sb::schdle[5] = sb::False;
        if(! cfgupdt) cfgupdt = true;
        if(sb::schdle[4] > 10) ui->seconddown->setEnabled(true);
        if(sb::schdle[4] < 99) ui->secondup->setEnabled(true);
        ui->windowposition->setEnabled(true);
    }
    else if(sb::schdle[5] == sb::False)
    {
        sb::schdle[5] = sb::True;
        if(! cfgupdt) cfgupdt = true;
        ui->windowposition->setDisabled(true);

        for(QWdt *wdgt : QWL{ui->secondup, ui->seconddown})
            if(wdgt->isEnabled()) wdgt->setDisabled(true);
    }
}

void systemback::on_dayup_clicked()
{
    ++sb::schdle[1];
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
    if(! ui->daydown->isEnabled()) ui->daydown->setEnabled(true);
    if(sb::schdle[1] == 7) ui->dayup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3] > 0) ui->minutedown->setEnabled(true);
}

void systemback::on_daydown_clicked()
{
    --sb::schdle[1];
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
    if(! ui->dayup->isEnabled()) ui->dayup->setEnabled(true);

    if(sb::schdle[1] == 0)
    {
        if(sb::schdle[2] == 0)
        {
            if(sb::schdle[3] < 30)
            {
                sb::schdle[3] = 30;
                ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
            }

            if(ui->minutedown->isEnabled() && sb::schdle[3] < 31) ui->minutedown->setDisabled(true);
        }

        ui->daydown->setDisabled(true);
    }
}

void systemback::on_hourup_clicked()
{
    ++sb::schdle[2];
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
    if(! ui->hourdown->isEnabled()) ui->hourdown->setEnabled(true);
    if(sb::schdle[2] == 23) ui->hourup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3] > 0) ui->minutedown->setEnabled(true);
}

void systemback::on_hourdown_clicked()
{
    --sb::schdle[2];
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
    if(! ui->hourup->isEnabled()) ui->hourup->setEnabled(true);

    if(sb::schdle[2] == 0)
    {
        if(sb::schdle[1] == 0)
        {
            if(sb::schdle[3] < 30)
            {
                sb::schdle[3] = 30;
                ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
            }

            if(ui->minutedown->isEnabled() && sb::schdle[3] < 31) ui->minutedown->setDisabled(true);
        }

        ui->hourdown->setDisabled(true);
    }
}

void systemback::on_minuteup_clicked()
{
    sb::schdle[3] = sb::schdle[3] + (sb::schdle[3] == 55 ? 4 : 5);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
    if(! ui->minutedown->isEnabled()) ui->minutedown->setEnabled(true);
    if(sb::schdle[3] == 59) ui->minuteup->setDisabled(true);
}

void systemback::on_minutedown_clicked()
{
    sb::schdle[3] = sb::schdle[3] - (sb::schdle[3] == 59 ? 4 : 5);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
    if(! ui->minuteup->isEnabled()) ui->minuteup->setEnabled(true);
    if(sb::schdle[3] == 0 || (sb::schdle[3] == 30 && sb::like(0, {sb::schdle[1], sb::schdle[2]}, true))) ui->minutedown->setDisabled(true);
}

void systemback::on_secondup_clicked()
{
    sb::schdle[4] = sb::schdle[4] + (sb::schdle[4] == 95 ? 4 : 5);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));
    if(! ui->seconddown->isEnabled()) ui->seconddown->setEnabled(true);
    if(sb::schdle[4] == 99) ui->secondup->setDisabled(true);
}

void systemback::on_seconddown_clicked()
{
    sb::schdle[4] = sb::schdle[4] - (sb::schdle[4] == 99 ? 4 : 5);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));
    if(! ui->secondup->isEnabled()) ui->secondup->setEnabled(true);
    if(sb::schdle[4] == 10) ui->seconddown->setDisabled(true);
}

void systemback::on_windowposition_currentIndexChanged(cQStr &arg1)
{
    ui->windowposition->resize(fontMetrics().width(arg1) + ss(30), ui->windowposition->height());

    if(ui->schedulepanel->isVisible())
    {
        cchar *cval(arg1 == tr("Top left") ? "topleft"
                  : arg1 == tr("Top right") ? "topright"
                  : arg1 == tr("Center") ? "center"
                  : arg1 == tr("Bottom left") ? "bottomleft" : "bottomright");

        if(sb::schdlr[0] != cval)
        {
            sb::schdlr[0] = cval;
            if(! cfgupdt) cfgupdt = true;
        }
    }
}

void systemback::on_userdatainclude_clicked(bool chckd)
{
    if(chckd)
    {
        ullong hfree(sb::dfree("/home"));
        QFile file("/etc/passwd");

        if(hfree > 104857600 && sb::dfree("/root") > 104857600 && file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1))) && sb::dfree("/home/" % usr) != hfree) return ui->userdatainclude->setChecked(false);
            }
        else
            ui->userdatainclude->setChecked(false);
    }
}

void systemback::on_interrupt_clicked()
{
    if(! irblck && ! intrrpt)
    {
        if(! intrptimer)
        {
            pset(13);
            intrrpt = true;
            ui->interrupt->setDisabled(true);
            if(! sb::ExecKill) sb::ExecKill = true;

            if(sb::SBThrd.isRunning())
            {
                sb::ThrdKill = true;
                sb::thrdelay();
                sb::error("\n " % tr("Systemback worker thread is interrupted by the user.") % "\n\n");
            }

            connect((intrptimer = new QTimer), SIGNAL(timeout()), this, SLOT(on_interrupt_clicked()));
            intrptimer->start(100);
        }
        else if(sstart)
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
            close();
        }
        else
        {
            delete intrptimer;
            intrptimer = nullptr;

            for(QCB *ckbx : ui->sbpanel->findChildren<QCB *>())
                if(ckbx->isChecked()) ckbx->setChecked(false);

            on_pointpipe1_clicked();
            ui->statuspanel->hide();

            if(ui->livecreatepanel->isVisibleTo(ui->mainpanel))
                ui->livecreateback->setFocus();
            else if(! ui->sbpanel->isVisibleTo(ui->mainpanel))
            {
                ui->sbpanel->show();
                ui->function1->setText("Systemback");

                if(ui->restorepanel->isVisibleTo(ui->mainpanel))
                    ui->restorepanel->hide();
                else if(ui->copypanel->isVisibleTo(ui->mainpanel))
                    ui->copypanel->hide();
                else if(ui->repairpanel->isVisibleTo(ui->mainpanel))
                    ui->repairpanel->hide();

                ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
            }

            ui->mainpanel->show();
            windowmove(ss(698), ss(465));
        }
    }
}

void systemback::on_livewritestart_clicked()
{
    dialogopen(108);
}

void systemback::on_schedulerlater_clicked()
{
    if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
    close();
}

void systemback::on_scalingup_clicked()
{
    ui->scalingfactor->setText([this] {
            if(ui->scalingfactor->text() == "auto")
            {
                ui->scalingdown->setEnabled(true);
                return "x1";
            }
            else if(ui->scalingfactor->text() == "x1")
                return "x1.5";

            ui->scalingup->setDisabled(true);
            return "x2";
        }());
}

void systemback::on_scalingdown_clicked()
{
    ui->scalingfactor->setText([this] {
            if(ui->scalingfactor->text() == "x2")
            {
                ui->scalingup->setEnabled(true);
                return "x1.5";
            }
            else if(ui->scalingfactor->text() == "x1.5")
                return "x1";

            ui->scalingdown->setDisabled(true);
            return "auto";
        }());
}

void systemback::on_newrestorepoint_clicked()
{
    statustart();

    auto error([this] {
            if(intrrpt)
                intrrpt = false;
            else
            {
                dialogopen([this] {
                        if(sb::dfree(sb::sdir[1]) < 104857600)
                            return 304;
                        else
                        {
                            if(! sb::ThrdDbg.isEmpty()) prntdbgmsg();
                            return 318;
                        }
                    }());

                if(! sstart) pntupgrade();
            }
        });

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun.type != 12) pset(12);
            if(! sb::remove(sb::sdir[1] % '/' % item) || intrrpt) return error();
        }

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3)) ++ppipe;

    if(ppipe > 0)
    {
        uchar dnum(0);

        for(uchar a(9) ; a > 1 ; --a)
            if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3))
            {
                ++dnum;
                pset(14, chr((QStr::number(dnum) % '/' % QStr::number(ppipe))));
                if(! QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || intrrpt) return error();
            }
    }

    pset(15);
    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(sb::sdir[1], ".S00_" % dtime)) return error();

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! QFile::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) return error();

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return error();
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    if(intrrpt) return error();
    emptycache();

    if(sstart)
    {
        sb::ThrdKill = true;
        close();
    }
    else
    {
        pntupgrade();
        ui->statuspanel->hide();
        ui->mainpanel->show();
        ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
        windowmove(ss(698), ss(465));
    }
}

void systemback::on_pointdelete_clicked()
{
    statustart();
    uchar dnum(0);

    for(uchar a : {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 10, 11, 12, 13, 14})
    {
        if(getppipe(a)->isChecked())
        {
            ++dnum;
            pset(16, chr((QStr::number(dnum) % '/' % QStr::number(ppipe))));

            if(! QFile::rename(sb::sdir[1] % [a]() -> QStr {
                    switch(a) {
                    case 9:
                        return "/S10_";
                    case 10 ... 14:
                        return "/H0" % QStr::number(a - 9) % '_';
                    default:
                        return "/S0" % QStr::number(a + 1) % '_';
                    }
                }() % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || intrrpt) return [this] {
                        if(intrrpt)
                            intrrpt = false;
                        else
                        {
                            dialogopen(328);
                            pntupgrade();
                        }
                    }();
        }
    }

    pntupgrade();
    emptycache();
    ui->statuspanel->hide();
    ui->mainpanel->show();
    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
    windowmove(ss(698), ss(465));
}

void systemback::on_livecreatenew_clicked()
{
    statustart();
    pset(17, " 1/3");
    QStr ckernel(ckname()), lvtype(sb::isfile("/usr/share/initramfs-tools/scripts/casper") ? "casper" : "live");

    auto error([this](ushort dlg = 0) {
            if(! intrrpt && dlg != 326)
            {
                if(sb::dfree(sb::sdir[2]) < 104857600 || (sb::isdir("/home/.sbuserdata") && sb::dfree("/home") < 104857600))
                    dlg = 312;
                else if(dlg == 0)
                {
                    if(! sb::ThrdDbg.isEmpty()) prntdbgmsg();
                    dlg = 327;
                }
            }

            for(cQStr &dir : {"/.sblvtmp", "/media/.sblvtmp", "/var/.sblvtmp", "/home/.sbuserdata", "/root/.sbuserdata"})
                if(sb::isdir(dir)) sb::remove(dir);

            if(sb::autoiso == sb::True) on_livecreatemenu_clicked();

            if(intrrpt)
                intrrpt = false;
            else
            {
                if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate")) sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
                dialogopen(dlg);
            }
        });

    if((sb::exist(sb::sdir[2] % "/.sblivesystemcreate") && ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate"))
        || intrrpt || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate") || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/.disk") || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/" % lvtype) || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/syslinux")) return error();

    QStr ifname(ui->livename->text() == "auto" ? "systemback_live_" % QDateTime().currentDateTime().toString("yyyy-MM-dd") : ui->livename->text());
    uchar ncount(0);

    while(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive"))
    {
        ++ncount;
        ncount == 1 ? ifname.append("_1") : ifname = sb::left(ifname, sb::rinstr(ifname, "_")) % QStr::number(ncount);
    }

    if(intrrpt || ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/.disk/info", "Systemback Live (" % ifname % ") - Release " % sb::right(ui->systembackversion->text(), - sb::rinstr(ui->systembackversion->text(), "_")) % '\n') || ! sb::copy("/boot/vmlinuz-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/vmlinuz") || intrrpt) return error();
    QStr fname;

    {
        QFile file("/etc/passwd");
        if(! file.open(QIODevice::ReadOnly)) return error();

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());

            if(cline.startsWith(guname() % ':'))
            {
                QSL uslst(cline.split(':'));
                if(uslst.count() > 4) fname = sb::left(uslst.at(4), sb::instr(uslst.at(4), ",") - 1);
                break;
            }
        }
    }

    if(intrrpt) return error();
    irblck = true;

    if(lvtype == "casper")
    {
        QStr did;

        if(sb::isfile("/etc/lsb-release"))
        {
            QFile file("/etc/lsb-release");
            if(! file.open(QIODevice::ReadOnly)) return error();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith("DISTRIB_ID="))
                {
                    did = sb::right(cline, - sb::instr(cline, "="));
                    break;
                }
            }
        }

        if(did.isEmpty()) did = "Ubuntu";
        QFile file("/etc/hostname");
        if(! file.open(QIODevice::ReadOnly)) return error();
        QStr hname(file.readLine().trimmed());
        file.close();
        if(! sb::crtfile("/etc/casper.conf", "USERNAME=\"" % guname() % "\"\nUSERFULLNAME=\"" % fname % "\"\nHOST=\"" % hname % "\"\nBUILD_SYSTEM=\"" % did % "\"\n\nexport USERNAME USERFULLNAME HOST BUILD_SYSTEM\n") || intrrpt) return error();
        QSL incl{"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"};

        for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
            if(! sb::like(item, incl) && ! QFile::setPermissions("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther)) return error();
    }
    else
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", "#!/bin/sh\nif [ \"$BOOT\" = live ] && [ ! -e /root/etc/fstab ]\nthen touch /root/etc/fstab\nfi\n");
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) return error();
    }

    bool xmntry;

    if((xmntry = sb::isfile("/etc/X11/xorg.conf")))
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", "#!/bin/sh\nif [ -s /root/etc/X11/xorg.conf ] && grep noxconf /proc/cmdline >/dev/null 2>&1\nthen rm /root/etc/X11/xorg.conf\nfi\n");
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) return error();
    }

    uchar rv(sb::exec("update-initramfs -tck" % ckernel));

    if(lvtype == "casper")
    {
        QSL incl{"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"};

        for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
            if(! sb::like(item, incl) && ! QFile::setPermissions("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) return error();
    }
    else if(! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab"))
        return error();

    if(rv > 0) return error(326);
    irblck = false;
    if((xmntry && ! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf")) || ! sb::copy("/boot/initrd.img-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/initrd.gz")) return error();

    if(sb::isfile("/usr/lib/syslinux/isolinux.bin"))
    {
        if(! sb::copy("/usr/lib/syslinux/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") || ! sb::copy("/usr/lib/syslinux/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32")) return error();
    }
    else if(! sb::copy("/usr/lib/ISOLINUX/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") || ! sb::copy("/usr/lib/syslinux/modules/bios/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/libcom32.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libcom32.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/libutil.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libutil.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/ldlinux.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/ldlinux.c32"))
        return error();

    if(! sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/syslinux/splash.png") || ! sb::lvprpr(ui->userdatainclude->isChecked())) return error();
    QStr ide;

    for(cQStr &item : {"/.sblvtmp/cdrom", "/.sblvtmp/dev", "/.sblvtmp/mnt", "/.sblvtmp/proc", "/.sblvtmp/run", "/.sblvtmp/srv", "/.sblvtmp/sys", "/.sblvtmp/tmp", "/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/usr", "/initrd.img", "/initrd.img.old", "/vmlinuz", "/vmlinuz.old"})
        if(sb::exist(item)) ide.append(' ' % item);

    if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata"))
    {
        ide.append(' ' % sb::sdir[2] % "/.sblivesystemcreate/userdata/home");
        if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata/root")) ide.append(' ' % sb::sdir[2] % "/.sblivesystemcreate/userdata/root");
    }
    else
    {
        if(sb::isdir("/home/.sbuserdata")) ide.append(" /home/.sbuserdata/home");
        if(sb::isdir("/root/.sbuserdata")) ide.append(" /root/.sbuserdata/root");
    }

    if(intrrpt) return error();
    pset(18, " 2/3");
    QStr elist;

    for(cQStr &excl : {"/boot/efi/EFI", "/etc/fstab", "/etc/mtab", "/etc/udev/rules.d/70-persistent-cd.rules", "/etc/udev/rules.d/70-persistent-net.rules"})
        if(sb::exist(excl)) elist.append(" -e " % excl);

    for(cQStr &cdir : {"/etc/rc0.d", "/etc/rc1.d", "/etc/rc2.d", "/etc/rc3.d", "/etc/rc4.d", "/etc/rc5.d", "/etc/rc6.d", "/etc/rcS.d"})
        if(sb::isdir(cdir))
            for(cQStr &item : QDir(cdir).entryList(QDir::Files))
                if(item.contains("cryptdisks")) elist.append(" -e " % cdir % '/' % item);

    if(sb::exec("mksquashfs" % ide % ' ' % sb::sdir[2] % "/.sblivesystemcreate/.systemback /media/.sblvtmp/media /var/.sblvtmp/var " % sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs " % (sb::xzcmpr == sb::True ? "-comp xz " : nullptr) % "-info -b 1M -no-duplicates -no-recovery -always-use-fragments" % elist, nullptr, sb::Prgrss) > 0) return error(310);
    pset(19, " 3/3");
    sb::Progress = -1;
    for(cQStr &dir : {"/.sblvtmp", "/media/.sblvtmp", "/var/.sblvtmp"}) sb::remove(dir);

    for(cQStr &dir : {"/home/.sbuserdata", "/root/.sbuserdata"})
        if(sb::isdir(dir)) sb::remove(dir);

    if(intrrpt) return error();
    QStr rpart, grxorg, srxorg, prmtrs;
    if(QFile(sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs").size() > 4294967295) rpart = "root=LABEL=SBROOT ";

    if(sb::isfile("/etc/default/grub"))
    {
        QFile file("/etc/default/grub");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith("GRUB_CMDLINE_LINUX_DEFAULT=") && cline.count('\"') > 1)
                {
                    if(! prmtrs.isEmpty()) prmtrs.clear();
                    QStr pprt;

                    for(cQStr &cprmtr : sb::left(sb::right(cline, - sb::instr(cline, "\"")), -1).split(' '))
                        if(! cprmtr.isEmpty() && ! (pprt.isEmpty() && sb::like(cprmtr, {"_quiet_", "_splash_", "_xforcevesa_"})))
                        {
                            if(cprmtr.contains("\\\""))
                            {
                                if(pprt.isEmpty())
                                    pprt = cprmtr % ' ';
                                else
                                {
                                    prmtrs.append(' ' % pprt.append(cprmtr).replace("\\\"", "\""));
                                    pprt.clear();
                                }
                            }
                            else if(pprt.isEmpty())
                                prmtrs.append(' ' % cprmtr);
                            else
                                pprt.append(cprmtr % ' ');
                        }
                }
            }
    }

    if(xmntry) grxorg = "menuentry \"" % tr("Boot Live without xorg.conf file") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " noxconf quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n", srxorg = "label noxconf\n  menu label " % tr("Boot Live without xorg.conf file") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz noxconf quiet splash" % prmtrs % "\n\n";
#ifdef __amd64__
    if(sb::isfile("/usr/share/systemback/efi-amd64.bootfiles") && (sb::exec("tar -xJf /usr/share/systemback/efi-amd64.bootfiles -C " % sb::sdir[2] % "/.sblivesystemcreate --no-same-owner --no-same-permissions") > 0 || ! sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/boot/grub/splash.png") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/grub.cfg", "if loadfont /boot/grub/font.pf2\nthen\n  set gfxmode=auto\n  insmod efi_gop\n  insmod efi_uga\n  insmod gfxterm\n  terminal_output gfxterm\nfi\n\nset theme=/boot/grub/theme.cfg\n\nmenuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/theme.cfg", "title-color: \"white\"\ntitle-text: \"Systemback Live (" % ifname % ")\"\ntitle-font: \"Sans Regular 16\"\ndesktop-color: \"black\"\ndesktop-image: \"/boot/grub/splash.png\"\nmessage-color: \"white\"\nmessage-bg-color: \"black\"\nterminal-font: \"Sans Regular 12\"\n\n+ boot_menu {\n  top = 150\n  left = 15%\n  width = 75%\n  height = 130\n  item_font = \"Sans Regular 12\"\n  item_color = \"grey\"\n  selected_item_color = \"white\"\n  item_height = 20\n  item_padding = 15\n  item_spacing = 5\n}\n\n+ vbox {\n  top = 100%\n  left = 2%\n  + label {text = \"" % tr("Press 'E' key to edit") % "\" font = \"Sans 10\" color = \"white\" align = \"left\"}\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/loopback.cfg", "menuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz" % rpart % "boot=" % lvtype % " quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n"))) return error();
#endif
    if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", "default vesamenu.c32\nprompt 0\ntimeout 100\n\nmenu title Systemback Live (" % ifname % ")\nmenu tabmsg " % tr("Press TAB key to edit") % "\nmenu background splash.png\n\nlabel live\n  menu label " % tr("Boot Live system") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz quiet splash" % prmtrs % "\n\nlabel safe\n  menu label " % tr("Boot Live in safe graphics mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz xforcevesa nomodeset quiet splash" % prmtrs % "\n\n" % srxorg % "label debug\n  menu label " % tr("Boot Live in debug mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz" % prmtrs % '\n')
        || intrrpt || ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/.systemback")
        || intrrpt || (sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata") && ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/userdata"))
        || intrrpt) return error();

    if(sb::ThrdLng[0] > 0) sb::ThrdLng[0] = 0;
    sb::ThrdStr[0] = sb::sdir[2] % '/' % ifname % ".sblive";
    ui->progressbar->setValue(0);

    if(sb::exec("tar -cf " % sb::sdir[2] % '/' % ifname % ".sblive -C " % sb::sdir[2] % "/.sblivesystemcreate .", nullptr, sb::Prgrss) > 0)
    {
        if(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive")) sb::remove(sb::sdir[2] % '/' % ifname % ".sblive");
        return error(311);
    }

    if(! QFile::setPermissions(sb::sdir[2] % '/' % ifname % ".sblive", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) return error();

    if(sb::autoiso == sb::True)
    {
        ullong isize(sb::fsize(sb::sdir[2] % '/' % ifname % ".sblive"));

        if(isize < 4294967295 && isize + 52428800 < sb::dfree(sb::sdir[2]))
        {
            pset(20, " 4/3+1");
            sb::Progress = -1;
            if(! QFile::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.cfg") || ! QFile::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux", sb::sdir[2] % "/.sblivesystemcreate/isolinux") || intrrpt) return error();
            ui->progressbar->setValue(0);

            if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o " % sb::sdir[2] % '/' % ifname % ".iso " % sb::sdir[2] % "/.sblivesystemcreate", nullptr, sb::Prgrss) > 0)
            {
                if(sb::isfile(sb::sdir[2] % '/' % ifname % ".iso")) sb::remove(sb::sdir[2] % '/' % ifname % ".iso");
                return error(311);
            }

            if(sb::exec("isohybrid " % sb::sdir[2] % '/' % ifname % ".iso") > 0 || ! QFile::setPermissions(sb::sdir[2] % '/' % ifname % ".iso", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther) || intrrpt) return error();
        }
    }

    emptycache();
    sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
    on_livecreatemenu_clicked();
    dialogopen(207);
}

void systemback::on_liveconvert_clicked()
{
    statustart();
    pset(21, " 1/2");
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));

    auto error([&](ushort dlg = 0) {
            if(sb::isdir(sb::sdir[2] % "/.sblivesystemconvert")) sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
            if(sb::isfile(path % ".iso")) sb::remove(path % ".iso");

            if(intrrpt)
                intrrpt = false;
            else
                dialogopen(dlg == 0 ? 334 : dlg);
        });

    if((sb::exist(sb::sdir[2] % "/.sblivesystemconvert") && ! sb::remove(sb::sdir[2] % "/.sblivesystemconvert")) || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemconvert")) return error();
    sb::ThrdLng[0] = sb::fsize(path % ".sblive"), sb::ThrdStr[0] = sb::sdir[2] % "/.sblivesystemconvert";
    if(sb::exec("tar -xf " % path % ".sblive -C " % sb::sdir[2] % "/.sblivesystemconvert --no-same-owner --no-same-permissions", nullptr, sb::Prgrss) > 0) return error(335);
    if(! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemconvert/syslinux/isolinux.cfg") || ! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux", sb::sdir[2] % "/.sblivesystemconvert/isolinux") || intrrpt) return error();
    pset(21, " 2/2");
    sb::Progress = -1;
    ui->progressbar->setValue(0);
    if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o " % path % ".iso " % sb::sdir[2] % "/.sblivesystemconvert", nullptr, sb::Prgrss) > 0) return error(324);
    if(sb::exec("isohybrid " % path % ".iso") > 0 || ! QFile::setPermissions(path % ".iso", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) return error();
    sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(intrrpt) return error();
    emptycache();
    ui->livelist->currentItem()->setText(sb::left(ui->livelist->currentItem()->text(), sb::rinstr(ui->livelist->currentItem()->text(), " ")) % "sblive+iso)");
    ui->liveconvert->setDisabled(true);
    ui->statuspanel->hide();
    ui->mainpanel->show();
    ui->livecreateback->setFocus();
    windowmove(ss(698), ss(465));
}

void systemback::prntdbgmsg()
{
    sb::error("\n " % tr("Systemback worker thread error because the following item:") % "\n\n " % sb::ThrdDbg % "\n\n");
}

void systemback::on_partitiondelete_clicked()
{
    busy();
    ui->copycover->show();

    switch(ui->partitionsettings->item(ui->partitionsettings->currentItem()->row(), 8)->text().toUShort()) {
    case sb::Extended:
        sb::delpart(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text());
        break;
    default:
        sb::mkptable(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), grub.isEFI ? "gpt" : "msdos");
    }

    on_partitionrefresh2_clicked();
    busy(false);
}

void systemback::on_newpartition_clicked()
{
    busy();
    ui->copycover->show();
    QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().contains("mmc") ? 12 : 8)));
    ullong start(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 9)->text().toULongLong());
    uchar type;

    switch(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 8)->text().toUShort()) {
    case sb::Freespace:
    {
        uchar pcnt(0), fcnt(0);

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                case sb::GPT:
                case sb::Extended:
                    type = sb::Primary;
                    goto exec;
                case sb::Primary:
                    ++pcnt;
                    break;
                case sb::Freespace:
                    ++fcnt;
                }

        if(pcnt < 3 || (fcnt == 1 && ui->partitionsize->value() == ui->partitionsize->maximum()))
        {
            type = sb::Primary;
            break;
        }
        else if(! sb::mkpart(dev, start, ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong(), sb::Extended))
            goto end;

        start += 1048576;
    }
    default:
        type = sb::Logical;
    }

exec:
    sb::mkpart(dev, start, ui->partitionsize->value() == ui->partitionsize->maximum() ? ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong() : ullong(ui->partitionsize->value()) * 1048576, type);
end:
    on_partitionrefresh2_clicked();
    busy(false);
}

void systemback::on_languageoverride_clicked(bool chckd)
{
    if(chckd)
    {
        QStr lname(ui->languages->currentText());

        sb::lang = lname == "المصرية العربية" ? "ar_EG"
                 : lname == "Català" ? "ca_ES"
                 : lname == "Čeština" ? "cs_CS"
                 : lname == "English (common)" ? "en_EN"
                 : lname == "English (United Kingdom)" ? "en_GB"
                 : lname == "Español" ? "es_ES"
                 : lname == "Suomi" ? "fi_FI"
                 : lname == "Français" ? "fr_FR"
                 : lname == "Galego" ? "gl_ES"
                 : lname == "Magyar" ? "hu_HU"
                 : lname == "Bahasa Indonesia" ? "id_ID"
                 : lname == "Português (Brasil)" ? "pt_BR"
                 : lname == "Română" ? "ro_RO"
                 : lname == "Türkçe" ? "tr_TR" : "zh_CN";

        ui->languages->setEnabled(true);
    }
    else
    {
        sb::lang = "auto";
        ui->languages->setDisabled(true);
    }

    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_languages_currentIndexChanged(cQStr &arg1)
{
    ui->languages->resize(fontMetrics().width(arg1) + ss(30), ui->languages->height());

    if(ui->languages->isEnabled())
    {
        sb::lang = arg1 == "المصرية العربية" ? "ar_EG"
                 : arg1 == "Català" ? "ca_ES"
                 : arg1 == "Čeština" ? "cs_CS"
                 : arg1 == "English (common)" ? "en_EN"
                 : arg1 == "English (United Kingdom)" ? "en_GB"
                 : arg1 == "Español" ? "es_ES"
                 : arg1 == "Suomi" ? "fi_FI"
                 : arg1 == "Français" ? "fr_FR"
                 : arg1 == "Galego" ? "gl_ES"
                 : arg1 == "Magyar" ? "hu_HU"
                 : arg1 == "Bahasa Indonesia" ? "id_ID"
                 : arg1 == "Português (Brasil)" ? "pt_BR"
                 : arg1 == "Română" ? "ro_RO"
                 : arg1 == "Türkçe" ? "tr_TR" : "zh_CN";

        if(! cfgupdt) cfgupdt = true;
    }
}

void systemback::on_styleoverride_clicked(bool chckd)
{
    sb::style = chckd ? ui->styles->currentText() : "auto";
    ui->styles->setEnabled(chckd);
    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_styles_currentIndexChanged(cQStr &arg1)
{
    ui->styles->resize(fontMetrics().width(arg1) + ss(30), ui->styles->height());

    if(ui->styles->isEnabled())
    {
        sb::style = arg1;
        if(! cfgupdt) cfgupdt = true;
    }
}

void systemback::on_alwaysontop_clicked(bool chckd)
{
    if(chckd)
    {
        setwontop();
        sb::waot = sb::True;
    }
    else
    {
        sb::waot = sb::False;
        setwontop(false);
    }

    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_incrementaldisable_clicked(bool chckd)
{
    sb::incrmtl = chckd ? sb::False : sb::True;
    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_cachemptydisable_clicked(bool chckd)
{
    sb::ecache = chckd ? sb::False : sb::True;
    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_usexzcompressor_clicked(bool chckd)
{
    sb::xzcmpr = chckd ? sb::True : sb::False;
    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_autoisocreate_clicked(bool chckd)
{
    sb::autoiso = chckd ? sb::True : sb::False;
    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_schedulerdisable_clicked(bool chckd)
{
    if(chckd)
    {
        on_schedulerrefresh_clicked();
        ui->adduser->setEnabled(true);
    }
    else
    {
        sb::schdlr[1] = "false";
        ui->schedulerusers->setText(nullptr);
        if(ui->adduser->isEnabled()) ui->adduser->setDisabled(true);
        if(! cfgupdt) cfgupdt = true;
    }

    ui->users->setEnabled(chckd);
    ui->schedulerrefresh->setEnabled(chckd);
}

void systemback::on_users_currentIndexChanged(cQStr &arg1)
{
    ui->users->setToolTip(arg1);
}

void systemback::on_adduser_clicked()
{
    if(ui->users->count() == 1)
    {
        if(sb::schdlr[1] != "everyone")
        {
            sb::schdlr[1] = "everyone";
            ui->schedulerusers->setText(tr("Everyone"));
        }

        ui->users->clear();
        ui->adduser->setDisabled(true);
    }
    else
    {
        sb::schdlr[1] == "everyone" ? sb::schdlr[1] = ':' % ui->users->currentText() : sb::schdlr[1].append(',' % ui->users->currentText());
        ui->schedulerusers->setText(sb::right(sb::schdlr[1], -1));
        ui->users->removeItem(ui->users->findText(ui->users->currentText()));
    }

    if(! cfgupdt) cfgupdt = true;
}

void systemback::on_schedulerrefresh_clicked()
{
    busy();

    if(sb::schdlr[1] != "everyone")
    {
        sb::schdlr[1] = "everyone";
        ui->schedulerusers->setText(tr("Everyone"));
        if(! cfgupdt) cfgupdt = true;
    }

    ui->users->count() == 0 ? ui->adduser->setEnabled(true) : ui->users->clear();
    ui->users->addItem("root");
    QFile file("/etc/passwd");

    if(file.open(QIODevice::ReadOnly))
        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ui->users->addItem(usr);
        }

    busy(false);
}

void systemback::on_grubreinstallrestore_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubreinstallrestore->resize(fontMetrics().width(arg1) + ss(30), ui->grubreinstallrestore->height());
}

void systemback::on_grubinstallcopy_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubinstallcopy->resize(fontMetrics().width(arg1) + ss(30), ui->grubinstallcopy->height());
}

void systemback::on_grubreinstallrepair_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubreinstallrepair->resize(fontMetrics().width(arg1) + ss(30), ui->grubreinstallrepair->height());
}
