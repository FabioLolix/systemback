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

#include "ui_systemback.h"
#include "systemback.hpp"
#include <QStringBuilder>
#include <QDesktopWidget>
#include <QFontDatabase>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <sys/swap.h>
#include <X11/Xlib.h>
#include <unistd.h>

#ifdef KeyRelease
#undef KeyRelease
#endif

#ifdef KeyPress
#undef KeyPress
#endif

systemback::systemback(QWidget *parent) : QMainWindow(parent, Qt::FramelessWindowHint), ui(new Ui::systemback)
{
    ui->setupUi(this);
    installEventFilter(this);
    unity = sb::pisrng("unity-panel-service");
    uchkd = nrxth = nohmcpy = cfgupdt = utblock = intrrpt = false;

    if(! sb::like(font().family(), {"_Ubuntu_", "_FreeSans_"}) || fontInfo().pixelSize() != 15 || fontInfo().weight() != QFont::Normal || fontInfo().bold() || fontInfo().italic() || fontInfo().overline() || fontInfo().strikeOut() || fontInfo().underline())
    {
        sfctr = fontInfo().pixelSize() > 28 ? Max : fontInfo().pixelSize() > 21 ? High : Normal;
        { while(sfctr > Normal && (qApp->desktop()->width() - ss(30) < ss(698) || qApp->desktop()->height() - ss(30) < ss(465))) sfctr = sfctr == Max ? High : Normal;
        QFont font;
        font.setFamily(QFontDatabase().families().contains("Ubuntu") ? "Ubuntu" : "FreeSans");
        font.setPixelSize(ss(15));
        if(font.weight() != QFont::Normal) font.setWeight(QFont::Normal);
        if(font.bold()) font.setBold(false);
        if(font.italic()) font.setItalic(false);
        if(font.overline()) font.setOverline(false);
        if(font.strikeOut()) font.setStrikeOut(false);
        if(font.underline()) font.setUnderline(false);
        setFont(font);
        font.setPixelSize(ss(27));
        ui->buttonspanel->setFont(font);
        font.setPixelSize(ss(17));
        font.setBold(true);
        ui->passwordtitletext->setFont(font); }

        if(sfctr > Normal)
        {
            for(QWidget *wdgt : findChildren<QWidget *>()) wdgt->setGeometry(ss(wdgt->x()), ss(wdgt->y()), ss(wdgt->width()), ss(wdgt->height()));
            for(QPushButton *pbtn : findChildren<QPushButton *>()) pbtn->setIconSize(QSize(ss(pbtn->iconSize().width()), ss(pbtn->iconSize().height())));
            ui->includeusers->setMinimumSize(ss(112), ss(32));
            ui->grubreinstallrestore->setMinimumSize(ui->includeusers->minimumSize());
            ui->grubreinstallrestoredisable->setMinimumSize(ui->includeusers->minimumSize());
            ui->grubinstallcopy->setMinimumSize(ui->includeusers->minimumSize());
            ui->grubinstallcopydisable->setMinimumSize(ui->includeusers->minimumSize());
            ui->grubreinstallrepair->setMinimumSize(ui->includeusers->minimumSize());
            ui->grubreinstallrepairdisable->setMinimumSize(ui->includeusers->minimumSize());
            ui->windowposition->setMinimumSize(ui->includeusers->minimumSize());
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
    else
        sfctr = Normal;

    if(qApp->arguments().count() == 3 && qApp->arguments().value(1) == "schedule")
    {
        sstart = true;
        dialog = 0;
    }
    else
    {
        sstart = false;

        if(getuid() + getgid() > 0)
            dialog = 17;
        else if(! sb::lock(sb::Sblock))
            dialog = 1;
        else if(sb::lock(sb::Dpkglock))
            dialog = 0;
        else
            dialog = 2;
    }

    ui->statuspanel->hide();
    ui->dialogpanel->move(0, 0);
    ui->restorepanel->hide();
    ui->copypanel->hide();
    ui->installpanel->hide();
    ui->livecreatepanel->hide();
    ui->repairpanel->hide();
    ui->excludepanel->hide();
    ui->schedulepanel->hide();
    ui->aboutpanel->hide();
    ui->licensepanel->hide();
    ui->choosepanel->hide();
    ui->buttonspanel->hide();
    ui->resizepanel->hide();
    ui->functionmenu2->hide();
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
    utimer = new QTimer;
    utimer->setInterval(500);
    bttnstimer = new QTimer;
    bttnstimer->setInterval(100);
    dlgtimer = new QTimer;
    dlgtimer->setInterval(1000);
    intrrptimer = new QTimer;
    intrrptimer->setInterval(100);
    connect(utimer, SIGNAL(timeout()), this, SLOT(unitimer()));
    connect(bttnstimer, SIGNAL(timeout()), this, SLOT(buttonstimer()));
    connect(dlgtimer, SIGNAL(timeout()), this, SLOT(dialogtimer()));
    connect(intrrptimer, SIGNAL(timeout()), this, SLOT(interrupt()));
    connect(ui->function3, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
    connect(ui->function3, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
    connect(ui->function3, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
    connect(ui->windowbutton3, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
    connect(ui->windowbutton3, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
    connect(ui->buttonspanel, SIGNAL(Mouse_Leave()), this, SLOT(bleave()));
    connect(ui->windowmaximize, SIGNAL(Mouse_Enter()), this, SLOT(wmaxenter()));
    connect(ui->windowmaximize, SIGNAL(Mouse_Leave()), this, SLOT(wmaxleave()));
    connect(ui->windowmaximize, SIGNAL(Mouse_Pressed()), this, SLOT(wmaxpressed()));
    connect(ui->windowmaximize, SIGNAL(Mouse_Released()), this, SLOT(wmaxreleased()));
    connect(ui->windowminimize, SIGNAL(Mouse_Enter()), this, SLOT(wminenter()));
    connect(ui->windowminimize, SIGNAL(Mouse_Leave()), this, SLOT(wminleave()));
    connect(ui->windowminimize, SIGNAL(Mouse_Pressed()), this, SLOT(wminpressed()));
    connect(ui->windowminimize, SIGNAL(Mouse_Released()), this, SLOT(wminreleased()));
    connect(ui->windowclose, SIGNAL(Mouse_Enter()), this, SLOT(wcenter()));
    connect(ui->windowclose, SIGNAL(Mouse_Leave()), this, SLOT(wcleave()));
    connect(ui->windowclose, SIGNAL(Mouse_Pressed()), this, SLOT(wcpressed()));
    connect(ui->windowclose, SIGNAL(Mouse_Released()), this, SLOT(wcreleased()));

    if(dialog > 0)
    {
        ui->mainpanel->hide();
        ui->passwordpanel->hide();
        ui->schedulerpanel->hide();
        dialogopen();
    }
    else
    {
        grub = "pc";
        ppipe = busycnt = 0;
        irfsc = false;
        ui->dialogpanel->hide();
        ui->statuspanel->move(0, 0);
        ui->storagedirbutton->hide();
        ui->storagedir->resize(ss(236), ss(28));
        ui->installpanel->move(ui->sbpanel->pos());
        ui->fullnamepipe->hide();
        ui->fullnameerror->hide();
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
        ui->statuspanel->setBackgroundRole(QPalette::Foreground);
        ui->substatuspanel->setBackgroundRole(QPalette::Background);
        ui->function1->setForegroundRole(QPalette::Base);
        ui->function2->setForegroundRole(QPalette::Base);
        ui->function4->setForegroundRole(QPalette::Base);
        ui->windowbutton1->setForegroundRole(QPalette::Base);
        ui->windowbutton2->setForegroundRole(QPalette::Base);
        ui->windowbutton4->setForegroundRole(QPalette::Base);
        ui->storagedirarea->setBackgroundRole(QPalette::Base);
        ui->interrupt->setStyleSheet("QPushButton:enabled {color: red}");
        if(sb::isdir("/.systemback") && (sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/live/image/live/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))) on_installmenu_clicked();
        connect(ui->function1, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
        connect(ui->function1, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
        connect(ui->function1, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
        connect(ui->function1, SIGNAL(Mouse_DblClick()), this, SLOT(wdblclck()));
        connect(ui->function2, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
        connect(ui->function2, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
        connect(ui->function2, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
        connect(ui->function4, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed()));
        connect(ui->function4, SIGNAL(Mouse_Move()), this, SLOT(wmove()));
        connect(ui->function4, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));
        connect(ui->windowbutton1, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
        connect(ui->windowbutton1, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
        connect(ui->windowbutton2, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
        connect(ui->windowbutton2, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
        connect(ui->windowbutton4, SIGNAL(Mouse_Enter()), this, SLOT(benter()));
        connect(ui->windowbutton4, SIGNAL(Mouse_Pressed()), this, SLOT(benter()));
        connect(ui->chooseresize, SIGNAL(Mouse_Enter()), this, SLOT(chssenter()));
        connect(ui->chooseresize, SIGNAL(Mouse_Leave()), this, SLOT(chssleave()));
        connect(ui->chooseresize, SIGNAL(Mouse_Pressed()), this, SLOT(chsspressed()));
        connect(ui->chooseresize, SIGNAL(Mouse_Released()), this, SLOT(chssreleased()));
        connect(ui->chooseresize, SIGNAL(Mouse_Move()), this, SLOT(chssmove()));
        connect(ui->copyresize, SIGNAL(Mouse_Enter()), this, SLOT(cpyenter()));
        connect(ui->copyresize, SIGNAL(Mouse_Leave()), this, SLOT(cpyleave()));
        connect(ui->copyresize, SIGNAL(Mouse_Pressed()), this, SLOT(chsspressed()));
        connect(ui->copyresize, SIGNAL(Mouse_Released()), this, SLOT(chssreleased()));
        connect(ui->copyresize, SIGNAL(Mouse_Move()), this, SLOT(cpymove()));
        connect(ui->excluderesize, SIGNAL(Mouse_Enter()), this, SLOT(xcldenter()));
        connect(ui->excluderesize, SIGNAL(Mouse_Leave()), this, SLOT(xcldleave()));
        connect(ui->excluderesize, SIGNAL(Mouse_Pressed()), this, SLOT(chsspressed()));
        connect(ui->excluderesize, SIGNAL(Mouse_Released()), this, SLOT(chssreleased()));
        connect(ui->excluderesize, SIGNAL(Mouse_Move()), this, SLOT(xcldmove()));
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
        connect(ui->umountdelete, SIGNAL(Mouse_Leave()), this, SLOT(umntleave()));

        if(qApp->arguments().count() == 3 && qApp->arguments().value(1) == "authorization")
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
                {
                    while(! file.atEnd())
                    {
                        QStr usrs(file.readLine().trimmed());

                        if(sb::like(usrs, {"_sudo:*", "_admins:*"}))
                        {
                            usrs = sb::right(usrs, - sb::rinstr(usrs, ":"));

                            for(cQStr &usr : usrs.split(','))
                                if(! usr.isEmpty() && ui->admins->findText(usr) == -1) ui->admins->addItem(usr);
                        }
                    }
                }
            }

            if(ui->admins->count() == 0)
                ui->admins->addItem("root");
            else if(ui->admins->findText(qApp->arguments().value(2)) != -1)
                ui->admins->setCurrentIndex(ui->admins->findText(qApp->arguments().value(2)));

            wgeom[0] = qApp->desktop()->width() / 2 - ss(188);
            wgeom[1] = qApp->desktop()->height() / 2 - ss(112);
            wgeom[2] = ss(376);
            wgeom[3] = ss(224);
            setFixedSize(wgeom[2], wgeom[3]);
            move(wgeom[0], wgeom[1]);
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

                if(qApp->arguments().value(2) == "topleft")
                {
                    wgeom[0] = ss(30);
                    wgeom[1] = ss(30);
                }
                else if(qApp->arguments().value(2) == "center")
                {
                    wgeom[0] = qApp->desktop()->width() / 2 - ss(201);
                    wgeom[1] = qApp->desktop()->height() / 2 - ss(80);
                }
                 else if(qApp->arguments().value(2) == "bottomleft")
                {
                    wgeom[0] = ss(30);
                    wgeom[1] = qApp->desktop()->height() - ss(191);
                }
                else if(qApp->arguments().value(2) == "bottomright")
                {
                    wgeom[0] = qApp->desktop()->width() - ss(432);
                    wgeom[1] = qApp->desktop()->height() - ss(191);
                }
                else
                {
                    wgeom[0] = qApp->desktop()->width() - ss(432);
                    wgeom[1] = ss(30);
                }

                wgeom[2] = ss(402);
                wgeom[3] = ss(161);
                setFixedSize(wgeom[2], wgeom[3]);
                move(wgeom[0], wgeom[1]);
                shdltimer = new QTimer;
                connect(shdltimer, SIGNAL(timeout()), this, SLOT(schedulertimer()));
                shdltimer->start(1000);
                setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
            }
            else
            {
                ui->schedulerpanel->hide();
                wgeom[0] = qApp->desktop()->width() / 2 - ss(349);
                wgeom[1] = qApp->desktop()->height() / 2 - ss(232);
                wgeom[2] = ss(698);
                wgeom[3] = ss(465);
                setFixedSize(wgeom[2], wgeom[3]);
                move(wgeom[0], wgeom[1]);
            }
        }
    }

    if(unity) setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
}

systemback::~systemback()
{
    if(cfgupdt) sb::cfgwrite();

    if(! nrxth)
    {
        QStr xauth(qgetenv("XAUTHORITY"));
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

        if(! utimer->isActive())
        {
            sb::cfgread();

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
                ui->liveworkdir->setText(sb::sdir[2]);
                ui->restorepanel->move(ui->sbpanel->pos());
                ui->copypanel->move(ui->sbpanel->pos());
                ui->livecreatepanel->move(ui->sbpanel->pos());
                ui->repairpanel->move(ui->sbpanel->pos());
                ui->excludepanel->move(ui->sbpanel->pos());
                ui->schedulepanel->move(ui->sbpanel->pos());
                ui->aboutpanel->move(ui->sbpanel->pos());
                ui->licensepanel->move(ui->sbpanel->pos());
                ui->choosepanel->move(ui->sbpanel->pos());
                ui->restorepanel->setBackgroundRole(QPalette::Background);
                ui->copypanel->setBackgroundRole(QPalette::Background);
                ui->livecreatepanel->setBackgroundRole(QPalette::Background);
                ui->repairpanel->setBackgroundRole(QPalette::Background);
                ui->excludepanel->setBackgroundRole(QPalette::Background);
                ui->schedulepanel->setBackgroundRole(QPalette::Background);
                ui->aboutpanel->setBackgroundRole(QPalette::Background);
                ui->licensepanel->setBackgroundRole(QPalette::Background);
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
                ui->grubinstallcopydisable->hide();
                ui->grubinstallcopydisable->addItem(tr("Disabled"));
                ui->grubreinstallrepairdisable->hide();
                ui->grubreinstallrepairdisable->addItem(tr("Disabled"));
                ui->usersettingscopy->hide();
                ui->copycover->hide();
                ui->livecreatecover->hide();
                ui->repaircover->hide();
                if(sb::schdle[0] == "on") ui->schedulerstate->click();
                if(sb::schdle[5] == "on") ui->silentmode->setChecked(true);
                ui->windowposition->addItems({tr("Top left"), tr("Top right"), tr("Center"), tr("Bottom left"), tr("Bottom right")});

                if(sb::schdle[6] == "topright")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Top right")));
                else if(sb::schdle[6] == "center")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Center")));
                else if(sb::schdle[6] == "bottomleft")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Bottom left")));
                else if(sb::schdle[6] == "bottomright")
                    ui->windowposition->setCurrentIndex(ui->windowposition->findText(tr("Bottom right")));

                ui->schedulerday->setText(sb::schdle[1] % ' ' % tr("day(s)"));
                ui->schedulerhour->setText(sb::schdle[2] % ' ' % tr("hour(s)"));
                ui->schedulerminute->setText(sb::schdle[3] % ' ' % tr("minute(s)"));
                ui->schedulersecond->setText(sb::schdle[4] % ' ' % tr("seconds"));
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
                ui->warning->resize(ui->warning->fontMetrics().width(ui->warning->text()) + ss(7), ui->warning->height());
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
                ui->filesystem->addItems({"ext4", "ext3", "ext2"});
                if(sb::execsrch("mkfs.btrfs")) ui->filesystem->addItem("btrfs");
                if(sb::execsrch("mkfs.reiserfs")) ui->filesystem->addItem("reiserfs");
                if(sb::execsrch("mkfs.jfs")) ui->filesystem->addItem("jfs");
                if(sb::execsrch("mkfs.xfs")) ui->filesystem->addItem("xfs");
                { QFile file(":version");
                file.open(QIODevice::ReadOnly);
                ui->systembackversion->setText(file.readLine().trimmed() % "_Qt" % (QStr(qVersion()) == QStr(QT_VERSION_STR) ? QStr(qVersion()) : QStr(qVersion()) % '(' % QStr(QT_VERSION_STR) % ')') % '_' % sb::getarch()); }
                ui->repairmountpoint->addItems({nullptr, "/mnt", "/mnt/home", "/mnt/boot"});

                if(sb::efiprob())
                {
                    grub = "efi-amd64";
                    ui->repairmountpoint->addItem("/mnt/boot/efi");
                }

                ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"});
                ui->repairmountpoint->setCurrentIndex(1);
                on_partitionrefresh_clicked();
                on_livedevicesrefresh_clicked();
                on_pointexclude_clicked();
                ui->storagedir->resize(ss(210), ss(28));
                ui->storagedirbutton->show();
                ui->repairmenu->setEnabled(true);
                ui->aboutmenu->setEnabled(true);
                ui->pnumber3->setEnabled(true);
                ui->pnumber4->setEnabled(true);
                ui->pnumber5->setEnabled(true);
                ui->pnumber6->setEnabled(true);
                ui->pnumber7->setEnabled(true);
                ui->pnumber8->setEnabled(true);
                ui->pnumber9->setEnabled(true);
                ui->pnumber10->setEnabled(true);

                if(! ui->installpanel->isVisible())
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
            ickernel = sb::ickernel() || sb::execsrch("unionfs-fuse");
            busy(false);
            utimer->start();
        }
        else if(ui->statuspanel->isVisible())
        {
            if(! prun.isEmpty())
            {
                if(points == "    ")
                    points = " .  ";
                else if(points == " .  ")
                    points = " .. ";
                else if(points == " .. ")
                    points = " ...";
                else if(points == " ...")
                    points = "    ";
                else
                    points = "    ";

                ui->processrun->setText(prun % points);
            }

            if(sb::like(prun, {'_' % tr("Creating restore point") % '_', '_' % tr("Restoring the full system") % '_', '_' % tr("Restoring the system files") % '_', '_' % tr("Restoring user(s) configuration files") % '_', '_' % tr("Repairing the system files") % '_', '_' % tr("Repairing the full system") % '_', '_' % tr("Copying the system") % '_', '_' % tr("Installing the system") % '_', '_' % tr("Creating Live system") % '\n' % tr("process") % " 3/3_", '_' % tr("Writing Live image to USB device") % '_', '_' % tr("Converting Live system image") % "\n*"}))
            {
                if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                schar cperc(sb::Progress);

                if(cperc != -1)
                {
                    if(ui->progressbar->maximum() == 0) ui->progressbar->setMaximum(100);

                    if(cperc < 100)
                    {
                        if(ui->progressbar->value() < cperc)
                            ui->progressbar->setValue(cperc);
                        else if(cperc == 99 && ui->progressbar->value() == 99)
                            ui->progressbar->setValue(100);
                    }
                    else if(ui->progressbar->value() < 100)
                        ui->progressbar->setValue(100);
                }
                else if(ui->progressbar->maximum() == 100)
                    ui->progressbar->setMaximum(0);
            }
            else if(prun == tr("Creating Live system") % '\n' % tr("process") % " 2/3")
            {
                if(irfsc)
                {
                    if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);
                }
                else if(! ui->interrupt->isEnabled())
                    ui->interrupt->setEnabled(true);

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
                if(sb::like(prun, {'_' % tr("Deleting restore point") % "*", '_' % tr("Deleting old restore point") % "*", '_' % tr("Deleting incomplete restore point") % '_', '_' % tr("Creating Live system") % "\n*"}))
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
            {
                if(sb::mcheck("/mnt"))
                {
                    if(ui->grubrepair->isChecked())
                    {
                        if(sb::execsrch("update-grub2") && sb::isfile("/mnt/var/lib/dpkg/info/grub-" % grub % ".list"))
                        {
                            if(ui->grubreinstallrepairdisable->isVisible())
                            {
                                ui->grubreinstallrepairdisable->hide();
                                ui->grubreinstallrepair->show();
                            }

                            if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
                        }
                        else
                        {
                            if(ui->grubreinstallrepair->isVisible())
                            {
                                ui->grubreinstallrepair->hide();
                                ui->grubreinstallrepairdisable->show();
                            }

                            if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
                        }
                    }
                    else if(! ui->repairnext->isEnabled())
                        ui->repairnext->setEnabled(true);
                }
                else
                {
                    if(ui->grubrepair->isChecked() && ui->grubreinstallrepair->isVisible())
                    {
                        ui->grubreinstallrepair->hide();
                        ui->grubreinstallrepairdisable->show();
                    }

                    if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
                }
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
    {
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
        qApp->restoreOverrideCursor();
        break;
    case 1:
        if(! qApp->overrideCursor()) qApp->setOverrideCursor(Qt::WaitCursor);
    }
}

inline bool systemback::minside(cQPoint &wpos, cQRect &wgeom)
{
    return QCursor::pos().x() < pos().x() + wpos.x() || QCursor::pos().y() < pos().y() + wpos.y() || QCursor::pos().x() > pos().x() + wpos.x() + wgeom.width() || QCursor::pos().y() > pos().y() + wpos.y() + wgeom.height() ? false : true;
}

QStr systemback::guname()
{
    if(! uchkd && (ui->admins->count() == 0 || ui->admins->currentText() == "root"))
    {
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

        for(cQStr &usr : usrs)
            if(sb::isdir("/home/" % usr))
            {
                ui->admins->addItem(usr);
                ui->admins->setCurrentIndex(ui->admins->findText(usr));
                goto ok;
            }

        if(usrs.count() > 0)
        {
            cQStr &usr(usrs.at(0));
            ui->admins->addItem(usr);
            ui->admins->setCurrentIndex(ui->admins->findText(usr));
        }
        else if(ui->admins->currentText().isNull())
            ui->admins->addItem("root");

    ok:
        uchkd = true;
    }

    return ui->admins->currentText();
}

void systemback::buttonstimer()
{
    if(minside(ui->buttonspanel->pos(), ui->buttonspanel->geometry()))
    {
        if(ui->windowmaximize->isVisible())
        {
            if(minside(ui->buttonspanel->pos() + ui->windowmaximize->pos(), ui->windowmaximize->geometry()))
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
        if(minside(ui->buttonspanel->pos() + ui->windowminimize->pos(), ui->windowminimize->geometry()))
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
            if(minside(ui->buttonspanel->pos() + ui->windowclose->pos(), ui->windowclose->geometry()))
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
    }
    else
    {
        ui->buttonspanel->hide();
        bttnstimer->stop();
    }
}

void systemback::schedulertimer()
{
    if(ui->schedulernumber->text().isEmpty())
        ui->schedulernumber->setText(sb::schdle[4] % 's');
    else if(ui->schedulernumber->text() == "1s")
        on_schedulerstart_clicked();
    else
        ui->schedulernumber->setText(QStr::number(sb::left(ui->schedulernumber->text(), - 1).toShort() - 1) % 's');
}

void systemback::dialogtimer()
{
    ui->dialognumber->setText(QStr::number(sb::left(ui->dialognumber->text(), -1).toShort() - 1) % "s");
    if(ui->dialognumber->text() == "0s" && sb::like(ui->dialogok->text(), {'_' % tr("Reboot") % '_', '_' % tr("X restart") % '_'})) on_dialogok_clicked();
}

void systemback::wpressed()
{
    if(size() != qApp->desktop()->availableGeometry().size()) qApp->setOverrideCursor(Qt::SizeAllCursor);
}

void systemback::wreleased()
{
    qApp->restoreOverrideCursor();

    if(size() != qApp->desktop()->availableGeometry().size())
    {
        if(x() < 0)
            wgeom[0] = ss(30);
        else if(x() > qApp->desktop()->width() - width())
            wgeom[0] = qApp->desktop()->width() - width() - ss(30);

        if(y() < 0)
            wgeom[1] = ss(30);
        else if(y() > qApp->desktop()->height() - height())
            wgeom[1] = qApp->desktop()->height() - height() - ss(30);

        if(x() != wgeom[0] || y() != wgeom[1]) move(wgeom[0], wgeom[1]);
    }
}

void systemback::wdblclck()
{
    if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->choosepanel->isVisible())
    {
        if(size() == qApp->desktop()->availableGeometry().size())
        {
            setGeometry(wgeom[4], wgeom[5], wgeom[2], wgeom[3]);
            setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
        }
        else
        {
            wgeom[4] = wgeom[0];
            wgeom[5] = wgeom[1];
            setMaximumSize(qApp->desktop()->availableGeometry().size());
            setGeometry(qApp->desktop()->availableGeometry());

            if(ui->copypanel->isVisible())
            {
                ui->partitionsettings->resizeColumnToContents(2);
                ui->partitionsettings->resizeColumnToContents(3);
                ui->partitionsettings->resizeColumnToContents(4);
            }
        }
    }
}

void systemback::benter()
{
    if(ui->statuspanel->isVisible())
    {
        if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
        {
            ui->windowmaximize->hide();
            ui->windowminimize->move(ss(2), ss(2));
        }

        if(ui->windowclose->isVisible()) ui->windowclose->hide();
        if(ui->buttonspanel->width() != ss(48)) ui->buttonspanel->resize(ss(48), ss(48));
    }
    else if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->choosepanel->isVisible())
    {
        if(ui->windowmaximize->isHidden())
        {
            ui->windowmaximize->show();
            ui->windowminimize->move(ss(47), ss(2));
        }

        if(size() == qApp->desktop()->availableGeometry().size())
        {
            if(ui->windowmaximize->text() == "□") ui->windowmaximize->setText("▭");
        }
        else if(ui->windowmaximize->text() == "▭")
            ui->windowmaximize->setText("□");

        if(ui->windowclose->x() != ss(92)) ui->windowclose->move(ss(92), ss(2));
        if(ui->windowclose->isHidden()) ui->windowclose->show();
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
        if(ui->windowclose->isHidden()) ui->windowclose->show();
        if(ui->buttonspanel->width() != ss(93)) ui->buttonspanel->resize(ss(93), ss(48));
    }

    if(ui->buttonspanel->x() != width() - ui->buttonspanel->width()) ui->buttonspanel->move(width() - ui->buttonspanel->width(), 0);
    ui->buttonspanel->show();
    buttonstimer();
    bttnstimer->start();
}

void systemback::bleave()
{
    ui->buttonspanel->hide();
    bttnstimer->stop();
}

void systemback::wmaxenter()
{
     ui->windowmaximize->setBackgroundRole(QPalette::Background);
     ui->windowmaximize->setForegroundRole(QPalette::Text);
}

void systemback::wmaxleave()
{
    ui->windowmaximize->setBackgroundRole(QPalette::Foreground);
    ui->windowmaximize->setForegroundRole(QPalette::Base);
}

void systemback::wmaxpressed()
{
    ui->windowmaximize->setForegroundRole(QPalette::Highlight);
}

void systemback::wmaxreleased()
{
    if(ui->buttonspanel->isVisible() && ui->windowmaximize->foregroundRole() == QPalette::Highlight)
    {
        if(ui->windowmaximize->text() == "□")
        {
            wgeom[4] = wgeom[0];
            wgeom[5] = wgeom[1];
            setMaximumSize(qApp->desktop()->availableGeometry().size());
            setGeometry(qApp->desktop()->availableGeometry());

            if(ui->copypanel->isVisible())
            {
                ui->partitionsettings->resizeColumnToContents(2);
                ui->partitionsettings->resizeColumnToContents(3);
                ui->partitionsettings->resizeColumnToContents(4);
            }
        }
        else
        {
            setGeometry(wgeom[4], wgeom[5], wgeom[2], wgeom[3]);
            setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
        }
    }

    if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
}

void systemback::wminenter()
{
    ui->windowminimize->setBackgroundRole(QPalette::Background);
    ui->windowminimize->setForegroundRole(QPalette::Text);
}

void systemback::wminleave()
{
    ui->windowminimize->setBackgroundRole(QPalette::Foreground);
    ui->windowminimize->setForegroundRole(QPalette::Base);
}

void systemback::wminpressed()
{
    ui->windowminimize->setForegroundRole(QPalette::Highlight);
}

void systemback::wminreleased()
{
    if(ui->buttonspanel->isVisible() && ui->windowminimize->foregroundRole() == QPalette::Highlight) showMinimized();
}

void systemback::wcenter()
{
    ui->windowclose->setBackgroundRole(QPalette::Background);
    ui->windowclose->setForegroundRole(QPalette::Text);
}

void systemback::wcleave()
{
    ui->windowclose->setBackgroundRole(QPalette::Foreground);
    ui->windowclose->setForegroundRole(QPalette::Base);
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

void systemback::chssenter()
{
    if(size() != qApp->desktop()->availableGeometry().size() && ui->chooseresize->width() == ss(10)) ui->chooseresize->setGeometry(ui->chooseresize->x() - ss(20), ui->chooseresize->y() - ss(20), ss(30), ss(30));
}

void systemback::chssleave()
{
    if(ui->chooseresize->width() == ss(30) && ! qApp->overrideCursor()) ui->chooseresize->setGeometry(ui->chooseresize->x() + ss(20), ui->chooseresize->y() + ss(20), ss(10), ss(10));
}

void systemback::chsspressed()
{
    if(size() != qApp->desktop()->availableGeometry().size()) qApp->setOverrideCursor(Qt::SizeFDiagCursor);
}

void systemback::chssreleased()
{
    if(qApp->overrideCursor()) qApp->restoreOverrideCursor();
}

void systemback::chssmove()
{
    if(size() != qApp->desktop()->availableGeometry().size())
    {
        wgeom[2] = QCursor::pos().x() - x() + width() - ui->chooseresize->x() - ui->chooseresize->MouseX - ss(1);
        wgeom[3] = QCursor::pos().y() - y() + height() - ui->chooseresize->y() - ui->chooseresize->MouseY - ss(24);
        if(width() != wgeom[2] || height() != wgeom[3]) resize(wgeom[2], wgeom[3]);
    }
}

void systemback::cpyenter()
{
    if(size() != qApp->desktop()->availableGeometry().size() && ui->copyresize->width() == ss(10)) ui->copyresize->setGeometry(ui->copyresize->x() - ss(20), ui->copyresize->y() - ss(20), ss(30), ss(30));
}

void systemback::cpyleave()
{
    if(ui->copyresize->width() == ss(30) && ! qApp->overrideCursor()) ui->copyresize->setGeometry(ui->copyresize->x() + ss(20), ui->copyresize->y() + ss(20), ss(10), ss(10));
}

void systemback::cpymove()
{
    if(size() != qApp->desktop()->availableGeometry().size())
    {
        wgeom[2] = QCursor::pos().x() - x() + width() - ui->copyresize->x() - ui->copyresize->MouseX - ss(1);
        wgeom[3] = QCursor::pos().y() - y() + height() - ui->copyresize->y() - ui->copyresize->MouseY - ss(24);
        if(width() != wgeom[2] || height() != wgeom[3]) resize(wgeom[2], wgeom[3]);
    }
}

void systemback::xcldenter()
{
    if(size() != qApp->desktop()->availableGeometry().size() && ui->excluderesize->width() == ss(10)) ui->excluderesize->setGeometry(ui->excluderesize->x() - ss(20), ui->excluderesize->y() - ss(20), ss(30), ss(30));
}

void systemback::xcldleave()
{
    if(ui->excluderesize->width() == ss(30) && ! qApp->overrideCursor()) ui->excluderesize->setGeometry(ui->excluderesize->x() + ss(20), ui->excluderesize->y() + ss(20), ss(10), ss(10));
}

void systemback::xcldmove()
{
    if(size() != qApp->desktop()->availableGeometry().size())
    {
        wgeom[2] = QCursor::pos().x() - x() + width() - ui->excluderesize->x() - ui->excluderesize->MouseX - ss(1);
        wgeom[3] = QCursor::pos().y() - y() + height() - ui->excluderesize->y() - ui->excluderesize->MouseY - ss(24);
        if(width() != wgeom[2] || height() != wgeom[3]) resize(wgeom[2], wgeom[3]);
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
    if(minside(ui->aboutpanel->pos() + ui->homepage1->pos(), ui->homepage1->geometry()))
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
    if(minside(ui->aboutpanel->pos() + ui->homepage2->pos(), ui->homepage2->geometry()))
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
    if(minside(ui->aboutpanel->pos() + ui->email->pos(), ui->email->geometry()))
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
    if(minside(ui->aboutpanel->pos() + ui->donate->pos(), ui->donate->geometry()))
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
    if(! ui->umountdelete->isEnabled() && ui->umountdelete->text() == tr("! Delete !")) ui->umountdelete->setEnabled(true);
}

void systemback::on_usersettingscopy_stateChanged(int arg1)
{
    ui->usersettingscopy->setText(arg1 == 1 ? tr("Transfer user configuration files") : tr("Transfer user configuration and data files"));
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
    if(sstart && dialog != 16)
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
        if(ui->keepfiles->isChecked() && ui->includeusers->currentIndex() == 0)
            mthd = 3;
        else if(ui->keepfiles->isChecked())
            mthd = 4;
        else if(ui->includeusers->currentIndex() == 0)
            mthd = 5;
        else
            mthd = 6;

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
                uchar rv(0);

                if(ui->autorestoreoptions->isChecked() || ui->grubreinstallrestore->currentText() == "Auto")
                {
                    if(fcmp)
                        sb::exec("update-grub");
                    else
                    {
                        sb::exec("update-grub");
                        if(intrrpt) goto exit;
                        QStr mntdev;

                        {
                            QStr mnts(sb::fload("/proc/self/mounts"));
                            QTS in(&mnts, QIODevice::ReadOnly);

                            while(! in.atEnd())
                            {
                                QStr cline(in.readLine());

                                if(cline.contains(" /boot "))
                                {
                                    mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                                    break;
                                }
                                else if(cline.contains(" / "))
                                    mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                            }
                        }

                        if(intrrpt) goto exit;
                        rv = sb::exec("grub-install --force " % sb::left(mntdev, 8));
                    }
                }
                else
                {
                    sb::exec("update-grub");
                    if(intrrpt) goto exit;
                    rv = sb::exec("grub-install --force " % ui->grubreinstallrestore->currentText());
                }

                if(intrrpt) goto exit;
                if(rv > 0) dialog = 23;
            }

            sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        if(! sb::ilike(dialog, {23, 60}))
        {
            switch(mthd) {
            case 1:
                dialog = 20;
                break;
            case 2:
                dialog = 19;
                break;
            default:
                dialog = ui->keepfiles->isChecked() ? 10 : 9;
            }
        }
    }
    else
        dialog = 60;

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
        sb::mount("/dev", "/mnt/dev");
        sb::mount("/dev/pts", "/mnt/dev/pts");
        sb::mount("/proc", "/mnt/proc");
        sb::mount("/sys", "/mnt/sys");
        if(intrrpt) goto exit;

        if(ui->grubreinstallrepair->currentText() == "Auto")
        {
            QStr mntdev, mnts(sb::fload("/proc/self/mounts"));
            QTS in(&mnts, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(in.readLine());

                if(cline.contains(" /mnt/boot "))
                {
                    mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                    break;
                }
                else if(cline.contains(" /mnt "))
                    mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
            }

            sb::crtfile("/mnt/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % sb::left(mntdev, 8) % '\n');
        }
        else
            sb::crtfile("/mnt/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % ui->grubreinstallrepair->currentText() % '\n');

        QFile::setPermissions("/mnt/grubinst", QFile::ExeOwner);
        if(intrrpt) goto exit;
        dialog = sb::exec("chroot /mnt /grubinst") == 0 ? 32 : 37;
        QFile::remove("/mnt/grubinst");
        sb::umount("/mnt/dev");
        sb::umount("/mnt/dev/pts");
        sb::umount("/mnt/proc");
        sb::umount("/mnt/sys");
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

        if(pname == tr("Live image"))
        {
            if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
            {
                QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                QDir().mkdir("/.systembacklivepoint");
            }

            if(! sb::mount(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs") ? "/lib/live/mount/medium/live/filesystem.squashfs" : "/live/image/live/filesystem.squashfs", "/.systembacklivepoint", "loop"))
            {
                dialog = 55;
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
                sb::mount("/dev", "/mnt/dev");
                sb::mount("/dev/pts", "/mnt/dev/pts");
                sb::mount("/proc", "/mnt/proc");
                sb::mount("/sys", "/mnt/sys");
                if(intrrpt) goto exit;

                if(ui->autorepairoptions->isChecked() || ui->grubreinstallrepair->currentText() == "Auto")
                {
                    if(fcmp)
                        sb::crtfile("/mnt/grubinst", "#!/bin/sh\nupdate-grub\n");
                    else
                    {
                        QStr mntdev, mnts(sb::fload("/proc/self/mounts"));
                        QTS in(&mnts, QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            QStr cline(in.readLine());

                            if(cline.contains(" /mnt/boot "))
                            {
                                mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                                break;
                            }
                            else if(cline.contains(" /mnt "))
                                mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                        }

                        sb::crtfile("/mnt/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % sb::left(mntdev, 8) % '\n');
                    }
                }
                else
                    sb::crtfile("/mnt/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % ui->grubreinstallrepair->currentText() % '\n');

                QFile::setPermissions("/mnt/grubinst", QFile::ExeOwner);
                if(intrrpt) goto exit;
                if(sb::exec("chroot /mnt /grubinst") > 0) dialog = ui->fullrepair->isChecked() ? 24 : 11;
                QFile::remove("/mnt/grubinst");
                sb::umount("/mnt/dev");
                sb::umount("/mnt/dev/pts");
                sb::umount("/mnt/proc");
                sb::umount("/mnt/sys");
                if(intrrpt) goto exit;
            }

            prun = tr("Emptying cache");
            sb::fssync();
            sb::crtfile("/proc/sys/vm/drop_caches", "3");

            if(sb::ilike(dialog, {5, 6, 41}))
            {
                if(ppipe == 1 && sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
                dialog = ui->fullrepair->isChecked() ? 12 : 13;
            }
        }
        else
            dialog = 61;
    }

    dialogopen();
}

void systemback::systemcopy()
{
    QStr mnts[2];
    goto start;
error:
    if(intrrpt) goto exit;

    if(sb::ilike(dialog, {22, 34}))
    {
        sb::umount("/.sbsystemcopy/dev");
        sb::umount("/.sbsystemcopy/dev/pts");
        sb::umount("/.sbsystemcopy/proc");
        sb::umount("/.sbsystemcopy/sys");
    }
    else if(! sb::ilike(dialog, {31, 36, 51, 52, 53, 54}))
    {
        if(sb::dfree("/.sbsystemcopy") > 104857600 && (! sb::isdir("/.sbsystemcopy/home") || sb::dfree("/.sbsystemcopy/home") > 104857600) && (! sb::isdir("/.sbsystemcopy/boot") || sb::dfree("/.sbsystemcopy/boot") > 52428800))
        {
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 39 : 40;
        }
        else
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 21 : 35;
    }

    mnts[0] = sb::fload("/proc/self/mounts");

    {
        QTS in(&mnts[0], QIODevice::ReadOnly);
        while(! in.atEnd()) mnts[1].prepend(in.readLine() % '\n');
        in.setString(&mnts[1], QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, {"* /.sbsystemcopy*", "* /.systembacklivepoint *"})) sb::umount(sb::left(cline, sb::instr(cline, " ") - 1));
        }
    }

    QDir().rmdir("/.sbsystemcopy");
    if(sb::isdir("/.systembacklivepoint")) QDir().rmdir("/.systembacklivepoint");
    dialogopen();
    return;
exit:
    mnts[0] = sb::fload("/proc/self/mounts");

    {
        QTS in(&mnts[0], QIODevice::ReadOnly);
        while(! in.atEnd()) mnts[1].prepend(in.readLine() % '\n');
        in.setString(&mnts[1], QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, {"* /.sbsystemcopy*", "* /.systembacklivepoint *"})) sb::umount(sb::left(cline, sb::instr(cline, " ") - 1));
        }
    }

    QDir().rmdir("/.sbsystemcopy");
    if(sb::isdir("/.systembacklivepoint")) QDir().rmdir("/.systembacklivepoint");
    intrrpt = false;
    return;
start:
    statustart();
    prun = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? tr("Copying the system") : tr("Installing the system");
    QSL msort;

    for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        if(! ui->partitionsettings->item(a, 4)->text().isEmpty() && (ui->partitionsettings->item(a, 4)->text() != "/home" || ui->partitionsettings->item(a, 3)->text().isEmpty()))
            msort.append(ui->partitionsettings->item(a, 4)->text() % (ui->partitionsettings->item(a, 6)->text() == "x" ? QStr('\n' % ui->partitionsettings->item(a, 5)->text() % '\n') : "\n-\n") % ui->partitionsettings->item(a, 0)->text());

    msort.sort();
    if(! sb::isdir("/.sbsystemcopy") && ! QDir().mkdir("/.sbsystemcopy")) goto error;
    ushort rv;

    for(cQStr &vals : msort)
    {
        QSL cval(vals.split('\n'));
        cQStr &mpoint(cval.at(0)), &fstype(cval.at(1)), &part(cval.at(2));
        if(sb::mcheck(part)) sb::umount(part);
        if(intrrpt) goto exit;
        sb::fssync();
        if(intrrpt) goto exit;

        if(fstype != "-")
        {
            QStr lbl("SB@" % (mpoint.startsWith('/') ? sb::right(mpoint, -1) : mpoint));

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
                rv = sb::exec("mkfs.btrfs -fL " % lbl % ' ' % part);
                if(rv > 0) rv = sb::exec("mkfs.btrfs -L " % lbl % ' ' % part);
            }
            else
                 rv = sb::exec("mkfs." % fstype % " -L " % lbl % ' ' % part);

            if(intrrpt) goto exit;

            if(rv > 0)
            {
                dialogdev = part;
                dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 36 : 52;
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

            if(sb::mount(part, "/.sbsystemcopy" % mpoint))
            {
                if(fstype == "btrfs")
                {
                    sb::exec("btrfs subvolume create /.sbsystemcopy" % mpoint % "/@" % sb::right(mpoint, -1));
                    sb::umount(part);
                    if(intrrpt) goto exit;

                    if(! sb::mount(part, "/.sbsystemcopy" % mpoint, "defaults,subvol=@" % sb::right(mpoint, -1)))
                    {
                        dialogdev = part;
                        dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 31 : 51;
                        goto error;
                    }
                }
            }
            else
            {
                dialogdev = part;
                dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 31 : 51;
                goto error;
            }
        }

        if(intrrpt) goto exit;
    }

    msort.clear();

    if(pname == tr("Currently running system"))
    {
        if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
        {
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
        }
        else if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, nullptr))
            goto error;

        if(ui->userdatafilescopy->isVisibleTo(ui->copypanel) && sb::schdle[0] == "on" && ! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", "storagedir=" % sb::sdir[0] % "\nliveworkdir=" % sb::sdir[2] % "\npointsnumber=" % QStr::number(sb::pnumber) % "\ntimer=off\nschedule=" % sb::schdle[1] % ':' % sb::schdle[2] % ':' % sb::schdle[3] % ':' % sb::schdle[4] % "\nsilentmode=" % sb::schdle[5] % "\nwindowposition=" % sb::schdle[6] % '\n')) goto error;
    }
    else if(pname == tr("Live image"))
    {
        if(! sb::isdir("/.systembacklivepoint") && ! QDir().mkdir("/.systembacklivepoint"))
        {
            QFile::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
            if(! QDir().mkdir("/.systembacklivepoint")) goto error;
        }

        if(intrrpt) goto exit;
        QStr mdev(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs") ? "/lib/live/mount/medium/live/filesystem.squashfs" : "/live/image/live/filesystem.squashfs");

        if(! sb::mount(mdev, "/.systembacklivepoint", "loop"))
        {
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 53 : 54;
            goto error;
        }

        if(intrrpt) goto exit;

        if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
        {
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
        }
        else if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, "/.systembacklivepoint"))
            goto error;

        sb::umount("/.systembacklivepoint");
        if(intrrpt) goto exit;
        QDir().rmdir("/.systembacklivepoint");

        if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
        {
            QStr cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));

            if(cfg.contains("timer=on"))
            {
                QStr nconf;

                {
                    QTS in(&cfg, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        nconf.append((cline == "timer=on" ? "timer=off" : cline) % '\n');
                    }
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", nconf)) goto error;
            }
        }
    }
    else
    {
        if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
        {
            switch(ui->usersettingscopy->checkState()) {
            case Qt::Unchecked:
                if(! sb::scopy(5, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname)) goto error;
                break;
            case Qt::PartiallyChecked:
                if(! sb::scopy(3, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname)) goto error;
                break;
            case Qt::Checked:
                if(! sb::scopy(4, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname)) goto error;
            }
        }
        else if(! sb::scopy(nohmcpy ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname))
            goto error;

        if(intrrpt) goto exit;

        if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
        {
            QStr cfg(sb::fload("/.sbsystemcopy/etc/systemback.conf"));

            if(cfg.contains("timer=on"))
            {
                QStr nconf;

                {
                    QTS in(&cfg, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        nconf.append((cline == "timer=on" ? "timer=off" : cline) % '\n');
                    }
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/systemback.conf", nconf)) goto error;
            }
        }
    }

    if(intrrpt) goto exit;
    sb::Progress = -1;

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        QStr nfile;

        if(guname() != ui->username->text())
        {
            if(sb::isdir("/.sbsystemcopy/home/" % guname()) && ((sb::exist("/.sbsystemcopy/home/" % ui->username->text()) && ! QFile::rename("/.sbsystemcopy/home/" % ui->username->text(), "/.sbsystemcopy/home/" % ui->username->text() % '_' % sb::rndstr())) || ! QFile::rename("/.sbsystemcopy/home/" % guname(), "/.sbsystemcopy/home/" % ui->username->text()))) goto error;

            {
                QFile file("/.sbsystemcopy/etc/group");
                if(! file.open(QIODevice::ReadOnly)) goto error;

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), ui->username->text());
                    nline = nline.replace(':' % guname() % ',', ':' % ui->username->text() % ',');
                    nline = nline.replace(',' % guname() % ',', ',' % ui->username->text() % ',');
                    if(nline.endsWith(':' % guname())) nline.replace(nline.length() - guname().length(), guname().length(), ui->username->text());
                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }
            }

            if(! sb::crtfile("/.sbsystemcopy/etc/group", nfile)) goto error;
            nfile.clear();

            {
                QFile file("/.sbsystemcopy/etc/gshadow");
                if(! file.open(QIODevice::ReadOnly)) goto error;

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), ui->username->text());
                    nline = nline.replace(':' % guname() % ',', ':' % ui->username->text() % ',');
                    nline = nline.replace(',' % guname() % ',', ',' % ui->username->text() % ',');
                    if(nline.endsWith(':' % guname())) nline.replace(nline.length() - guname().length(), guname().length(), ui->username->text());
                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }
            }

            if(! sb::crtfile("/.sbsystemcopy/etc/gshadow", nfile)) goto error;
            nfile.clear();

            if(sb::isfile("/.sbsystemcopy/etc/subuid") && sb::isfile("/.sbsystemcopy/etc/subgid"))
            {
                {
                    QFile file("/.sbsystemcopy/etc/subuid");
                    if(! file.open(QIODevice::ReadOnly)) goto error;

                    while(! file.atEnd())
                    {
                        QStr nline(file.readLine().trimmed());
                        if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), ui->username->text());
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
                        if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), ui->username->text());
                        nfile.append(nline % '\n');
                        if(intrrpt) goto exit;
                    }
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/subgid", nfile)) goto error;
                nfile.clear();
            }
        }

        {
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
                            nline.append(ui->username->text() % ':');
                            break;
                        case 4:
                            nline.append(ui->fullname->text() % ",,,:");
                            break;
                        case 5:
                            nline.append("/home/" % ui->username->text() % ':');
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

        if(! sb::crtfile("/.sbsystemcopy/etc/passwd", nfile)) goto error;
        nfile.clear();

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
                            nline.append(ui->username->text() % ':');
                            break;
                        case 1:
                            nline.append(QStr(crypt(ui->password1->text().toStdString().c_str(), QStr("$6$" % sb::rndstr(16)).toStdString().c_str())) % ':');
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
                            nline.append((ui->rootpassword1->text().isEmpty() ? "!" : QStr(crypt(ui->rootpassword1->text().toStdString().c_str(), QStr("$6$" % sb::rndstr(16)).toStdString().c_str()))) % ':');
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
            QStr ohname(file.readLine().trimmed());
            file.close();

            if(ohname != ui->hostname->text())
            {
                if(! sb::crtfile("/.sbsystemcopy/etc/hostname", ui->hostname->text() % '\n')) goto error;
                file.setFileName("/.sbsystemcopy/etc/hosts");
                if(! file.open(QIODevice::ReadOnly)) goto error;

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    nline = nline.replace('\t' % ohname % '\t', '\t' % ui->hostname->text() % '\t');
                    if(nline.endsWith('\t' % ohname)) nline.replace(nline.length() - ohname.length(), ohname.length(), ui->hostname->text());
                    nfile.append(nline % '\n');
                    if(intrrpt) goto exit;
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/hosts", nfile)) goto error;
                nfile.clear();
                if(intrrpt) goto exit;
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/deluser", "#!/bin/sh\nfor rmuser in $(grep :\\$6\\$* /etc/shadow | cut -d : -f 1)\ndo [ $rmuser = " % ui->username->text() % " ] || [ $rmuser = root ] || userdel $rmuser\ndone\n") || ! QFile::setPermissions("/.sbsystemcopy/deluser", QFile::ExeOwner)) goto error;
        if(intrrpt) goto exit;
        if(sb::exec("chroot /.sbsystemcopy /deluser") > 0) goto error;
        QFile::remove("/.sbsystemcopy/deluser");
    }

    if(intrrpt) goto exit;
    QStr fstabtxt("# /etc/fstab: static file system information.\n#\n# Use 'blkid' to print the universally unique identifier for a\n# device; this may be used with UUID= as a more robust way to name devices\n# that works even if disks are added and removed. See fstab(5).\n#\n# <file system> <mount point>   <type>  <options>       <dump>  <pass>\n");

    for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
    {
        if(! ui->partitionsettings->item(a, 4)->text().isEmpty())
        {
            if(ui->partitionsettings->item(a, 4)->text() == "/home" && ! ui->partitionsettings->item(a, 3)->text().isEmpty())
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
                QStr uuid(sb::ruuid(ui->partitionsettings->item(a, 0)->text()));

                if(ui->partitionsettings->item(a, 4)->text() == "/")
                {
                    if(sb::like(ui->partitionsettings->item(a, 5)->text(), {"_ext4_", "_ext3_", "_ext2_", "_jfs_", "_xfs_"}))
                        fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   /   " % ui->partitionsettings->item(a, 5)->text() % "   defaults,errors=remount-ro   0   1\n");
                    else if(ui->partitionsettings->item(a, 5)->text() == "reiserfs")
                        fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   /   " % ui->partitionsettings->item(a, 5)->text() % "   notail   0   1\n");
                    else
                        fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   /   " % ui->partitionsettings->item(a, 5)->text() % "   defaults,subvol=@   0   1\n");
                }
                else if(ui->partitionsettings->item(a, 5)->text() == "reiserfs")
                    fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   " % ui->partitionsettings->item(a, 4)->text() % "   reiserfs   notail   0   2\n");
                else if(ui->partitionsettings->item(a, 5)->text() == "btrfs")
                    fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   " % ui->partitionsettings->item(a, 4)->text() % "   btrfs   defaults,subvol=@" % sb::right(ui->partitionsettings->item(a, 4)->text(), -1) % "   0   2\n");
                else
                    fstabtxt.append("# " % ui->partitionsettings->item(a, 4)->text() % "\nUUID=" % uuid % "   " % ui->partitionsettings->item(a, 4)->text() % "   " % ui->partitionsettings->item(a, 5)->text() % "   defaults   0   2\n");
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

            if(sb::like(cline, {"*/dev/cdrom*", "*/dev/sr*"}))
                fstabtxt.append("# cdrom\n" % cline % '\n');
            else if(cline.contains("/dev/fd"))
                fstabtxt.append("# floppy\n" % cline % '\n');

            if(intrrpt) goto exit;
        }
    }

    if(! sb::crtfile("/.sbsystemcopy/etc/fstab", fstabtxt)) goto error;
    fstabtxt.clear();

    if(ui->grubinstallcopy->isVisibleTo(ui->copypanel) && ui->grubinstallcopy->currentText() != tr("Disabled"))
    {
        if(intrrpt) goto exit;
        sb::mount("/dev", "/.sbsystemcopy/dev");
        sb::mount("/dev/pts", "/.sbsystemcopy/dev/pts");
        sb::mount("/proc", "/.sbsystemcopy/proc");
        sb::mount("/sys", "/.sbsystemcopy/sys");
        if(intrrpt) goto exit;

        if(ui->grubinstallcopy->currentText() == "Auto")
        {
            QStr mntdev;
            mnts[0] = sb::fload("/proc/self/mounts");

            {
                QTS in(&mnts[0], QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr cline(in.readLine());

                    if(cline.contains(" /.sbsystemcopy/boot "))
                    {
                        mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                        break;
                    }
                    else if(cline.contains(" /.sbsystemcopy "))
                        mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                }
            }

            if(! sb::crtfile("/.sbsystemcopy/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % sb::left(mntdev, 8) % '\n')) goto error;
        }
        else if(! sb::crtfile("/.sbsystemcopy/grubinst", "#!/bin/sh\nupdate-grub\ngrub-install --force " % ui->grubinstallcopy->currentText() % '\n'))
            goto error;

        if(! QFile::setPermissions("/.sbsystemcopy/grubinst", QFile::ExeOwner)) goto error;
        if(intrrpt) goto exit;

        if(sb::exec("chroot /.sbsystemcopy /grubinst") > 0)
        {
            dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 22 : 34;
            goto error;
        }

        QFile::remove("/.sbsystemcopy/grubinst");
        sb::umount("/.sbsystemcopy/dev");
        sb::umount("/.sbsystemcopy/dev/pts");
        sb::umount("/.sbsystemcopy/proc");
        sb::umount("/.sbsystemcopy/sys");
    }

    if(intrrpt) goto exit;
    prun = tr("Emptying cache");
    mnts[0] = sb::fload("/proc/self/mounts");

    {
        QTS in(&mnts[0], QIODevice::ReadOnly);
        while(! in.atEnd()) mnts[1].prepend(in.readLine() % '\n');
        in.setString(&mnts[1], QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(cline.contains(" /.sbsystemcopy")) sb::umount(sb::left(cline, sb::instr(cline, " ") -1));
        }
    }

    QDir().rmdir("/.sbsystemcopy");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
    dialog = ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 25 : 33;
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
        case 30:
            dialog = 57;
            break;
        case 58:
        case 59:
            dialogdev = ldev % '1';
        }

        dialogopen();
    }

    return;
start:
    statustart();
    prun = tr("Writing Live image to USB device");

    if(! sb::exist(ldev))
    {
        dialog = 59;
        goto error;
    }
    else if(sb::mcheck(ldev))
    {
        for(cQStr &sitem : QDir("/dev").entryList(QDir::System))
        {
            QStr item("/dev/" % sitem);

            if(item.length() > 8 && item.startsWith(ldev))
                while(sb::mcheck(item)) sb::umount(item);

            if(intrrpt) goto error;
        }

        sb::fssync();
        if(intrrpt) goto error;
    }

    if(! sb::mkptable(ldev))
    {
        dialog = 59;
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
            dialog = 59;
            goto error;
        }
    }
    else
    {
        lrdir = "sbroot";

        if(! sb::mkpart(ldev, 1048576, 104857600) || ! sb::mkpart(ldev))
        {
            dialog = 59;
            goto error;
        }

        if(intrrpt) goto error;

        if(sb::exec("mkfs.ext2 -L SBROOT " % ldev % '2') > 0)
        {
            dialog = 59;
            goto error;
        }
    }

    if(intrrpt) goto error;

    if(sb::exec("mkfs.vfat -F 32 -n SBLIVE " % ldev % '1') > 0)
    {
        dialog = 59;
        goto error;
    }

    if(intrrpt ||
        sb::exec("dd if=/usr/lib/syslinux/" % QStr(sb::isfile("/usr/lib/syslinux/mbr.bin") ? nullptr : "mbr/") % "mbr.bin of=" % ldev % " conv=notrunc bs=440 count=1") > 0 || ! sb::setpflag(ldev % '1', "boot") || ! sb::setpflag(ldev % '1', "lba") ||
        intrrpt ||
        (sb::exist("/.sblivesystemwrite") && (((sb::mcheck("/.sblivesystemwrite/sblive") && ! sb::umount("/.sblivesystemwrite/sblive")) || (sb::mcheck("/.sblivesystemwrite/sbroot") && ! sb::umount("/.sblivesystemwrite/sbroot"))) || ! sb::remove("/.sblivesystemwrite"))) ||
        intrrpt ||
        ! QDir().mkdir("/.sblivesystemwrite") || ! QDir().mkdir("/.sblivesystemwrite/sblive")) goto error;

    if(! sb::mount(ldev % '1', "/.sblivesystemwrite/sblive"))
    {
        dialog = 58;
        goto error;
    }

    if(intrrpt) goto error;

    if(lrdir == "sbroot")
    {
        if(! QDir().mkdir("/.sblivesystemwrite/sbroot")) goto error;

        if(! sb::mount(ldev % '2', "/.sblivesystemwrite/sbroot"))
        {
            dialog = 58;
            goto error;
        }
    }

    if(intrrpt) goto error;

    if(sb::dfree("/.sblivesystemwrite/" % lrdir) < size + 52428800)
    {
        dialog = 42;
        goto error;
    }

    sb::ThrdStr[0] = "/.sblivesystemwrite";
    sb::ThrdLng[0] = size;

    if(lrdir == "sblive")
    {
        if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --no-same-owner --no-same-permissions") > 0)
        {
            dialog = 43;
            goto error;
        }
    }
    else if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --exclude=casper/filesystem.squashfs --exclude=live/filesystem.squashfs --no-same-permissions --no-same-owner") > 0)
    {
        dialog = 43;
        goto error;
    }
    else if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sbroot --exclude=.disk --exclude=boot --exclude=EFI --exclude=syslinux --exclude=casper/initrd.gz --exclude=casper/vmlinuz --exclude=live/initrd.gz --exclude=live/vmlinuz --no-same-owner --no-same-permissions") > 0)
    {
        dialog = 43;
        goto error;
    }

    prun = tr("Emptying cache");
    if(sb::exec("syslinux -ifd syslinux " % ldev % '1') > 0) goto error;
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
    sb::umount("/.sblivesystemwrite/sblive");
    QDir().rmdir("/.sblivesystemwrite/sblive");

    if(lrdir == "sbroot")
    {
        sb::umount("/.sblivesystemwrite/sbroot");
        QDir().rmdir("/.sblivesystemwrite/sbroot");
    }

    QDir().rmdir("/.sblivesystemwrite");
    dialog = 44;
    dialogopen();
}

void systemback::dialogopen()
{
    if(ui->dialogcancel->isVisibleTo(ui->dialogpanel)) ui->dialogcancel->hide();
    if(ui->dialogquestion->isVisibleTo(ui->dialogpanel)) ui->dialogquestion->hide();
    if(ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->hide();
    if(ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->hide();
    if(ui->dialognumber->isVisibleTo(ui->dialogpanel)) ui->dialognumber->hide();

    switch(dialog) {
    case 1:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Another systemback process is currently running, please wait until it finishes."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 2:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Unable to get exclusive lock!") % "<p>" % tr("First, close all package manager."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 3:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("The specified name contain(s) unsupported character(s)!") % "<p>" % tr("Please enter a new name."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 4:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Restore the system files to the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 5:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Repair the system files with the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 6:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Repair the complete system with the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 7:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Restore the complete user(s) configuration files to the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 8:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Restore the user(s) configuration files to the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 9:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("User(s) configuration files full restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds."));
        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
        ui->dialogcancel->show();
        ui->dialognumber->show();
        dlgtimer->start();
        break;
    case 10:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("User(s) configuration files restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds."));
        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
        ui->dialogcancel->show();
        ui->dialognumber->show();
        dlgtimer->start();
        break;
    case 11:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System files repair are completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 12:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("Full system repair is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 13:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("System repair is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 14:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Copy the system, using the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 15:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Install the system, using the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 16:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Restore point creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 17:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Root privileges are required for running Systemback!"));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 18:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Restore complete system to the following restore point:") % "<p><b>" % pname);
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 19:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("System files restoration are completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds."));
        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
        ui->dialogcancel->show();
        ui->dialognumber->show();
        dlgtimer->start();
        break;
    case 20:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("Full system restoration is completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds."));
        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
        ui->dialogcancel->show();
        ui->dialognumber->show();
        dlgtimer->start();
        break;
    case 21:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition(s) doesn't have enough free space to copy the system. The copied system will not function properly."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 22:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copy is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 23:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System restoration is aborted!") % "<p>" % tr("An error occurred while reinstalling GRUB."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 24:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Full system repair is completed, but an error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 25:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("System copy is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 26:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating file system image."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 27:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating container file."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 28:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("Live system creation is completed.") % "<p>" % tr("The created .sblive file can be written to pendrive."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 29:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 30:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Format the <dev>, and write the following Live system image:").replace("<dev>", " <b>" % ui->livedevices->item(ui->livedevices->currentRow(), 0)->text() % "</b>") % "<p><b>" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % "</b>");
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 31:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition couldn't be mounted.") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 32:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("GRUB 2 repair is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 33:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("System install is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 34:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System install is completed, but an error occurred while installing GRUB!") % ' ' % tr("Need to manually install a bootloader."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 35:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition(s) doesn't have enough free space to install the system. The installed system will not function properly."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 36:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The specified partition couldn't be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 37:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("An error occurred while reinstalling GRUB!") % ' ' % tr("System may not bootable! (In general, the different architecture is causing the problem.)"));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 38:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Restore point creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 39:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copying is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 40:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 41:
        ui->dialogquestion->show();
        ui->dialogtext->setText(tr("Repair the GRUB 2 bootloader."));
        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));
        ui->dialogcancel->show();
        break;
    case 42:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The selected partition doesn't have enough space to write the Live system."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 43:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("An error occurred while unpacking Live system files."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 44:
        ui->dialoginfo->show();
        ui->dialogtext->setText(tr("Live system image write is completed."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 45:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while renaming essential Live files."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 46:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while creating .iso image."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 47:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("An error occurred while reading .sblive image."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 48:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("An error occurred while creating new initramfs image."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 49:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live system creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 50:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Restore point deletion is aborted!") % "<p>" % tr("An error occurred while during the process."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 51:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition couldn't be mounted.") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 52:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The specified partition couldn't be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 53:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System copy is aborted!") % "<p>" % tr("The Live image couldn't be mounted."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 54:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System installation is aborted!") % "<p>" % tr("The Live image couldn't be mounted."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 55:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System repair is aborted!") % "<p>" % tr("The Live image couldn't be mounted."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 56:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live conversion is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 57:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 58:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The specified partition couldn't be mounted.") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 59:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("Live write is aborted!") % "<p>" % tr("The specified partition couldn't be formatted (in use or unavailable).") % "<p><b>" % dialogdev);
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 60:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System restoration is aborted!") % "<p>" % tr("There isn't enough free space."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
        break;
    case 61:
        ui->dialogerror->show();
        ui->dialogtext->setText(tr("System repair is aborted!") % "<p>" % tr("There isn't enough free space."));
        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
    }

    if(ui->mainpanel->isVisible()) ui->mainpanel->hide();
    if(ui->statuspanel->isVisible()) ui->statuspanel->hide();
    if(ui->schedulerpanel->isVisible()) ui->schedulerpanel->hide();
    if(ui->dialogpanel->isHidden()) ui->dialogpanel->show();
    ui->dialogok->setFocus();

    if(width() != ui->dialogpanel->width())
    {
        if(utimer->isActive() && ! sstart)
            windowmove(ui->dialogpanel->width(), ui->dialogpanel->height());
        else
        {
            if(sstart && ! ui->function3->text().contains(' ')) ui->function3->setText("Systemback " % tr("scheduler"));
            wgeom[0] = qApp->desktop()->width() / 2 - ss(253);
            wgeom[1] = qApp->desktop()->height() / 2 - ss(100);
            wgeom[2] = ui->dialogpanel->width();
            wgeom[3] = ui->dialogpanel->height();
            setFixedSize(wgeom[2], wgeom[3]);
            move(wgeom[0], wgeom[1]);
        }
    }

    repaint();
    setwontop();
}

void systemback::setwontop(bool state)
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
}

void systemback::windowmove(ushort nwidth, ushort nheight, bool fxdw)
{
    if(size() == qApp->desktop()->availableGeometry().size()) setGeometry(wgeom[4], wgeom[5], wgeom[2], wgeom[3]);
    wgeom[2] = nwidth;
    wgeom[3] = nheight;

    if(size() != QSize(wgeom[2], wgeom[3]))
    {
        ui->resizepanel->show();
        repaint();
        short wmvxy[2], rghtlmt(qApp->desktop()->width() - wgeom[2] - ss(30)), bttmlmt(qApp->desktop()->height() - wgeom[3] - ss(30));
        wmvxy[0] = x() + (width() - wgeom[2]) / 2;

        if(wmvxy[0] < ss(30))
            wmvxy[0] = ss(30);
        else if(wmvxy[0] > rghtlmt)
            wmvxy[0] = rghtlmt;

        wmvxy[1] = y() + (height() - wgeom[3]) / 2;

        if(wmvxy[1] < ss(30))
            wmvxy[1] = ss(30);
        else if(wmvxy[1] > bttmlmt)
            wmvxy[1] = bttmlmt;

        wgeom[0] = wmvxy[0];
        wgeom[1] = wmvxy[1];
        setMinimumSize(0, 0);
        setMaximumSize(qApp->desktop()->availableGeometry().size());
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
    if(size() != qApp->desktop()->availableGeometry().size())
    {
        if(ui->function1->isVisible())
            move(QCursor::pos().x() - ui->function1->MouseX, QCursor::pos().y() - ui->function1->MouseY);
        else if(ui->function2->isVisible())
            move(QCursor::pos().x() - ui->function2->MouseX, QCursor::pos().y() - ui->function2->MouseY);
        else if(ui->function3->isVisible())
            move(QCursor::pos().x() - ui->function3->MouseX, QCursor::pos().y() - ui->function3->MouseY);
        else if(ui->function4->isVisible())
            move(QCursor::pos().x() - ui->function4->MouseX, QCursor::pos().y() - ui->function4->MouseY);
    }
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
        ui->function1->setForegroundRole(QPalette::Base);
        ui->windowbutton1->setForegroundRole(QPalette::Base);
        ui->function2->setForegroundRole(QPalette::Base);
        ui->windowbutton2->setForegroundRole(QPalette::Base);
        ui->function3->setForegroundRole(QPalette::Base);
        ui->windowbutton3->setForegroundRole(QPalette::Base);
        ui->function4->setForegroundRole(QPalette::Base);
        ui->windowbutton4->setForegroundRole(QPalette::Base);

        if(x() < 0 && x() > - width())
            wgeom[0] = ss(30);
        else if(x() > qApp->desktop()->width() - width() && x() < qApp->desktop()->width() + width())
            wgeom[0] = qApp->desktop()->width() - width() - ss(30);

        if(y() < 0 && y() > - height())
            wgeom[1] = ss(30);
        else if(y() > qApp->desktop()->height() - height() && y() < qApp->desktop()->height() + height())
            wgeom[1] = qApp->desktop()->height() - height() - ss(30);

        if(x() != wgeom[0] || y() != wgeom[1])
        {
            if(size() == qApp->desktop()->availableGeometry().size())
            {
                setGeometry(wgeom[4], wgeom[5], wgeom[2], wgeom[3]);
                setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
            }
            else
                move(wgeom[0], wgeom[1]);
        }

        return true;
    case QEvent::WindowDeactivate:
        ui->function1->setForegroundRole(QPalette::Dark);
        ui->windowbutton1->setForegroundRole(QPalette::Dark);
        ui->function2->setForegroundRole(QPalette::Dark);
        ui->windowbutton2->setForegroundRole(QPalette::Dark);
        ui->function3->setForegroundRole(QPalette::Dark);
        ui->windowbutton3->setForegroundRole(QPalette::Dark);
        ui->function4->setForegroundRole(QPalette::Dark);
        ui->windowbutton4->setForegroundRole(QPalette::Dark);
        return true;
    case QEvent::Resize:
        ui->mainpanel->resize(width(), height());
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
            ui->warning->move(ui->warning->x(), ui->choosepanel->height() - ss(41));
            ui->chooseresize->move(ui->choosepanel->width() - ui->chooseresize->width(), ui->choosepanel->height() - ui->chooseresize->height());

            if(size() == qApp->desktop()->availableGeometry().size())
                ui->chooseresize->setCursor(Qt::ArrowCursor);
            else if(ui->chooseresize->cursor().shape() == Qt::ArrowCursor)
                ui->chooseresize->setCursor(Qt::PointingHandCursor);
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
            ui->copyback->move(ui->copyback->x(), ui->copypanel->height() - ss(48));
            ui->copynext->move(ui->copypanel->width() - ss(152), ui->copyback->y());
            ui->copyresize->move(ui->copypanel->width() - ui->copyresize->width(), ui->copypanel->height() - ui->copyresize->height());
            ui->copycover->resize(ui->copypanel->width() - ss(10), ui->copypanel->height() - ss(10));

            if(size() == qApp->desktop()->availableGeometry().size())
                ui->copyresize->setCursor(Qt::ArrowCursor);
            else if(ui->copyresize->cursor().shape() == Qt::ArrowCursor)
                ui->copyresize->setCursor(Qt::PointingHandCursor);
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

            if(size() == qApp->desktop()->availableGeometry().size())
                ui->excluderesize->setCursor(Qt::ArrowCursor);
            else if(ui->excluderesize->cursor().shape() == Qt::ArrowCursor)
                ui->excluderesize->setCursor(Qt::PointingHandCursor);
        }

        repaint();
        return true;
    case QEvent::Move:
        wgeom[0] = x();
        wgeom[1] = y();
        return true;
    case QEvent::WindowStateChange:
        if(isMinimized() && unity) showNormal();
        if(ui->buttonspanel->isVisible()) ui->buttonspanel->hide();
        return true;
    default:
        return false;
    }
}

void systemback::keyPressEvent(QKeyEvent *ev)
{
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
            if(ui->point1->hasFocus())
            {
                if(ui->pointpipe1->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point2->hasFocus())
            {
                if(ui->pointpipe2->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point3->hasFocus())
            {
                if(ui->pointpipe3->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point4->hasFocus())
            {
                if(ui->pointpipe4->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point5->hasFocus())
            {
                if(ui->pointpipe5->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point6->hasFocus())
            {
                if(ui->pointpipe6->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point7->hasFocus())
            {
                if(ui->pointpipe7->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point8->hasFocus())
            {
                if(ui->pointpipe8->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point9->hasFocus())
            {
                if(ui->pointpipe9->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point10->hasFocus())
            {
                if(ui->pointpipe10->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point11->hasFocus())
            {
                if(ui->pointpipe11->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point12->hasFocus())
            {
                if(ui->pointpipe12->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point13->hasFocus())
            {
                if(ui->pointpipe13->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point14->hasFocus())
            {
                if(ui->pointpipe14->isChecked()) on_pointrename_clicked();
            }
            else if(ui->point15->hasFocus() && ui->pointpipe15->isChecked())
                on_pointrename_clicked();
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
            else if(ui->partitionsize->hasFocus())
            {
                if(ui->newpartition->isEnabled()) on_newpartition_clicked();
            }
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
        {
            if(! ui->copycover->isVisible()) on_partitionrefresh2_clicked();
        }
        else if(ui->livedevices->hasFocus())
        {
            if(! ui->livecreatecover->isVisible()) on_livedevicesrefresh_clicked();
        }
        else if(ui->livelist->hasFocus())
        {
            on_livecreatemenu_clicked();
            ui->livecreateback->setFocus();
        }
        else if(ui->dirchoose->hasFocus())
            on_dirrefresh_clicked();
        else if(ui->repairmountpoint->hasFocus())
        {
            if(! ui->repaircover->isVisible()) on_repairpartitionrefresh_clicked();
        }
        else if(ui->itemslist->hasFocus())
            on_pointexclude_clicked();

        break;
    case Qt::Key_Delete:
        if(ui->partitionsettings->hasFocus())
        {
            if(ui->umountdelete->isEnabled() && ! ui->copycover->isVisible() && ui->umountdelete->text() == tr("Umount")) on_umountdelete_clicked();
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
    switch(ev->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        qApp->sendEvent(qApp->focusObject(), &release);
    }
}

void systemback::on_admins_currentIndexChanged(const QStr &arg1)
{
    ui->admins->resize(fontMetrics().width(arg1) + ss(30), ss(32));
    if(! hash.isEmpty()) hash.clear();

    {
        QFile file("/etc/shadow");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(arg1 % ":"))
                {
                    hash = sb::mid(cline, arg1.length() + 2, sb::instr(cline, ":", arg1.length() + 2) - arg1.length() - 2);
                    break;
                }
            }
    }

    if(ui->adminpassword->text().length() > 0) ui->adminpassword->clear();

    if(! hash.isEmpty() && QStr(crypt("", hash.toStdString().c_str())) == hash)
    {
        ui->adminpasswordpipe->show();
        ui->adminpassword->setDisabled(true);
        ui->admins->setDisabled(true);
        ui->passwordinputok->setEnabled(true);
    }
}

void systemback::on_adminpassword_textChanged(const QStr &arg1)
{
    QStr ipasswd(arg1);

    if(arg1.isEmpty())
    {
        if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
    }
    else if(hash.isEmpty())
    {
        if(ui->adminpassworderror->isHidden()) ui->adminpassworderror->show();
    }
    else if(QStr(crypt(ipasswd.toStdString().c_str(), hash.toStdString().c_str())) == hash)
    {
        sb::delay(300);

        if(ipasswd == ui->adminpassword->text())
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
    shdltimer->stop();
    delete shdltimer;
    ui->function2->setText("Systemback " % tr("scheduler"));
    on_newrestorepoint_clicked();
}

void systemback::on_dialogcancel_clicked()
{
    if(dialog != 30)
    {
        if(dlgtimer->isActive())
        {
            dlgtimer->stop();
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

void systemback::on_point1_textChanged(const QStr &arg1)
{
    if(ui->point1->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe1->isChecked()) ui->pointpipe1->click();
            ui->pointpipe1->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe1->isEnabled())
    {
        if(ui->pointpipe1->isChecked()) ui->pointpipe1->click();
        ui->pointpipe1->setDisabled(true);
    }
}

void systemback::on_point2_textChanged(const QStr &arg1)
{
    if(ui->point2->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe2->isChecked()) ui->pointpipe2->click();
            ui->pointpipe2->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe2->isEnabled())
    {
        if(ui->pointpipe2->isChecked()) ui->pointpipe2->click();
        ui->pointpipe2->setDisabled(true);
    }
}

void systemback::on_point3_textChanged(const QStr &arg1)
{
    if(ui->point3->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe3->isChecked()) ui->pointpipe3->click();
            ui->pointpipe3->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe3->isEnabled())
    {
        if(ui->pointpipe3->isChecked()) ui->pointpipe3->click();
        ui->pointpipe3->setDisabled(true);
    }
}

void systemback::on_point4_textChanged(const QStr &arg1)
{
    if(ui->point4->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe4->isChecked()) ui->pointpipe4->click();
            ui->pointpipe4->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe4->isEnabled())
    {
        if(ui->pointpipe4->isChecked()) ui->pointpipe4->click();
        ui->pointpipe4->setDisabled(true);
    }
}

void systemback::on_point5_textChanged(const QStr &arg1)
{
    if(ui->point5->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe5->isChecked()) ui->pointpipe5->click();
            ui->pointpipe5->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe5->isEnabled())
    {
        if(ui->pointpipe5->isChecked()) ui->pointpipe5->click();
        ui->pointpipe5->setDisabled(true);
    }
}

void systemback::on_point6_textChanged(const QStr &arg1)
{
    if(ui->point6->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe6->isChecked()) ui->pointpipe6->click();
            ui->pointpipe6->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe6->isEnabled())
    {
        if(ui->pointpipe6->isChecked()) ui->pointpipe6->click();
        ui->pointpipe6->setDisabled(true);
    }
}

void systemback::on_point7_textChanged(const QStr &arg1)
{
    if(ui->point7->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe7->isChecked()) ui->pointpipe7->click();
            ui->pointpipe7->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe7->isEnabled())
    {
        if(ui->pointpipe7->isChecked()) ui->pointpipe7->click();
        ui->pointpipe7->setDisabled(true);
    }
}

void systemback::on_point8_textChanged(const QStr &arg1)
{
    if(ui->point8->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe8->isChecked()) ui->pointpipe8->click();
            ui->pointpipe8->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe8->isEnabled())
    {
        if(ui->pointpipe8->isChecked()) ui->pointpipe8->click();
        ui->pointpipe8->setDisabled(true);
    }
}

void systemback::on_point9_textChanged(const QStr &arg1)
{
    if(ui->point9->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe9->isChecked()) ui->pointpipe9->click();
            ui->pointpipe9->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe9->isEnabled())
    {
        if(ui->pointpipe9->isChecked()) ui->pointpipe9->click();
        ui->pointpipe9->setDisabled(true);
    }
}

void systemback::on_point10_textChanged(const QStr &arg1)
{
    if(ui->point10->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe10->isChecked()) ui->pointpipe10->click();
            ui->pointpipe10->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe10->isEnabled())
    {
        if(ui->pointpipe10->isChecked()) ui->pointpipe10->click();
        ui->pointpipe10->setDisabled(true);
    }
}

void systemback::on_point11_textChanged(const QStr &arg1)
{
    if(ui->point11->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe11->isChecked()) ui->pointpipe11->click();
            ui->pointpipe11->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe11->isEnabled())
    {
        if(ui->pointpipe11->isChecked()) ui->pointpipe11->click();
        ui->pointpipe11->setDisabled(true);
    }
}

void systemback::on_point12_textChanged(const QStr &arg1)
{
    if(ui->point12->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe12->isChecked()) ui->pointpipe12->click();
            ui->pointpipe12->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe12->isEnabled())
    {
        if(ui->pointpipe12->isChecked()) ui->pointpipe12->click();
        ui->pointpipe12->setDisabled(true);
    }
}

void systemback::on_point13_textChanged(const QStr &arg1)
{
    if(ui->point13->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe13->isChecked()) ui->pointpipe13->click();
            ui->pointpipe13->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe13->isEnabled())
    {
        if(ui->pointpipe13->isChecked()) ui->pointpipe13->click();
        ui->pointpipe13->setDisabled(true);
    }
}

void systemback::on_point14_textChanged(const QStr &arg1)
{
    if(ui->point14->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe14->isChecked()) ui->pointpipe14->click();
            ui->pointpipe14->setDisabled(true);
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
        }
    }
    else if(ui->pointpipe14->isEnabled())
    {
        if(ui->pointpipe14->isChecked()) ui->pointpipe14->click();
        ui->pointpipe14->setDisabled(true);
    }
}

void systemback::on_point15_textChanged(const QStr &arg1)
{
    if(ui->point15->isEnabled())
    {
        if(arg1.isEmpty())
        {
            if(ui->pointpipe15->isChecked()) ui->pointpipe15->click();
            ui->pointpipe15->setDisabled(true);
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
    if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub % ".list"))
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

            if(usr.contains(":/home/"))
            {
                usr = sb::left(usr, sb::instr(usr, ":") -1);
                if(sb::isdir("/home/" % usr)) ui->includeusers->addItem(usr);
            }

            qApp->processEvents();
        }
}

void systemback::on_copymenu_clicked()
{
    if(ppipe == 1)
    {
        if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub % ".list"))
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
    else if(sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub % ".list"))
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

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        ui->usersettingscopy->hide();
        ui->userdatafilescopy->show();
    }

    if(sb::like(pname, {'_' % tr("Currently running system") % '_', '_' % tr("Live image") % '_'}))
    {
        if(! ui->userdatafilescopy->isEnabled()) ui->userdatafilescopy->setEnabled(true);
    }
    else if(ui->userdatafilescopy->isEnabled())
    {
        ui->userdatafilescopy->setDisabled(true);
        if(ui->userdatafilescopy->isChecked()) ui->userdatafilescopy->setChecked(false);
    }

    short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
    ui->sbpanel->hide();
    ui->copypanel->show();
    ui->function1->setText(tr("System copy"));
    ui->copyback->setFocus();
    if(nwidth > ss(698)) windowmove(nwidth < ss(850) ? nwidth : ss(850), ss(465), false);
    setMinimumSize(ss(698), ss(465));
    setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));

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
        {
            if(item.endsWith(".sblive") && ! item.contains(' ') && ! sb::islink(sb::sdir[2] % '/' % item) && sb::fsize(sb::sdir[2] % '/' % item) > 0)
            {
                QLWI *lwi(new QLWI(sb::left(item, -7) % " (" % QStr::number(qRound64(sb::fsize(sb::sdir[2] % '/' % item) * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB, " % (sb::stype(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") == sb::Isfile && sb::fsize(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") > 0 ? "sblive+iso" : "sblive") % ')'));
                ui->livelist->addItem(lwi);
            }

            qApp->processEvents();
        }
}

void systemback::on_repairmenu_clicked()
{
    if(pname != tr("Currently running system") && ! pname.isEmpty())
    {
        if(! ui->systemrepair->isEnabled())
        {
            ui->systemrepair->setEnabled(true);
            ui->fullrepair->setEnabled(true);
        }

        on_systemrepair_clicked();
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
            utimer->stop();
            dialog = 2;
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
    setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
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

void systemback::on_partitionrefresh_clicked()
{
    busy();
    if(! ui->copycover->isVisible()) ui->copycover->show();
    if(ui->copynext->isEnabled()) ui->copynext->setDisabled(true);
    if(ui->mountpoint->count() > 0) ui->mountpoint->clear();
    ui->mountpoint->addItems({nullptr, "/", "/home", "/boot"});
    if(grub == "efi-amd64") ui->mountpoint->addItem("/boot/efi");
    ui->mountpoint->addItems({"/tmp", "/usr", "/var", "/srv", "/opt", "/usr/local", "SWAP"});

    if(ui->mountpoint->isEnabled())
    {
        ui->mountpoint->setDisabled(true);
        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
    }

    if(ui->filesystem->isEnabled())
    {
        ui->filesystem->setDisabled(true);
        ui->format->setDisabled(true);
     }

    if(ui->umountdelete->text() == tr("! Delete !"))
    {
        ui->umountdelete->setText(tr("Umount"));
        ui->umountdelete->setStyleSheet(nullptr);
    }

    if(ui->umountdelete->isEnabled()) ui->umountdelete->setDisabled(true);

    if(nohmcpy)
    {
        nohmcpy = false;
        if(sb::like(pname, {'_' % tr("Currently running system") % '_', '_' % tr("Live image") % '_'})) ui->userdatafilescopy->setEnabled(true);
    }

    if(ui->partitionsettings->rowCount() > 0) ui->partitionsettings->clearContents();
    if(ui->repairpartition->count() > 0) ui->repairpartition->clear();

    if(size() != qApp->desktop()->availableGeometry().size())
    {
        ui->partitionsettings->resizeColumnToContents(2);
        ui->partitionsettings->resizeColumnToContents(3);
        ui->partitionsettings->resizeColumnToContents(4);
    }

    if(ui->grubinstallcopy->count() > 0)
    {
        ui->grubinstallcopy->clear();
        ui->grubreinstallrestore->clear();
        ui->grubreinstallrepair->clear();
    }

    ui->grubinstallcopy->addItems({"Auto", tr("Disabled")});
    ui->grubreinstallrestore->addItems({"Auto", tr("Disabled")});
    ui->grubreinstallrepair->addItems({"Auto", tr("Disabled")});

    QSL plst;
    sb::readprttns(plst);

    for(cQStr &dts : plst)
    {
        QStr path(dts.split('\n').at(0));

        if(path.length() == 8)
        {
            ui->grubinstallcopy->addItem(path);
            ui->grubreinstallrestore->addItem(path);
            ui->grubreinstallrepair->addItem(path);
        }
    }

    schar sn(-1);
    QStr mntlst(sb::fload("/proc/self/mounts")), swplst(sb::fload("/proc/swaps"));

    for(cQStr &cdts : plst)
    {
        QSL dts(cdts.split('\n'));
        cQStr &path(dts.at(0)), &type(dts.at(2));
        ullong bsize(dts.at(1).toULongLong());

        if(path.length() == 8)
        {
            QFont font;
            font.setWeight(QFont::DemiBold);
            ++sn;
            ui->partitionsettings->setRowCount(sn + 1);
            if(sn > 0) ui->partitionsettings->setRowHeight(sn, ss(25));
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
            ui->partitionsettings->setItem(sn, 7, tp);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 9, lngth);
        }
        else
        {
            switch(type.toShort()) {
            case sb::Extended:
            {
                QFont font;
                font.setWeight(QFont::DemiBold);
                font.setItalic(true);
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
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
                ui->grubinstallcopy->addItem(path);
                ui->grubreinstallrestore->addItem(path);
                ui->grubreinstallrepair->addItem(path);
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
                ui->partitionsettings->setItem(sn, 2, lbl);
                QTblWI *mpt(new QTblWI);

                if(uuid.isEmpty())
                {
                    if(QStr('\n' % mntlst).contains('\n' % path % ' '))
                    {
                        QStr mnt(sb::right(mntlst, - sb::instr(mntlst, path % ' ')));
                        short spc(sb::instr(mnt, " "));
                        mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                    }
                }
                else
                {
                    if(QStr('\n' % mntlst).contains('\n' % path % ' '))
                    {
                        if(QStr('\n' % mntlst).count('\n' % path % ' ') > 1 || QStr('\n' % mntlst).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                            mpt->setText(tr("Multiple mount points"));
                        else
                        {
                            QStr mnt(sb::right(mntlst, - sb::instr(mntlst, path % ' ')));
                            short spc(sb::instr(mnt, " "));
                            mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                        }
                    }
                    else if(QStr('\n' % mntlst).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                    {
                        if(QStr('\n' % mntlst).count("\n/dev/disk/by-uuid/" % uuid % ' ') > 1)
                            mpt->setText(tr("Multiple mount points"));
                        else
                        {
                            QStr mnt(sb::right(mntlst, - sb::instr(mntlst, "/dev/disk/by-uuid/" % uuid % ' ')));
                            short spc(sb::instr(mnt, " "));
                            mpt->setText(sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " "));
                        }
                    }
                    else if(QStr('\n' % swplst).contains('\n' % path % ' '))
                        mpt->setText("SWAP");
                }

                mpt->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 3, mpt);
                if(mpt->text().isEmpty()) ui->repairpartition->addItem(path);
                QTblWI *empty(new QTblWI);
                empty->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 4, empty);
                QTblWI *fs(new QTblWI(dts.at(4)));
                fs->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 5, fs);
                QTblWI *frmt(new QTblWI("-"));
                frmt->setTextAlignment(Qt::AlignCenter);
                ui->partitionsettings->setItem(sn, 6, frmt);
                break;
            }
            case sb::Freespace:
            case sb::Emptyspace:
                QFont font;
                font.setItalic(true);
                ++sn;
                ui->partitionsettings->setRowCount(sn + 1);
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
            ui->partitionsettings->setItem(sn, 7, tp);
            QTblWI *start(new QTblWI(dts.at(3)));
            ui->partitionsettings->setItem(sn, 8, start);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 9, lngth);
        }
    }

    ui->partitionsettings->resizeColumnToContents(0);
    ui->partitionsettings->resizeColumnToContents(1);

    if(size() == qApp->desktop()->availableGeometry().size())
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

    if(! ui->partitionsettingspanel1->isVisible())
    {
        if(ui->partitionsettingspanel2->isVisible())
            ui->partitionsettingspanel2->setHidden(true);
        else
            ui->partitionsettingspanel3->setHidden(true);

        ui->partitionsettingspanel1->setVisible(true);
    }
}

void systemback::on_partitionrefresh3_clicked()
{
    on_partitionrefresh2_clicked();
}

void systemback::on_umountdelete_clicked()
{
    busy();
    ui->copycover->show();

    if(ui->umountdelete->text() == tr("Umount"))
    {
        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() != "SWAP")
        {
            QStr mnts[2];
            mnts[0] = sb::fload("/proc/self/mounts");
            QTS in(&mnts[0], QIODevice::ReadOnly);
            while(! in.atEnd()) mnts[1].prepend(in.readLine() % '\n');
            in.setString(&mnts[1], QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(in.readLine());
                if(sb::like(cline, {"* " % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().replace(" ", "\\040") % " *", "* " % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().replace(" ", "\\040") % "/*"})) sb::umount(sb::left(cline, sb::instr(cline, " ") - 1));
            }

            mnts[0] = sb::fload("/proc/self/mounts");

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            {
                QStr mpt(ui->partitionsettings->item(a, 3)->text());
                if(! mpt.isEmpty() && mpt != "SWAP" && ! mnts[0].contains(' ' % mpt.replace(" ", "\\040") % ' ')) ui->partitionsettings->item(a, 3)->setText(nullptr);
            }

            sb::fssync();
        }
        else if(! QStr('\n' % sb::fload("/proc/swaps")).contains('\n' % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text() % ' ') || swapoff(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().toStdString().c_str()) == 0)
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->setText(nullptr);

        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().isEmpty())
        {
            ui->umountdelete->setText(tr("! Delete !"));
            ui->umountdelete->setStyleSheet("QPushButton:enabled {color: red}");
            if(minside(ui->copypanel->pos() + ui->partitionsettingspanel1->pos() + ui->umountdelete->pos(), ui->umountdelete->geometry())) ui->umountdelete->setDisabled(true);
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

void systemback::on_umount_clicked()
{
    busy();
    ui->copycover->show();
    bool umntd(true);

    {
        QStr mnts[2];
        mnts[0] = sb::fload("/proc/self/mounts");
        { QTS in(&mnts[0], QIODevice::ReadOnly);
        while(! in.atEnd()) mnts[1].prepend(in.readLine() % '\n'); }

        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
            if(! ui->partitionsettings->item(a, 3)->text().isEmpty())
            {
                if(ui->partitionsettings->item(a, 3)->text() == "SWAP")
                    swapoff(ui->partitionsettings->item(a, 0)->text().toStdString().c_str());
                else
                {
                    QTS in(&mnts[1], QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        if(sb::like(cline, {"* " % ui->partitionsettings->item(a, 3)->text().replace(" ", "\\040") % " *", "* " % ui->partitionsettings->item(a, 3)->text().replace(" ", "\\040") % "/*"})) sb::umount(sb::left(cline, sb::instr(cline, " ") - 1));
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
                    ui->partitionsettings->item(a, 3)->setText(nullptr);
                else if(umntd)
                    umntd = false;
            }
        }
    }

    sb::fssync();

    if(umntd)
    {
        ui->umount->setDisabled(true);
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
    if(! ui->copycover->isVisible())
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
    if(! ui->livecreatecover->isVisible())
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
    if(! ui->repaircover->isVisible())
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
    windowmove(ss(698), ss(465));
    ui->excludepanel->hide();
    ui->sbpanel->show();
    ui->function1->setText("Systemback");
    ui->functionmenunext->setFocus();
    repaint();
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

        if(! ui->storagedirbutton->isVisible())
        {
            ui->storagedir->resize(ss(210), ss(28));
            ui->storagedirbutton->show();
            ui->pointrename->setDisabled(true);
            ui->pointdelete->setDisabled(true);
        }

        if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);

        if(! sb::isfile("/cdrom/casper/filesystem.squashfs") && ! sb::isfile("/live/image/live/filesystem.squashfs") && ! sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
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

            if(! ui->restoremenu->isEnabled() && ! sb::isfile("/cdrom/casper/filesystem.squashfs")&& ! sb::isfile("/live/image/live/filesystem.squashfs")&& ! sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs")) ui->restoremenu->setEnabled(true);

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
    ui->livecreatecover->show();
    if(ui->livedevices->rowCount() > 0) ui->livedevices->clearContents();
    QSL dlst;
    sb::readlvprttns(dlst);
    schar sn(-1);

    for(cQStr &cdts : dlst)
    {
        QSL dts(cdts.split('\n'));
        ullong bsize(dts.at(2).toULongLong());
        ++sn;
        ui->livedevices->setRowCount(sn + 1);
        QTblWI *dev(new QTblWI(dts.at(0)));
        ui->livedevices->setItem(sn, 0, dev);
        QTblWI *size(new QTblWI);

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

void systemback::on_pointexclude_clicked()
{
    busy();
    if(ui->itemslist->topLevelItemCount() > 0) ui->itemslist->clear();
    if(ui->excludedlist->count() > 0) ui->excludedlist->clear();

    if(ui->additem->isEnabled())
        ui->additem->setDisabled(true);
    else if(ui->removeitem->isEnabled())
        ui->removeitem->setDisabled(true);

    if(sb::isfile("/etc/systemback.excludes"))
    {
        QFile file("/etc/systemback.excludes");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith('.'))
                {
                    if(ui->pointexclude->isChecked()) ui->excludedlist->addItem(cline);
                }
                else if(ui->liveexclude->isChecked())
                    ui->excludedlist->addItem(cline);
            }
    }
    else
        sb::crtfile("/etc/systemback.excludes");

    {
        QFile file("/etc/passwd");

        if(file.open(QIODevice::ReadOnly))
        {
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());

                if(usr.contains(":/home/"))
                {
                    usr = sb::left(usr, sb::instr(usr, ":") -1);

                    if(sb::isdir("/home/" % usr))
                        for(cQStr &item : QDir("/home/" % usr).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                            if(((item.startsWith('.') && ui->pointexclude->isChecked()) || (! item.startsWith('.') && ui->liveexclude->isChecked())) && ui->excludedlist->findItems(item, Qt::MatchExactly).isEmpty() && ! sb::like(item, {"_.gvfs_", "_.Xauthority_", "_.ICEauthority_"}))
                            {
                                if(ui->itemslist->findItems(item, Qt::MatchExactly).isEmpty())
                                {
                                    QTrWI *twi(new QTrWI);
                                    twi->setText(0, item);

                                    if(sb::access("/home/" % usr % '/' % item) && sb::stype("/home/" % usr % '/' % item) == sb::Isdir)
                                    {
                                        twi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                                        ui->itemslist->addTopLevelItem(twi);
                                        QSL sdlst(QDir("/home/" % usr % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

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
                                else if(sb::access("/home/" % usr % '/' % item))
                                {
                                    if(sb::stype("/home/" % usr % '/' % item) == sb::Isdir)
                                    {
                                        QTrWI *ctwi(ui->itemslist->findItems(item, Qt::MatchExactly).at(0));
                                        if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(":pictures/dir.png"));
                                        QSL sdlst(QDir("/home/" % usr % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)), itmlst;
                                        for(ushort a(0) ; a < ctwi->childCount() ; ++a) itmlst.append(ctwi->child(a)->text(0));

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(ui->excludedlist->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                                            {
                                                for(cQStr &citem : itmlst)
                                                    if(citem == sitem) goto unext;

                                                QTrWI *sctwi(new QTrWI);
                                                sctwi->setText(0, sitem);
                                                ctwi->addChild(sctwi);
                                            }

                                        unext:;
                                        }
                                    }
                                }
                            }
                }
            }
        }
    }

    for(cQStr &item : QDir("/root").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
        if(((item.startsWith('.') && ui->pointexclude->isChecked()) || (! item.startsWith('.') && ui->liveexclude->isChecked())) && ui->excludedlist->findItems(item, Qt::MatchExactly).isEmpty() && ! sb::like(item, {"_.gvfs_", "_.Xauthority_", "_.ICEauthority_"}))
        {
            if(ui->itemslist->findItems(item, Qt::MatchExactly).isEmpty())
            {
                QTrWI *twi(new QTrWI);
                twi->setText(0, item);

                if(sb::access("/root/" % item) && sb::stype("/root/" % item) == sb::Isdir)
                {
                    twi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    ui->itemslist->addTopLevelItem(twi);

                    for(cQStr &sitem : QDir("/root/" % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
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
            else if(sb::access("/root/" % item))
            {
                if(sb::stype("/root/" % item) == sb::Isdir)
                {
                    QTrWI *ctwi(ui->itemslist->findItems(item, Qt::MatchExactly).at(0));
                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    QSL itmlst;
                    for(ushort a(0) ; a < ctwi->childCount() ; ++a) itmlst.append(ctwi->child(a)->text(0));

                    for(cQStr &sitem : QDir("/root/" % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                    {
                        if(ui->excludedlist->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                        {
                            for(cQStr &citem : itmlst)
                                if(citem == sitem) goto rnext;

                            QTrWI *sctwi(new QTrWI);
                            sctwi->setText(0, sitem);
                            ctwi->addChild(sctwi);
                        }

                    rnext:;
                    }
                }
            }
        }

    ui->itemslist->sortItems(0, Qt::AscendingOrder);
    if(ui->excludepanel->isVisible() && ! ui->excludeback->hasFocus()) ui->excludeback->setFocus();
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
        if(dialog == 23)
        {
            dialog = ui->fullrestore->isChecked() ? 20 : 19;
            dialogopen();
        }
        else if(dialog == 16)
        {
            statustart();

            for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                if(item.startsWith(".S00_"))
                {
                    prun = tr("Deleting incomplete restore point");

                    if(sb::remove(sb::sdir[1] % '/' % item))
                    {
                        prun = tr("Emptying cache");
                        sb::fssync();
                        sb::crtfile("/proc/sys/vm/drop_caches", "3");

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
                            dialog = 50;
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
        else if(! utimer->isActive() || sstart)
            close();
        else if(sb::ilike(dialog, {3, 21, 26, 27, 28, 29, 31, 35, 36, 39, 40, 42, 43, 44, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 57, 58, 59}))
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
        else if(dialog == 33)
        {
            if(sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/live/image/live/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
                close();
            else
                on_dialogcancel_clicked();
        }
        else
            on_dialogcancel_clicked();
    }
    else if(ui->dialogok->text() == tr("Start"))
    {
        switch(dialog) {
        case 4:
        case 7:
        case 8:
        case 18:
            restore();
            break;
        case 5:
        case 6:
        case 41:
            repair();
            break;
        case 14:
        case 15:
            systemcopy();
            break;
        case 30:
            livewrite();
        }
    }
    else if(ui->dialogok->text() == tr("Reboot"))
    {
        sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", nullptr, false, true);
        close();
    }
    else if(ui->dialogok->text() == tr("X restart"))
    {
        sb::xrestart();
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
    if(dialog == 3) dialog = 0;

    if(ui->pointpipe1->isChecked() && ui->point1->text() != sb::pnames[0])
    {
        if(! ui->point1->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S01_" % sb::pnames[0], sb::sdir[1] % "/S01_" % ui->point1->text()))
            ui->pointpipe1->click();
        else
            dialog = 3;
    }

    if(ui->pointpipe2->isChecked() && ui->point2->text() != sb::pnames[1])
    {
        if(! ui->point2->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S02_" % sb::pnames[1], sb::sdir[1] % "/S02_" % ui->point2->text()))
            ui->pointpipe2->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe3->isChecked() && ui->point3->text() != sb::pnames[2])
    {
        if(! ui->point3->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S03_" % sb::pnames[2], sb::sdir[1] % "/S03_" % ui->point3->text()))
            ui->pointpipe3->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe4->isChecked() && ui->point4->text() != sb::pnames[3])
    {
        if(! ui->point4->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S04_" % sb::pnames[3], sb::sdir[1] % "/S04_" % ui->point4->text()))
            ui->pointpipe4->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe5->isChecked() && ui->point5->text() != sb::pnames[4])
    {
        if(! ui->point5->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S05_" % sb::pnames[4], sb::sdir[1] % "/S05_" % ui->point5->text()))
            ui->pointpipe5->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe6->isChecked() && ui->point6->text() != sb::pnames[5])
    {
        if(! ui->point6->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S06_" % sb::pnames[5], sb::sdir[1] % "/S06_" % ui->point6->text()))
            ui->pointpipe6->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe7->isChecked() && ui->point7->text() != sb::pnames[6])
    {
        if(! ui->point7->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S07_" % sb::pnames[6], sb::sdir[1] % "/S07_" % ui->point7->text()))
            ui->pointpipe7->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe8->isChecked() && ui->point8->text() != sb::pnames[7])
    {
        if(! ui->point8->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S08_" % sb::pnames[7], sb::sdir[1] % "/S08_" % ui->point8->text()))
            ui->pointpipe8->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe9->isChecked() && ui->point9->text() != sb::pnames[8])
    {
        if(! ui->point9->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S09_" % sb::pnames[8], sb::sdir[1] % "/S09_" % ui->point9->text()))
            ui->pointpipe9->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe10->isChecked() && ui->point10->text() != sb::pnames[9])
    {
        if(! ui->point10->text().contains(' ') && QFile::rename(sb::sdir[1] % "/S10_" % sb::pnames[9], sb::sdir[1] % "/S10_" % ui->point10->text()))
            ui->pointpipe10->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe11->isChecked() && ui->point11->text() != sb::pnames[10])
    {
        if(! ui->point11->text().contains(' ') && QFile::rename(sb::sdir[1] % "/H01_" % sb::pnames[10], sb::sdir[1] % "/H01_" % ui->point11->text()))
            ui->pointpipe11->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe12->isChecked() && ui->point12->text() != sb::pnames[11])
    {
        if(! ui->point12->text().contains(' ') && QFile::rename(sb::sdir[1] % "/H02_" % sb::pnames[11], sb::sdir[1] % "/H02_" % ui->point12->text()))
            ui->pointpipe12->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe13->isChecked() && ui->point13->text() != sb::pnames[12])
    {
        if(! ui->point13->text().contains(' ') && QFile::rename(sb::sdir[1] % "/H03_" % sb::pnames[12], sb::sdir[1] % "/H03_" % ui->point13->text()))
            ui->pointpipe13->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe14->isChecked() && ui->point14->text() != sb::pnames[13])
    {
        if(! ui->point14->text().contains(' ') && QFile::rename(sb::sdir[1] % "/H04_" % sb::pnames[13], sb::sdir[1] % "/H04_" % ui->point14->text()))
            ui->pointpipe14->click();
        else if(dialog != 3)
            dialog = 3;
    }

    if(ui->pointpipe15->isChecked() && ui->point15->text() != sb::pnames[14])
    {
        if(! ui->point15->text().contains(' ') && QFile::rename(sb::sdir[1] % "/H05_" % sb::pnames[14], sb::sdir[1] % "/H05_" % ui->point15->text()))
            ui->pointpipe15->click();
        else if(dialog != 3)
            dialog = 3;
    }

    pointupgrade();
    busy(false);
    if(dialog == 3) dialogopen();
}

void systemback::on_autorestoreoptions_clicked(bool checked)
{
    if(checked)
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

void systemback::on_autorepairoptions_clicked(bool checked)
{
    if(checked)
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
    ui->function1->setText(tr("Storage directory"));
    ui->dirchooseok->setFocus();
    windowmove(ss(642), ss(481), false);
    setMinimumSize(ss(642), ss(481));
    setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
    on_dirrefresh_clicked();
}

void systemback::on_liveworkdirbutton_clicked()
{
    ui->livecreatepanel->hide();
    ui->choosepanel->show();
    ui->function1->setText(tr("Working directory"));
    ui->dirchooseok->setFocus();
    windowmove(ss(642), ss(481), false);
    setMinimumSize(ss(642), ss(481));
    setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
    on_dirrefresh_clicked();
}

void systemback::on_dirrefresh_clicked()
{
    busy();
    if(ui->dirchoose->topLevelItemCount() != 0) ui->dirchoose->clear();
    QStr pwdrs(sb::fload("/etc/passwd"));
    QSL excl({"bin", "boot", "cdrom", "dev", "etc", "lib", "lib32", "lib64", "opt", "proc", "root", "run", "sbin", "selinux", "srv", "sys", "tmp", "usr", "var"});

    for(cQStr &item : QDir("/").entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
    {
        QTrWI *twi(new QTrWI);
        twi->setText(0, item);

        if(excl.contains(item))
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

                if(item == "home" && pwdrs.contains(":/home/" % sitem % ":"))
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

void systemback::on_dirchoose_currentItemChanged(QTrWI *current)
{
    if(current)
    {
        const QTrWI *twi(current);
        QStr path('/' % current->text(0));

        while(twi->parent())
        {
            twi = twi->parent();
            path.prepend('/' % twi->text(0));
        }

        if(sb::isdir(path))
        {
            ui->dirpath->setText(path);

            if(current->textColor(0) == Qt::red)
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
            current->setHidden(true);

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

void systemback::on_dirchoose_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent)
    {
        item->setBackgroundColor(0, Qt::transparent);
        busy();
        const QTrWI *twi(item);
        QStr path('/' % twi->text(0));

        while(twi->parent())
        {
            twi = twi->parent();
            path.prepend('/' % twi->text(0));
        }

        if(sb::isdir(path))
        {
            for(ushort a(0) ; a < item->childCount() ; ++a)
            {
                QTrWI *ctwi(item->child(a));

                if(ctwi->textColor(0) != Qt::red)
                {
                    QStr iname(ctwi->text(0));

                    if(sb::isdir(path % '/' % iname))
                    {
                        if(sb::islnxfs(path % '/' % iname))
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
                        delete ctwi;
                }
            }
        }
        else
        {
            delete item;

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

        busy(false);
    }
}

void systemback::on_dirchoosecancel_clicked()
{
    ui->choosepanel->hide();

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
                on_livecreatemenu_clicked();
            }
        }

        windowmove(ss(698), ss(465));
        ui->dirchoose->clear();
    }
    else
    {
        ui->dirchoose->currentItem()->setHidden(true);
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

void systemback::on_includeusers_currentIndexChanged(const QStr &arg1)
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
        dialog = 18;
    else if(ui->systemrestore->isChecked())
        dialog = 4;
    else if(ui->keepfiles->isChecked())
        dialog = 8;
    else
        dialog = 7;

    dialogopen();
}

void systemback::on_livelist_currentItemChanged(QLWI *current)
{
    if(current)
    {
        if(sb::isfile(sb::sdir[2] % '/' % sb::left(current->text(), sb::instr(current->text(), " ") - 1) % ".sblive"))
        {
            if(! ui->livedelete->isEnabled()) ui->livedelete->setEnabled(true);
            ullong isize(sb::fsize(sb::sdir[2] % '/' % sb::left(current->text(), sb::instr(current->text(), " ") - 1) % ".sblive"));

            if(isize > 0 && isize < 4294967295 && isize * 2 + 104857600 < sb::dfree(sb::sdir[2]) && ! sb::exist(sb::sdir[2] % '/' % sb::left(current->text(), sb::instr(current->text(), " ") - 1) % ".iso"))
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
            delete current;
            if(ui->livelist->currentItem()) ui->livelist->currentItem()->setSelected(false);
            if(ui->livedelete->isEnabled()) ui->livedelete->setDisabled(true);
            if(ui->liveconvert->isEnabled()) ui->liveconvert->setDisabled(true);
            if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);
        }
    }
}

void systemback::on_livedelete_clicked()
{
    busy();
    sb::remove(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive");
    sb::remove(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso");
    on_livecreatemenu_clicked();
    busy(false);
}

void systemback::on_livedevices_currentItemChanged(QTblWI *current, QTblWI *previous)
{
    if(current && (! previous || current->row() != previous->row()))
    {
        ui->livedevices->item(current->row(), 3)->setText("x");
        if(previous) ui->livedevices->item(previous->row(), 3)->setText("-");
        if(ui->livelist->currentItem() && ! ui->livewritestart->isEnabled()) ui->livewritestart->setEnabled(true);
    }
}

void systemback::on_systemrepair_clicked()
{
    if(ui->grubrepair->isChecked())
    {
        if(ui->autorepairoptions->isEnabled()) ui->autorepairoptions->setDisabled(true);
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(! ui->grubreinstallrepair->isEnabled())
        {
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

        if(ppipe == 1)
        {
            if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub % ".list"))
            {
                if(! ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
                {
                    ui->grubreinstallrepair->show();
                    ui->grubreinstallrepairdisable->hide();
                }
            }
            else if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
            {
                ui->grubreinstallrepair->hide();
                ui->grubreinstallrepairdisable->show();
            }
        }
        else if(pname == tr("Live image"))
        {
            if(sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub % ".list"))
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
        }
    }
}

void systemback::on_fullrepair_clicked()
{
    on_systemrepair_clicked();
}

void systemback::on_grubrepair_clicked()
{
    on_systemrepair_clicked();
}

void systemback::on_repairnext_clicked()
{
    if(ui->systemrepair->isChecked())
        dialog = 5;
    else if(ui->fullrepair->isChecked())
        dialog = 6;
    else
      dialog = 41;

    dialogopen();
}

void systemback::on_skipfstabrestore_clicked(bool checked)
{
    if(checked && ! sb::isfile("/etc/fstab")) ui->skipfstabrestore->setChecked(false);
}

void systemback::on_skipfstabrepair_clicked(bool checked)
{
    if(checked && ! sb::isfile("/mnt/etc/fstab")) ui->skipfstabrepair->setChecked(false);
}

void systemback::on_installnext_clicked()
{
    if(ppipe == 1)
    {
        if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub % ".list"))
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
    else if(sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub % ".list"))
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

    if(ui->userdatafilescopy->isVisibleTo(ui->copypanel))
    {
        ui->userdatafilescopy->hide();
        ui->usersettingscopy->show();
    }

    short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
    ui->installpanel->hide();
    ui->copypanel->show();
    ui->copyback->setFocus();
    if(nwidth > ss(698)) windowmove(nwidth < ss(850) ? nwidth : ss(850), ss(465), false);
    setMinimumSize(ss(698), ss(465));
    setMaximumSize(qApp->desktop()->availableGeometry().width() - ss(60), qApp->desktop()->availableGeometry().height() - ss(60));
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

void systemback::on_partitionsettings_currentItemChanged(QTblWI *current, QTblWI *previous)
{
    if(current && (! previous || current->row() != previous->row()))
    {
        if(ui->partitionsettingspanel2->isVisible())
            for(ushort a(previous->row() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
            {
                ui->partitionsettings->item(a, 0)->setBackground(QBrush());
                ui->partitionsettings->item(a, 0)->setForeground(QBrush());
            }

        uchar type(ui->partitionsettings->item(current->row(), 7)->text().toShort()), pcount(0);

        switch(type) {
        case sb::MSDOS:
        case sb::GPT:
        case sb::Clear:
        case sb::Extended:
        {
            if(! ui->partitionsettingspanel2->isVisible())
            {
                if(ui->partitionsettingspanel1->isVisible())
                    ui->partitionsettingspanel1->setHidden(true);
                else
                    ui->partitionsettingspanel3->setHidden(true);

                ui->partitionsettingspanel2->setVisible(true);
            }

            bool mntd(false), mntcheck(false);

            for(ushort a(current->row() + 1) ; a < ui->partitionsettings->rowCount() && ((type == sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(sb::left(ui->partitionsettings->item(current->row(), 0)->text(), 8)) && sb::ilike(ui->partitionsettings->item(a, 7)->text().toUShort(), {sb::Logical, sb::Emptyspace})) || (type != sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(ui->partitionsettings->item(current->row(), 0)->text()))) ; ++a)
            {
                ui->partitionsettings->item(a, 0)->setBackground(QPalette().highlight());
                ui->partitionsettings->item(a, 0)->setForeground(QPalette().highlightedText());

                if(! mntcheck && ! ui->partitionsettings->item(a, 3)->text().isEmpty())
                {
                    if(! mntd) mntd = true;

                    if(((ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) && sb::sdir[0].startsWith(ui->partitionsettings->item(a, 3)->text())) || sb::like(ui->partitionsettings->item(a, 3)->text(), {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
                        mntcheck = true;
                    else if(sb::isfile("/etc/fstab"))
                    {
                        QFile file("/etc/fstab");

                        if(file.open(QIODevice::ReadOnly))
                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed().replace('\t', ' '));

                                if(sb::like(cline.replace("\\040", " "), {"* " % ui->partitionsettings->item(a, 3)->text() % " *", "* " % ui->partitionsettings->item(a, 3)->text() % "/ *"}))
                                {
                                    mntcheck = true;
                                    break;
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
                if(! ui->umount->isEnabled()) ui->umount->setEnabled(true);
            }
            else if(ui->umount->isEnabled())
                ui->umount->setDisabled(true);

            break;
        }
        case sb::Freespace:
            for(ushort a(0) ; a < ui->partitionsettings->rowCount() && pcount < 4 ; ++a)
                if(ui->partitionsettings->item(a, 0)->text().startsWith(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), 8)))
                {
                    switch(ui->partitionsettings->item(a, 7)->text().toUShort()) {
                    case sb::GPT:
                        pcount = 5;
                    case sb::Primary:
                    case sb::Extended:
                        ++pcount;
                    }
                }
        case sb::Emptyspace:
            if(! ui->partitionsettingspanel3->isVisible())
            {
                if(ui->partitionsettingspanel1->isVisible())
                    ui->partitionsettingspanel1->setHidden(true);
                else
                    ui->partitionsettingspanel2->setHidden(true);

                ui->partitionsettingspanel3->setVisible(true);
            }

            ui->partitionsize->setMaximum((ui->partitionsettings->item(current->row(), 9)->text().toULongLong() * 10 / 1048576 + 5) / 10);
            ui->partitionsize->setValue(ui->partitionsize->maximum());
            break;
        default:
            if(! ui->partitionsettingspanel1->isVisible())
            {
                if(ui->partitionsettingspanel2->isVisible())
                    ui->partitionsettingspanel2->setHidden(true);
                else
                    ui->partitionsettingspanel3->setHidden(true);

                ui->partitionsettingspanel1->setVisible(true);
            }

            if(ui->partitionsettings->item(current->row(), 3)->text().isEmpty())
            {
                if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);

                if(! ui->filesystem->isEnabled())
                {
                    ui->filesystem->setEnabled(true);
                    ui->format->setEnabled(true);
                }

                if(ui->umountdelete->text() == tr("Umount"))
                {
                    ui->umountdelete->setText(tr("! Delete !"));
                    ui->umountdelete->setStyleSheet("QPushButton:enabled {color: red}");
                }

                if(! ui->umountdelete->isEnabled()) ui->umountdelete->setEnabled(true);

                if(ui->mountpoint->currentIndex() == 0)
                {
                    if(! ui->mountpoint->currentText().isEmpty()) ui->mountpoint->setCurrentText(nullptr);
                }
                else
                    ui->mountpoint->setCurrentIndex(0);
            }
            else if(ui->partitionsettings->item(current->row(), 3)->text() == "SWAP")
            {
                if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);

                if(ui->filesystem->isEnabled())
                {
                    ui->filesystem->setDisabled(true);
                    ui->format->setDisabled(true);
                }

                if(ui->umountdelete->text() == tr("! Delete !"))
                {
                    ui->umountdelete->setText(tr("Umount"));
                    ui->umountdelete->setStyleSheet(nullptr);
                }

                if(! ui->umountdelete->isEnabled()) ui->umountdelete->setEnabled(true);

                if(ui->mountpoint->currentText() != "SWAP")
                {
                    if(ui->mountpoint->currentIndex() > 0)
                        ui->mountpoint->setCurrentIndex(0);
                    else if(! ui->mountpoint->currentText().isEmpty())
                        ui->mountpoint->setCurrentText(nullptr);
                }
                else if(ui->partitionsettings->item(current->row(), 4)->text() == "SWAP")
                {
                    if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                }
                else if(! ui->changepartition->isEnabled())
                    ui->changepartition->setEnabled(true);
            }
            else if(ui->partitionsettings->item(current->row(), 3)->text() == "/home" && ui->userdatafilescopy->isVisible())
            {
                if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);

                if(ui->filesystem->isEnabled())
                {
                    ui->filesystem->setDisabled(true);
                    ui->format->setDisabled(true);
                }

                if(ui->umountdelete->text() == tr("! Delete !"))
                {
                    ui->umountdelete->setText(tr("Umount"));
                    ui->umountdelete->setStyleSheet(nullptr);
                }

                if(ui->umountdelete->isEnabled()) ui->umountdelete->setDisabled(true);

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

                if(ui->filesystem->isEnabled())
                {
                    ui->filesystem->setDisabled(true);
                    ui->format->setDisabled(true);
                }

                if(ui->mountpoint->currentIndex() > 0)
                    ui->mountpoint->setCurrentIndex(0);
                else if(! ui->mountpoint->currentText().isEmpty())
                    ui->mountpoint->setCurrentText(nullptr);

                bool mntcheck(false);

                if(sb::sdir[0].startsWith(ui->partitionsettings->item(current->row(), 3)->text()) || sb::like(ui->partitionsettings->item(current->row(), 3)->text(), {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
                    mntcheck = true;
                else if(sb::isfile("/etc/fstab"))
                {
                    QFile file("/etc/fstab");

                    if(file.open(QIODevice::ReadOnly))
                        while(! file.atEnd())
                        {
                            QStr cline(file.readLine().trimmed().replace('\t', ' '));

                            if(sb::like(cline.replace("\\040", " "), {"* " % ui->partitionsettings->item(current->row(), 3)->text() % " *", "* " % ui->partitionsettings->item(current->row(), 3)->text() % "/ *"}))
                            {
                                mntcheck = true;
                                break;
                            }
                        }
                }

                if(ui->umountdelete->text() == tr("! Delete !"))
                {
                    ui->umountdelete->setText(tr("Umount"));
                    ui->umountdelete->setStyleSheet(nullptr);
                }

                if(mntcheck)
                {
                    if(ui->umountdelete->isEnabled()) ui->umountdelete->setDisabled(true);
                }
                else if(! ui->umountdelete->isEnabled())
                    ui->umountdelete->setEnabled(true);
            }

            if(! ui->format->isChecked()) ui->format->setChecked(true);
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

    if(! ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text().isEmpty())
    {
        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text() == "/")
        {
            ui->copynext->setDisabled(true);
            ui->mountpoint->addItem("/");
        }
        else if(sb::like(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text(), {"_/home_", "_/boot_", "_/boot/efi_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_"}))
            ui->mountpoint->addItem(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text());
    }

    if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().isEmpty())
    {
        if(ui->mountpoint->currentText() == "/boot/efi")
        {
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/boot/efi");
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "vfat") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("vfat");
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
        }
        else if(ui->mountpoint->currentText() == "SWAP")
        {
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("SWAP");
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "swap") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("swap");

            if(sb::isfile("/proc/swaps"))
            {
                if(QStr('\n' % sb::fload("/proc/swaps")).contains('\n' % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text() % ' '))
                {
                    if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "x") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("-");
                }
                else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-")
                    ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
            }
            else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-")
                ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
        }
        else
        {
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText(ui->mountpoint->currentText());
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText(ui->filesystem->currentText());

            if(ui->format->isChecked())
            {
                if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
            }
            else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "x")
                ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("-");

            if(ui->mountpoint->currentText() == "/") ui->copynext->setEnabled(true);
        }
    }
    else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home")
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

    busy(false);
}

void systemback::on_filesystem_currentIndexChanged(const QStr &arg1)
{
    if(! ui->format->isChecked() && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != arg1) ui->format->setChecked(true);
}

void systemback::on_format_clicked(bool checked)
{
    if(! checked && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != ui->filesystem->currentText()) ui->format->setChecked(true);
}

void systemback::on_mountpoint_currentTextChanged(const QStr &arg1)
{
    if(sb::like(arg1, {"_/boot/efi_", "_SWAP_"}) || sb::like(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text(), {"_/home_", "_SWAP_"}))
    {
        if(ui->filesystem->isEnabled())
        {
            ui->filesystem->setDisabled(true);
            ui->format->setDisabled(true);
        }
    }
    else if(! ui->filesystem->isEnabled())
    {
        ui->filesystem->setEnabled(true);
        ui->format->setEnabled(true);
    }

    if(arg1.isEmpty() || arg1 == ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text() || (ui->usersettingscopy->isVisible() && arg1.startsWith("/home/")) || (arg1 != "/boot/efi" && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 9)->text().toULongLong() < 268435456))
    {
        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
    }
    else
    {
        bool check(false);

        if((ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && arg1 != "/home") || (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "SWAP" && arg1 != "SWAP"))
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

            if(arg1.startsWith('/') && ! sb::like(arg1, {"* *", "*'*", "*\"*", "*//*", "*/_"}))
            {
                sb::delay(300);

                if(arg1 == ui->mountpoint->currentText())
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

void systemback::on_copynext_clicked()
{
    dialog = ui->function1->text() == tr("System copy") ? 14 : 15;
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
    if(grub == "efi-amd64") ui->repairmountpoint->addItem("/mnt/boot/efi");
    ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"});
    ui->repairmountpoint->setCurrentIndex(1);
    on_systemrepair_clicked();
    on_partitionrefresh_clicked();
    on_repairmountpoint_currentTextChanged("/mnt");
    ui->repaircover->hide();
    if(ui->repairpanel->isVisible() && ! ui->repairback->hasFocus()) ui->repairback->setFocus();
    busy(false);
}

void systemback::on_repairmountpoint_currentTextChanged(const QStr &arg1)
{
    if(ui->repairpartition->count() == 0 || ! arg1.startsWith("/mnt") || sb::like(arg1, {"* *", "*'*", "*\"*", "*//*", "*/_"}) || (arg1 != "/mnt" && ! sb::mcheck("/mnt")) || sb::mcheck(arg1 % '/'))
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

            if(arg1 == ui->repairmountpoint->currentText())
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
            on_partitionrefresh_clicked();

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

        if(ui->grubrepair->isChecked()) on_grubrepair_clicked();
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

void systemback::on_livename_textChanged(const QStr &arg1)
{
    if(ui->livenamepipe->isVisible()) ui->livenamepipe->hide();
    if(ui->livecreatenew->isEnabled()) ui->livecreatenew->setDisabled(true);

    if(arg1 == "auto")
    {
        QFont font;
        font.setItalic(true);
        ui->livename->setFont(font);
        if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
    }
    else
    {
        if(ui->livename->fontInfo().italic())
        {
            QFont font;
            ui->livename->setFont(font);
        }

        if(arg1.isEmpty() || arg1.size() > 32 || sb::like(arg1, {"* *", "*'*", "*\"*", "*_.iso", "*.Iso_", "*.ISo_", "*.IsO_", "*.ISO_", "*.iSo_", "*.iSO_", "*.isO_"}))
        {
            if(! ui->livenameerror->isVisible()) ui->livenameerror->show();
        }
        else
        {
            sb::delay(300);

            if(arg1 == ui->livename->text())
            {
                QStr lname("/tmp/" % arg1 % '_' % sb::rndstr());

                if(QDir().mkdir(lname))
                {
                    sb::remove(lname);
                    if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
                    ui->livenamepipe->show();
                }
                else if(! ui->livenameerror->isVisible())
                    ui->livenameerror->show();
            }
        }
    }
}

void systemback::on_itemslist_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent)
    {
        item->setBackgroundColor(0, Qt::transparent);
        busy();
        const QTrWI *twi(item);
        QStr path('/' % twi->text(0));

        while(twi->parent())
        {
            twi = twi->parent();
            path.prepend('/' % twi->text(0));
        }

        {
            QFile file("/etc/passwd");

            if(file.open(QIODevice::ReadOnly))
            {
                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());

                    if(usr.contains(":/home/"))
                    {
                        usr = sb::left(usr, sb::instr(usr, ":") -1);

                        if(sb::stype("/home/" % usr % path) == sb::Isdir)
                        {
                            for(ushort a(0) ; a < item->childCount() ; ++a)
                            {
                                QTrWI *ctwi(item->child(a));
                                QStr iname(ctwi->text(0));

                                if(sb::stype("/home/" % usr % path % '/' % iname) == sb::Isdir)
                                {
                                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                                    QSL itmlst;
                                    for(ushort b(0) ; b < ctwi->childCount() ; ++b) itmlst.append(ctwi->child(b)->text(0));

                                    for(cQStr &siname : QDir("/home/" % usr % path % '/' % iname).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                                    {
                                        if(ui->excludedlist->findItems(sb::right(path, -1) % '/' % iname % '/' % siname, Qt::MatchExactly).isEmpty())
                                        {
                                            for(ushort b(0) ; b < itmlst.count() ; ++b)
                                                if(itmlst.at(b) == siname)
                                                {
                                                    itmlst.removeAt(b);
                                                    goto unext;
                                                }

                                            QTrWI *sctwi(new QTrWI);
                                            sctwi->setText(0, siname);
                                            ctwi->addChild(sctwi);
                                        }

                                    unext:;
                                    }
                                }

                                ctwi->sortChildren(0, Qt::AscendingOrder);
                            }
                        }
                    }
                }
            }
        }

        if(sb::stype("/root" % path) == sb::Isdir)
        {
            for(ushort a(0) ; a < item->childCount() ; ++a)
            {
                QTrWI *ctwi(item->child(a));
                QStr iname(ctwi->text(0));

                if(sb::stype("/root" % path % '/' % iname) == sb::Isdir)
                {
                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    QSL itmlst;
                    for(ushort b(0) ; b < ctwi->childCount() ; ++b) itmlst.append(ctwi->child(b)->text(0));

                    for(cQStr &siname : QDir("/root" % path % '/' % iname).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                    {
                        if(ui->excludedlist->findItems(sb::right(path, -1) % '/' % iname % '/' % siname, Qt::MatchExactly).isEmpty())
                        {
                            for(ushort b(0) ; b < itmlst.count() ; ++b)
                                if(itmlst.at(b) == siname)
                                {
                                    itmlst.removeAt(b);
                                    goto rnext;
                                }

                            QTrWI *sctwi(new QTrWI);
                            sctwi->setText(0, siname);
                            ctwi->addChild(sctwi);
                        }

                    rnext:;
                    }
                }

                ctwi->sortChildren(0, Qt::AscendingOrder);
            }
        }

        busy(false);
    }
}

void systemback::on_itemslist_currentItemChanged(QTrWI *current)
{
    if(current && ! ui->additem->isEnabled())
    {
        ui->additem->setEnabled(true);

        if(ui->removeitem->isEnabled())
        {
            ui->excludedlist->currentItem()->setSelected(false);
            ui->excludedlist->setCurrentRow(-1);
            ui->removeitem->setDisabled(true);
        }
    }
}

void systemback::on_excludedlist_currentItemChanged(QLWI *current)
{
    if(current && ! ui->removeitem->isEnabled())
    {
        ui->removeitem->setEnabled(true);

        if(ui->additem->isEnabled())
        {
            ui->itemslist->currentItem()->setSelected(false);
            ui->itemslist->setCurrentItem(nullptr);
            ui->additem->setDisabled(true);
        }
    }
}

void systemback::on_additem_clicked()
{
    busy();
    const QTrWI *twi(ui->itemslist->currentItem());
    QStr path(twi->text(0));

    while(twi->parent())
    {
        twi = twi->parent();
        path.prepend(twi->text(0) % '/');
    }

    QFile file("/etc/systemback.excludes");

    if(file.open(QIODevice::Append))
    {
        file.write(QStr(path % '\n').toLocal8Bit());
        file.flush();
        ui->excludedlist->addItem(path);
        delete ui->itemslist->currentItem();
        if(ui->itemslist->currentItem()) ui->itemslist->currentItem()->setSelected(false);
        ui->additem->setDisabled(true);
        ui->excludeback->setFocus();
    }

    busy(false);
}

void systemback::on_removeitem_clicked()
{
    busy();
    QStr excdlst;
    QFile file("/etc/systemback.excludes");

    if(file.open(QIODevice::ReadOnly))
    {
        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
            if(cline != ui->excludedlist->currentItem()->text()) excdlst.append(cline % '\n');
        }

        file.close();
        sb::crtfile("/etc/systemback.excludes", excdlst);
        on_pointexclude_clicked();
        delete ui->excludedlist->currentItem();
        if(ui->excludedlist->currentItem()) ui->excludedlist->currentItem()->setSelected(false);
        ui->removeitem->setDisabled(true);
        ui->excludeback->setFocus();
    }

    busy(false);
}

void systemback::on_fullname_textChanged(const QStr &arg1)
{
    if(arg1.isEmpty())
    {
        if(ui->fullnamepipe->isVisible())
            ui->fullnamepipe->hide();
        else if(ui->fullnameerror->isVisible())
            ui->fullnameerror->hide();

        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }
    else if(sb::like(arg1, {"_ *", "* _", "*  *", "_-*", "*/*", "*:*", "*#*", "*,*", "*'*", "*\"*"}))
    {
        if(! ui->fullnameerror->isVisible())
        {
            if(ui->fullnamepipe->isVisible()) ui->fullnamepipe->hide();
            ui->fullnameerror->show();
        }

        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }
    else if(! ui->fullnamepipe->isVisible())
    {
        if(ui->fullnameerror->isVisible()) ui->fullnameerror->hide();
        ui->fullnamepipe->show();
    }
}

void systemback::on_username_textChanged(const QStr &arg1)
{
    if(arg1 != arg1.toLower())
        ui->username->setText(arg1.toLower());
    else
    {
        if(ui->usernamepipe->isVisible()) ui->usernamepipe->hide();
        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);

        if(arg1.isEmpty())
        {
            if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
        }
        else if(sb::like(arg1, {"_-*", "* *", "*/*", "*:*", "*#*", "*,*", "*'*", "*\"*"}))
        {
            if(! ui->usernameerror->isVisible()) ui->usernameerror->show();
        }
        else
        {
            sb::delay(300);

            if(arg1 == ui->username->text())
            {
                uchar rval(sb::exec("useradd " % arg1, nullptr, true));

                if(rval == 0)
                {
                    sb::exec("userdel " % arg1, nullptr, true);

                    if(arg1 == ui->username->text())
                    {
                        if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
                        ui->usernamepipe->show();
                    }
                }
                else if(rval == 9 && arg1 == ui->username->text())
                {
                    if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
                    ui->usernamepipe->show();
                }
                else if(arg1 == ui->username->text() && ! ui->usernameerror->isVisible())
                    ui->usernameerror->show();
            }
        }
    }
}

void systemback::on_hostname_textChanged(const QStr &arg1)
{
    if(ui->hostnamepipe->isVisible()) ui->hostnamepipe->hide();
    if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);

    if(arg1.isEmpty())
    {
        if(ui->hostnameerror->isVisible()) ui->hostnameerror->hide();
    }
    else
    {
        if(! arg1.contains(' '))
        {
            sb::delay(300);

            if(arg1 == ui->hostname->text())
            {
                QFile file("/etc/hostname");

                if(file.open(QIODevice::ReadOnly))
                {
                    QStr hname(file.readLine().trimmed());
                    file.close();

                    if(sb::exec("hostname " % arg1, nullptr, true) == 0)
                    {
                        sb::exec("hostname " % hname, nullptr, true);

                        if(arg1 == ui->hostname->text())
                        {
                            if(ui->hostnameerror->isVisible()) ui->hostnameerror->hide();
                            ui->hostnamepipe->show();
                        }
                    }
                    else if(arg1 == ui->hostname->text() && ! ui->hostnameerror->isVisible())
                        ui->hostnameerror->show();
                }
                else if(! ui->hostnameerror->isVisible())
                    ui->hostnameerror->show();
            }
        }
        else if(! ui->hostnameerror->isVisible())
            ui->hostnameerror->show();
    }
}

void systemback::on_password1_textChanged(const QStr &arg1)
{
    if(ui->passwordpipe->isVisible()) ui->passwordpipe->hide();
    if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);

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
        else if(! ui->passworderror->isVisible())
            ui->passworderror->show();
    }
}

void systemback::on_password2_textChanged()
{
    on_password1_textChanged(ui->password1->text());
}

void systemback::on_rootpassword1_textChanged(const QStr &arg1)
{
    if(ui->rootpasswordpipe->isVisible()) ui->rootpasswordpipe->hide();
    if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);

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
        else if(! ui->rootpassworderror->isVisible())
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
        if(sb::schdle[0] != "on")
        {
            sb::schdle[0] = "on";
            if(! cfgupdt) cfgupdt = true;
            if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        ui->schedulerstate->setText(tr("Enabled"));
        if(sb::schdle[1] != "0") ui->daydown->setEnabled(true);
        if(sb::schdle[1] != "7") ui->dayup->setEnabled(true);
        if(sb::schdle[2] != "0") ui->hourdown->setEnabled(true);
        if(sb::schdle[2] != "23") ui->hourup->setEnabled(true);
        if(sb::schdle[3] != "0" && (sb::schdle[1] != "0" || sb::schdle[2] != "0" || sb::schdle[3].toUShort() > 30)) ui->minutedown->setEnabled(true);
        if(sb::schdle[3] != "59") ui->minuteup->setEnabled(true);
        ui->silentmode->setEnabled(true);

        if(sb::schdle[5] == "off")
        {
            if(sb::schdle[4] != "10") ui->seconddown->setEnabled(true);
            if(sb::schdle[4] != "99") ui->secondup->setEnabled(true);
            ui->windowposition->setEnabled(true);
        }

    }
    else
    {
        sb::schdle[0] = "off";
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

void systemback::on_silentmode_clicked(bool checked)
{
    if(! checked)
    {
        sb::schdle[5] = "off";
        if(! cfgupdt) cfgupdt = true;
        if(sb::schdle[4] != "10") ui->seconddown->setEnabled(true);
        if(sb::schdle[4] != "99") ui->secondup->setEnabled(true);
        ui->windowposition->setEnabled(true);
    }
    else if(sb::schdle[5] == "off")
    {
        sb::schdle[5] = "on";
        if(! cfgupdt) cfgupdt = true;
        if(ui->secondup->isEnabled()) ui->secondup->setDisabled(true);
        if(ui->seconddown->isEnabled()) ui->seconddown->setDisabled(true);
        ui->windowposition->setDisabled(true);
    }
}

void systemback::on_dayup_clicked()
{
    sb::schdle[1] = QStr::number(sb::schdle[1].toUShort() + 1);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerday->setText(sb::schdle[1] % ' ' % tr("day(s)"));
    if(! ui->daydown->isEnabled()) ui->daydown->setEnabled(true);
    if(sb::schdle[1] == "7") ui->dayup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3].toUShort() > 0) ui->minutedown->setEnabled(true);
}

void systemback::on_daydown_clicked()
{
    sb::schdle[1] = QStr::number(sb::schdle[1].toUShort() - 1);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerday->setText(sb::schdle[1] % ' ' % tr("day(s)"));
    if(! ui->dayup->isEnabled()) ui->dayup->setEnabled(true);

    if(sb::schdle[1] == "0")
    {
        if(sb::schdle[2] == "0")
        {
            if(sb::schdle[3].toUShort() < 30)
            {
                sb::schdle[3] = "30";
                ui->schedulerminute->setText(sb::schdle[3] % ' ' % tr("minute(s)"));
            }

            if(ui->minutedown->isEnabled() && sb::schdle[3].toUShort() < 31) ui->minutedown->setDisabled(true);
        }

        ui->daydown->setDisabled(true);
    }
}

void systemback::on_hourup_clicked()
{
    sb::schdle[2] = QStr::number(sb::schdle[2].toUShort() + 1);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerhour->setText(sb::schdle[2] % ' ' % tr("hour(s)"));
    if(! ui->hourdown->isEnabled()) ui->hourdown->setEnabled(true);
    if(sb::schdle[2] == "23") ui->hourup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3].toUShort() > 0) ui->minutedown->setEnabled(true);
}

void systemback::on_hourdown_clicked()
{
    sb::schdle[2] = QStr::number(sb::schdle[2].toUShort() - 1);
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerhour->setText(sb::schdle[2] % ' ' % tr("hour(s)"));
    if(! ui->hourup->isEnabled()) ui->hourup->setEnabled(true);

    if(sb::schdle[2] == "0")
    {
        if(sb::schdle[1] == "0")
        {
            if(sb::schdle[3].toUShort() < 30)
            {
                sb::schdle[3] = "30";
                ui->schedulerminute->setText(sb::schdle[3] % ' ' % tr("minute(s)"));
            }

            if(ui->minutedown->isEnabled() && sb::schdle[3].toUShort() < 31) ui->minutedown->setDisabled(true);
        }

        ui->hourdown->setDisabled(true);
    }
}

void systemback::on_minuteup_clicked()
{
    sb::schdle[3] = QStr::number(sb::schdle[3].toUShort() + (sb::schdle[3] == "55" ? 4 : 5));
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerminute->setText(sb::schdle[3] % ' ' % tr("minute(s)"));
    if(! ui->minutedown->isEnabled()) ui->minutedown->setEnabled(true);
    if(sb::schdle[3] == "59") ui->minuteup->setDisabled(true);
}

void systemback::on_minutedown_clicked()
{
    sb::schdle[3] = QStr::number(sb::schdle[3].toUShort() - (sb::schdle[3] == "59" ? 4 : 5));
    if(! cfgupdt) cfgupdt = true;
    ui->schedulerminute->setText(sb::schdle[3] % ' ' % tr("minute(s)"));
    if(! ui->minuteup->isEnabled()) ui->minuteup->setEnabled(true);
    if((sb::schdle[1] == "0" && sb::schdle[2] == "0" && sb::schdle[3] == "30") || sb::schdle[3] == "0") ui->minutedown->setDisabled(true);
}

void systemback::on_secondup_clicked()
{
    sb::schdle[4] = QStr::number(sb::schdle[4].toUShort() + (sb::schdle[4] == "95" ? 4 : 5));
    if(! cfgupdt) cfgupdt = true;
    ui->schedulersecond->setText(sb::schdle[4] % ' ' % tr("seconds"));
    if(! ui->seconddown->isEnabled()) ui->seconddown->setEnabled(true);
    if(sb::schdle[4] == "99") ui->secondup->setDisabled(true);
}

void systemback::on_seconddown_clicked()
{
    sb::schdle[4] = QStr::number(sb::schdle[4].toUShort() - (sb::schdle[4] == "99" ? 4 : 5));
    if(! cfgupdt) cfgupdt = true;
    ui->schedulersecond->setText(sb::schdle[4] % ' ' % tr("seconds"));
    if(! ui->secondup->isEnabled()) ui->secondup->setEnabled(true);
    if(sb::schdle[4] == "10") ui->seconddown->setDisabled(true);
}

void systemback::on_windowposition_currentIndexChanged(const QStr &arg1)
{
    if(ui->schedulepanel->isVisible())
    {
        if(arg1 == tr("Top left") && sb::schdle[6] != "topleft")
        {
            sb::schdle[6] = "topleft";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Top right") && sb::schdle[6] != "topright")
        {
            sb::schdle[6] = "topright";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Center") && sb::schdle[6] != "center")
        {
            sb::schdle[6] = "center";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Bottom left") && sb::schdle[6] != "bottomleft")
        {
            sb::schdle[6] = "bottomleft";
            if(! cfgupdt) cfgupdt = true;
        }
        else if(arg1 == tr("Bottom right") && sb::schdle[6] != "bottomright")
        {
            sb::schdle[6] = "bottomright";
            if(! cfgupdt) cfgupdt = true;
        }
    }
}

void systemback::on_userdatainclude_clicked(bool checked)
{
    if(checked)
    {
        ullong hfree(sb::dfree("/home"));
        QFile file("/etc/passwd");

        if(hfree > 104857600 && sb::dfree("/root") > 104857600 && file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());

                if(usr.contains(":/home/"))
                {
                    usr = sb::left(usr, sb::instr(usr, ":") -1);

                    if(sb::isdir("/home/" % usr) && sb::dfree("/home/" % usr) != hfree)
                    {
                        ui->userdatainclude->setChecked(false);
                        break;
                    }
                }
            }
        else
            ui->userdatainclude->setChecked(false);
    }
}

void systemback::interrupt()
{
    if(! intrrpt)
    {
        if(! intrrptimer->isActive())
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

            intrrptimer->start();
        }
        else if(sstart)
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
            close();
        }
        else
        {
            intrrptimer->stop();
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

void systemback::on_interrupt_clicked()
{
    QTimer::singleShot(0, this, SLOT(interrupt()));
}

void systemback::on_livewritestart_clicked()
{
    dialog = 30;
    dialogopen();
}

void systemback::on_schedulerlater_clicked()
{
    if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
    close();
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
            dialog = 16;
        else
        {
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = 38;
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
        if(! sb::pnames[a].isEmpty() && (a < 9 ? (a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) : true)) ++ppipe;

    if(ppipe > 0)
    {
        uchar dnum(0);

        for(uchar a(9) ; a > 1 ; --a)
            if(! sb::pnames[a].isEmpty() && (a < 9 ? (a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) : true))
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
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");

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
        dialog = 50;
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
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
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

    if(dialog != 48)
    {
        if(sb::dfree(sb::sdir[2]) < 104857600 || (sb::isdir("/home/.sbuserdata") && sb::dfree("/home") < 104857600))
            dialog = 29;
        else if(dialog == 0)
        {
            if(! sb::ThrdDbg.isEmpty()) printdbgmsg();
            dialog = 49;
        }
    }

    if(sb::isdir("/.sblvtmp")) sb::remove("/.sblvtmp");
    if(sb::isdir("/media/.sblvtmp")) sb::remove("/media/.sblvtmp");
    if(sb::isdir("/var/.sblvtmp")) sb::remove("/var/.sblvtmp");
    if(sb::isdir("/home/.sbuserdata")) sb::remove("/home/.sbuserdata");
    if(sb::isdir("/root/.sbuserdata")) sb::remove("/root/.sbuserdata");
    if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate")) sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
    dialogopen();
    return;
exit:
    if(sb::isdir("/.sblvtmp")) sb::remove("/.sblvtmp");
    if(sb::isdir("/media/.sblvtmp")) sb::remove("/media/.sblvtmp");
    if(sb::isdir("/var/.sblvtmp")) sb::remove("/var/.sblvtmp");
    if(sb::isdir("/home/.sbuserdata")) sb::remove("/home/.sbuserdata");
    if(sb::isdir("/root/.sbuserdata")) sb::remove("/root/.sbuserdata");
    intrrpt = false;
    return;
start:
    statustart();
    prun = tr("Creating Live system") % '\n' % tr("process") % " 1/3";
    QStr ckernel(sb::ckname()), lvtype(sb::isfile("/usr/share/initramfs-tools/scripts/casper") ? "casper" : "live"), ifname;

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
    if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/.disk/info", "Systemback Live (" % ifname % ") - Release " % sb::getarch() % '\n') || ! sb::copy("/boot/vmlinuz-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/vmlinuz")) goto error;
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

                for(uchar a(0) ; a < uslst.count() ; ++a)
                    if(a == 4)
                    {
                        fname = sb::left(uslst.at(a), sb::instr(uslst.at(a), ",") - 1);
                        break;
                    }

                break;
            }
        }
    }

    if(intrrpt) goto exit;
    irfsc = true;

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
    else if(sb::isfile("/usr/share/initramfs-tools/scripts/live-bottom/30accessibility"))
    {
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/live-bottom/30accessibility", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther)) goto error;
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
    else if(sb::isfile("/usr/share/initramfs-tools/scripts/live-bottom/30accessibility"))
    {
        if(! QFile::setPermissions("/usr/share/initramfs-tools/scripts/live-bottom/30accessibility", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther)) goto error;
    }
    else if(! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab"))
        goto error;

    if(rv > 0)
    {
        dialog = 48;
        goto error;
    }

    irfsc = false;
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
    prun = tr("Creating Live system") % '\n' % tr("process") % " 2/3";

    if(sb::exec("mksquashfs" % ide % ' ' % sb::sdir[2] % "/.sblivesystemcreate/.systemback /media/.sblvtmp/media /var/.sblvtmp/var " % sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs -info -b 1M -no-duplicates -no-recovery -always-use-fragments -e /etc/fstab -e /etc/mtab -e /etc/udev/rules.d/70-persistent-cd.rules -e /etc/udev/rules.d/70-persistent-net.rules") > 0)
    {
        dialog = 26;
        goto error;
    }

    sb::Progress = -1;
    ui->progressbar->setValue(0);
    prun = tr("Creating Live system") % '\n' % tr("process") % " 3/3";
    sb::remove("/.sblvtmp");
    sb::remove("/media/.sblvtmp");
    sb::remove("/var/.sblvtmp");
    if(sb::isdir("/home/.sbuserdata")) sb::remove("/home/.sbuserdata");
    if(sb::isdir("/root/.sbuserdata")) sb::remove("/root/.sbuserdata");
    if(intrrpt) goto exit;
    QStr rpart, grxorg, srxorg;
    if(QFile(sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs").size() > 4294967295) rpart = "root=LABEL=SBROOT ";

    if(xmntry)
    {
        grxorg = "menuentry \"" % tr("Boot Live without xorg.conf file") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " noxconf quiet splash\n  initrd /" % lvtype % "/initrd.gz\n}\n\n";
        srxorg = "label noxconf\n  menu label " % tr("Boot Live without xorg.conf file") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz noxconf quiet splash\n\n";
    }

    if(sb::getarch() == "amd64" && sb::isfile("/usr/share/systemback/efi-amd64.bootfiles") && (sb::exec("tar -xJf /usr/share/systemback/efi-amd64.bootfiles -C " % sb::sdir[2] % "/.sblivesystemcreate --no-same-owner --no-same-permissions") > 0 || ! sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/boot/grub/splash.png") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/grub.cfg", "if loadfont /boot/grub/font.pf2\nthen\n  set gfxmode=auto\n  insmod efi_gop\n  insmod efi_uga\n  insmod gfxterm\n  terminal_output gfxterm\nfi\n\nset theme=/boot/grub/theme.cfg\n\nmenuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " quiet splash\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % "\n  initrd /" % lvtype % "/initrd.gz\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/theme.cfg", "title-color: \"white\"\ntitle-text: \"Systemback Live (" % ifname % ")\"\ntitle-font: \"Sans Regular 16\"\ndesktop-color: \"black\"\ndesktop-image: \"/boot/grub/splash.png\"\nmessage-color: \"white\"\nmessage-bg-color: \"black\"\nterminal-font: \"Sans Regular 12\"\n\n+ boot_menu {\n  top = 150\n  left = 15%\n  width = 75%\n  height = 130\n  item_font = \"Sans Regular 12\"\n  item_color = \"grey\"\n  selected_item_color = \"white\"\n  item_height = 20\n  item_padding = 15\n  item_spacing = 5\n}\n\n+ vbox {\n  top = 100%\n  left = 2%\n  + label {text = \"" % tr("Press 'E' key to edit") % "\" font = \"Sans 10\" color = \"white\" align = \"left\"}\n}\n") ||
        ! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/loopback.cfg", "menuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz" % rpart % "boot=" % lvtype % " quiet splash\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % "\n  initrd /" % lvtype % "/initrd.gz\n}\n"))) goto error;

    if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", "default vesamenu.c32\nprompt 0\ntimeout 100\n\nmenu title Systemback Live (" % ifname % ")\nmenu tabmsg " % tr("Press TAB key to edit") % "\nmenu background splash.png\n\nlabel live\n  menu label " % tr("Boot Live system") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz quiet splash\n\nlabel safe\n  menu label " % tr("Boot Live in safe graphics mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz xforcevesa nomodeset quiet splash\n\n" % srxorg % "label debug\n  menu label " % tr("Boot Live in debug mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz\n")) goto error;
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
        dialog = 27;
        goto error;
    }

    if(! QFile::setPermissions(sb::sdir[2] % '/' % ifname % ".sblive", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) goto error;
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
    dialog = 28;
    sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
    on_livecreatemenu_clicked();
    dialogopen();
}

void systemback::on_liveconvert_clicked()
{
    if(dialog > 0) dialog = 0;
    goto start;
error:
    if(intrrpt) goto exit;
    if(dialog == 0) dialog = 56;
    dialogopen();
    return;
exit:
    if(sb::isdir(sb::sdir[2] % "/.sblivesystemconvert")) sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(sb::isfile(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso")) sb::remove(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso");
    intrrpt = false;
    return;
start:
    statustart();
    prun = tr("Converting Live system image") % '\n' % tr("process") % " 1/2";
    if((sb::exist(sb::sdir[2] % "/.sblivesystemconvert") && ! sb::remove(sb::sdir[2] % "/.sblivesystemconvert")) || ! QDir().mkdir(sb::sdir[2] % "/.sblivesystemconvert")) goto error;
    sb::ThrdLng[0] = sb::fsize(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive");
    sb::ThrdStr[0] = sb::sdir[2] % "/.sblivesystemconvert";

    if(sb::exec("tar -xf " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C " % sb::sdir[2] % "/.sblivesystemconvert --no-same-owner --no-same-permissions") > 0)
    {
        dialog = 47;
        goto error;
    }

    if(! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemconvert/syslinux/isolinux.cfg") || ! QFile::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux", sb::sdir[2] % "/.sblivesystemconvert/isolinux")) goto error;
    if(intrrpt) goto exit;
    prun = tr("Converting Live system image") % '\n' % tr("process") % " 2/2";
    sb::Progress = -1;
    ui->progressbar->setValue(0);

    if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso " % sb::sdir[2] % "/.sblivesystemconvert") > 0)
    {
        if(sb::isfile(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso")) sb::remove(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso");
        dialog = 46;
        goto error;
    }

    if(intrrpt) goto exit;
    if(sb::exec("isohybrid " % sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso") > 0 || ! QFile::setPermissions(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".iso", QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) goto error;
    sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(intrrpt) goto exit;
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
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

    switch(ui->partitionsettings->item(ui->partitionsettings->currentItem()->row(), 7)->text().toShort()) {
    case sb::Extended:
        sb::delpart(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text());
        break;
    default:
        sb::mkptable(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), grub == "efi-amd64" ? "gpt" : "msdos");
    }

    on_partitionrefresh2_clicked();
    busy(false);
}

void systemback::on_newpartition_clicked()
{
    busy();
    ui->copycover->show();
    QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), 8));
    ullong start(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 8)->text().toULongLong());
    uchar type;

    switch(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text().toUShort()) {
    case sb::Freespace:
    {
        uchar pcount(0), fcount(0);

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
            {
                switch(ui->partitionsettings->item(a, 7)->text().toUShort()) {
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
            }

        if(pcount > 2 && (fcount > 1 || ui->partitionsize->value() < ui->partitionsize->maximum()))
        {
            if(! sb::mkpart(dev, start, ui->partitionsettings->item(ui->partitionsettings->currentRow(), 9)->text().toULongLong(), sb::Extended)) goto end;
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
    sb::mkpart(dev, start, ui->partitionsize->value() == ui->partitionsize->maximum() ? ui->partitionsettings->item(ui->partitionsettings->currentRow(), 9)->text().toULongLong() : ullong(ui->partitionsize->value()) * 1048576, type);
end:
    on_partitionrefresh2_clicked();
    busy(false);
}
