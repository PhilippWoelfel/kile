//
//
// C++ Interface: kilestdactions
//
// Description: 
//
//
// Author: Jeroen Wijnhout <Jeroen.Wijnhout@kdemail.net>, (C) 2003

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KILESTDACTIONS_H
#define KILESTDACTIONS_H

#include "kactioncollection.h"
#include <QWidget>

namespace KileStdActions
{
	void setupStdTags(KileInfo *ki, const QObject *receiver, KActionCollection *actionCollection, QWidget *parentWidget);
	void setupBibTags(const QObject *receiver, KActionCollection *actionCollection);
	void setupMathTags(const QObject *receiver, KActionCollection *actionCollection);
}

#endif
