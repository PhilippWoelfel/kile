/***************************************************************************
    begin                : Sun Apr 21 2002
    copyright            : (C) 2002 - 2003 by Pascal Brachet, 2003 Jeroen Wijnhout
    email                : Jeroen.Wijnhout@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kileapplication.h"

#include <kglobal.h>
#include <kstandarddirs.h>
#include <ktexteditor/document.h>

#include <QDesktopWidget>
#include <q3frame.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <qpainter.h>
#include <qfont.h>
#include <qfontmetrics.h>

static Q3Frame *pix=0;
static QWidget *splash=0;


KileApplication::KileApplication()
{
	//FIXME: rework this
	QRect screen = QApplication::desktop()->screenGeometry();
	QPixmap pm(KGlobal::dirs()->findResource("appdata","pics/kile_splash.png"));
	splash = new QWidget(0, "splash", Qt::WDestructiveClose | Qt::WStyle_Customize |  Qt::FramelessWindowHint | Qt::WX11BypassWM | Qt::WStyle_StaysOnTop);
	pix=new Q3Frame(splash,"pix", Qt::FramelessWindowHint);
	pix->setBackgroundPixmap(pm);
	pix->setLineWidth(0);
	pix->setGeometry(0, 0, 398, 120);
	splash->adjustSize();
	splash->setCaption("Kile");
	splash->move(screen.center() - QPoint(splash->width() / 2, splash->height() / 2));
	splash->show();
}
KileApplication::~KileApplication(){
}

void KileApplication::closeSplash()
{
splash->hide();
delete splash;
}