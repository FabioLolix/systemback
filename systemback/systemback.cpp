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
#include <QTextStream>
#include <QDateTime>
#include <QDir>
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
    if(sb::style == "auto")
        cfgupdt = false;
    else if(QStyleFactory::keys().contains(sb::style))
    {
        qApp->setStyle(QStyleFactory::create(sb::style));
        cfgupdt = false;
    }
    else
    {
        sb::style = "auto";
        cfgupdt = true;
    }

    schar snum(qApp->desktop()->screenNumber(this));
    wismax = nrxth = false;
    ui->setupUi(this);
    ui->statuspanel->hide();
    ui->scalingbuttonspanel->hide();
    ui->buttonspanel->hide();
    ui->resizepanel->hide();
    ui->dialogpanel->move(0, 0);
    ui->dialogpanel->setBackgroundRole(QPalette::Foreground);
    ui->subdialogpanel->setBackgroundRole(QPalette::Background);
    ui->buttonspanel->setBackgroundRole(QPalette::Highlight);
    ui->function3->setForegroundRole(QPalette::Base);
    ui->windowbutton3->setForegroundRole(QPalette::Base);
    ui->windowmaximize->setBackgroundRole(QPalette::Foreground);
    ui->windowmaximize->setForegroundRole(QPalette::Base);
    ui->windowminimize->setBackgroundRole(QPalette::Foreground);
    ui->windowminimize->setForegroundRole(QPalette::Base);
    ui->windowclose->setBackgroundRole(QPalette::Foreground);
    ui->windowclose->setForegroundRole(QPalette::Base);
    connect(&utimer, SIGNAL(timeout()), this, SLOT(unitimer()));
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

    if(getuid() + getgid() > 0)
        dialog = 305;
    else if(qApp->arguments().count() == 2 && qApp->arguments().value(1) == "schedule")
    {
        sstart = true;
        dialog = 0;
    }
    else if(! sb::lock(sb::Sblock))
        dialog = 300;
    else if(sb::lock(sb::Dpkglock))
    {
        dialog = 0;
        sstart = false;
    }
    else
        dialog = 301;

    {
        QFont font;
        if(! sb::like(font.family(), {"_Ubuntu_", "_FreeSans_"})) font.setFamily(QFontDatabase().families().contains("Ubuntu") ? "Ubuntu" : "FreeSans");
        if(font.weight() != QFont::Normal) font.setWeight(QFont::Normal);
        if(font.bold()) font.setBold(false);
        if(font.italic()) font.setItalic(false);
        if(font.overline()) font.setOverline(false);
        if(font.strikeOut()) font.setStrikeOut(false);
        if(font.underline()) font.setUnderline(false);
        if(font != this->font()) setFont(font);

        if(! sb::like(sb::wsclng, {"_auto_", "_1_"}) || fontInfo().pixelSize() != 15)
        {
            sfctr = sb::wsclng == "auto" ? fontInfo().pixelSize() > 28 ? Max : fontInfo().pixelSize() > 21 ? High : Normal : sb::wsclng == "2" ? Max : sb::wsclng == "1.5" ? High : Normal;
            while(sfctr > Normal && (qApp->desktop()->screenGeometry(snum).width() - ss(30) < ss(698) || qApp->desktop()->screenGeometry(snum).height() - ss(30) < ss(465))) sfctr = sfctr == Max ? High : Normal;
            font.setPixelSize(ss(15));
            setFont(font);
            font.setPixelSize(ss(27));
            ui->buttonspanel->setFont(font);
            font.setPixelSize(ss(17));
            font.setBold(true);
            ui->passwordtitletext->setFont(font);

            if(sfctr > Normal)
            {
                for(QWidget *wdgt : findChildren<QWidget *>()) wdgt->setGeometry(ss(wdgt->x()), ss(wdgt->y()), ss(wdgt->width()), ss(wdgt->height()));
                for(QPushButton *pbtn : findChildren<QPushButton *>()) pbtn->setIconSize(QSize(ss(pbtn->iconSize().width()), ss(pbtn->iconSize().height())));

                if(! sstart && dialog == 0)
                {
                    ui->includeusers->setMinimumSize(ss(112), ss(32));
                    ui->grubreinstallrestore->setMinimumSize(ui->includeusers->minimumSize());
                    ui->grubreinstallrestoredisable->setMinimumSize(ui->includeusers->minimumSize());
                    ui->grubinstallcopy->setMinimumSize(ui->includeusers->minimumSize());
                    ui->grubinstallcopydisable->setMinimumSize(ui->includeusers->minimumSize());
                    ui->repairpartition->setMinimumSize(ui->includeusers->minimumSize());
                    ui->grubreinstallrepair->setMinimumSize(ui->includeusers->minimumSize());
                    ui->grubreinstallrepairdisable->setMinimumSize(ui->includeusers->minimumSize());
                    ui->windowposition->setMinimumSize(ui->includeusers->minimumSize());
                    ui->languages->setMinimumSize(ui->includeusers->minimumSize());
                    ui->styles->setMinimumSize(ui->includeusers->minimumSize());
                    ui->users->setMinimumSize(ui->includeusers->minimumSize());
                    ui->admins->setMinimumSize(ui->includeusers->minimumSize());
                    ui->partitionsettings->verticalHeader()->setDefaultSectionSize(ss(20));
                    ui->livedevices->verticalHeader()->setDefaultSectionSize(ui->partitionsettings->verticalHeader()->defaultSectionSize());
                    QStyleOption optn;
                    optn.init(ui->pointpipe1);
                    QStr nsize(QStr::number(ss(ui->pointpipe1->style()->subElementRect(QStyle::SE_CheckBoxClickRect, &optn).width())));
                    for(QCheckBox *ckbx : findChildren<QCheckBox *>()) ckbx->setStyleSheet("QCheckBox::indicator{width:" % nsize % "px; height:" % nsize % "px;}");
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
        ui->mainpanel->hide();
        ui->passwordpanel->hide();
        ui->schedulerpanel->hide();
        dialogopen(snum);
    }
    else
    {
        dlgtimer = intrrptimer = nullptr;
        intrrpt = irblck = utblock = false;
        ppipe = busycnt = 0;
        ui->dialogpanel->hide();
        ui->statuspanel->move(0, 0);
        ui->statuspanel->setBackgroundRole(QPalette::Foreground);
        ui->substatuspanel->setBackgroundRole(QPalette::Background);
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
            icnt = 0;
            cpos = -1;
            nohmcpy = uchkd = false;
            ui->restorepanel->hide();
            ui->copypanel->hide();
            ui->installpanel->hide();
            ui->livecreatepanel->hide();
            ui->repairpanel->hide();
            ui->excludepanel->hide();
            ui->schedulepanel->hide();
            ui->aboutpanel->hide();
            ui->licensepanel->hide();
            ui->settingspanel->hide();
            ui->choosepanel->hide();
            ui->functionmenu2->hide();
            ui->storagedirbutton->hide();
            ui->storagedir->resize(ss(236), ss(28));
            ui->installpanel->move(ui->sbpanel->pos());
            ui->fullnamepipe->hide();
            ui->usernamepipe->hide();
            ui->usernameerror->hide();
            ui->passwordpipe->hide();
            ui->passworderror->hide();
            ui->rootpasswordpipe->hide();
            ui->rootpassworderror->hide();
            ui->hostnamepipe->hide();
            ui->hostnameerror->hide();
            ui->mainpanel->setBackgroundRole(QPalette::Foreground);
            ui->sbpanel->setBackgroundRole(QPalette::Background);
            ui->installpanel->setBackgroundRole(QPalette::Background);
            ui->function1->setForegroundRole(QPalette::Base);
            ui->function2->setForegroundRole(QPalette::Base);
            ui->function4->setForegroundRole(QPalette::Base);
            ui->scalingbutton->setForegroundRole(QPalette::Base);
            ui->scalingbuttonspanel->move(0, 0);
            ui->scalingbuttonspanel->setBackgroundRole(QPalette::Highlight);
            ui->subscalingbuttonspanel->setBackgroundRole(QPalette::Background);

            if(sb::wsclng == "auto")
                ui->scalingdown->setDisabled(true);
            else if(sb::wsclng == "2")
            {
                ui->scalingfactor->setText("x2");
                ui->scalingup->setDisabled(true);
            }
            else
                ui->scalingfactor->setText('x' % sb::wsclng);

            ui->scalingfactor->setBackgroundRole(QPalette::Base);
            ui->windowbutton1->setForegroundRole(QPalette::Base);
            ui->windowbutton2->setForegroundRole(QPalette::Base);
            ui->windowbutton4->setForegroundRole(QPalette::Base);
            ui->storagedirarea->setBackgroundRole(QPalette::Base);
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

            if(! sb::isfile("/cdrom/casper/filesystem.squashfs") && ! sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
                ui->livename->setText("auto");
            else if(sb::isdir("/.systemback"))
                on_installmenu_clicked();
        }

        if(qApp->arguments().count() == 3 && qApp->arguments().value(1) == "authorization" && (ui->sbpanel->isVisibleTo(ui->mainpanel) || ! sb::like(sb::fload("/proc/self/mounts"), {"* / overlayfs *", "* / aufs *", "* / unionfs *", "* / fuse.unionfs-fuse *"})))
        {
            ui->mainpanel->hide();
            ui->schedulerpanel->hide();
            ui->passwordpanel->move(0, 0);
            ui->adminpasswordpipe->hide();
            ui->adminpassworderror->hide();
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
                {
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + ss(30);
                    wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + ss(30);
                }
                else if(sb::schdlr[0] == "center")
                {
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() / 2 - ss(201);
                    wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() / 2 - ss(80);
                }
                else if(sb::schdlr[0] == "bottomleft")
                {
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + ss(30);
                    wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() - ss(191);
                }
                else if(sb::schdlr[0] == "bottomright")
                {
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() - ss(432);
                    wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + qApp->desktop()->screenGeometry(snum).height() - ss(191);
                }
                else
                {
                    wgeom[0] = qApp->desktop()->screenGeometry(snum).x() + qApp->desktop()->screenGeometry(snum).width() - ss(432);
                    wgeom[1] = qApp->desktop()->screenGeometry(snum).y() + ss(30);
                }

                setFixedSize((wgeom[2] = ss(402)), (wgeom[3] = ss(161)));
                move(wgeom[0], wgeom[1]);
                connect((shdltimer = new QTimer), SIGNAL(timeout()), this, SLOT(schedulertimer()));
                shdltimer->start(1000);
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

    delete ui;
}

void systemback::closeEvent(QCloseEvent *ev)
{
    if(ui->statuspanel->isVisible() && ! (sstart && sb::ThrdKill) && prun != tr("Upgrading the system")) ev->ignore();
}

void systemback::unitimer()
{
    if(! utblock)
    {
        utblock = true;

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
                ui->storagedir->setCursorPosition(0);
                ui->liveworkdir->setText(sb::sdir[2]);
                ui->liveworkdir->setToolTip(sb::sdir[2]);
                ui->liveworkdir->setCursorPosition(0);
                ui->restorepanel->move(ui->sbpanel->pos());
                ui->copypanel->move(ui->sbpanel->pos());
                ui->livecreatepanel->move(ui->sbpanel->pos());
                ui->repairpanel->move(ui->sbpanel->pos());
                ui->excludepanel->move(ui->sbpanel->pos());
                ui->schedulepanel->move(ui->sbpanel->pos());
                ui->aboutpanel->move(ui->sbpanel->pos());
                ui->licensepanel->move(ui->sbpanel->pos());
                ui->settingspanel->move(ui->sbpanel->pos());
                ui->choosepanel->move(ui->sbpanel->pos());
                ui->restorepanel->setBackgroundRole(QPalette::Background);
                ui->copypanel->setBackgroundRole(QPalette::Background);
                ui->livecreatepanel->setBackgroundRole(QPalette::Background);
                ui->repairpanel->setBackgroundRole(QPalette::Background);
                ui->excludepanel->setBackgroundRole(QPalette::Background);
                ui->schedulepanel->setBackgroundRole(QPalette::Background);
                ui->aboutpanel->setBackgroundRole(QPalette::Background);
                ui->licensepanel->setBackgroundRole(QPalette::Background);
                ui->settingspanel->setBackgroundRole(QPalette::Background);
                ui->choosepanel->setBackgroundRole(QPalette::Background);
                ui->liveworkdirarea->setBackgroundRole(QPalette::Base);
                ui->schedulerday->setBackgroundRole(QPalette::Base);
                ui->schedulerhour->setBackgroundRole(QPalette::Base);
                ui->schedulerminute->setBackgroundRole(QPalette::Base);
                ui->schedulersecond->setBackgroundRole(QPalette::Base);
                ui->partitiondelete->setStyleSheet("QPushButton:enabled {color: red}");
                { QPalette pal(ui->license->palette());
                pal.setBrush(QPalette::Base, pal.background());
                ui->license->setPalette(pal); }
                ui->partitionsettings->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Label"), tr("Current mount point"), tr("New mount point"), tr("Filesystem"), tr("Format")});
                ui->partitionsettings->setColumnHidden(7, true);
                ui->partitionsettings->setColumnHidden(8, true);
                ui->partitionsettings->setColumnHidden(9, true);
                ui->partitionsettings->setColumnHidden(10, true);
                { QFont font;
                font.setPixelSize(ss(14));
                ui->partitionsettings->horizontalHeader()->setFont(font);
                ui->livedevices->horizontalHeader()->setFont(font); }
                ui->partitionsettings->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
                ui->partitionsettings->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
                ui->partitionsettings->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
                ui->partitionsettings->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
                ui->livedevices->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Device"), tr("Format")});
                ui->livedevices->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
                ui->livedevices->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
                ui->livedevices->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
                ui->livenamepipe->hide();
                ui->livenameerror->hide();
                ui->partitionsettingspanel2->hide();
                ui->partitionsettingspanel3->hide();
                ui->grubreinstallrestoredisable->hide();
                ui->grubreinstallrestoredisable->addItem(tr("Disabled"));
                ui->grubinstallcopydisable->addItem(tr("Disabled"));
                ui->grubreinstallrepairdisable->hide();
                ui->grubreinstallrepairdisable->addItem(tr("Disabled"));
                ui->usersettingscopy->hide();
                ui->repaircover->hide();
                if(sb::schdle[0] == sb::True) ui->schedulerstate->click();
                if(sb::schdle[5] == sb::True) ui->silentmode->setChecked(true);
                ui->windowposition->addItems({tr("Top left"), tr("Top right"), tr("Center"), tr("Bottom left"), tr("Bottom right")});

                if(sb::schdlr[0] == "topright")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Top right")));
                if(sb::schdlr[0] == "center")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Center")));
                else if(sb::schdlr[0] == "bottomleft")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Bottom left")));
                else if(sb::schdlr[0] == "bottomright")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Bottom right")));

                ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
                ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
                ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
                ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));

                if(sb::isdir("/usr/share/systemback/lang"))
                {
                    {
                        QSL lst("English");
                        lst.reserve(13);

                        for(cQStr &item : QDir("/usr/share/systemback/lang").entryList(QDir::Files))
                        {
                            QStr lcode(sb::left(sb::right(item, -11), -3));

                            if(lcode == "ar_EG")
                                lst.append("المصرية العربية");
                            else if(lcode == "ca_ES")
                                lst.append("Català");
                            else if(lcode == "cs")
                                lst.append("Čeština");
                            else if(lcode == "es")
                                lst.append("Español");
                            else if(lcode == "fi")
                                lst.append("Suomi");
                            else if(lcode == "fr")
                                lst.append("Français");
                            else if(lcode == "gl_ES")
                                lst.append("Galego");
                            else if(lcode == "hu")
                                lst.append("Magyar");
                            else if(lcode == "id")
                                lst.append("Bahasa Indonesia");
                            else if(lcode == "pt_BR")
                                lst.append("Português (Brasil)");
                            else if(lcode == "ro")
                                lst.append("Română");
                            else if(lcode == "tr")
                                lst.append("Türkçe");
                            else if(lcode == "zh_CN")
                                lst.append("中文（简体）");
                        }

                        lst.sort();
                        ui->languages->addItems(lst);
                    }

                    if(sb::lang != "auto")
                    {
                        if(sb::lang == "ar_EG")
                            ui->languages->setCurrentIndex(ui->languages->findText("المصرية العربية"));
                        else if(sb::lang == "ca_ES")
                            ui->languages->setCurrentIndex(ui->languages->findText("Català"));
                        else if(sb::lang == "cs_CS")
                            ui->languages->setCurrentIndex(ui->languages->findText("Čeština"));
                        else if(sb::lang == "en_EN")
                            ui->languages->setCurrentIndex(ui->languages->findText("English"));
                        else if(sb::lang == "es_ES")
                            ui->languages->setCurrentIndex(ui->languages->findText("Español"));
                        else if(sb::lang == "fi_FI")
                            ui->languages->setCurrentIndex(ui->languages->findText("Suomi"));
                        else if(sb::lang == "fr_FR")
                            ui->languages->setCurrentIndex(ui->languages->findText("Français"));
                        else if(sb::lang == "gl_ES")
                            ui->languages->setCurrentIndex(ui->languages->findText("Galego"));
                        else if(sb::lang == "hu_HU")
                            ui->languages->setCurrentIndex(ui->languages->findText("Magyar"));
                        else if(sb::lang == "pt_BR")
                            ui->languages->setCurrentIndex(ui->languages->findText("Português (Brasil)"));
                        else if(sb::lang == "ro_RO")
                            ui->languages->setCurrentIndex(ui->languages->findText("Română"));
                        else if(sb::lang == "tr_TR")
                            ui->languages->setCurrentIndex(ui->languages->findText("Türkçe"));
                        else if(sb::lang == "zh_CN")
                            ui->languages->setCurrentIndex(ui->languages->findText("中文（简体）"));
                        else if(sb::lang != "id_ID")
                            ui->languages->setCurrentIndex(-1);

                        if(ui->languages->currentIndex() == -1)
                        {
                            sb::lang = "auto";
                            ui->languageoverride->setDisabled(true);
                            if(! cfgupdt) cfgupdt = true;
                        }
                        else
                        {
                            ui->languageoverride->setChecked(true);
                            ui->languages->setEnabled(true);
                        }
                    }

                    for(QLabel *lbl : findChildren<QLabel *>())
                        if(lbl->alignment() == (Qt::AlignLeft | Qt::AlignVCenter) && lbl->text().isRightToLeft()) lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                }
                else
                {
                    ui->languageoverride->setDisabled(true);

                    if(sb::lang != "auto")
                    {
                        sb::lang = "auto";
                        if(! cfgupdt) cfgupdt = true;
                    }
                }

                ui->styles->addItems(QStyleFactory::keys());

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
                        {
                            ui->users->setEnabled(true);
                            ui->adduser->setEnabled(true);
                        }

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
                ui->grubreinstallrestoredisable->move(ui->grubreinstallrestore->x(), ui->grubreinstallrestore->y());
                ui->grubinstalltext->resize(fontMetrics().width(ui->grubinstalltext->text()) + ss(7), ui->grubinstalltext->height());
                ui->grubinstallcopy->move(ui->grubinstalltext->x() + ui->grubinstalltext->width(), ui->grubinstallcopy->y());
                ui->grubinstallcopydisable->move(ui->grubinstallcopy->x(), ui->grubinstallcopy->y());
                ui->grubreinstallrepairtext->resize(fontMetrics().width(ui->grubreinstallrepairtext->text()) + ss(7), ui->grubreinstallrepairtext->height());
                ui->grubreinstallrepair->move(ui->grubreinstallrepairtext->x() + ui->grubreinstallrepairtext->width(), ui->grubreinstallrepair->y());
                ui->grubreinstallrepairdisable->move(ui->grubreinstallrepair->x(), ui->grubreinstallrepair->y());
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
                if(sb::execsrch("mkfs.btrfs")) ui->filesystem->addItem("btrfs");
                if(sb::execsrch("mkfs.reiserfs")) ui->filesystem->addItem("reiserfs");
                if(sb::execsrch("mkfs.jfs")) ui->filesystem->addItem("jfs");
                if(sb::execsrch("mkfs.xfs")) ui->filesystem->addItem("xfs");
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
                                if(cline.endsWith("efivars.ko") && sb::isfile("/lib/modules/" % ckernel % '/' % cline) && sb::exec("modprobe efivars", nullptr, true) == 0 && sb::isdir("/sys/firmware/efi")) goto isefi;
                            }
                    }
                }

                goto noefi;
            isefi:
                grub.name = "efi-amd64-bin";
                grub.isEFI = true;
                ui->repairmountpoint->addItem("/mnt/boot/efi");
                ui->grubinstallcopy->hide();
                ui->grubinstallcopy->addItems({"EFI", tr("Disabled")});
                ui->grubreinstallrestore->addItems({"EFI", tr("Disabled")});
                ui->grubreinstallrepair->addItems({"EFI", tr("Disabled")});
                ui->grubinstallcopy->adjustSize();
                ui->efiwarning->move(ui->grubinstallcopy->x() + ui->grubinstallcopy->width() + ss(5), ui->grubinstallcopy->y() - ss(4));
                ui->efiwarning->resize(ui->copypanel->width() - ui->efiwarning->x() - ss(8), ui->efiwarning->height());
                ui->efiwarning->setForegroundRole(QPalette::Highlight);
                goto next_1;
            noefi:
#endif
                grub.name = "pc-bin";
                grub.isEFI = false;
                ui->grubinstallcopydisable->hide();
                ui->efiwarning->hide();
#ifdef __amd64__
            next_1:
#endif
                ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"});
                ui->repairmountpoint->setCurrentIndex(1);
                on_partitionrefresh_clicked();
                on_livedevicesrefresh_clicked();
                on_pointexclude_clicked();
                ui->storagedir->resize(ss(210), ss(28));
                ui->storagedirbutton->show();
                ui->repairmenu->setEnabled(true);
                ui->aboutmenu->setEnabled(true);
                ui->settingsmenu->setEnabled(true);
                ui->pnumber3->setEnabled(true);
                ui->pnumber4->setEnabled(true);
                ui->pnumber5->setEnabled(true);
                ui->pnumber6->setEnabled(true);
                ui->pnumber7->setEnabled(true);
                ui->pnumber8->setEnabled(true);
                ui->pnumber9->setEnabled(true);
                ui->pnumber10->setEnabled(true);

                if(ui->installpanel->isHidden())
                {
                    ui->copymenu->setEnabled(true);
                    ui->installmenu->setEnabled(true);
                    ui->systemupgrade->setEnabled(true);
                    ui->excludemenu->setEnabled(true);
                    ui->schedulemenu->setEnabled(true);
                    pname = tr("Currently running system");
                }
            }

            pointupgrade();
            if(sstart) ui->schedulerstart->setEnabled(true);
            QStr ckernel(ckname()), fend[]{"order", "builtin"};

            for(uchar a(0) ; a < 2 ; ++a)
                if(sb::isfile("/lib/modules/" % ckernel % "/modules." % fend[a]))
                {
                    QFile file("/lib/modules/" % ckernel % "/modules." % fend[a]);

                    if(file.open(QIODevice::ReadOnly))
                        while(! file.atEnd())
                            if(sb::like(file.readLine().trimmed(), {"*aufs.ko_", "*overlayfs.ko_", "*unionfs.ko_"}))
                            {
                                ickernel = true;
                                goto next_2;
                            }
                }

            ickernel = sb::execsrch("unionfs-fuse");
        next_2:
            busy(false);
            utimer.start(500);
        }
        else if(ui->statuspanel->isVisible())
        {
            if(! prun.isEmpty()) ui->processrun->setText(prun % (points.contains('.') ? points.endsWith('.') ? points = "    " : points.replace(". ", "..") : points = " .  "));

            if(sb::like(prun, {'_' % tr("Creating restore point") % '_', '_' % tr("Restoring the full system") % '_', '_' % tr("Restoring the system files") % '_', '_' % tr("Restoring user(s) configuration files") % '_', '_' % tr("Repairing the system files") % '_', '_' % tr("Repairing the full system") % '_', '_' % tr("Copying the system") % '_', '_' % tr("Installing the system") % '_', '_' % tr("Creating Live system") % '\n' % tr("process") % " 3/3*", '_' % tr("Creating Live system") % '\n' % tr("process") % " 4/3+1_", '_' % tr("Writing Live image to target device") % '_', '_' % tr("Converting Live system image") % "\n*"}))
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
            }
            else if(prun.startsWith(tr("Creating Live system") % '\n' % tr("process") % " 2"))
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
            }
            else
            {
                if(! irblck && sb::like(prun, {'_' % tr("Deleting restore point") % "*", '_' % tr("Deleting old restore point") % "*", '_' % tr("Deleting incomplete restore point") % '_', '_' % tr("Creating Live system") % "\n*"}))
                {
                    if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                }
                else if(ui->interrupt->isEnabled())
                    ui->interrupt->setDisabled(true);

                if(ui->progressbar->maximum() == 100) ui->progressbar->setMaximum(0);
            }
        }
        else
        {
            if(! sstart)
            {
                if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write))
                {
                    if(! ui->storagedirarea->styleSheet().isEmpty())
                    {
                        ui->storagedirarea->setStyleSheet(nullptr);
                        ui->storagedir->setStyleSheet(nullptr);
                        fontcheck(Strgdr);
                        pointupgrade();
                    }

                    if(ppipe == 0 && pname == tr("Currently running system") && ! ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setEnabled(true);

                    if((ui->point1->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S01_" % sb::pnames[0])) ||
                        (ui->point2->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S02_" % sb::pnames[1])) ||
                        (ui->point3->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S03_" % sb::pnames[2])) ||
                        (ui->point4->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S04_" % sb::pnames[3])) ||
                        (ui->point5->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S05_" % sb::pnames[4])) ||
                        (ui->point6->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S06_" % sb::pnames[5])) ||
                        (ui->point7->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S07_" % sb::pnames[6])) ||
                        (ui->point8->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S08_" % sb::pnames[7])) ||
                        (ui->point9->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S09_" % sb::pnames[8])) ||
                        (ui->point10->isEnabled() && ! sb::isdir(sb::sdir[1] % "/S10_" % sb::pnames[9])) ||
                        (ui->point11->isEnabled() && ! sb::isdir(sb::sdir[1] % "/H01_" % sb::pnames[10])) ||
                        (ui->point12->isEnabled() && ! sb::isdir(sb::sdir[1] % "/H02_" % sb::pnames[11])) ||
                        (ui->point13->isEnabled() && ! sb::isdir(sb::sdir[1] % "/H03_" % sb::pnames[12])) ||
                        (ui->point14->isEnabled() && ! sb::isdir(sb::sdir[1] % "/H04_" % sb::pnames[13])) ||
                        (ui->point15->isEnabled() && ! sb::isdir(sb::sdir[1] % "/H05_" % sb::pnames[14]))) accesserror();
                }
                else
                {
                    if(ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) accesserror();
                    if(ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setDisabled(true);

                    if(ui->storagedirarea->styleSheet().isEmpty())
                    {
                        ui->storagedirarea->setStyleSheet("background-color: rgb(255, 103, 103)");
                        ui->storagedir->setStyleSheet("background-color: rgb(255, 103, 103)");
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
                                ui->liveworkdirarea->setStyleSheet(nullptr);
                                ui->liveworkdir->setStyleSheet(nullptr);
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
                                ui->liveworkdirarea->setStyleSheet("background-color: rgb(255, 103, 103)");
                                ui->liveworkdir->setStyleSheet("background-color: rgb(255, 103, 103)");
                                fontcheck(Lvwrkdr);
                            }

                            if(ui->livecreatenew->isEnabled()) ui->livecreatenew->setDisabled(true);
                            if(ui->livedelete->isEnabled()) ui->livedelete->setDisabled(true);
                            if(ui->liveconvert->isEnabled()) ui->liveconvert->setDisabled(true);
                            if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);

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

            if(! prun.isEmpty()) prun.clear();
            if(! ui->processrun->text().isEmpty()) ui->processrun->clear();
            if(! points.isEmpty()) points.clear();
            if(sb::Progress != -1) sb::Progress = -1;
            if(ui->progressbar->maximum() == 0) ui->progressbar->setMaximum(100);
            if(ui->progressbar->value() > 0) ui->progressbar->setValue(0);
            if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);
        }

        utblock = false;
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
            case 698:
            case 506:
            case 402:
            case 201:
            case 154:
                return ssize + 1;
            default:
                return ssize;
            }
        }
    default:
        return size;
    }
}

void systemback::fontcheck(uchar wdgt)
{
    switch(wdgt) {
    case Strgdr:
        if(ui->storagedir->font().pixelSize() != ss(15))
        {
            ui->storagedirarea->setFont(font());
            ui->storagedir->setFont(font());
            QFont font;
            font.setPixelSize(ss(15));
            font.setBold(true);
            ui->storagedirbutton->setFont(font);
        }

        break;
    case Lvwrkdr:
        if(ui->liveworkdir->font().pixelSize() != ss(15))
        {
            ui->liveworkdirarea->setFont(font());
            ui->liveworkdir->setFont(font());
            QFont font;
            font.setPixelSize(ss(15));
            font.setBold(true);
            ui->liveworkdirbutton->setFont(font);
        }

        break;
    case Dpath:
        if(ui->dirpath->font().pixelSize() != ss(15)) ui->dirpath->setFont(font());
        break;
    case Rpnts:
        for(QWidget *lndt : ui->sbpanel->findChildren<QLineEdit *>())
            if(lndt->font().pixelSize() != ss(15)) lndt->setFont(font());
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
        if(! qApp->overrideCursor() || qApp->overrideCursor()->shape() != Qt::WaitCursor) qApp->setOverrideCursor(Qt::WaitCursor);
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
        wxy[0] = scrxy > 0 && x() == 0 ? scrxy : x();
        wxy[1] = (scrxy = qApp->desktop()->availableGeometry(snum).y()) > 0 && y() == 0 ? scrxy : y();
    }
    else
    {
        wxy[0] = x();
        wxy[1] = y();
    }
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
                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());
                    if(sb::like(usr, {"*:x:1000:10*", "*:x:1001:10*", "*:x:1002:10*", "*:x:1003:10*", "*:x:1004:10*", "*:x:1005:10*", "*:x:1006:10*", "*:x:1007:10*", "*:x:1008:10*", "*:x:1009:10*", "*:x:1010:10*", "*:x:1011:10*", "*:x:1012:10*", "*:x:1013:10*", "*:x:1014:10*", "*:x:1015:10*"})) usrs.append(sb::left(usr, sb::instr(usr, ":") -1));
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

QStr systemback::gdetect(cQStr rdir)
{
    QStr mnts(sb::fload("/proc/self/mounts", true));
    QTS in(&mnts, QIODevice::ReadOnly);

    while(! in.atEnd())
    {
        QStr cline(in.readLine());

        if(sb::like(cline, {"* " % rdir % " *", "* " % rdir % (rdir.endsWith('/') ? nullptr : "/") % "boot *"}))
        {
            if(sb::like(cline, {"_/dev/sd*", "_/dev/hd*", "_/dev/vd*"}))
                return sb::left(cline, 8);
            else if(cline.startsWith("/dev/mmcblk"))
                return sb::left(cline, 12);
            else if(cline.startsWith("/dev/disk/by-uuid"))
            {
                QStr uid(sb::right(sb::left(cline, sb::instr(cline, " ") - 1), -18));

                if(sb::islink("/dev/disk/by-uuid/" % uid))
                {
                    QStr dev(QFile("/dev/disk/by-uuid/" % uid).readLink());
                    return dev.contains("mmc") ? sb::left(dev, 12) : sb::left(dev, 8);
                }
            }

            break;
        }
    }

    return nullptr;
}

void systemback::emptycache()
{
    prun = sb::ecache == sb::True ? tr("Emptying cache") : tr("Flushing filesystem buffers");
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
        {
            ui->partitionsettings->resizeColumnToContents(2);
            ui->partitionsettings->resizeColumnToContents(3);
            ui->partitionsettings->resizeColumnToContents(4);
        }
    }
}

void systemback::schedulertimer()
{
    if(ui->schedulernumber->text().isEmpty())
        ui->schedulernumber->setText(QStr::number(sb::schdle[4]) % 's');
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
    if(! wismax) qApp->setOverrideCursor(Qt::SizeAllCursor);
}

void systemback::wreleased()
{
    if(qApp->overrideCursor() && qApp->overrideCursor()->shape() == Qt::SizeAllCursor) busycnt > 0 ? qApp->setOverrideCursor(Qt::WaitCursor) : qApp->restoreOverrideCursor();

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

        if(size() != QSize(wgeom[0], wgeom[1])) move(wgeom[0], wgeom[1]);
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
            if(ui->buttonspanel->width() != ss(48)) ui->buttonspanel->resize(ss(48), ss(48));
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
            if(ui->buttonspanel->width() != ss(138)) ui->buttonspanel->resize(ss(138), ss(48));
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
            if(ui->buttonspanel->width() != ss(93)) ui->buttonspanel->resize(ss(93), ss(48));
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

void systemback::apokkeyreleased()
{
    on_passwordinputok_clicked();
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
    if(! wismax) qApp->setOverrideCursor(Qt::SizeFDiagCursor);
}

void systemback::chsreleased()
{
    if(qApp->overrideCursor() && qApp->overrideCursor()->shape() == Qt::SizeFDiagCursor) busycnt > 0 ? qApp->setOverrideCursor(Qt::WaitCursor) : qApp->restoreOverrideCursor();
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
        sb::wsclng = nsclng;
        sb::cfgwrite();
        if(cfgupdt) cfgupdt = false;
        nrxth = true;
        sb::unlock(sb::Sblock);
        sb::exec("systemback", nullptr, false, true);
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
        sb::exec("su -c \"xdg-open https://sourceforge.net/projects/systemback &\" " % guname(), nullptr, false, true);
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
        sb::exec("su -c \"xdg-open https://launchpad.net/systemback &\" " % guname(), nullptr, false, true);
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
        sb::exec("su -c \"xdg-email nemh@freemail.hu &\" " % guname(), nullptr, false, true);
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
        sb::exec("su -c \"xdg-open 'https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZQ668BBR7UCEQ' &\" " % guname(), nullptr, false, true);
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

void systemback::pointupgrade()
{
    sb::pupgrade();

    if(! sb::pnames[0].isEmpty())
    {
        if(! ui->point1->isEnabled())
        {
            ui->point1->setEnabled(true);
            ui->point1->setText(sb::pnames[0]);
        }
        else if(ui->point1->text() != sb::pnames[0])
            ui->point1->setText(sb::pnames[0]);
    }
    else if(ui->point1->isEnabled())
    {
        ui->point1->setDisabled(true);
        if(ui->point1->text() != tr("empty")) ui->point1->setText(tr("empty"));
    }

    if(! sb::pnames[1].isEmpty())
    {
        if(! ui->point2->isEnabled())
        {
            ui->point2->setEnabled(true);
            ui->point2->setText(sb::pnames[1]);
        }
        else if(ui->point2->text() != sb::pnames[1])
            ui->point2->setText(sb::pnames[1]);
    }
    else if(ui->point2->isEnabled())
    {
        ui->point2->setDisabled(true);
        if(ui->point2->text() != tr("empty")) ui->point2->setText(tr("empty"));
    }

    if(! sb::pnames[2].isEmpty())
    {
        if(! ui->point3->isEnabled())
        {
            ui->point3->setEnabled(true);
            if(sb::pnumber == 3 && ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point3->setText(sb::pnames[2]);
        }
        else if(ui->point3->text() != sb::pnames[2])
            ui->point3->setText(sb::pnames[2]);
    }
    else if(ui->point3->isEnabled())
    {
        ui->point3->setDisabled(true);
        if(! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);
        if(ui->point3->text() != tr("empty")) ui->point3->setText(tr("empty"));
    }

    if(! sb::pnames[3].isEmpty())
    {
        if(! ui->point4->isEnabled())
        {
            ui->point4->setEnabled(true);
            if(sb::pnumber < 5 && ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point4->setText(sb::pnames[3]);
        }
        else if(ui->point4->text() != sb::pnames[3])
            ui->point4->setText(sb::pnames[3]);
    }
    else if(ui->point4->isEnabled())
    {
        ui->point4->setDisabled(true);
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);

        if(sb::pnumber == 3)
        {
            if(ui->point4->text() != tr("not used")) ui->point4->setText(tr("not used"));
        }
        else if(ui->point4->text() != tr("empty"))
            ui->point4->setText(tr("empty"));
    }

    if(! sb::pnames[4].isEmpty())
    {
        if(! ui->point5->isEnabled())
        {
            ui->point5->setEnabled(true);
            if(sb::pnumber < 6 && ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point5->setText(sb::pnames[4]);
        }
        else if(ui->point5->text() != sb::pnames[4])
            ui->point5->setText(sb::pnames[4]);
    }
    else if(ui->point5->isEnabled())
    {
        ui->point5->setDisabled(true);
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);

        if(sb::pnumber < 5)
        {
            if(ui->point5->text() != tr("not used")) ui->point5->setText(tr("not used"));
        }
        else if(ui->point5->text() != tr("empty"))
            ui->point5->setText(tr("empty"));
    }

    if(! sb::pnames[5].isEmpty())
    {
        if(! ui->point6->isEnabled())
        {
            ui->point6->setEnabled(true);
            if(sb::pnumber < 7 && ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point6->setText(sb::pnames[5]);
        }
        else if(ui->point6->text() != sb::pnames[5])
            ui->point6->setText(sb::pnames[5]);
    }
    else if(ui->point6->isEnabled())
    {
        ui->point6->setDisabled(true);
        if(! ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet(nullptr);

        if(sb::pnumber < 6)
        {
            if(ui->point6->text() != tr("not used")) ui->point6->setText(tr("not used"));
        }
        else if(ui->point6->text() != tr("empty"))
            ui->point6->setText(tr("empty"));
    }

    if(! sb::pnames[6].isEmpty())
    {
        if(! ui->point7->isEnabled())
        {
            ui->point7->setEnabled(true);
            if(sb::pnumber < 8 && ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point7->setText(sb::pnames[6]);
        }
        else if(ui->point7->text() != sb::pnames[6])
            ui->point7->setText(sb::pnames[6]);
    }
    else if(ui->point7->isEnabled())
    {
        ui->point7->setDisabled(true);
        if(! ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet(nullptr);

        if(sb::pnumber < 7)
        {
            if(ui->point7->text() != tr("not used")) ui->point7->setText(tr("not used"));
        }
        else if(ui->point7->text() != tr("empty"))
            ui->point7->setText(tr("empty"));
    }

    if(! sb::pnames[7].isEmpty())
    {
        if(! ui->point8->isEnabled())
        {
            ui->point8->setEnabled(true);
            if(sb::pnumber < 9 && ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point8->setText(sb::pnames[7]);
        }
        else if(ui->point8->text() != sb::pnames[7])
            ui->point8->setText(sb::pnames[7]);
    }
    else if(ui->point8->isEnabled())
    {
        ui->point8->setDisabled(true);
        if(! ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet(nullptr);

        if(sb::pnumber < 8)
        {
            if(ui->point8->text() != tr("not used")) ui->point8->setText(tr("not used"));
        }
        else if(ui->point8->text() != tr("empty"))
            ui->point8->setText(tr("empty"));
    }

    if(! sb::pnames[8].isEmpty())
    {
        if(! ui->point9->isEnabled())
        {
            ui->point9->setEnabled(true);
            if(sb::pnumber < 10 && ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point9->setText(sb::pnames[8]);
        }
        else if(ui->point9->text() != sb::pnames[8])
            ui->point9->setText(sb::pnames[8]);
    }
    else if(ui->point9->isEnabled())
    {
        ui->point9->setDisabled(true);
        if(! ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet(nullptr);

        if(sb::pnumber < 9)
        {
            if(ui->point9->text() != tr("not used")) ui->point9->setText(tr("not used"));
        }
        else if(ui->point9->text() != tr("empty"))
            ui->point9->setText(tr("empty"));
    }

    if(! sb::pnames[9].isEmpty())
    {
        if(! ui->point10->isEnabled())
        {
            ui->point10->setEnabled(true);
            if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
            ui->point10->setText(sb::pnames[9]);
        }
        else if(ui->point10->text() != sb::pnames[9])
            ui->point10->setText(sb::pnames[9]);
    }
    else if(ui->point10->isEnabled())
    {
        ui->point10->setDisabled(true);
        if(! ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet(nullptr);

        if(sb::pnumber < 10)
        {
            if(ui->point10->text() != tr("not used")) ui->point10->setText(tr("not used"));
        }
        else if(ui->point10->text() != tr("empty"))
            ui->point10->setText(tr("empty"));
    }

    if(! sb::pnames[10].isEmpty())
    {
        if(! ui->point11->isEnabled())
        {
            ui->point11->setEnabled(true);
            ui->point11->setText(sb::pnames[10]);
        }
        else if(ui->point11->text() != sb::pnames[10])
            ui->point11->setText(sb::pnames[10]);
    }
    else if(ui->point11->isEnabled())
    {
        ui->point11->setDisabled(true);
        if(ui->point11->text() != tr("empty")) ui->point11->setText(tr("empty"));
    }

    if(! sb::pnames[11].isEmpty())
    {
        if(! ui->point12->isEnabled())
        {
            ui->point12->setEnabled(true);
            ui->point12->setText(sb::pnames[11]);
        }
        else if(ui->point12->text() != sb::pnames[11])
            ui->point12->setText(sb::pnames[11]);
    }
    else if(ui->point12->isEnabled())
    {
        ui->point12->setDisabled(true);
        if(ui->point12->text() != tr("empty")) ui->point12->setText(tr("empty"));
    }

    if(! sb::pnames[12].isEmpty())
    {
        if(! ui->point13->isEnabled())
        {
            ui->point13->setEnabled(true);
            ui->point13->setText(sb::pnames[12]);
        }
        else if(ui->point13->text() != sb::pnames[12])
            ui->point13->setText(sb::pnames[12]);
    }
    else if(ui->point13->isEnabled())
    {
        ui->point13->setDisabled(true);
        if(ui->point13->text() != tr("empty")) ui->point13->setText(tr("empty"));
    }

    if(! sb::pnames[13].isEmpty())
    {
        if(! ui->point14->isEnabled())
        {
            ui->point14->setEnabled(true);
            ui->point14->setText(sb::pnames[13]);
        }
        else if(ui->point14->text() != sb::pnames[13])
            ui->point14->setText(sb::pnames[13]);
    }
    else if(ui->point14->isEnabled())
    {
        ui->point14->setDisabled(true);
        if(ui->point14->text() != tr("empty")) ui->point14->setText(tr("empty"));
    }

    if(! sb::pnames[14].isEmpty())
    {
        if(! ui->point15->isEnabled())
        {
            ui->point15->setEnabled(true);
            ui->point15->setText(sb::pnames[14]);
        }
        else if(ui->point15->text() != sb::pnames[14])
            ui->point15->setText(sb::pnames[14]);
    }
    else if(ui->point15->isEnabled())
    {
        ui->point15->setDisabled(true);
        if(ui->point15->text() != tr("empty")) ui->point15->setText(tr("empty"));
    }

    on_pointpipe1_clicked();
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

void systemback::accesserror()
{
    pointupgrade();

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
}

void systemback::restore()
{
    goto start;
exit:
    intrrpt = false;
    return;
start:
    statustart();
    uchar mthd;

    if(ui->fullrestore->isChecked())
    {
        mthd = 1;
        prun = tr("Restoring the full system");
    }
    else if(ui->systemrestore->isChecked())
    {
        mthd = 2;
        prun = tr("Restoring the system files");
    }
    else
    {
        mthd = ui->keepfiles->isChecked() ? ui->includeusers->currentIndex() == 0 ? 3 : 4 : ui->includeusers->currentIndex() == 0 ? 5 : 6;
        prun = tr("Restoring user(s) configuration files");
    }

    bool fcmp(false), sfstab(false);

    if(mthd < 3)
    {
        if(sb::isfile("/etc/fstab") && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") && sb::fload("/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab"))
        {
            fcmp = true;
            if(ui->autorestoreoptions->isChecked()) sfstab = true;
        }

        if(! ui->autorestoreoptions->isChecked() && ui->skipfstabrestore->isChecked()) sfstab = true;
    }

    if(intrrpt) goto exit;

    if(sb::srestore(mthd, ui->includeusers->currentText(), sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, sfstab))
    {
        if(intrrpt) goto exit;
        sb::Progress = -1;

        if(mthd < 3)
        {
            if(ui->grubreinstallrestore->isVisibleTo(ui->restorepanel) && (! ui->grubreinstallrestore->isEnabled() || ui->grubreinstallrestore->currentText() != tr("Disabled")))
            {
                sb::exec("update-grub");
                if(intrrpt) goto exit;

                if((! ui->autorestoreoptions->isChecked() && ui->grubreinstallrestore->currentText() != "Auto") || ! fcmp)
                {
                    if(sb::exec("grub-install --force " % (ui->autorestoreoptions->isChecked() || ui->grubreinstallrestore->currentText() == "Auto" ? grub.isEFI ? nullptr : gdetect() : grub.isEFI ? nullptr : ui->grubreinstallrestore->currentText())) > 0) dialog = 308;
                    if(intrrpt) goto exit;
                }
            }

            sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        if(! sb::like(dialog, {308, 338}))
            switch(mthd) {
            case 1:
                dialog = 205;
                break;
            case 2:
                dialog = 204;
                break;
            default:
                dialog = ui->keepfiles->isChecked() ? 201 : 200;
            }
    }
    else
        dialog = 338;

    if(intrrpt) goto exit;
    dialogopen();
}

void systemback::repair()
{
    goto start;
exit:
    intrrpt = false;
    return;
start:
    statustart();
    uchar mthd;

    if(ui->systemrepair->isChecked())
    {
        mthd = 2;
        prun = tr("Repairing the system files");
    }
    else if(ui->fullrepair->isChecked())
    {
        mthd = 1;
        prun = tr("Repairing the full system");
    }
    else
    {
        mthd = 0;
        prun = tr("Repairing the GRUB 2");
    }

    if(mthd == 0)
    {
        QSL mlst{"dev", "dev/pts", "proc", "sys"};
        if(sb::mcheck("/run")) mlst.append("/run");
        for(cQStr &bpath : mlst) sb::mount('/' % bpath, "/mnt/" % bpath);
        dialog = sb::exec("chroot /mnt sh -c \"update-grub ; grub-install --force " % (ui->grubreinstallrepair->currentText() == "Auto" ? gdetect("/mnt") : grub.isEFI ? nullptr : ui->grubreinstallrepair->currentText()) % "\"") == 0 ? 208 : 317;
        for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
        if(intrrpt) goto exit;
    }
    else
    {
        bool fcmp(false), sfstab(false);

        if(mthd < 3)
        {
            if(sb::isfile("/mnt/etc/fstab") && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") && sb::fload("/mnt/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab"))
            {
                fcmp = true;
                if(ui->autorepairoptions->isChecked()) sfstab = true;
            }

            if(! ui->autorepairoptions->isChecked() && ui->skipfstabrepair->isChecked()) sfstab = true;
        }

        if(intrrpt) goto exit;
        bool rv;

        if(ppipe == 0)
        {
            if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
            {
                QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                QDir().mkdir("/.systembacklivepoint");
            }

            if(! sb::mount(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs", "/.systembacklivepoint", "loop"))
            {
                dialog = 333;
                dialogopen();
                return;
            }

            if(intrrpt) goto exit;
            rv = sb::srestore(mthd, nullptr, "/.systembacklivepoint", "/mnt", sfstab);
            sb::umount("/.systembacklivepoint");
            QDir().rmdir("/.systembacklivepoint");
        }
        else
            rv = sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, "/mnt", sfstab);

        if(intrrpt) goto exit;

        if(rv)
        {
            if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel) && (! ui->grubreinstallrepair->isEnabled() || ui->grubreinstallrepair->currentText() != tr("Disabled")))
            {
                QSL mlst{"dev", "dev/pts", "proc", "sys"};
                if(sb::mcheck("/run")) mlst.append("/run");
                for(cQStr &bpath : mlst) sb::mount('/' % bpath, "/mnt/" % bpath);
                sb::exec("chroot /mnt update-grub");
                if(intrrpt) goto exit;
                if(((! ui->autorepairoptions->isChecked() && ui->grubreinstallrepair->currentText() != "Auto") || ! fcmp) && sb::exec("chroot /mnt grub-install --force " % (ui->autorepairoptions->isChecked() || ui->grubreinstallrepair->currentText() == "Auto" ? grub.isEFI ? nullptr : gdetect("/mnt") : grub.isEFI ? nullptr : ui->grubreinstallrepair->currentText())) > 0) dialog = ui->fullrepair->isChecked() ? 309 : 303;
                for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
                if(intrrpt) goto exit;
            }

            emptycache();

            if(sb::like(dialog, {101, 102, 109}))
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
    goto start;
error:
    if(intrrpt) goto exit;

    if(! sb::like(dialog, {307, 313, 314, 316, 329, 330, 331, 332}))
    {
        if(sb::dfree("/.sbsystemcopy") > 104857600 && (! sb::isdir("/.sbsystemcopy/home") || sb::dfree("/.sbsystemcopy/home") > 104857600) && (! sb::isdir("/.sbsystemcopy/boot") || sb::dfree("/.sbsystemcopy/boot") > 52428800) && (! sb::isdir("/.sbsystemcopy/boot/efi") || sb::dfree("/.sbsystemcopy/boot/efi") > 10485760))
        {
            irblck = true;
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 319 : 320;
        }
        else
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 306 : 315;
    }
exit:
    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, {"* /.sbsystemcopy*", "* /.sbmountpoints*", "* /.systembacklivepoint *"})) sb::umount(cline.split(' ').at(1));
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
        dialogopen();
    }

    return;
start:
    statustart();
    prun = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? tr("Copying the system") : tr("Installing the system");
    if(! sb::isdir("/.sbsystemcopy") && ! QDir().mkdir("/.sbsystemcopy")) goto error;

    {
        QSL msort;
        msort.reserve(ui->partitionsettings->rowCount());

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! nmpt.isEmpty() && (nmpt != "/home" || ui->partitionsettings->item(a, 3)->text().isEmpty()))
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
            if(intrrpt) goto exit;
            sb::fssync();
            if(intrrpt) goto exit;

            if(fstype.length() > 2)
            {
                QStr lbl("SB@" % (mpoint.startsWith('/') ? sb::right(mpoint, -1) : mpoint));
                ushort rv;

                if(fstype == "swap")
                    rv = sb::exec("mkswap -L " % lbl % ' ' % part);
                else if(fstype == "jfs")
                    rv = sb::exec("mkfs.jfs -qL " % lbl % ' ' % part);
                else if(fstype == "reiserfs")
                    rv = sb::exec("mkfs.reiserfs -ql " % lbl % ' ' % part);
                else if(fstype == "xfs")
                    rv = sb::exec("mkfs.xfs -fL " % lbl % ' ' % part);
                else if(fstype == "vfat")
                    rv = sb::setpflag(part, "boot") ? sb::exec("mkfs.vfat -F 32 -n " % lbl.toUpper() % ' ' % part) : 255;
                else if(fstype == "btrfs")
                {
                    rv = ckd.contains(part) ? 0 : sb::exec("mkfs.btrfs -fL " % lbl % ' ' % part);
                    if(rv > 0) rv = sb::exec("mkfs.btrfs -L " % lbl % ' ' % part);
                }
                else
                    rv = sb::exec("mkfs." % fstype % " -FL " % lbl % ' ' % part);

                if(intrrpt) goto exit;

                if(rv > 0)
                {
                    dialogdev = part;
                    dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 316 : 330;
                    goto error;
                }
            }

            if(mpoint != "SWAP")
            {
                if(! sb::isdir("/.sbsystemcopy" % mpoint))
                {
                    QStr path("/.sbsystemcopy");

                    for(cQStr &cpath : mpoint.split('/'))
                    {
                        path.append('/' % cpath);

                        if(! sb::isdir(path) && ! QDir().mkdir(path))
                        {
                            QFile::rename(path, path % '_' % sb::rndstr());
                            if(! QDir().mkdir(path)) goto error;
                        }
                    }
                }

                if(intrrpt) goto exit;

                if(fstype.length() == 2 || fstype == "btrfs")
                {
                    QStr mpt("/.sbmountpoints");

                    for(uchar a(0) ; a < 4 ; ++a)
                        switch(a) {
                        case 0:
                        case 1:
                            if(! sb::isdir(mpt))
                            {
                                if(sb::exist(mpt)) QFile::remove(mpt);
                                if(! QDir().mkdir(mpt)) goto error;
                            }

                            if(a == 0)
                                mpt.append('/' % sb::right(part, - sb::rinstr(part, "/")));
                            else if(! ckd.contains(part))
                            {
                                if(sb::mount(part, mpt))
                                    ckd.append(part);
                                else
                                    goto merr;
                            }

                            break;
                        case 2:
                        case 3:
                            sb::exec("btrfs subvolume create " % mpt % "/@" % sb::right(mpoint, -1));
                            if(intrrpt) goto exit;

                            if(sb::mount(part, "/.sbsystemcopy" % mpoint, "noatime,subvol=@" % sb::right(mpoint, -1)))
                                ++a;
                            else if(a == 3 || ! QFile::rename(mpt % "/@" % sb::right(mpoint, -1), mpt % "/@" % sb::right(mpoint, -1) % '_' % sb::rndstr()))
                                goto merr;
                        }
                }
                else if(! sb::mount(part, "/.sbsystemcopy" % mpoint))
                    goto merr;
            }

            if(intrrpt) goto exit;
            continue;
        merr:
            dialogdev = part;
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 313 : 329;
            goto error;
        }
    }

    if(ppipe == 0)
    {
        if(pname == tr("Currently running system"))
        {
            if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
                switch(ui->usersettingscopy->checkState()) {
                case Qt::Unchecked:
                    if(! sb::scopy(5, guname(), nullptr)) goto error;
                    break;
                case Qt::PartiallyChecked:
                    if(! sb::scopy(3, guname(), nullptr)) goto error;
                    break;
                case Qt::Checked:
                    if(! sb::scopy(4, guname(), nullptr)) goto error;
                }
            else if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, nullptr) || (sb::schdle[0] == sb::True && ! sb::cfgwrite("/.sbsystemcopy/etc/systemback.conf")))
                goto error;
        }
        else
        {
            if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
            {
                QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                if(! QDir().mkdir("/.systembacklivepoint")) goto error;
            }

            if(intrrpt) goto exit;
            QStr mdev(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs");

            if(! sb::mount(mdev, "/.systembacklivepoint", "loop"))
            {
                dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 331 : 332;
                goto error;
            }

            if(intrrpt) goto exit;

            if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
                switch(ui->usersettingscopy->checkState()) {
                case Qt::Unchecked:
                    if(! sb::scopy(5, guname(), "/.systembacklivepoint")) goto error;
                    break;
                case Qt::PartiallyChecked:
                    if(! sb::scopy(3, guname(), "/.systembacklivepoint")) goto error;
                    break;
                case Qt::Checked:
                    if(! sb::scopy(4, guname(), "/.systembacklivepoint")) goto error;
                }
            else
            {
                if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, "/.systembacklivepoint")) goto error;
                QBA cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));
                if(cfg.contains("enabled=true") && ! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", cfg.replace("enabled=true", "enabled=false"))) goto error;
            }

            sb::umount("/.systembacklivepoint");
            QDir().rmdir("/.systembacklivepoint");
        }
    }
    else
    {
        if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
        {
            if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname)) goto error;
            QBA cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));
            if(cfg.contains("enabled=true") && ! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", cfg.replace("enabled=true", "enabled=false"))) goto error;
        }
        else if(! sb::scopy(ui->usersettingscopy->isChecked() ? 4 : 5, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname))
            goto error;
    }

    if(intrrpt) goto exit;
    sb::Progress = -1;

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        if(sb::exec("chroot /.sbsystemcopy sh -c \"for rmuser in $(grep :\\$6\\$* /etc/shadow | cut -d : -f 1) ; do [ $rmuser = " % guname() % " ] || [ $rmuser = root ] || userdel $rmuser ; done\"") > 0) goto error;
        QStr nfile;

        if(guname() != "root")
        {
            QStr nuname(ui->username->text());

            if(guname() != nuname)
            {
                if(sb::isdir("/.sbsystemcopy/home/" % guname()) && ((sb::exist("/.sbsystemcopy/home/" % nuname) && ! QFile::rename("/.sbsystemcopy/home/" % nuname, "/.sbsystemcopy/home/" % nuname % '_' % sb::rndstr())) || ! QFile::rename("/.sbsystemcopy/home/" % guname(), "/.sbsystemcopy/home/" % nuname))) goto error;

                if(sb::isfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"))
                {
                    QStr cnt(sb::fload("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"));
                    if(cnt.contains("file:///home/" % guname() % '/') && ! sb::crtfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks", cnt.replace("file:///home/" % guname() % '/', "file:///home/" % nuname % '/'))) goto error;
                }

                if(sb::isfile("/.sbsystemcopy/etc/subuid") && sb::isfile("/.sbsystemcopy/etc/subgid"))
                {
                    {
                        QFile file("/.sbsystemcopy/etc/subuid");
                        if(! file.open(QIODevice::ReadOnly)) goto error;

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) goto exit;
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subuid", nfile)) goto error;
                    nfile.clear();

                    {
                        QFile file("/.sbsystemcopy/etc/subgid");
                        if(! file.open(QIODevice::ReadOnly)) goto error;

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) goto exit;
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subgid", nfile)) goto error;
                    nfile.clear();
                }
            }

            for(cQStr &cfname : {"/.sbsystemcopy/etc/group", "/.sbsystemcopy/etc/gshadow"})
            {
                QFile file(cfname);
                if(! file.open(QIODevice::ReadOnly)) goto error;

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
                    if(intrrpt) goto exit;
                }

                if(! sb::crtfile(cfname , nfile)) goto error;
                nfile.clear();
            }

            QFile file("/.sbsystemcopy/etc/passwd");
            if(! file.open(QIODevice::ReadOnly)) goto error;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(guname() % ':'))
                {
                    QSL uslst(cline.split(':'));
                    QStr nline;

                    for(uchar a(0) ; a < uslst.count() ; ++a)
                        switch(a) {
                        case 0:
                            nline.append(nuname % ':');
                            break;
                        case 4:
                            nline.append(ui->fullname->text() % ",,,:");
                            break;
                        case 5:
                            nline.append("/home/" % nuname % ':');
                            break;
                        default:
                            nline.append(uslst.at(a) % ':');
                        }

                    nfile.append(sb::left(nline, -1) % '\n');
                }
                else
                    nfile.append(cline % '\n');

                if(intrrpt) goto exit;
            }

            if(! sb::crtfile("/.sbsystemcopy/etc/passwd", nfile)) goto error;
            nfile.clear();
        }

        {
            QFile file("/.sbsystemcopy/etc/shadow");
            if(! file.open(QIODevice::ReadOnly)) goto error;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(guname() % ':'))
                {
                    QSL uslst(cline.split(':'));
                    QStr nline;

                    for(uchar a(0) ; a < uslst.count() ; ++a)
                        switch(a) {
                        case 0:
                            nline.append((guname() == "root" ? "root" : ui->username->text()) % ':');
                            break;
                        case 1:
                            nline.append(QStr(crypt(chr(ui->password1->text()), chr(("$6$" % sb::rndstr(16))))) % ':');
                            break;
                        default:
                            nline.append(uslst.at(a) % ':');
                        }

                    nfile.append(sb::left(nline, -1) % '\n');
                }
                else if(cline.startsWith("root:"))
                {
                    QSL uslst(cline.split(':'));
                    QStr nline;

                    for(uchar a(0) ; a < uslst.count() ; ++a)
                        switch(a) {
                        case 1:
                            nline.append((ui->rootpassword1->text().isEmpty() ? "!" : QStr(crypt(chr(ui->rootpassword1->text()), chr(("$6$" % sb::rndstr(16)))))) % ':');
                            break;
                        default:
                            nline.append(uslst.at(a) % ':');
                        }

                    nfile.append(sb::left(nline, -1) % '\n');
                }
                else
                    nfile.append(cline % '\n');

                if(intrrpt) goto exit;
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/shadow", nfile)) goto error;
        nfile.clear();

        {
            QFile file("/.sbsystemcopy/etc/hostname");
            if(! file.open(QIODevice::ReadOnly)) goto error;
            QStr ohname(file.readLine().trimmed()), nhname(ui->hostname->text());
            file.close();

            if(ohname != nhname)
            {
                if(! sb::crtfile("/.sbsystemcopy/etc/hostname", nhname % '\n')) goto error;
                file.setFileName("/.sbsystemcopy/etc/hosts");
                if(! file.open(QIODevice::ReadOnly)) goto error;

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    nline.replace('\t' % ohname % '\t', '\t' % nhname % '\t');
                    if(nline.endsWith('\t' % ohname)) nline.replace(nline.length() - ohname.length(), ohname.length(), nhname);
                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/hosts", nfile)) goto error;
                nfile.clear();
            }
        }

        QBA ddm(sb::isfile("/.sbsystemcopy/etc/X11/default-display-manager") ? sb::fload("/.sbsystemcopy/etc/X11/default-display-manager").trimmed() : nullptr);

        if(sb::like(ddm, {"*lightdm_", "*kdm_"}))
        {
            QStr fpath("/.sbsystemcopy/etc/"), vrbl;

            if(ddm.endsWith("lightdm"))
            {
                fpath.append("lightdm/lightdm.conf");
                vrbl = "autologin-user=";
            }
            else
            {
                fpath.append("kde4/kdm/kdmrc");
                vrbl = "AutoLoginUser=";
            }

            if(sb::isfile(fpath))
            {
                QFile file(fpath);
                if(! file.open(QIODevice::ReadOnly)) goto error;
                uchar mdfd(0);

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(mdfd == 0 && nline.startsWith(vrbl))
                    {
                        if(nline.endsWith('='))
                            break;
                        else if(nline.endsWith('=' % guname()))
                        {
                            nline = vrbl % ui->username->text();
                            ++mdfd;
                        }
                        else
                        {
                            nline = vrbl;
                            mdfd = vrbl.at(0).isUpper() ? 2 : 1;
                        }
                    }

                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }

                if(mdfd > 0 && ! sb::crtfile(fpath, mdfd == 1 ? nfile : nfile.replace("AutoLoginEnabled=true", "AutoLoginEnabled=false"))) goto error;
            }
        }
        else if(sb::like(ddm, {"*gdm3_", "*mdm_"}))
        {
            QStr fpath(ddm.endsWith("gdm3") ? "/.sbsystemcopy/etc/gdm3/daemon.conf" : "/.sbsystemcopy/etc/mdm/mdm.conf");

            if(sb::isfile(fpath))
            {
                QFile file(fpath);
                if(! file.open(QIODevice::ReadOnly)) goto error;
                uchar mdfd(0);

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(sb::like(nline, {"_AutomaticLogin=*", "_TimedLogin=*"}))
                        if(! nline.endsWith('='))
                        {
                            if(nline.endsWith('=' % guname()))
                            {
                                nline = sb::left(nline, sb::instr(nline, "=")) % ui->username->text();
                                if(mdfd == 0) ++mdfd;
                            }
                            else
                            {
                                nline = sb::left(nline, sb::instr(nline, "="));
                                mdfd = nline.at(0) == 'A' ? mdfd == 3 ? 4 : 2 : mdfd == 2 ? 4 : 3;
                            }
                        }

                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }

                if(mdfd > 0)
                {
                    if(mdfd > 1)
                    {
                        if(sb::like(mdfd, {2, 4})) nfile.replace("AutomaticLoginEnable=true", "AutomaticLoginEnable=false");
                        if(sb::like(mdfd, {3, 4})) nfile.replace("TimedLoginEnable=true", "TimedLoginEnable=false");
                    }

                    if(! sb::crtfile(fpath, nfile)) goto error;
                }
            }
        }

        QBA mid(sb::rndstr(16).toUtf8().toHex() % '\n');
        if(intrrpt) goto exit;
        if(! sb::crtfile("/.sbsystemcopy/etc/machine-id", mid) || (sb::isdir("/.sbsystemcopy/var/lib/dbus") && ! QFile::setPermissions("/.sbsystemcopy/etc/machine-id", QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther) && ! sb::crtfile("/.sbsystemcopy/var/lib/dbus/machine-id", mid))) goto error;
    }

    if(intrrpt) goto exit;

    {
        QStr fstabtxt("# /etc/fstab: static file system information.\n#\n# Use 'blkid' to print the universally unique identifier for a\n# device; this may be used with UUID= as a more robust way to name devices\n# that works even if disks are added and removed. See fstab(5).\n#\n# <file system> <mount point>   <type>  <options>       <dump>  <pass>\n");

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! nmpt.isEmpty())
            {
                if(nmpt == "/home" && ! ui->partitionsettings->item(a, 3)->text().isEmpty())
                {
                    QFile file("/etc/fstab");
                    if(! file.open(QIODevice::ReadOnly)) goto error;

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());

                        if(sb::like(cline, {"* /home *", "*\t/home *", "* /home\t*", "*\t/home\t*", "* /home/ *", "*\t/home/ *", "* /home/\t*", "*\t/home/\t*"}))
                        {
                            fstabtxt.append("# /home\n" % cline % '\n');
                            break;
                        }
                    }
                }
                else
                {
                    QStr uuid(sb::ruuid(ui->partitionsettings->item(a, 0)->text())), nfs(ui->partitionsettings->item(a, 5)->text());

                    if(nmpt == "/")
                    {
                        if(sb::like(nfs, {"_ext4_", "_ext3_", "_ext2_", "_jfs_", "_xfs_"}))
                            fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   /   " % nfs % "   noatime,errors=remount-ro   0   1\n");
                        else if(nfs == "reiserfs")
                            fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   /   " % nfs % "   noatime,notail   0   1\n");
                        else
                            fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   /   " % nfs % "   noatime,subvol=@   0   1\n");
                    }
                    else if(nmpt == "SWAP")
                        fstabtxt.append("# SWAP\nUUID=" % uuid % "   none   swap   sw   0   0\n");
                    else if(nfs == "reiserfs")
                        fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   " % nmpt % "   reiserfs   noatime,notail   0   2\n");
                    else if(nfs == "btrfs")
                        fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   " % nmpt % "   btrfs   noatime,subvol=@" % sb::right(nmpt, -1) % "   0   2\n");
                    else
                        fstabtxt.append("# " % nmpt % "\nUUID=" % uuid % "   " % nmpt % "   " % nfs % "   noatime   0   2\n");
                }
            }

            if(intrrpt) goto exit;
        }

        if(sb::isfile("/etc/fstab"))
        {
            QFile file("/etc/fstab");
            if(! file.open(QIODevice::ReadOnly)) goto error;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(! cline.startsWith('#'))
                {
                    if(sb::like(cline, {"*/dev/cdrom*", "*/dev/sr*"}))
                        fstabtxt.append("# cdrom\n" % cline % '\n');
                    else if(cline.contains("/dev/fd"))
                        fstabtxt.append("# floppy\n" % cline % '\n');
                }

                if(intrrpt) goto exit;
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/fstab", fstabtxt)) goto error;
    }

    {
        QStr cfpath(ppipe == 0 ? "/etc/crypttab" : QStr(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/crypttab"));

        if(sb::isfile(cfpath))
        {
            QFile file(cfpath);
            if(! file.open(QIODevice::ReadOnly)) goto error;

            while(! file.atEnd())
            {
                QBA cline(file.readLine().trimmed());

                if(! cline.startsWith('#') && cline.contains("UUID="))
                {
                    if(intrrpt) goto exit;
                    if(! sb::crtfile("/.sbsystemcopy/etc/mtab") || sb::exec("chroot /.sbsystemcopy update-initramfs -tck all") > 0) goto error;
                    break;
                }
            }
        }
    }

    if(ui->grubinstallcopy->isVisibleTo(ui->copypanel) && ui->grubinstallcopy->currentText() != tr("Disabled"))
    {
        if(intrrpt) goto exit;
        { QSL mlst{"dev", "dev/pts", "proc", "sys"};
        if(sb::mcheck("/run")) mlst.append("/run");
        for(cQStr &bpath : mlst) sb::mount('/' % bpath, "/.sbsystemcopy/" % bpath); }

        if(sb::exec("chroot /.sbsystemcopy sh -c \"update-grub ; grub-install --force " % (ui->grubinstallcopy->currentText() == "Auto" ? gdetect("/.sbsystemcopy") : grub.isEFI ? nullptr : ui->grubinstallcopy->currentText()) % "\"") > 0)
        {
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 307 : 314;
            goto error;
        }
    }
    else if(sb::execsrch("update-grub", "/.sbsystemcopy"))
    {
        if(intrrpt) goto exit;
        { QSL mlst{"dev", "dev/pts", "proc", "sys"};
        if(sb::mcheck("/run")) mlst.append("/run");
        for(cQStr &bpath : mlst) sb::mount('/' % bpath, "/.sbsystemcopy/" % bpath); }
        sb::exec("chroot /.sbsystemcopy update-grub");
    }

    if(intrrpt) goto exit;
    prun = sb::ecache == sb::True ? tr("Emptying cache") : tr("Flushing filesystem buffers");

    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, {"* /.sbsystemcopy*", "* /.sbmountpoints*"})) sb::umount(cline.split(' ').at(1));
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
    dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 206 : 209;
    dialogopen();
}

void systemback::livewrite()
{
    QStr ldev(ui->livedevices->item(ui->livedevices->currentRow(), 0)->text());
    goto start;
error:
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
    {
        switch(dialog) {
        case 108:
            dialog = 335;
            break;
        case 336:
        case 337:
            dialogdev = ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1';
        }

        dialogopen();
    }

    return;
start:
    statustart();
    prun = tr("Writing Live image to target device");

    if(! sb::exist(ldev))
    {
        dialog = 337;
        goto error;
    }
    else if(sb::mcheck(ldev))
    {
        for(cQStr &sitem : QDir("/dev").entryList(QDir::System))
        {
            QStr item("/dev/" % sitem);

            if(item.length() > (ldev.contains("mmc") ? 12 : 8) && item.startsWith(ldev))
                while(sb::mcheck(item)) sb::umount(item);

            if(intrrpt) goto error;
        }

        if(sb::mcheck(ldev)) sb::umount(ldev);
        sb::fssync();
        if(intrrpt) goto error;
    }

    if(! sb::mkptable(ldev))
    {
        dialog = 337;
        goto error;
    }

    if(intrrpt) goto error;
    ullong size(sb::fsize(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive"));
    QStr lrdir;

    if(size < 4294967295)
    {
        lrdir = "sblive";

        if(! sb::mkpart(ldev))
        {
            dialog = 337;
            goto error;
        }
    }
    else
    {
        lrdir = "sbroot";

        if(! sb::mkpart(ldev, 1048576, 104857600) || ! sb::mkpart(ldev))
        {
            dialog = 337;
            goto error;
        }

        if(intrrpt) goto error;

        if(sb::exec("mkfs.ext2 -FL SBROOT " % ldev % (ldev.contains("mmc") ? "p" : nullptr) % '2') > 0)
        {
            dialog = 337;
            goto error;
        }
    }

    if(intrrpt) goto error;

    if(sb::exec("mkfs.vfat -F 32 -n SBLIVE " % ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1') > 0)
    {
        dialog = 337;
        goto error;
    }

    if(intrrpt ||
        sb::exec("dd if=/usr/lib/syslinux/" % QStr(sb::isfile("/usr/lib/syslinux/mbr.bin") ? nullptr : "mbr/") % "mbr.bin of=" % ldev % " conv=notrunc bs=440 count=1") > 0 || ! sb::setpflag(ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1', "boot") || ! sb::setpflag(ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1', "lba") ||
        intrrpt ||
        (sb::exist("/.sblivesystemwrite") && (((sb::mcheck("/.sblivesystemwrite/sblive") && ! sb::umount("/.sblivesystemwrite/sblive")) || (sb::mcheck("/.sblivesystemwrite/sbroot") && ! sb::umount("/.sblivesystemwrite/sbroot"))) || ! sb::remove("/.sblivesystemwrite"))) ||
        intrrpt ||
        ! QDir().mkdir("/.sblivesystemwrite") || ! QDir().mkdir("/.sblivesystemwrite/sblive")) goto error;

    if(! sb::mount(ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1', "/.sblivesystemwrite/sblive"))
    {
        dialog = 336;
        goto error;
    }

    if(intrrpt) goto error;

    if(lrdir == "sbroot")
    {
        if(! QDir().mkdir("/.sblivesystemwrite/sbroot")) goto error;

        if(! sb::mount(ldev % (ldev.contains("mmc") ? "p" : nullptr) % '2', "/.sblivesystemwrite/sbroot"))
        {
            dialog = 336;
            goto error;
        }
    }

    if(intrrpt) goto error;

    if(sb::dfree("/.sblivesystemwrite/" % lrdir) < size + 52428800)
    {
        dialog = 321;
        goto error;
    }

    sb::ThrdStr[0] = "/.sblivesystemwrite";
    sb::ThrdLng[0] = size;

    if(lrdir == "sblive")
    {
        if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --no-same-owner --no-same-permissions") > 0)
        {
            dialog = 322;
            goto error;
        }
    }
    else if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --exclude=casper/filesystem.squashfs --exclude=live/filesystem.squashfs --no-same-permissions --no-same-owner") > 0)
    {
        dialog = 322;
        goto error;
    }
    else if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sbroot --exclude=.disk --exclude=boot --exclude=EFI --exclude=syslinux --exclude=casper/initrd.gz --exclude=casper/vmlinuz --exclude=live/initrd.gz --exclude=live/vmlinuz --no-same-owner --no-same-permissions") > 0)
    {
        dialog = 322;
        goto error;
    }

    prun = sb::ecache == sb::True ? tr("Emptying cache") : tr("Flushing filesystem buffers");
    if(sb::exec("syslinux -ifd syslinux " % ldev % (ldev.contains("mmc") ? "p" : nullptr) % '1') > 0) goto error;
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
    dialog = 210;
    dialogopen();
}

void systemback::dialogopen(schar snum)
{
    if(ui->dialognumber->isVisibleTo(ui->dialogpanel)) ui->dialognumber->hide();

    if(dialog < 200)
    {
        if(ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->hide();
        if(ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->hide();
        if(! ui->dialogquestion->isVisibleTo(ui->dialogpanel)) ui->dialogquestion->show();
        if(! ui->dialogcancel->isVisibleTo(ui->dialogpanel)) ui->dialogcancel->show();
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));

        switch(dialog) {
        case 100:
            ui->dialogtext->setText(tr("Restore the system files to the following restore point:") % "<p><b>" % pname);
            break;
        case 101:
            ui->dialogtext->setText(tr("Repair the system files with the following restore point:") % "<p><b>" % pname);
            break;
        case 102:
            ui->dialogtext->setText(tr("Repair the complete system with the following restore point:") % "<p><b>" % pname);
            break;
        case 103:
            ui->dialogtext->setText(tr("Restore the complete user(s) configuration files to the following restore point:") % "<p><b>" % pname);
            break;
        case 104:
            ui->dialogtext->setText(tr("Restore the user(s) configuration files to the following restore point:") % "<p><b>" % pname);
            break;
        case 105:
            ui->dialogtext->setText(tr("Copy the system, using the following restore point:") % "<p><b>" % pname);
            break;
        case 106:
            ui->dialogtext->setText(tr("Install the system, using the following restore point:") % "<p><b>" % pname);
            break;
        case 107:
            ui->dialogtext->setText(tr("Restore complete system to the following restore point:") % "<p><b>" % pname);
            break;
        case 108:
            ui->dialogtext->setText(tr("Format the %1, and write the following Live system image:").arg(" <b>" % ui->livedevices->item(ui->livedevices->currentRow(), 0)->text() % "</b>") % "<p><b>" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % "</b>");
            break;
        case 109:
            ui->dialogtext->setText(tr("Repair the GRUB 2 bootloader."));
        }
    }
    else
    {
        if(ui->dialogquestion->isVisibleTo(ui->dialogpanel)) ui->dialogquestion->hide();
        if(ui->dialogcancel->isVisibleTo(ui->dialogpanel)) ui->dialogcancel->hide();
        bool cntd(false);

        if(dialog < 300)
        {
            if(ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->hide();
            if(! ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->show();

            switch(dialog) {
            case 200:
                ui->dialogtext->setText(tr("User(s) configuration files full restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds."));
                if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                ui->dialogcancel->show();
                ui->dialognumber->show();
                cntd = true;
                break;
            case 201:
                ui->dialogtext->setText(tr("User(s) configuration files restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds."));
                if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                ui->dialogcancel->show();
                ui->dialognumber->show();
                cntd = true;
                break;
            case 202:
                ui->dialogtext->setText(tr("Full system repair is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 203:
                ui->dialogtext->setText(tr("System repair is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 204:
                ui->dialogtext->setText(tr("System files restoration are completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds."));
                if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                ui->dialogcancel->show();
                ui->dialognumber->show();
                cntd = true;
                break;
            case 205:
                ui->dialogtext->setText(tr("Full system restoration is completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds."));
                if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                ui->dialogcancel->show();
                ui->dialognumber->show();
                cntd = true;
                break;
            case 206:
                ui->dialogtext->setText(tr("System copy is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 207:
                ui->dialogtext->setText(tr("Live system creation is completed.") % "<p>" % tr("The created .sblive file can be written to pendrive."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 208:
                ui->dialogtext->setText(tr("GRUB 2 repair is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 209:
                ui->dialogtext->setText(tr("System install is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                break;
            case 210:
                ui->dialogtext->setText(tr("Live system image write is completed."));
                if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
            }

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

            switch(dialog) {
            case 300:
                ui->dialogtext->setText(tr("Another systemback process is currently running, please wait until it finishes."));
                break;
            case 301:
                ui->dialogtext->setText(tr("Unable to get exclusive lock!") % "<p>" % tr("First, close all package manager."));
                break;
            case 302:
                ui->dialogtext->setText(tr("The specified name contain(s) unsupported character(s)!") % "<p>" % tr("Please enter a new name."));
                break;
            case 303:
                ui->dialogtext->setText(tr("System files repair are completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
                break;
            case 304:
                ui->dialogtext->setText(tr("Restore point creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process."));
                break;
            case 305:
                ui->dialogtext->setText(tr("Root privileges are required for running Systemback!"));
                break;
            case 306:
                ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to copy the system. The copied system will not function properly."));
                break;
            case 307:
                ui->dialogtext->setText(tr("System copy is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader."));
                break;
            case 308:
                ui->dialogtext->setText(tr("System restoration is aborted!") % "<p>" % tr("An error occurred while reinstalling GRUB."));
                break;
            case 309:
                ui->dialogtext->setText(tr("Full system repair is completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
                break;
            case 310:
                ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating file system image."));
                break;
            case 311:
                ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating container file."));
                break;
            case 312:
                ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process."));
                break;
            case 313:
                ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dialogdev);
                break;
            case 314:
                ui->dialogtext->setText(tr("System install is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader."));
                break;
            case 315:
                ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to install the system. The installed system will not function properly."));
                break;
            case 316:
                ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
                break;
            case 317:
                ui->dialogtext->setText(tr("An error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
                break;
            case 318:
                ui->dialogtext->setText(tr("Restore point creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 319:
                ui->dialogtext->setText(tr("System copying is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 320:
                ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 321:
                ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The selected device does not have enough space to write the Live system."));
                break;
            case 322:
                ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("An error occurred while unpacking Live system files."));
                break;
            case 323:
                ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while renaming essential Live files."));
                break;
            case 324:
                ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while creating .iso image."));
                break;
            case 325:
                ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while reading .sblive image."));
                break;
            case 326:
                ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating new initramfs image."));
                break;
            case 327:
                ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 328:
                ui->dialogtext->setText(tr("Restore point deletion is aborted!") % "<p>" % tr("An error occurred while during the process."));
                break;
            case 329:
                ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dialogdev);
                break;
            case 330:
                ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
                break;
            case 331:
                ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The Live image could not be mounted."));
                break;
            case 332:
                ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The Live image could not be mounted."));
                break;
            case 333:
                ui->dialogtext->setText(tr("System repair is aborted!") % "<p>" % tr("The Live image could not be mounted."));
                break;
            case 334:
                ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 335:
                ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
                break;
            case 336:
                ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dialogdev);
                break;
            case 337:
                ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
                break;
            case 338:
                ui->dialogtext->setText(tr("System restoration is aborted!") % "<p>" % tr("There is not enough free space."));
                break;
            case 339:
                ui->dialogtext->setText(tr("System repair is aborted!") % "<p>" % tr("There is not enough free space."));
            }
        }
    }

    if(ui->mainpanel->isVisible()) ui->mainpanel->hide();
    if(ui->statuspanel->isVisible()) ui->statuspanel->hide();
    if(ui->schedulerpanel->isVisible()) ui->schedulerpanel->hide();
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
        ev.xclient.type = ClientMessage;
        ev.xclient.message_type = XInternAtom(dsply, "_NET_WM_STATE", 0);
        ev.xclient.display = dsply;
        ev.xclient.window = winId();
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = state ? 1 : 0;
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_ABOVE", 0);
        XSendEvent(dsply, XDefaultRootWindow(dsply), 0, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_STAYS_ON_TOP", 0);
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
        if(ui->function1->foregroundRole() == QPalette::Dark)
        {
            ui->function1->setForegroundRole(QPalette::Base);
            ui->windowbutton1->setForegroundRole(QPalette::Base);
            ui->function2->setForegroundRole(QPalette::Base);
            ui->windowbutton2->setForegroundRole(QPalette::Base);
            ui->function3->setForegroundRole(QPalette::Base);
            ui->windowbutton3->setForegroundRole(QPalette::Base);
            ui->function4->setForegroundRole(QPalette::Base);
            ui->windowbutton4->setForegroundRole(QPalette::Base);
            goto gcheck;
        }

        return false;
    case QEvent::WindowDeactivate:
        if(ui->function1->foregroundRole() == QPalette::Base)
        {
            ui->function1->setForegroundRole(QPalette::Dark);
            ui->windowbutton1->setForegroundRole(QPalette::Dark);
            ui->function2->setForegroundRole(QPalette::Dark);
            ui->windowbutton2->setForegroundRole(QPalette::Dark);
            ui->function3->setForegroundRole(QPalette::Dark);
            ui->windowbutton3->setForegroundRole(QPalette::Dark);
            ui->function4->setForegroundRole(QPalette::Dark);
            ui->windowbutton4->setForegroundRole(QPalette::Dark);
            if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();

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
            ui->partitionsettingspanel2->move(ui->partitionsettingspanel1->pos());
            ui->partitionsettingspanel3->move(ui->partitionsettingspanel1->pos());
            ui->partitionoptionstext->setGeometry(0, ui->copypanel->height() - ss(160), ui->copypanel->width(), ui->partitionoptionstext->height());
            ui->userdatafilescopy->move(ui->userdatafilescopy->x(), ui->copypanel->height() - ss(122) - (sfctr == High ? 1 : 2));
            ui->usersettingscopy->move(ui->usersettingscopy->x(), ui->userdatafilescopy->y());
            ui->grubinstalltext->move(ui->grubinstalltext->x(), ui->copypanel->height() - ss(96));
            ui->grubinstallcopy->move(ui->grubinstallcopy->x(), ui->grubinstalltext->y());
            ui->grubinstallcopydisable->move(ui->grubinstallcopydisable->x(), ui->grubinstalltext->y());
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

        if(! wismax && size() != QSize(wgeom[2], wgeom[3]))
        {
            wgeom[2] = width();
            wgeom[3] = height();
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
            wgeom[0] = x();
            wgeom[1] = y();
        }

        return false;
    case QEvent::WindowStateChange:
    {
        if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
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

        if(size() != QSize(wgeom[0], wgeom[1])) move(wgeom[0], wgeom[1]);
    }

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
                if(ui->pointrename->isEnabled() && ((ui->point1->hasFocus() && ui->pointpipe1->isChecked()) ||
                                                    (ui->point2->hasFocus() && ui->pointpipe2->isChecked()) ||
                                                    (ui->point3->hasFocus() && ui->pointpipe3->isChecked()) ||
                                                    (ui->point4->hasFocus() && ui->pointpipe4->isChecked()) ||
                                                    (ui->point5->hasFocus() && ui->pointpipe5->isChecked()) ||
                                                    (ui->point6->hasFocus() && ui->pointpipe6->isChecked()) ||
                                                    (ui->point7->hasFocus() && ui->pointpipe7->isChecked()) ||
                                                    (ui->point8->hasFocus() && ui->pointpipe8->isChecked()) ||
                                                    (ui->point9->hasFocus() && ui->pointpipe9->isChecked()) ||
                                                    (ui->point10->hasFocus() && ui->pointpipe10->isChecked()) ||
                                                    (ui->point11->hasFocus() && ui->pointpipe11->isChecked()) ||
                                                    (ui->point12->hasFocus() && ui->pointpipe12->isChecked()) ||
                                                    (ui->point13->hasFocus() && ui->pointpipe13->isChecked()) ||
                                                    (ui->point14->hasFocus() && ui->pointpipe14->isChecked()) ||
                                                    (ui->point15->hasFocus() && ui->pointpipe15->isChecked()))) on_pointrename_clicked();
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
                if(ui->point1->hasFocus())
                {
                    if(ui->point1->text() != sb::pnames[0]) ui->point1->setText(sb::pnames[0]);
                }
                else if(ui->point2->hasFocus())
                {
                    if(ui->point2->text() != sb::pnames[1]) ui->point2->setText(sb::pnames[1]);
                }
                else if(ui->point3->hasFocus())
                {
                    if(ui->point3->text() != sb::pnames[2]) ui->point3->setText(sb::pnames[2]);
                }
                else if(ui->point4->hasFocus())
                {
                    if(ui->point4->text() != sb::pnames[3]) ui->point4->setText(sb::pnames[3]);
                }
                else if(ui->point5->hasFocus())
                {
                    if(ui->point5->text() != sb::pnames[4]) ui->point5->setText(sb::pnames[4]);
                }
                else if(ui->point6->hasFocus())
                {
                    if(ui->point6->text() != sb::pnames[5]) ui->point6->setText(sb::pnames[5]);
                }
                else if(ui->point7->hasFocus())
                {
                    if(ui->point7->text() != sb::pnames[6]) ui->point7->setText(sb::pnames[6]);
                }
                else if(ui->point8->hasFocus())
                {
                    if(ui->point8->text() != sb::pnames[7]) ui->point8->setText(sb::pnames[7]);
                }
                else if(ui->point9->hasFocus())
                {
                    if(ui->point9->text() != sb::pnames[8]) ui->point9->setText(sb::pnames[8]);
                }
                else if(ui->point10->hasFocus())
                {
                    if(ui->point10->text() != sb::pnames[9]) ui->point10->setText(sb::pnames[9]);
                }
                else if(ui->point11->hasFocus())
                {
                    if(ui->point11->text() != sb::pnames[10]) ui->point11->setText(sb::pnames[10]);
                }
                else if(ui->point12->hasFocus())
                {
                    if(ui->point12->text() != sb::pnames[11]) ui->point12->setText(sb::pnames[11]);
                }
                else if(ui->point13->hasFocus())
                {
                    if(ui->point13->text() != sb::pnames[12]) ui->point13->setText(sb::pnames[12]);
                }
                else if(ui->point14->hasFocus())
                {
                    if(ui->point14->text() != sb::pnames[13]) ui->point14->setText(sb::pnames[13]);
                }
                else if(ui->point15->hasFocus() && ui->point15->text() != sb::pnames[14])
                    ui->point15->setText(sb::pnames[14]);
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
    ui->admins->resize(fontMetrics().width(arg1) + ss(30), ss(32));
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
        ui->adminpassword->setDisabled(true);
        ui->admins->setDisabled(true);
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
    else if(hash.isEmpty())
    {
        if(ui->adminpassworderror->isHidden()) ui->adminpassworderror->show();
    }
    else if(QStr(crypt(chr(arg1), chr(hash))) == hash)
    {
        sb::delay(300);

        if(ccnt == icnt)
        {
            if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
            ui->adminpasswordpipe->show();
            ui->adminpassword->setDisabled(true);
            ui->admins->setDisabled(true);
            ui->passwordinputok->setEnabled(true);
            ui->passwordinputok->setFocus();
        }
    }
    else
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

        if(ui->pointpipe1->isChecked())
            ui->pointpipe1->click();
        else if(ui->pointpipe2->isChecked())
            ui->pointpipe2->click();
        else if(ui->pointpipe3->isChecked())
            ui->pointpipe3->click();
        else if(ui->pointpipe4->isChecked())
            ui->pointpipe4->click();
        else if(ui->pointpipe5->isChecked())
            ui->pointpipe5->click();
        else if(ui->pointpipe6->isChecked())
            ui->pointpipe6->click();
        else if(ui->pointpipe7->isChecked())
            ui->pointpipe7->click();
        else if(ui->pointpipe8->isChecked())
            ui->pointpipe8->click();
        else if(ui->pointpipe9->isChecked())
            ui->pointpipe9->click();
        else if(ui->pointpipe10->isChecked())
            ui->pointpipe10->click();
        else if(ui->pointpipe11->isChecked())
            ui->pointpipe11->click();
        else if(ui->pointpipe12->isChecked())
            ui->pointpipe12->click();
        else if(ui->pointpipe13->isChecked())
            ui->pointpipe13->click();
        else if(ui->pointpipe14->isChecked())
            ui->pointpipe14->click();
        else if(ui->pointpipe15->isChecked())
            ui->pointpipe15->click();
    }

    ui->dialogpanel->hide();
    ui->mainpanel->show();
    ui->functionmenunext->setFocus();
    windowmove(ss(698), ss(465));
    setwontop(false);
}

void systemback::on_pnumber3_clicked()
{
    if(sb::pnumber > 3)
    {
        sb::pnumber = 3;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet("background-color: rgb(255, 103, 103)");

    if(ui->point4->isEnabled())
    {
        if(ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point4->text() == tr("empty"))
        ui->point4->setText(tr("not used"));

    if(ui->point5->isEnabled())
    {
        if(ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point5->text() == tr("empty"))
        ui->point5->setText(tr("not used"));

    if(ui->point6->isEnabled())
    {
        if(ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point6->text() == tr("empty"))
        ui->point6->setText(tr("not used"));

    if(ui->point7->isEnabled())
    {
        if(ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point7->text() == tr("empty"))
        ui->point7->setText(tr("not used"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("empty"))
        ui->point8->setText(tr("not used"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber4_clicked()
{
    if(sb::pnumber != 4)
    {
        sb::pnumber = 4;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point5->text() == tr("empty"))
        ui->point5->setText(tr("not used"));

    if(ui->point6->isEnabled())
    {
        if(ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point6->text() == tr("empty"))
        ui->point6->setText(tr("not used"));

    if(ui->point7->isEnabled())
    {
        if(ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point7->text() == tr("empty"))
        ui->point7->setText(tr("not used"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("empty"))
        ui->point8->setText(tr("not used"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber5_clicked()
{
    if(sb::pnumber != 5)
    {
        sb::pnumber = 5;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point6->text() == tr("empty"))
        ui->point6->setText(tr("not used"));

    if(ui->point7->isEnabled())
    {
        if(ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point7->text() == tr("empty"))
        ui->point7->setText(tr("not used"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("empty"))
        ui->point8->setText(tr("not used"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber6_clicked()
{
    if(sb::pnumber != 6)
    {
        sb::pnumber = 6;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point6->text() == tr("not used"))
        ui->point6->setText(tr("empty"));

    if(ui->point7->isEnabled())
    {
        if(ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point7->text() == tr("empty"))
        ui->point7->setText(tr("not used"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("empty"))
        ui->point8->setText(tr("not used"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber7_clicked()
{
    if(sb::pnumber != 7)
    {
        sb::pnumber = 7;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(! ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet(nullptr);
    }
    else if(ui->point6->text() == tr("not used"))
        ui->point6->setText(tr("empty"));

    if(ui->point7->isEnabled())
    {
        if(ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point7->text() == tr("not used"))
        ui->point7->setText(tr("empty"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("empty"))
        ui->point8->setText(tr("not used"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber8_clicked()
{
    if(sb::pnumber != 8)
    {
        sb::pnumber = 8;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(! ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet(nullptr);
    }
    else if(ui->point6->text() == tr("not used"))
        ui->point6->setText(tr("empty"));

    if(ui->point7->isEnabled())
    {
        if(! ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet(nullptr);
    }
    else if(ui->point7->text() == tr("not used"))
        ui->point7->setText(tr("empty"));

    if(ui->point8->isEnabled())
    {
        if(ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point8->text() == tr("not used"))
        ui->point8->setText(tr("empty"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("empty"))
        ui->point9->setText(tr("not used"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber9_clicked()
{
    if(sb::pnumber < 9)
    {
        sb::pnumber = 9;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(! ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet(nullptr);
    }
    else if(ui->point6->text() == tr("not used"))
        ui->point6->setText(tr("empty"));

    if(ui->point7->isEnabled())
    {
        if(! ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet(nullptr);
    }
    else if(ui->point7->text() == tr("not used"))
        ui->point7->setText(tr("empty"));

    if(ui->point8->isEnabled())
    {
        if(! ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet(nullptr);
    }
    else if(ui->point8->text() == tr("not used"))
        ui->point8->setText(tr("empty"));

    if(ui->point9->isEnabled())
    {
        if(ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point9->text() == tr("not used"))
        ui->point9->setText(tr("empty"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("empty"))
        ui->point10->setText(tr("not used"));

    fontcheck(Rpnts);
}

void systemback::on_pnumber10_clicked()
{
    if(sb::pnumber != 10)
    {
        sb::pnumber = 10;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ui->point3->isEnabled() && ! ui->point3->styleSheet().isEmpty()) ui->point3->setStyleSheet(nullptr);

    if(ui->point4->isEnabled())
    {
        if(! ui->point4->styleSheet().isEmpty()) ui->point4->setStyleSheet(nullptr);
    }
    else if(ui->point4->text() == tr("not used"))
        ui->point4->setText(tr("empty"));

    if(ui->point5->isEnabled())
    {
        if(! ui->point5->styleSheet().isEmpty()) ui->point5->setStyleSheet(nullptr);
    }
    else if(ui->point5->text() == tr("not used"))
        ui->point5->setText(tr("empty"));

    if(ui->point6->isEnabled())
    {
        if(! ui->point6->styleSheet().isEmpty()) ui->point6->setStyleSheet(nullptr);
    }
    else if(ui->point6->text() == tr("not used"))
        ui->point6->setText(tr("empty"));

    if(ui->point7->isEnabled())
    {
        if(! ui->point7->styleSheet().isEmpty()) ui->point7->setStyleSheet(nullptr);
    }
    else if(ui->point7->text() == tr("not used"))
        ui->point7->setText(tr("empty"));

    if(ui->point8->isEnabled())
    {
        if(! ui->point8->styleSheet().isEmpty()) ui->point8->setStyleSheet(nullptr);
    }
    else if(ui->point8->text() == tr("not used"))
        ui->point8->setText(tr("empty"));

    if(ui->point9->isEnabled())
    {
        if(! ui->point9->styleSheet().isEmpty()) ui->point9->setStyleSheet(nullptr);
    }
    else if(ui->point9->text() == tr("not used"))
        ui->point9->setText(tr("empty"));

    if(ui->point10->isEnabled())
    {
        if(ui->point10->styleSheet().isEmpty()) ui->point10->setStyleSheet("background-color: rgb(255, 103, 103)");
    }
    else if(ui->point10->text() == tr("not used"))
        ui->point10->setText(tr("empty"));

    fontcheck(Rpnts);
}

void systemback::on_point1_textChanged(cQStr &arg1)
{
    if(ui->point1->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe1->isChecked()) ui->pointpipe1->click();
            ui->pointpipe1->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point1->cursorPosition() - 1);
            ui->point1->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point1->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[0])
            {
                if(! ui->pointpipe1->isChecked()) ui->pointpipe1->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe1->isChecked())
                ui->pointpipe1->click();

            if(! ui->pointpipe1->isEnabled()) ui->pointpipe1->setEnabled(true);
            if(! ui->point1->hasFocus()) ui->point1->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe1->isEnabled())
    {
        if(ui->pointpipe1->isChecked()) ui->pointpipe1->click();
        ui->pointpipe1->setDisabled(true);
    }
}

void systemback::on_point2_textChanged(cQStr &arg1)
{
    if(ui->point2->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe2->isChecked()) ui->pointpipe2->click();
            ui->pointpipe2->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point2->cursorPosition() - 1);
            ui->point2->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point2->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[1])
            {
                if(! ui->pointpipe2->isChecked()) ui->pointpipe2->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe2->isChecked())
                ui->pointpipe2->click();

            if(! ui->pointpipe2->isEnabled()) ui->pointpipe2->setEnabled(true);
            if(! ui->point2->hasFocus()) ui->point2->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe2->isEnabled())
    {
        if(ui->pointpipe2->isChecked()) ui->pointpipe2->click();
        ui->pointpipe2->setDisabled(true);
    }
}

void systemback::on_point3_textChanged(cQStr &arg1)
{
    if(ui->point3->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe3->isChecked()) ui->pointpipe3->click();
            ui->pointpipe3->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point3->cursorPosition() - 1);
            ui->point3->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point3->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[2])
            {
                if(! ui->pointpipe3->isChecked()) ui->pointpipe3->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe3->isChecked())
                ui->pointpipe3->click();

            if(! ui->pointpipe3->isEnabled()) ui->pointpipe3->setEnabled(true);
            if(! ui->point3->hasFocus()) ui->point3->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe3->isEnabled())
    {
        if(ui->pointpipe3->isChecked()) ui->pointpipe3->click();
        ui->pointpipe3->setDisabled(true);
    }
}

void systemback::on_point4_textChanged(cQStr &arg1)
{
    if(ui->point4->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe4->isChecked()) ui->pointpipe4->click();
            ui->pointpipe4->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point4->cursorPosition() - 1);
            ui->point4->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point4->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[3])
            {
                if(! ui->pointpipe4->isChecked()) ui->pointpipe4->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe4->isChecked())
                ui->pointpipe4->click();

            if(! ui->pointpipe4->isEnabled()) ui->pointpipe4->setEnabled(true);
            if(! ui->point4->hasFocus()) ui->point4->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe4->isEnabled())
    {
        if(ui->pointpipe4->isChecked()) ui->pointpipe4->click();
        ui->pointpipe4->setDisabled(true);
    }
}

void systemback::on_point5_textChanged(cQStr &arg1)
{
    if(ui->point5->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe5->isChecked()) ui->pointpipe5->click();
            ui->pointpipe5->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point5->cursorPosition() - 1);
            ui->point5->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point5->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[4])
            {
                if(! ui->pointpipe5->isChecked()) ui->pointpipe5->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe5->isChecked())
                ui->pointpipe5->click();

            if(! ui->pointpipe5->isEnabled()) ui->pointpipe5->setEnabled(true);
            if(! ui->point5->hasFocus()) ui->point5->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe5->isEnabled())
    {
        if(ui->pointpipe5->isChecked()) ui->pointpipe5->click();
        ui->pointpipe5->setDisabled(true);
    }
}

void systemback::on_point6_textChanged(cQStr &arg1)
{
    if(ui->point6->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe6->isChecked()) ui->pointpipe6->click();
            ui->pointpipe6->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point6->cursorPosition() - 1);
            ui->point6->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point6->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[5])
            {
                if(! ui->pointpipe6->isChecked()) ui->pointpipe6->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe6->isChecked())
                ui->pointpipe6->click();

            if(! ui->pointpipe6->isEnabled()) ui->pointpipe6->setEnabled(true);
            if(! ui->point6->hasFocus()) ui->point6->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe6->isEnabled())
    {
        if(ui->pointpipe6->isChecked()) ui->pointpipe6->click();
        ui->pointpipe6->setDisabled(true);
    }
}

void systemback::on_point7_textChanged(cQStr &arg1)
{
    if(ui->point7->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe7->isChecked()) ui->pointpipe7->click();
            ui->pointpipe7->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point7->cursorPosition() - 1);
            ui->point7->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point7->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[6])
            {
                if(! ui->pointpipe7->isChecked()) ui->pointpipe7->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe7->isChecked())
                ui->pointpipe7->click();

            if(! ui->pointpipe7->isEnabled()) ui->pointpipe7->setEnabled(true);
            if(! ui->point7->hasFocus()) ui->point7->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe7->isEnabled())
    {
        if(ui->pointpipe7->isChecked()) ui->pointpipe7->click();
        ui->pointpipe7->setDisabled(true);
    }
}

void systemback::on_point8_textChanged(cQStr &arg1)
{
    if(ui->point8->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe8->isChecked()) ui->pointpipe8->click();
            ui->pointpipe8->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point8->cursorPosition() - 1);
            ui->point8->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point8->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[7])
            {
                if(! ui->pointpipe8->isChecked()) ui->pointpipe8->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe8->isChecked())
                ui->pointpipe8->click();

            if(! ui->pointpipe8->isEnabled()) ui->pointpipe8->setEnabled(true);
            if(! ui->point8->hasFocus()) ui->point8->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe8->isEnabled())
    {
        if(ui->pointpipe8->isChecked()) ui->pointpipe8->click();
        ui->pointpipe8->setDisabled(true);
    }
}

void systemback::on_point9_textChanged(cQStr &arg1)
{
    if(ui->point9->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe9->isChecked()) ui->pointpipe9->click();
            ui->pointpipe9->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point9->cursorPosition() - 1);
            ui->point9->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point9->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[8])
            {
                if(! ui->pointpipe9->isChecked()) ui->pointpipe9->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe9->isChecked())
                ui->pointpipe9->click();

            if(! ui->pointpipe9->isEnabled()) ui->pointpipe9->setEnabled(true);
            if(! ui->point9->hasFocus()) ui->point9->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe9->isEnabled())
    {
        if(ui->pointpipe9->isChecked()) ui->pointpipe9->click();
        ui->pointpipe9->setDisabled(true);
    }
}

void systemback::on_point10_textChanged(cQStr &arg1)
{
    if(ui->point10->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe10->isChecked()) ui->pointpipe10->click();
            ui->pointpipe10->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point10->cursorPosition() - 1);
            ui->point10->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point10->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[9])
            {
                if(! ui->pointpipe10->isChecked()) ui->pointpipe10->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe10->isChecked())
                ui->pointpipe10->click();

            if(! ui->pointpipe10->isEnabled()) ui->pointpipe10->setEnabled(true);
            if(! ui->point10->hasFocus()) ui->point10->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe10->isEnabled())
    {
        if(ui->pointpipe10->isChecked()) ui->pointpipe10->click();
        ui->pointpipe10->setDisabled(true);
    }
}

void systemback::on_point11_textChanged(cQStr &arg1)
{
    if(ui->point11->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe11->isChecked()) ui->pointpipe11->click();
            ui->pointpipe11->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point11->cursorPosition() - 1);
            ui->point11->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point11->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[10])
            {
                if(! ui->pointpipe11->isChecked()) ui->pointpipe11->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe11->isChecked())
                ui->pointpipe11->click();

            if(! ui->pointpipe11->isEnabled()) ui->pointpipe11->setEnabled(true);
            if(! ui->point11->hasFocus()) ui->point11->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe11->isEnabled())
    {
        if(ui->pointpipe11->isChecked()) ui->pointpipe11->click();
        ui->pointpipe11->setDisabled(true);
    }
}

void systemback::on_point12_textChanged(cQStr &arg1)
{
    if(ui->point12->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe12->isChecked()) ui->pointpipe12->click();
            ui->pointpipe12->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point12->cursorPosition() - 1);
            ui->point12->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point12->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[11])
            {
                if(! ui->pointpipe12->isChecked()) ui->pointpipe12->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe12->isChecked())
                ui->pointpipe12->click();

            if(! ui->pointpipe12->isEnabled()) ui->pointpipe12->setEnabled(true);
            if(! ui->point12->hasFocus()) ui->point12->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe12->isEnabled())
    {
        if(ui->pointpipe12->isChecked()) ui->pointpipe12->click();
        ui->pointpipe12->setDisabled(true);
    }
}

void systemback::on_point13_textChanged(cQStr &arg1)
{
    if(ui->point13->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe13->isChecked()) ui->pointpipe13->click();
            ui->pointpipe13->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point13->cursorPosition() - 1);
            ui->point13->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point13->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[12])
            {
                if(! ui->pointpipe13->isChecked()) ui->pointpipe13->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe13->isChecked())
                ui->pointpipe13->click();

            if(! ui->pointpipe13->isEnabled()) ui->pointpipe13->setEnabled(true);
            if(! ui->point13->hasFocus()) ui->point13->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe13->isEnabled())
    {
        if(ui->pointpipe13->isChecked()) ui->pointpipe13->click();
        ui->pointpipe13->setDisabled(true);
    }
}

void systemback::on_point14_textChanged(cQStr &arg1)
{
    if(ui->point14->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe14->isChecked()) ui->pointpipe14->click();
            ui->pointpipe14->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point14->cursorPosition() - 1);
            ui->point14->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point14->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[13])
            {
                if(! ui->pointpipe14->isChecked()) ui->pointpipe14->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe14->isChecked())
                ui->pointpipe14->click();

            if(! ui->pointpipe14->isEnabled()) ui->pointpipe14->setEnabled(true);
            if(! ui->point14->hasFocus()) ui->point14->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe14->isEnabled())
    {
        if(ui->pointpipe14->isChecked()) ui->pointpipe14->click();
        ui->pointpipe14->setDisabled(true);
    }
}

void systemback::on_point15_textChanged(cQStr &arg1)
{
    if(ui->point15->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe15->isChecked()) ui->pointpipe15->click();
            ui->pointpipe15->setDisabled(true);
        }
        else if(sb::like(arg1, {"* *", "*/*"}))
        {
            uchar cpos(ui->point15->cursorPosition() - 1);
            ui->point15->setText(QStr(arg1).replace(cpos, 1, nullptr));
            ui->point15->setCursorPosition(cpos);
        }
        else
        {
            if(arg1 != sb::pnames[14])
            {
                if(! ui->pointpipe15->isChecked()) ui->pointpipe15->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ui->pointpipe15->isChecked())
                ui->pointpipe15->click();

            if(! ui->pointpipe15->isEnabled()) ui->pointpipe15->setEnabled(true);
            if(! ui->point15->hasFocus()) ui->point15->setCursorPosition(0);
        }
    }
    else if(ui->pointpipe15->isEnabled())
    {
        if(ui->pointpipe15->isChecked()) ui->pointpipe15->click();
        ui->pointpipe15->setDisabled(true);
    }
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
        if((ppipe == 0 && sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) || (ppipe == 1 && sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")))
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
    if(ui->livedelete->isEnabled()) ui->livedelete->setDisabled(true);
    if(ui->liveconvert->isEnabled()) ui->liveconvert->setDisabled(true);
    if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);

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
        {
            ui->systemrepair->setEnabled(true);
            ui->fullrepair->setEnabled(true);
        }

        rmntcheck();
    }
    else if(ui->systemrepair->isEnabled())
    {
        ui->systemrepair->setDisabled(true);
        ui->fullrepair->setDisabled(true);
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
    prun = tr("Upgrading the system");
    QDateTime ofdate(QFileInfo("/usr/bin/systemback").lastModified());
    sb::unlock(sb::Dpkglock);
    sb::exec("xterm +sb -bg grey85 -fg grey25 -fa a -fs 9 -geometry 80x24+80+70 -n \"System upgrade\" -T \"System upgrade\" -cr grey40 -selbg grey86 -bw 0 -bc -bcf 500 -bcn 500 -e sbsysupgrade");

    if(isVisible())
    {
        if(ofdate != QFileInfo("/usr/bin/systemback").lastModified())
        {
            nrxth = true;
            sb::unlock(sb::Sblock);
            sb::exec("systemback", nullptr, false, true);
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
            dialog = 301;
            dialogopen();
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

        if(! ui->grubinstallcopy->isVisibleTo(ui->copypanel))
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

    if(nohmcpy)
    {
        nohmcpy = false;
        if(ppipe == 0) ui->userdatafilescopy->setEnabled(true);
    }

    if(ui->partitionsettings->rowCount() > 0) ui->partitionsettings->clearContents();
    if(ui->repairpartition->count() > 0) ui->repairpartition->clear();

    if(! wismax)
    {
        ui->partitionsettings->resizeColumnToContents(2);
        ui->partitionsettings->resizeColumnToContents(3);
        ui->partitionsettings->resizeColumnToContents(4);
    }

    QSL plst;
    sb::readprttns(plst);

    if(! grub.isEFI)
    {
        if(ui->grubinstallcopy->count() > 0)
        {
            ui->grubinstallcopy->clear();
            ui->grubreinstallrestore->clear();
            ui->grubreinstallrepair->clear();
        }

        ui->grubinstallcopy->addItems({"Auto", tr("Disabled")});
        ui->grubreinstallrestore->addItems({"Auto", tr("Disabled")});
        ui->grubreinstallrepair->addItems({"Auto", tr("Disabled")});

        for(cQStr &dts : plst)
        {
            QStr path(dts.split('\n').at(0));

            if(sb::like(path.length(), {8, 12}))
            {
                ui->grubinstallcopy->addItem(path);
                ui->grubreinstallrestore->addItem(path);
                ui->grubreinstallrepair->addItem(path);
            }
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
            QFont font;
            font.setWeight(QFont::DemiBold);
            QTblWI *dev(new QTblWI(path));
            dev->setTextAlignment(Qt::AlignBottom);
            dev->setFont(font);
            ui->partitionsettings->setItem(sn, 0, dev);
            QTblWI *size(new QTblWI);

            if(bsize < 1073741824)
                size->setText(QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB");
            else if(bsize < 1073741824000)
                size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB");
            else
                size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");

            size->setTextAlignment(Qt::AlignRight | Qt::AlignBottom);
            size->setFont(font);
            ui->partitionsettings->setItem(sn, 1, size);
            QTblWI *empty(new QTblWI);
            ui->partitionsettings->setItem(sn, 2, empty);
            ui->partitionsettings->setItem(sn, 3, empty->clone());
            ui->partitionsettings->setItem(sn, 4, empty->clone());
            ui->partitionsettings->setItem(sn, 5, empty->clone());
            ui->partitionsettings->setItem(sn, 6, empty->clone());
            QTblWI *tp(new QTblWI(type));
            ui->partitionsettings->setItem(sn, 8, tp);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 10, lngth);
        }
        else
        {
            switch(type.toUShort()) {
            case sb::Extended:
            {
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QFont font;
                font.setWeight(QFont::DemiBold);
                font.setItalic(true);
                QTblWI *dev(new QTblWI(path));
                dev->setFont(font);
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);

                if(bsize < 1073741824)
                    size->setText(QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB");
                else if(bsize < 1073741824000)
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB");
                else
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");

                size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                size->setFont(font);
                ui->partitionsettings->setItem(sn, 1, size);
                QTblWI *empty(new QTblWI);
                ui->partitionsettings->setItem(sn, 2, empty);
                ui->partitionsettings->setItem(sn, 3, empty->clone());
                ui->partitionsettings->setItem(sn, 4, empty->clone());
                ui->partitionsettings->setItem(sn, 5, empty->clone());
                ui->partitionsettings->setItem(sn, 6, empty->clone());
                break;
            }
            case sb::Primary:
            case sb::Logical:
            {
                cQStr &uuid(dts.at(6));

                if(! grub.isEFI)
                {
                    ui->grubinstallcopy->addItem(path);
                    ui->grubreinstallrestore->addItem(path);
                    ui->grubreinstallrepair->addItem(path);
                }

                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);

                if(bsize < 1073741824)
                    size->setText(QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB");
                else if(bsize < 1073741824000)
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB");
                else
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");

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
                else
                {
                    if(QStr('\n' % mnts[0]).contains('\n' % path % ' '))
                    {
                        if(QStr('\n' % mnts[0]).count('\n' % path % ' ') > 1 || QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                            mpt->setText(tr("Multiple mount points"));
                        else
                        {
                            QStr mnt(sb::right(mnts[0], - sb::instr(mnts[0], path % ' ')));
                            short spc(sb::instr(mnt, " "));
                            mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                        }
                    }
                    else if(QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                    {
                        if(QStr('\n' % mnts[0]).count("\n/dev/disk/by-uuid/" % uuid % ' ') > 1)
                            mpt->setText(tr("Multiple mount points"));
                        else
                        {
                            QStr mnt(sb::right(mnts[0], - sb::instr(mnts[0], "/dev/disk/by-uuid/" % uuid % ' ')));
                            short spc(sb::instr(mnt, " "));
                            mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                        }
                    }
                    else if(QStr('\n' % mnts[1]).contains('\n' % path % ' '))
                        mpt->setText("SWAP");
                }

                mpt->setTextAlignment(Qt::AlignCenter);
                mpt->text().isEmpty() ? ui->repairpartition->addItem(path) : mpt->setToolTip(mpt->text());
                ui->partitionsettings->setItem(sn, 3, mpt);
                QTblWI *empty(new QTblWI);
                empty->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 4, empty);
                QTblWI *fs(new QTblWI(dts.at(4)));
                fs->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 5, fs);
                QTblWI *frmt(new QTblWI("-"));
                frmt->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 6, frmt);
                ui->partitionsettings->setItem(sn, 7, fs->clone());
                break;
            }
            case sb::Freespace:
            case sb::Emptyspace:
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
                QFont font;
                font.setItalic(true);
                QTblWI *dev(new QTblWI(path));
                dev->setFont(font);
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *size(new QTblWI);

                if(bsize < 1073741824)
                    size->setText(QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB");
                else if(bsize < 1073741824000)
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB");
                else
                    size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");

                size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                size->setFont(font);
                ui->partitionsettings->setItem(sn, 1, size);
                QTblWI *empty(new QTblWI);
                ui->partitionsettings->setItem(sn, 2, empty);
                ui->partitionsettings->setItem(sn, 3, empty->clone());
                ui->partitionsettings->setItem(sn, 4, empty->clone());
                ui->partitionsettings->setItem(sn, 5, empty->clone());
                ui->partitionsettings->setItem(sn, 6, empty->clone());
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

    ui->partitionsettings->resizeColumnToContents(0);
    ui->partitionsettings->resizeColumnToContents(1);

    if(wismax)
    {
        ui->partitionsettings->resizeColumnToContents(2);
        ui->partitionsettings->resizeColumnToContents(3);
        ui->partitionsettings->resizeColumnToContents(4);
    }

    ui->partitionsettings->resizeColumnToContents(5);
    ui->partitionsettings->resizeColumnToContents(6);
    if(ui->copypanel->isVisible() && ! ui->copyback->hasFocus()) ui->copyback->setFocus();
    ui->copycover->hide();
    busy(false);
}

void systemback::on_partitionrefresh2_clicked()
{
    on_partitionrefresh_clicked();

    if(! ui->partitionsettingspanel1->isVisibleTo(ui->copypanel))
    {
        if(ui->partitionsettingspanel2->isVisibleTo(ui->copypanel))
            ui->partitionsettingspanel2->hide();
        else
            ui->partitionsettingspanel3->hide();

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
            ui->filesystem->setEnabled(true);
            ui->format->setEnabled(true);
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

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        if(sb::like(cline, {"* " % mpt.replace(" ", "\\040") % " *", "* " % mpt % "/*"})) sb::umount(cline.split(' ').at(1));
                    }
                }
            }
        }

        mnts[0] = sb::fload("/proc/self/mounts");
        mnts[1] = sb::fload("/proc/swaps");

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

    if(ui->license->toPlainText().isEmpty())
    {
        busy();
        ui->license->setText(sb::fload("/usr/share/common-licenses/GPL-3"));
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
    bool rnmenbl(false);
    if(ppipe > 0) ppipe = 0;

    if(ui->pointpipe1->isChecked())
    {
        ++ppipe;
        cpoint = "S01";
        pname = sb::pnames[0];
        if(ui->point1->text() != sb::pnames[0]) rnmenbl = true;
    }
    else if(ui->point1->isEnabled() && ui->point1->text() != sb::pnames[0] && ! ui->point1->text().isEmpty())
        ui->point1->setText(sb::pnames[0]);

    if(ui->pointpipe2->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S02";
            pname = sb::pnames[1];
        }

        if(ui->point2->text() != sb::pnames[1] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point2->isEnabled() && ui->point2->text() != sb::pnames[1] && ! ui->point2->text().isEmpty())
        ui->point2->setText(sb::pnames[1]);

    if(ui->pointpipe3->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S03";
            pname = sb::pnames[2];
        }

        if(ui->point3->text() != sb::pnames[2] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point3->isEnabled() && ui->point3->text() != sb::pnames[2] && ! ui->point3->text().isEmpty())
        ui->point3->setText(sb::pnames[2]);

    if(ui->pointpipe4->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S04";
            pname = sb::pnames[3];
        }

        if(ui->point4->text() != sb::pnames[3] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point4->isEnabled() && ui->point4->text() != sb::pnames[3] && ! ui->point4->text().isEmpty())
        ui->point4->setText(sb::pnames[3]);

    if(ui->pointpipe5->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S05";
            pname = sb::pnames[4];
        }

        if(ui->point5->text() != sb::pnames[4] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point5->isEnabled() && ui->point5->text() != sb::pnames[4] && ! ui->point5->text().isEmpty())
        ui->point5->setText(sb::pnames[4]);

    if(ui->pointpipe6->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S06";
            pname = sb::pnames[5];
        }

        if(ui->point6->text() != sb::pnames[5] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point6->isEnabled() && ui->point6->text() != sb::pnames[5] && ! ui->point6->text().isEmpty())
        ui->point6->setText(sb::pnames[5]);

    if(ui->pointpipe7->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S07";
            pname = sb::pnames[6];
        }

        if(ui->point7->text() != sb::pnames[6] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point7->isEnabled() && ui->point7->text() != sb::pnames[6] && ! ui->point7->text().isEmpty())
        ui->point7->setText(sb::pnames[6]);

    if(ui->pointpipe8->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S08";
            pname = sb::pnames[7];
        }

        if(ui->point8->text() != sb::pnames[7] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point8->isEnabled() && ui->point8->text() != sb::pnames[7] && ! ui->point8->text().isEmpty())
        ui->point8->setText(sb::pnames[7]);

    if(ui->pointpipe9->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S09";
            pname = sb::pnames[8];
        }

        if(ui->point9->text() != sb::pnames[8] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point9->isEnabled() && ui->point9->text() != sb::pnames[8] && ! ui->point9->text().isEmpty())
        ui->point9->setText(sb::pnames[8]);

    if(ui->pointpipe10->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S10";
            pname = sb::pnames[9];
        }

        if(ui->point10->text() != sb::pnames[9] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point10->isEnabled() && ui->point10->text() != sb::pnames[9] && ! ui->point10->text().isEmpty())
        ui->point10->setText(sb::pnames[9]);

    if(ui->pointpipe11->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "H01";
            pname = sb::pnames[10];
        }

        if(ui->point11->text() != sb::pnames[10] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point11->isEnabled() && ui->point11->text() != sb::pnames[10] && ! ui->point11->text().isEmpty())
        ui->point11->setText(sb::pnames[10]);

    if(ui->pointpipe12->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "H02";
            pname = sb::pnames[11];
        }

        if(ui->point12->text() != sb::pnames[11] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point12->isEnabled() && ui->point12->text() != sb::pnames[11] && ! ui->point12->text().isEmpty())
        ui->point12->setText(sb::pnames[11]);

    if(ui->pointpipe13->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "H03";
            pname = sb::pnames[12];
        }

        if(ui->point13->text() != sb::pnames[12] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point13->isEnabled() && ui->point13->text() != sb::pnames[12] && ! ui->point13->text().isEmpty())
        ui->point13->setText(sb::pnames[12]);

    if(ui->pointpipe14->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "H04";
            pname = sb::pnames[13];
        }

        if(ui->point14->text() != sb::pnames[13] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point14->isEnabled() && ui->point14->text() != sb::pnames[13] && ! ui->point14->text().isEmpty())
        ui->point14->setText(sb::pnames[13]);

    if(ui->pointpipe15->isChecked())
    {
        if(++ppipe == 1)
        {
            cpoint = "S05";
            pname = sb::pnames[14];
        }

        if(ui->point15->text() != sb::pnames[14] && ! rnmenbl) rnmenbl = true;
    }
    else if(ui->point15->isEnabled() && ui->point15->text() != sb::pnames[14] && ! ui->point15->text().isEmpty())
        ui->point15->setText(sb::pnames[14]);

    if(ppipe == 0)
    {
        if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);

        if(ui->storagedirbutton->isHidden())
        {
            ui->storagedir->resize(ss(210), ss(28));
            ui->storagedirbutton->show();
            ui->pointrename->setDisabled(true);
            ui->pointdelete->setDisabled(true);
        }

        if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);

        if(! sb::isfile("/cdrom/casper/filesystem.squashfs") && ! sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
        {
            if(! ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setEnabled(true);
            if(! ui->livecreatemenu->isEnabled()) ui->livecreatemenu->setEnabled(true);
            pname = tr("Currently running system");
        }
        else if(sb::isdir("/.systemback"))
        {
            if(! ui->copymenu->isEnabled())
            {
                ui->copymenu->setEnabled(true);
                ui->installmenu->setEnabled(true);
            }

            pname = tr("Live image");
        }
        else
            pname.clear();
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
                ui->storagedir->resize(ss(236), ss(28));
                ui->pointdelete->setEnabled(true);
            }

            if(ui->pointpipe11->isChecked() || ui->pointpipe12->isChecked() || ui->pointpipe13->isChecked() || ui->pointpipe14->isChecked() || ui->pointpipe15->isChecked())
            {
                if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
            }
            else if(! ui->point15->isEnabled() && ! ui->pointhighlight->isEnabled())
                ui->pointhighlight->setEnabled(true);

            if(! ui->restoremenu->isEnabled() && ! sb::isfile("/cdrom/casper/filesystem.squashfs") && ! sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs")) ui->restoremenu->setEnabled(true);

            if(! ui->copymenu->isEnabled())
            {
                ui->copymenu->setEnabled(true);
                ui->installmenu->setEnabled(true);
            }

            if(ui->livecreatemenu->isEnabled()) ui->livecreatemenu->setDisabled(true);
            if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);
        }
        else
        {
            if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);
            ui->copymenu->setDisabled(true);
            ui->installmenu->setDisabled(true);
            ui->repairmenu->setDisabled(true);
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

        if(bsize < 1073741824)
            size->setText(QStr::number((bsize * 10 / 1048576 + 5) / 10) % " MiB");
        else if(bsize < 1073741824000)
            size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB");
        else
            size->setText(QStr::number(qRound64(bsize * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB");

        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->livedevices->setItem(sn, 1, size);
        QTblWI *name(new QTblWI(dts.at(1)));
        name->setTextAlignment(Qt::AlignCenter);
        name->setToolTip(dts.at(1));
        ui->livedevices->setItem(sn, 2, name);
        QTblWI *format(new QTblWI("-"));
        format->setTextAlignment(Qt::AlignCenter);
        ui->livedevices->setItem(sn, 3, format);
    }

    ui->livedevices->resizeColumnToContents(0);
    ui->livedevices->resizeColumnToContents(1);
    ui->livedevices->resizeColumnToContents(2);
    ui->livedevices->resizeColumnToContents(3);
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
            if(((ui->pointexclude->isChecked() && item.startsWith('.')) || (ui->liveexclude->isChecked() && ! item.startsWith('.'))) && ! sb::like(item, {"_.gvfs_", "_.Xauthority_", "_.ICEauthority_"}) && ui->excludedlist->findItems(item, Qt::MatchExactly).isEmpty())
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
                else if(sb::access(dir % '/' % item))
                {
                    if(sb::stype(dir % '/' % item) == sb::Isdir)
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
        {
            dialog = ui->fullrestore->isChecked() ? 205 : 204;
            dialogopen();
        }
        else if(dialog == 304)
        {
            statustart();

            for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                if(item.startsWith(".S00_"))
                {
                    prun = tr("Deleting incomplete restore point");

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
                        {
                            dialog = 328;
                            dialogopen();
                        }
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
            if(sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
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
        case 103:
        case 104:
        case 107:
            restore();
            break;
        case 101:
        case 102:
        case 109:
            repair();
            break;
        case 105:
        case 106:
            systemcopy();
            break;
        case 108:
            livewrite();
        }
    else if(ui->dialogok->text() == tr("Reboot"))
    {
        sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", nullptr, false, true);
        close();
    }
    else if(ui->dialogok->text() == tr("X restart"))
    {
        DIR *dir(opendir("/proc"));
        dirent *ent;

        while((ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! sb::like(iname, {"_._", "_.._"}) && ent->d_type == DT_DIR && sb::isnum(iname) && sb::islink("/proc/" % iname % "/exe") && QFile::readLink("/proc/" % iname % "/exe").endsWith("/Xorg"))
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
    pointupgrade();
    busy(false);
}

void systemback::on_pointrename_clicked()
{
    busy();
    if(dialog == 302) dialog = 0;

    if(ui->pointpipe1->isChecked() && ui->point1->text() != sb::pnames[0])
    {
        if(QFile::rename(sb::sdir[1] % "/S01_" % sb::pnames[0], sb::sdir[1] % "/S01_" % ui->point1->text()))
            ui->pointpipe1->click();
        else
            dialog = 302;
    }

    if(ui->pointpipe2->isChecked() && ui->point2->text() != sb::pnames[1])
    {
        if(QFile::rename(sb::sdir[1] % "/S02_" % sb::pnames[1], sb::sdir[1] % "/S02_" % ui->point2->text()))
            ui->pointpipe2->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe3->isChecked() && ui->point3->text() != sb::pnames[2])
    {
        if(QFile::rename(sb::sdir[1] % "/S03_" % sb::pnames[2], sb::sdir[1] % "/S03_" % ui->point3->text()))
            ui->pointpipe3->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe4->isChecked() && ui->point4->text() != sb::pnames[3])
    {
        if(QFile::rename(sb::sdir[1] % "/S04_" % sb::pnames[3], sb::sdir[1] % "/S04_" % ui->point4->text()))
            ui->pointpipe4->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe5->isChecked() && ui->point5->text() != sb::pnames[4])
    {
        if(QFile::rename(sb::sdir[1] % "/S05_" % sb::pnames[4], sb::sdir[1] % "/S05_" % ui->point5->text()))
            ui->pointpipe5->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe6->isChecked() && ui->point6->text() != sb::pnames[5])
    {
        if(QFile::rename(sb::sdir[1] % "/S06_" % sb::pnames[5], sb::sdir[1] % "/S06_" % ui->point6->text()))
            ui->pointpipe6->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe7->isChecked() && ui->point7->text() != sb::pnames[6])
    {
        if(QFile::rename(sb::sdir[1] % "/S07_" % sb::pnames[6], sb::sdir[1] % "/S07_" % ui->point7->text()))
            ui->pointpipe7->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe8->isChecked() && ui->point8->text() != sb::pnames[7])
    {
        if(QFile::rename(sb::sdir[1] % "/S08_" % sb::pnames[7], sb::sdir[1] % "/S08_" % ui->point8->text()))
            ui->pointpipe8->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe9->isChecked() && ui->point9->text() != sb::pnames[8])
    {
        if(QFile::rename(sb::sdir[1] % "/S09_" % sb::pnames[8], sb::sdir[1] % "/S09_" % ui->point9->text()))
            ui->pointpipe9->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe10->isChecked() && ui->point10->text() != sb::pnames[9])
    {
        if(QFile::rename(sb::sdir[1] % "/S10_" % sb::pnames[9], sb::sdir[1] % "/S10_" % ui->point10->text()))
            ui->pointpipe10->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe11->isChecked() && ui->point11->text() != sb::pnames[10])
    {
        if(QFile::rename(sb::sdir[1] % "/H01_" % sb::pnames[10], sb::sdir[1] % "/H01_" % ui->point11->text()))
            ui->pointpipe11->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe12->isChecked() && ui->point12->text() != sb::pnames[11])
    {
        if(QFile::rename(sb::sdir[1] % "/H02_" % sb::pnames[11], sb::sdir[1] % "/H02_" % ui->point12->text()))
            ui->pointpipe12->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe13->isChecked() && ui->point13->text() != sb::pnames[12])
    {
        if(QFile::rename(sb::sdir[1] % "/H03_" % sb::pnames[12], sb::sdir[1] % "/H03_" % ui->point13->text()))
            ui->pointpipe13->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe14->isChecked() && ui->point14->text() != sb::pnames[13])
    {
        if(QFile::rename(sb::sdir[1] % "/H04_" % sb::pnames[13], sb::sdir[1] % "/H04_" % ui->point14->text()))
            ui->pointpipe14->click();
        else if(dialog != 302)
            dialog = 302;
    }

    if(ui->pointpipe15->isChecked() && ui->point15->text() != sb::pnames[14])
    {
        if(QFile::rename(sb::sdir[1] % "/H05_" % sb::pnames[14], sb::sdir[1] % "/H05_" % ui->point15->text()))
            ui->pointpipe15->click();
        else if(dialog != 302)
            dialog = 302;
    }

    pointupgrade();
    busy(false);
    if(dialog == 302) dialogopen();
}

void systemback::on_autorestoreoptions_clicked(bool chckd)
{
    if(chckd)
    {
        ui->skipfstabrestore->setDisabled(true);
        ui->grubreinstallrestore->setDisabled(true);
        ui->grubreinstallrestoredisable->setDisabled(true);
    }
    else
    {
        ui->skipfstabrestore->setEnabled(true);
        ui->grubreinstallrestore->setEnabled(true);
        ui->grubreinstallrestoredisable->setEnabled(true);
    }
}

void systemback::on_autorepairoptions_clicked(bool chckd)
{
    if(chckd)
    {
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(ui->grubreinstallrepair->isEnabled())
        {
            ui->grubreinstallrepair->setDisabled(true);
            ui->grubreinstallrepairdisable->setDisabled(true);
        }
    }
    else
    {
        if(! ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setEnabled(true);

        if(! ui->grubreinstallrepair->isEnabled())
        {
            ui->grubreinstallrepair->setEnabled(true);
            ui->grubreinstallrepairdisable->setEnabled(true);
        }
    }
}

void systemback::on_storagedirbutton_clicked()
{
    ui->sbpanel->hide();
    ui->choosepanel->show();
    ui->scalingbutton->hide();
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
    ui->livecreatepanel->hide();
    ui->choosepanel->show();
    ui->scalingbutton->hide();
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
    if(ui->dirchoose->topLevelItemCount() != 0) ui->dirchoose->clear();
    QStr pwdrs(sb::fload("/etc/passwd"));
    QSL excl{"bin", "boot", "cdrom", "dev", "etc", "lib", "lib32", "lib64", "opt", "proc", "root", "run", "sbin", "selinux", "srv", "sys", "tmp", "usr", "var"};

    for(cQStr &item : QDir("/").entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
    {
        QTrWI *twi(new QTrWI);
        twi->setText(0, item);
        QStr cpath(QDir('/' % item).canonicalPath());

        if(excl.contains(item) || excl.contains(sb::right(cpath, -1)) || pwdrs.contains(':' % cpath % ':'))
        {
            twi->setTextColor(0, Qt::red);
            twi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
            ui->dirchoose->addTopLevelItem(twi);
        }
        else if(sb::islnxfs('/' % item))
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
        else
        {
            twi->setTextColor(0, Qt::red);
            twi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
            ui->dirchoose->addTopLevelItem(twi);
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

                    if(sb::isdir(path % '/' % iname))
                    {
                        if(! pwdrs.contains(':' % QDir(path % '/' % iname).canonicalPath() % ':') && sb::islnxfs(path % '/' % iname))
                        {
                            if(iname == "Systemback")
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
                        else
                        {
                            ctwi->setTextColor(0, Qt::red);
                            ctwi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(ss(12), ss(12), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                        }
                    }
                    else
                        ctwi->setDisabled(true);
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

                sb::sdir[0] = ui->dirpath->text();
                if(! cfgupdt) cfgupdt = true;
                sb::sdir[1] = sb::sdir[0] % "/Systemback";
                ui->storagedir->setText(sb::sdir[0]);
                ui->storagedir->setToolTip(sb::sdir[0]);
                ui->storagedir->setCursorPosition(0);
                pointupgrade();
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
        ui->keepfiles->setDisabled(true);
        ui->includeusers->setDisabled(true);
        ui->autorestoreoptions->setEnabled(true);

        if(! ui->autorestoreoptions->isChecked())
        {
            ui->skipfstabrestore->setEnabled(true);
            ui->grubreinstallrestore->setEnabled(true);
            ui->grubreinstallrestoredisable->setEnabled(true);
        }
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
        ui->keepfiles->setEnabled(true);
        ui->includeusers->setEnabled(true);
        ui->autorestoreoptions->setDisabled(true);

        if(! ui->autorestoreoptions->isChecked())
        {
            ui->skipfstabrestore->setDisabled(true);
            ui->grubreinstallrestore->setDisabled(true);
            ui->grubreinstallrestoredisable->setDisabled(true);
        }
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
    if(ui->fullrestore->isChecked())
        dialog = 107;
    else if(ui->systemrestore->isChecked())
        dialog = 100;
    else if(ui->keepfiles->isChecked())
        dialog = 104;
    else
        dialog = 103;

    dialogopen();
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
            if(ui->livedelete->isEnabled()) ui->livedelete->setDisabled(true);
            if(ui->liveconvert->isEnabled()) ui->liveconvert->setDisabled(true);
            if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);
            ui->livecreateback->setFocus();
        }
    }
}

void systemback::on_livedelete_clicked()
{
    busy();
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));
    sb::remove(path % ".sblive");
    sb::remove(path % ".iso");
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
    if(sb::issmfs("/", "/mnt"))
    {
        if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
        {
            ui->grubreinstallrepair->hide();
            ui->grubreinstallrepairdisable->show();
        }

        if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
    }
    else
    {
        if(ui->grubrepair->isChecked())
        {
            if(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && sb::execsrch("update-grub2", "/mnt") && sb::isfile("/mnt/var/lib/dpkg/info/grub-" % grub.name % ".list"))
            {
                if(ui->grubreinstallrepairdisable->isVisibleTo(ui->repairpanel))
                {
                    ui->grubreinstallrepairdisable->hide();
                    ui->grubreinstallrepair->show();
                }

                if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
            }
            else
            {
                if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
                {
                    ui->grubreinstallrepair->hide();
                    ui->grubreinstallrepairdisable->show();
                }

                if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
            }
        }
        else
        {
            if(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && ((ppipe == 1 && sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")) || (ppipe > 1 && sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list"))))
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

            if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
        }
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
            ui->grubreinstallrepair->setEnabled(true);
            ui->grubreinstallrepairdisable->setEnabled(true);
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
    if(ui->systemrepair->isChecked())
        dialog = 101;
    else if(ui->fullrepair->isChecked())
        dialog = 102;
    else
      dialog = 109;

    dialogopen();
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
        if((ppipe == 0 && sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) || (ppipe == 1 && sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")))
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

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                if(ui->partitionsettings->item(a, 3)->text() == "/home" && ! ui->partitionsettings->item(a, 4)->text().isEmpty())
                {
                    ui->partitionsettings->item(a, 4)->setText(nullptr);
                    ui->mountpoint->addItem("/home");
                    nohmcpy = false;
                    break;
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

        uchar type(ui->partitionsettings->item(crrnt->row(), 8)->text().toUShort()), pcount(0);

        switch(type) {
        case sb::MSDOS:
        case sb::GPT:
        case sb::Clear:
        case sb::Extended:
        {
            if(ui->partitionsettingspanel2->isHidden())
            {
                if(ui->partitionsettingspanel1->isVisible())
                    ui->partitionsettingspanel1->hide();
                else
                    ui->partitionsettingspanel3->hide();

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
            }
            else if(! ui->partitiondelete->isEnabled())
                ui->partitiondelete->setEnabled(true);

            if(mntd && ! mntcheck)
            {
                if(! ui->unmount->isEnabled()) ui->unmount->setEnabled(true);
            }
            else if(ui->unmount->isEnabled())
                ui->unmount->setDisabled(true);

            break;
        }
        case sb::Freespace:
        {
            QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().contains("mmc") ? 12 : 8));

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() && pcount < 4 ; ++a)
                if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                    switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                    case sb::GPT:
                        pcount = 5;
                    case sb::Primary:
                    case sb::Extended:
                        ++pcount;
                    }
        }
        case sb::Emptyspace:
            if(ui->partitionsettingspanel3->isHidden())
            {
                if(ui->partitionsettingspanel1->isVisible())
                    ui->partitionsettingspanel1->hide();
                else
                    ui->partitionsettingspanel2->hide();

                ui->partitionsettingspanel3->show();
            }

            ui->partitionsize->setMaximum((ui->partitionsettings->item(crrnt->row(), 10)->text().toULongLong() * 10 / 1048576 + 5) / 10);
            ui->partitionsize->setValue(ui->partitionsize->maximum());
            break;
        default:
            if(ui->partitionsettingspanel1->isHidden())
            {
                if(ui->partitionsettingspanel2->isVisible())
                    ui->partitionsettingspanel2->hide();
                else
                    ui->partitionsettingspanel3->hide();

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
                    if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
                    if(! ui->unmountdelete->isEnabled()) ui->unmountdelete->setEnabled(true);

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
                else if(mpt == "/home" && ui->userdatafilescopy->isVisible())
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

                    bool mntcheck(false);

                    if(sb::sdir[0].startsWith(mpt) || sb::like(mpt, {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
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

                    if(mntcheck)
                    {
                        if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);
                    }
                    else if(! ui->unmountdelete->isEnabled())
                        ui->unmountdelete->setEnabled(true);
                }

                if(! ui->format->isChecked()) ui->format->setChecked(true);
            }
        }

        if(pcount == 4)
        {
            if(ui->newpartition->isEnabled()) ui->newpartition->setDisabled(true);
        }
        else if(! ui->newpartition->isEnabled())
            ui->newpartition->setEnabled(true);
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
                if((ppipe == 0 && sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) || (ppipe == 1 && sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")))
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
        else if(ui->mountpoint->currentText() == "SWAP")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "swap") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("swap");
        }
        else
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != ui->filesystem->currentText()) ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText(ui->filesystem->currentText());
            if(ui->mountpoint->currentText() == "/") ui->copynext->setEnabled(true);
        }

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

        if((ppipe == 0 && sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) || (ppipe == 1 && sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")))
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
    else if(mpt == "/home")
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/home");
        ui->userdatafilescopy->setDisabled(true);
        nohmcpy = true;
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

            if(arg1.isEmpty() || (arg1.length() > 1 && arg1.endsWith('/')) || arg1 == ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text() || (ui->usersettingscopy->isVisible() && arg1.startsWith("/home/")) || (arg1 != "/boot/efi" && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong() < 268435456))
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
            }
            else
            {
                bool check(false);

                if((grub.isEFI && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/boot/efi" && arg1 != "/boot/efi") || (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && arg1 != "/home") || (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "SWAP" && arg1 != "SWAP"))
                    check = true;
                else if(arg1 != "SWAP")
                    for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                        if(ui->partitionsettings->item(a, 4)->text() == arg1)
                        {
                            check = true;
                            break;
                        }

                if(check)
                {
                    if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                }
                else if(sb::like(arg1, {"_/_", "_/home_", "_/boot_", "_/boot/efi_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_", "_SWAP_"}))
                {
                    if(! ui->changepartition->isEnabled()) ui->changepartition->setEnabled(true);
                }
                else
                {
                    if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                    sb::delay(300);

                    if(ccnt == icnt)
                    {
                        QStr mpname(QStr(arg1 % '_' % sb::rndstr()).replace('/', '_'));

                        if(QDir().mkdir("/tmp/" % mpname))
                        {
                            sb::remove("/tmp/" % mpname);
                            ui->changepartition->setEnabled(true);
                        }
                    }
                }
            }
        }
    }
}

void systemback::on_copynext_clicked()
{
    dialog = ui->function1->text() == tr("System copy") ? 105 : 106;
    dialogopen();
}

void systemback::on_repairpartitionrefresh_clicked()
{
    busy();
    ui->repaircover->show();

    for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        if(ui->partitionsettings->item(a, 3)->text().startsWith("/mnt")) sb::umount(ui->partitionsettings->item(a, 0)->text());

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
    if(! arg1.isEmpty())
    {
        ui->repairpartition->resize(fontMetrics().width(arg1) + ss(30), ui->repairpartition->height());
        ui->repairpartition->move(ui->repairmountpoint->x() - ui->repairpartition->width() - ss(8), ui->repairpartition->y());
    }
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
    else
    {
        if(sb::like(arg1, {"_/mnt_", "_/mnt/home_", "_/mnt/boot_", "_/mnt/boot/efi_", "_/mnt/usr_", "_/mnt/usr/local_", "_/mnt/var_", "_/mnt/opt_"}))
        {
            if(! ui->repairmount->isEnabled()) ui->repairmount->setEnabled(true);
        }
        else
        {
            if(ui->repairmount->isEnabled()) ui->repairmount->setDisabled(true);
            sb::delay(300);

            if(ccnt == icnt)
            {
                QStr mpname(QStr(arg1 % '_' % sb::rndstr()).replace('/', '_'));

                if(QDir().mkdir("/tmp/" % mpname))
                {
                    sb::remove("/tmp/" % mpname);
                    ui->repairmount->setEnabled(true);
                }
            }
        }
    }
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
            {
                path.append('/' % cpath);

                if(! sb::isdir("/mnt/" % path) && ! QDir().mkdir("/mnt" % path))
                {
                    QFile::rename("/mnt" % path, "/mnt" % path % '_' % sb::rndstr());
                    QDir().mkdir("/mnt" % path);
                }
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

    if(sb::like(arg1, {"* *", "*/*"}) || arg1.toUtf8().length() > 32 || arg1.toLower().endsWith(".iso"))
        ui->livename->setText(QStr(arg1).replace((cpos = ui->livename->cursorPosition() - 1), 1, nullptr));
    else
    {
        if(ui->livenamepipe->isVisible()) ui->livenamepipe->hide();

        if(arg1 == "auto")
        {
            QFont font;
            font.setItalic(true);
            ui->livename->setFont(font);
            if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
        }
        else
        {
            if(ui->livecreatenew->isEnabled()) ui->livecreatenew->setDisabled(true);

            if(ui->livename->fontInfo().italic())
            {
                QFont font;
                ui->livename->setFont(font);
            }

            if(arg1.isEmpty())
            {
                if(ui->livenameerror->isHidden()) ui->livenameerror->show();
            }
            else
            {
                sb::delay(300);

                if(ccnt == icnt)
                {
                    QStr lname("/tmp/" % arg1 % '_' % sb::rndstr());

                    if(QDir().mkdir(lname))
                    {
                        sb::remove(lname);
                        if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
                        ui->livenamepipe->show();
                    }
                    else if(ui->livenameerror->isHidden())
                        ui->livenameerror->show();
                }
            }
        }
    }
}

void systemback::itmxpnd(cQSL &path, QTrWI *item)
{
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
        if(sb::stype("/root" % path) == sb::Isdir) itmxpnd({"/root", path}, item);
        QFile file("/etc/passwd");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::stype("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)) % path) == sb::Isdir) itmxpnd({"/home/" % usr, path}, item);
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
    else
    {
        bool ok(true);

        for(uchar a(0) ; a < arg1.length() ; ++a)
        {
            cQChar &ctr(arg1.at(a));

            if(ctr == ':' || ctr == ',' || ctr == '=' || ! (ctr.isLetterOrNumber() || ctr.isPrint()))
            {
                ok = false;
                break;
            }
        }

        if(! ok || sb::like(arg1, {"_ *", "*  *", "*ß*"}))
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
    else
    {
        bool ok(true);

        for(uchar a(0) ; a < arg1.length() ; ++a)
        {
            cQChar &ctr(arg1.at(a));

            if((ctr < 'a' || ctr > 'z') && ctr != '.' && ctr != '_' && ctr != '@' && (a == 0 || (! ctr.isDigit() && ctr != '-' && ctr != '$')))
            {
                ok = false;
                break;
            }
        }

        if(! ok || (arg1.contains('$') && (arg1.count('$') > 1 || ! arg1.endsWith('$'))))
            ui->username->setText(QStr(arg1).replace((cpos = ui->username->cursorPosition() - 1), 1, nullptr));
        else if(ui->usernamepipe->isHidden())
        {
            if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
            ui->usernamepipe->show();
        }
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
    else
    {
        bool ok(true);

        for(uchar a(0) ; a < arg1.length() ; ++a)
        {
            cQChar &ctr(arg1.at(a));

            if((ctr < 'a' || ctr > 'z') && (ctr < 'A' || ctr > 'Z') && ! ctr.isDigit() && (a == 0 || (ctr != '-' && ctr != '.')))
            {
                ok = false;
                break;
            }
        }

        if(! ok || (arg1.length() > 1 && sb::like(arg1, {"*..*", "*--*", "*.-*", "*-.*"})))
            ui->hostname->setText(QStr(arg1).replace((cpos = ui->hostname->cursorPosition() - 1), 1, nullptr));
        else if(ui->hostnamepipe->isHidden())
        {
            if(ui->hostnameerror->isVisible()) ui->hostnameerror->hide();
            ui->hostnamepipe->show();
        }
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
        if(ui->dayup->isEnabled()) ui->dayup->setDisabled(true);
        if(ui->daydown->isEnabled()) ui->daydown->setDisabled(true);
        if(ui->hourup->isEnabled()) ui->hourup->setDisabled(true);
        if(ui->hourdown->isEnabled()) ui->hourdown->setDisabled(true);
        if(ui->minuteup->isEnabled()) ui->minuteup->setDisabled(true);
        if(ui->minutedown->isEnabled()) ui->minutedown->setDisabled(true);
        if(ui->secondup->isEnabled()) ui->secondup->setDisabled(true);
        if(ui->seconddown->isEnabled()) ui->seconddown->setDisabled(true);
        ui->silentmode->setDisabled(true);
        if(ui->windowposition->isEnabled()) ui->windowposition->setDisabled(true);
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
        if(ui->secondup->isEnabled()) ui->secondup->setDisabled(true);
        if(ui->seconddown->isEnabled()) ui->seconddown->setDisabled(true);
        ui->windowposition->setDisabled(true);
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
    if(ui->schedulepanel->isVisible())
    {
        if(arg1 == tr("Top left") && sb::schdlr[0] != "topleft")
        {
            sb::schdlr[0] = "topleft";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Top right") && sb::schdlr[0] != "topright")
        {
            sb::schdlr[0] = "topright";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Center") && sb::schdlr[0] != "center")
        {
            sb::schdlr[0] = "center";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Bottom left") && sb::schdlr[0] != "bottomleft")
        {
            sb::schdlr[0] = "bottomleft";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Bottom right") && sb::schdlr[0] != "bottomright")
        {
            sb::schdlr[0] = "bottomright";
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

                if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1))) && sb::dfree("/home/" % usr) != hfree)
                {
                    ui->userdatainclude->setChecked(false);
                    break;
                }
            }
        else
            ui->userdatainclude->setChecked(false);
    }
}

void systemback::on_interrupt_clicked()
{
    if(! irblck && ! intrrpt)
    {
        if(! intrrptimer)
        {
            prun = tr("Interrupting the current process");
            ui->interrupt->setDisabled(true);
            intrrpt = true;
            if(! sb::ExecKill) sb::ExecKill = true;

            if(sb::SBThrd.isRunning())
            {
                sb::ThrdKill = true;
                sb::thrdelay();
                sb::error("\n " % tr("Systemback worker thread is interrupted by the user.") % "\n\n");
            }

            connect((intrrptimer = new QTimer), SIGNAL(timeout()), this, SLOT(on_interrupt_clicked()));
            intrrptimer->start(100);
        }
        else if(sstart)
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
            close();
        }
        else
        {
            delete intrrptimer;
            intrrptimer = nullptr;
            if(ui->pointpipe1->isChecked()) ui->pointpipe1->setChecked(false);
            if(ui->pointpipe2->isChecked()) ui->pointpipe2->setChecked(false);
            if(ui->pointpipe3->isChecked()) ui->pointpipe3->setChecked(false);
            if(ui->pointpipe4->isChecked()) ui->pointpipe4->setChecked(false);
            if(ui->pointpipe5->isChecked()) ui->pointpipe5->setChecked(false);
            if(ui->pointpipe6->isChecked()) ui->pointpipe6->setChecked(false);
            if(ui->pointpipe7->isChecked()) ui->pointpipe7->setChecked(false);
            if(ui->pointpipe8->isChecked()) ui->pointpipe8->setChecked(false);
            if(ui->pointpipe9->isChecked()) ui->pointpipe9->setChecked(false);
            if(ui->pointpipe10->isChecked()) ui->pointpipe10->setChecked(false);
            if(ui->pointpipe11->isChecked()) ui->pointpipe11->setChecked(false);
            if(ui->pointpipe12->isChecked()) ui->pointpipe12->setChecked(false);
            if(ui->pointpipe13->isChecked()) ui->pointpipe13->setChecked(false);
            if(ui->pointpipe14->isChecked()) ui->pointpipe14->setChecked(false);
            if(ui->pointpipe15->isChecked()) ui->pointpipe15->setChecked(false);
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
    dialog = 108;
    dialogopen();
}

void systemback::on_schedulerlater_clicked()
{
    if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
    close();
}

void systemback::on_scalingup_clicked()
{
    if(ui->scalingfactor->text() == "auto")
    {
        ui->scalingfactor->setText("x1");
        ui->scalingdown->setEnabled(true);
    }
    else if(ui->scalingfactor->text() == "x1")
        ui->scalingfactor->setText("x1.5");
    else
    {
        ui->scalingfactor->setText("x2");
        ui->scalingup->setDisabled(true);
    }
}

void systemback::on_scalingdown_clicked()
{
    if(ui->scalingfactor->text() == "x2")
    {
        ui->scalingfactor->setText("x1.5");
        ui->scalingup->setEnabled(true);
    }
    else if(ui->scalingfactor->text() == "x1.5")
        ui->scalingfactor->setText("x1");
    else
    {
        ui->scalingfactor->setText("auto");
        ui->scalingdown->setDisabled(true);
    }
}

void systemback::on_newrestorepoint_clicked()
{
    goto start;
error:
    if(intrrpt)
        intrrpt = false;
    else
    {
        if(sb::dfree(sb::sdir[1]) < 104857600)
            dialog = 304;
        else
        {
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = 318;
        }

        dialogopen();
        if(! sstart) pointupgrade();
    }

    return;
start:
    statustart();

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun != tr("Deleting incomplete restore point")) prun = tr("Deleting incomplete restore point");
            if(! sb::remove(sb::sdir[1] % '/' % item)) goto error;
            if(intrrpt) goto error;
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
                prun = tr("Deleting old restore point") % ' ' % QStr::number(dnum) % '/' % QStr::number(ppipe);
                if(! QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a])) goto error;
                if(intrrpt) goto error;
            }
    }

    prun = tr("Creating restore point");
    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(sb::sdir[1], ".S00_" % dtime)) goto error;

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! QFile::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) goto error;

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) goto error;
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    if(intrrpt) goto error;
    emptycache();

    if(sstart)
    {
        sb::ThrdKill = true;
        close();
    }
    else
    {
        pointupgrade();
        ui->statuspanel->hide();
        ui->mainpanel->show();
        ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
        windowmove(ss(698), ss(465));
    }
}

void systemback::on_pointdelete_clicked()
{
    goto start;
error:
    if(intrrpt)
        intrrpt = false;
    else
    {
        dialog = 328;
        dialogopen();
        pointupgrade();
    }

    return;
start:
    statustart();
    uchar dnum(0);

    for(schar a(9) ; a > -1 ; --a)
    {
        switch(a) {
        case 9:
            if(! ui->pointpipe10->isChecked()) continue;
            break;
        case 8:
            if(! ui->pointpipe9->isChecked()) continue;
            break;
        case 7:
            if(! ui->pointpipe8->isChecked()) continue;
            break;
        case 6:
            if(! ui->pointpipe7->isChecked()) continue;
            break;
        case 5:
            if(! ui->pointpipe6->isChecked()) continue;
            break;
        case 4:
            if(! ui->pointpipe5->isChecked()) continue;
            break;
        case 3:
            if(! ui->pointpipe4->isChecked()) continue;
            break;
        case 2:
            if(! ui->pointpipe3->isChecked()) continue;
            break;
        case 1:
            if(! ui->pointpipe2->isChecked()) continue;
            break;
        case 0:
            if(! ui->pointpipe1->isChecked()) continue;
        }

        ++dnum;
        prun = tr("Deleting restore point") % ' ' % QStr::number(dnum) % '/' % QStr::number(ppipe);
        if(! QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a])) goto error;
        if(intrrpt) goto error;
    }

    for(uchar a(10) ; a < 15 ; ++a)
    {
        switch(a) {
        case 10:
            if(! ui->pointpipe11->isChecked()) continue;
            break;
        case 11:
            if(! ui->pointpipe12->isChecked()) continue;
            break;
        case 12:
            if(! ui->pointpipe13->isChecked()) continue;
            break;
        case 13:
            if(! ui->pointpipe14->isChecked()) continue;
            break;
        case 14:
            if(! ui->pointpipe15->isChecked()) continue;
        }

        ++dnum;
        prun = tr("Deleting restore point") % ' ' % QStr::number(dnum) % '/' % QStr::number(ppipe);
        if(! QFile::rename(sb::sdir[1] % "/H0" % QStr::number(a - 9) % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a])) goto error;
        if(intrrpt) goto error;
    }

    pointupgrade();
    emptycache();
    ui->statuspanel->hide();
    ui->mainpanel->show();
    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
    windowmove(ss(698), ss(465));
}

void systemback::on_livecreatenew_clicked()
{
    if(dialog > 0) dialog = 0;
    goto start;
error:
    if(intrrpt) goto exit;

    if(dialog != 326)
    {
        if(sb::dfree(sb::sdir[2]) < 104857600 || (sb::isdir("/home/.sbuserdata") && sb::dfree("/home") < 104857600))
            dialog = 312;
        else if(dialog == 0)
        {
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = 327;
        }
    }
exit:
    if(sb::isdir("/.sblvtmp")) sb::remove("/.sblvtmp");
    if(sb::isdir("/media/.sblvtmp")) sb::remove("/media/.sblvtmp");
    if(sb::isdir("/var/.sblvtmp")) sb::remove("/var/.sblvtmp");
    if(sb::isdir("/home/.sbuserdata")) sb::remove("/home/.sbuserdata");
    if(sb::isdir("/root/.sbuserdata")) sb::remove("/root/.sbuserdata");
    if(sb::autoiso == sb::True) on_livecreatemenu_clicked();

    if(intrrpt)
        intrrpt = false;
    else
    {
        if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate")) sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
        dialogopen();
    }

    return;
start:
    statustart();
    prun = tr("Creating Live system") % '\n' % tr("process") % " 1/3" % (sb::autoiso == sb::True ? "+1" : nullptr);
    QStr ckernel(ckname()), lvtype(sb::isfile("/usr/share/initramfs-tools/scripts/casper") ? "casper" : "live"), ifname;

    if(sb::exist(sb::sdir[2] % "/.sblivesystemcreate"))
    {
        if(! sb::remove(sb::sdir[2] % "/.sblivesystemcreate")) goto error;
        if(intrrpt) goto exit;
    }

    if(! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate") || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/.disk") || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/" % lvtype) || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemcreate/syslinux")) goto error;
    ifname = ui->livename->text() == "auto" ? "systemback_live_" % QDateTime().currentDateTime().toString("yyyy-MM-dd") : ui->livename->text();
    uchar ncount(0);

    while(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive"))
    {
        ++ncount;
        ncount == 1 ? ifname.append("_1") : ifname = sb::left(ifname, sb::rinstr(ifname, "_")) % QStr::number(ncount);
    }

    if(intrrpt) goto exit;
    if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/.disk/info", "Systemback Live (" % ifname % ") - Release " % sb::right(ui->systembackversion->text(), - sb::rinstr(ui->systembackversion->text(), "_")) % '\n') || ! sb::copy("/boot/vmlinuz-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/vmlinuz")) goto error;
    if(intrrpt) goto exit;
    QStr fname;

    {
        QFile file("/etc/passwd");
        if(! file.open(QIODevice::ReadOnly)) goto error;

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

    if(intrrpt) goto exit;
    irblck = true;

    if(lvtype == "casper")
    {
        QStr did;

        if(sb::isfile("/etc/lsb-release"))
        {
            QFile file("/etc/lsb-release");
            if(! file.open(QIODevice::ReadOnly)) goto error;

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
        if(! file.open(QIODevice::ReadOnly)) goto error;
        QStr hname(file.readLine().trimmed());
        file.close();
        if(! sb::crtfile("/etc/casper.conf", "USERNAME=\"" % guname() % "\"\nUSERFULLNAME=\"" % fname % "\"\nHOST=\"" % hname % "\"\nBUILD_SYSTEM=\"" % did % "\"\n\nexport USERNAME USERFULLNAME HOST BUILD_SYSTEM\n")) goto error;
        if(intrrpt) goto exit;

        for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
            if(! sb::like(item, {"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"}) && ! QFile::setPermissions("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther)) goto error;
    }
    else
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", "#!/bin/sh\nif [ \"$BOOT\" = live ] && [ ! -e /root/etc/fstab ]\nthen touch /root/etc/fstab\nfi\n");
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) goto error;
    }

    bool xmntry(false);

    if(sb::isfile("/etc/X11/xorg.conf"))
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", "#!/bin/sh\nif [ -s /root/etc/X11/xorg.conf ] && grep noxconf /proc/cmdline >/dev/null 2>&1\nthen rm /root/etc/X11/xorg.conf\nfi\n");
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) goto error;
        xmntry = true;
    }

    uchar rv(sb::exec("update-initramfs -tck" % ckernel));

    if(lvtype == "casper")
    {
        for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
            if(! sb::like(item, {"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"}) && ! QFile::setPermissions("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) goto error;
    }
    else if(! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab"))
        goto error;

    if(rv > 0)
    {
        dialog = 326;
        goto error;
    }

    irblck = false;
    if((xmntry && ! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf")) || ! sb::copy("/boot/initrd.img-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/initrd.gz")) goto error;

    if(sb::isfile("/usr/lib/syslinux/isolinux.bin"))
    {
        if(! sb::copy("/usr/lib/syslinux/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") || ! sb::copy("/usr/lib/syslinux/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32")) goto error;
    }
    else if(! sb::copy("/usr/lib/ISOLINUX/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") || ! sb::copy("/usr/lib/syslinux/modules/bios/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/libcom32.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libcom32.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/libutil.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libutil.c32") || ! sb::copy("/usr/lib/syslinux/modules/bios/ldlinux.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/ldlinux.c32"))
        goto error;

    if(! sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/syslinux/splash.png") || ! sb::lvprpr(ui->userdatainclude->isChecked())) goto error;
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

    if(intrrpt) goto exit;
    prun = tr("Creating Live system") % '\n' % tr("process") % " 2/3" % (sb::autoiso == sb::True ? "+1" : nullptr);
    QStr elist;

    for(cQStr &excl : {"/boot/efi/EFI", "/etc/fstab", "/etc/mtab", "/etc/udev/rules.d/70-persistent-cd.rules", "/etc/udev/rules.d/70-persistent-net.rules"})
        if(sb::exist(excl)) elist.append(" -e " % excl);

    for(cQStr &cdir : {"/etc/rc0.d", "/etc/rc1.d", "/etc/rc2.d", "/etc/rc3.d", "/etc/rc4.d", "/etc/rc5.d", "/etc/rc6.d", "/etc/rcS.d"})
        if(sb::isdir(cdir))
            for(cQStr &item : QDir(cdir).entryList(QDir::Files))
                if(item.contains("cryptdisks")) elist.append(" -e " % cdir % '/' % item);

    if(sb::exec("mksquashfs" % ide % ' ' % sb::sdir[2] % "/.sblivesystemcreate/.systemback /media/.sblvtmp/media /var/.sblvtmp/var " % sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs " % (sb::xzcmpr == sb::True ? "-comp xz " : nullptr) % "-info -b 1M -no-duplicates -no-recovery -always-use-fragments" % elist) > 0)
    {
        dialog = 310;
        goto error;
    }

    sb::Progress = -1;
    ui->progressbar->setValue(0);
    prun = tr("Creating Live system") % '\n' % tr("process") % " 3/3" % (sb::autoiso == sb::True ? "+1" : nullptr);
    sb::remove("/.sblvtmp");
    sb::remove("/media/.sblvtmp");
    sb::remove("/var/.sblvtmp");
    if(sb::isdir("/home/.sbuserdata")) sb::remove("/home/.sbuserdata");
    if(sb::isdir("/root/.sbuserdata")) sb::remove("/root/.sbuserdata");
    if(intrrpt) goto exit;
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

    if(xmntry)
    {
        grxorg = "menuentry \"" % tr("Boot Live without xorg.conf file") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " noxconf quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n";
        srxorg = "label noxconf\n  menu label " % tr("Boot Live without xorg.conf file") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz noxconf quiet splash" % prmtrs % "\n\n";
    }
#ifdef __amd64__
    if(sb::isfile("/usr/share/systemback/efi-amd64.bootfiles") && (sb::exec("tar -xJf /usr/share/systemback/efi-amd64.bootfiles -C " % sb::sdir[2] % "/.sblivesystemcreate --no-same-owner --no-same-permissions") > 0 || ! sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/boot/grub/splash.png") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/grub.cfg", "if loadfont /boot/grub/font.pf2\nthen\n  set gfxmode=auto\n  insmod efi_gop\n  insmod efi_uga\n  insmod gfxterm\n  terminal_output gfxterm\nfi\n\nset theme=/boot/grub/theme.cfg\n\nmenuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/theme.cfg", "title-color: \"white\"\ntitle-text: \"Systemback Live (" % ifname % ")\"\ntitle-font: \"Sans Regular 16\"\ndesktop-color: \"black\"\ndesktop-image: \"/boot/grub/splash.png\"\nmessage-color: \"white\"\nmessage-bg-color: \"black\"\nterminal-font: \"Sans Regular 12\"\n\n+ boot_menu {\n  top = 150\n  left = 15%\n  width = 75%\n  height = 130\n  item_font = \"Sans Regular 12\"\n  item_color = \"grey\"\n  selected_item_color = \"white\"\n  item_height = 20\n  item_padding = 15\n  item_spacing = 5\n}\n\n+ vbox {\n  top = 100%\n  left = 2%\n  + label {text = \"" % tr("Press 'E' key to edit") % "\" font = \"Sans 10\" color = \"white\" align = \"left\"}\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/loopback.cfg", "menuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz" % rpart % "boot=" % lvtype % " quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n"))) goto error;
#endif
    if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", "default vesamenu.c32\nprompt 0\ntimeout 100\n\nmenu title Systemback Live (" % ifname % ")\nmenu tabmsg " % tr("Press TAB key to edit") % "\nmenu background splash.png\n\nlabel live\n  menu label " % tr("Boot Live system") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz quiet splash" % prmtrs % "\n\nlabel safe\n  menu label " % tr("Boot Live in safe graphics mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz xforcevesa nomodeset quiet splash" % prmtrs % "\n\n" % srxorg % "label debug\n  menu label " % tr("Boot Live in debug mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz" % prmtrs % '\n')) goto error;
    if(intrrpt) goto exit;
    if(! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/.systemback")) goto error;
    if(intrrpt) goto exit;
    if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata") && ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/userdata")) goto error;
    if(intrrpt) goto exit;
    if(sb::ThrdLng[0] > 0) sb::ThrdLng[0] = 0;
    sb::ThrdStr[0] = sb::sdir[2] % '/' % ifname % ".sblive";

    if(sb::exec("tar -cf " % sb::sdir[2] % '/' % ifname % ".sblive -C " % sb::sdir[2] % "/.sblivesystemcreate .") > 0)
    {
        if(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive")) sb::remove(sb::sdir[2] % '/' % ifname % ".sblive");
        dialog = 311;
        goto error;
    }

    if(! QFile::setPermissions(sb::sdir[2] % '/' % ifname % ".sblive", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) goto error;

    if(sb::autoiso == sb::True)
    {
        ullong isize(sb::fsize(sb::sdir[2] % '/' % ifname % ".sblive"));

        if(isize < 4294967295 && isize + 52428800 < sb::dfree(sb::sdir[2]))
        {
            sb::Progress = -1;
            ui->progressbar->setValue(0);
            prun = tr("Creating Live system") % '\n' % tr("process") % " 4/3+1";
            if(! QFile::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.cfg") || ! QFile::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux", sb::sdir[2] % "/.sblivesystemcreate/isolinux")) goto error;
            if(intrrpt) goto exit;

            if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o " % sb::sdir[2] % '/' % ifname % ".iso " % sb::sdir[2] % "/.sblivesystemcreate") > 0)
            {
                if(sb::isfile(sb::sdir[2] % '/' % ifname % ".iso")) sb::remove(sb::sdir[2] % '/' % ifname % ".iso");
                dialog = 311;
                goto error;
            }

            if(sb::exec("isohybrid " % sb::sdir[2] % '/' % ifname % ".iso") > 0 || ! QFile::setPermissions(sb::sdir[2] % '/' % ifname % ".iso", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) goto error;
        }
    }

    if(intrrpt) goto exit;
    emptycache();
    dialog = 207;
    sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
    on_livecreatemenu_clicked();
    dialogopen();
}

void systemback::on_liveconvert_clicked()
{
    if(dialog > 0) dialog = 0;
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));
    goto start;
error:
    if(sb::isdir(sb::sdir[2] % "/.sblivesystemconvert")) sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(sb::isfile(path % ".iso")) sb::remove(path % ".iso");

    if(intrrpt)
        intrrpt = false;
    else
    {
        if(dialog == 0) dialog = 334;
        dialogopen();
    }

    return;
start:
    statustart();
    prun = tr("Converting Live system image") % '\n' % tr("process") % " 1/2";
    if((sb::exist(sb::sdir[2] % "/.sblivesystemconvert") && ! sb::remove(sb::sdir[2] % "/.sblivesystemconvert")) || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemconvert")) goto error;
    sb::ThrdLng[0] = sb::fsize(path % ".sblive");
    sb::ThrdStr[0] = sb::sdir[2] % "/.sblivesystemconvert";

    if(sb::exec("tar -xf " % path % ".sblive -C " % sb::sdir[2] % "/.sblivesystemconvert --no-same-owner --no-same-permissions") > 0)
    {
        dialog = 325;
        goto error;
    }

    if(! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemconvert/syslinux/isolinux.cfg") || ! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux", sb::sdir[2] % "/.sblivesystemconvert/isolinux")) goto error;
    if(intrrpt) goto error;
    prun = tr("Converting Live system image") % '\n' % tr("process") % " 2/2";
    sb::Progress = -1;
    ui->progressbar->setValue(0);

    if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o " % path % ".iso " % sb::sdir[2] % "/.sblivesystemconvert") > 0)
    {
        dialog = 324;
        goto error;
    }

    if(sb::exec("isohybrid " % path % ".iso") > 0 || ! QFile::setPermissions(path % ".iso", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) goto error;
    sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(intrrpt) goto error;
    emptycache();
    ui->livelist->currentItem()->setText(sb::left(ui->livelist->currentItem()->text(), sb::rinstr(ui->livelist->currentItem()->text(), " ")) % "sblive+iso)");
    ui->liveconvert->setDisabled(true);
    ui->statuspanel->hide();
    ui->mainpanel->show();
    ui->livecreateback->setFocus();
    windowmove(ss(698), ss(465));
}

void systemback::printdbgmsg()
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
        uchar pcount(0), fcount(0);

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                case sb::GPT:
                    type = sb::Primary;
                    goto exec;
                case sb::Primary:
                    ++pcount;
                    break;
                case sb::Extended:
                    type = sb::Primary;
                    goto exec;
                case sb::Freespace:
                    ++fcount;
                }

        if(pcount > 2 && (fcount > 1 || ui->partitionsize->value() < ui->partitionsize->maximum()))
        {
            if(! sb::mkpart(dev, start, ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong(), sb::Extended)) goto end;
            type = sb::Logical;
            start += 1048576;
        }
        else
            type = sb::Primary;

        break;
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
        if(ui->languages->currentText() == "المصرية العربية")
            sb::lang = "ar_EG";
        else if(ui->languages->currentText() == "Català")
            sb::lang = "ca_ES";
        else if(ui->languages->currentText() == "Čeština")
            sb::lang = "cs_CS";
        else if(ui->languages->currentText() == "English")
            sb::lang = "en_EN";
        else if(ui->languages->currentText() == "Español")
            sb::lang = "es_ES";
        else if(ui->languages->currentText() == "Suomi")
            sb::lang = "fi_FI";
        else if(ui->languages->currentText() == "Français")
            sb::lang = "fr_FR";
        else if(ui->languages->currentText() == "Galego")
            sb::lang = "gl_ES";
        else if(ui->languages->currentText() == "Magyar")
            sb::lang = "hu_HU";
        else if(ui->languages->currentText() == "Bahasa Indonesia")
            sb::lang = "id_ID";
        else if(ui->languages->currentText() == "Português (Brasil)")
            sb::lang = "pt_BR";
        else if(ui->languages->currentText() == "Română")
            sb::lang = "ro_RO";
        else if(ui->languages->currentText() == "Türkçe")
            sb::lang = "tr_TR";
        else
            sb::lang = "zh_CN";

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
    if(ui->languages->isEnabled())
    {
        if(arg1 == "المصرية العربية")
            sb::lang = "ar_EG";
        else if(arg1 == "Català")
            sb::lang = "ca_ES";
        else if(arg1 == "Čeština")
            sb::lang = "cs_CS";
        else if(arg1 == "English")
            sb::lang = "en_EN";
        else if(arg1 == "Español")
            sb::lang = "es_ES";
        else if(arg1 == "Suomi")
            sb::lang = "fi_FI";
        else if(arg1 == "Français")
            sb::lang = "fr_FR";
        else if(arg1 == "Galego")
            sb::lang = "gl_ES";
        else if(arg1 == "Magyar")
            sb::lang = "hu_HU";
        else if(arg1 == "Bahasa Indonesia")
            sb::lang = "id_ID";
        else if(arg1 == "Português (Brasil)")
            sb::lang = "pt_BR";
        else if(arg1 == "Română")
            sb::lang = "ro_RO";
        else if(arg1 == "Türkçe")
            sb::lang = "tr_TR";
        else
            sb::lang = "zh_CN";

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
