/***************************************************************************
                          kilestructurewidget.h  -  description
                             -------------------
    begin                : Sun Dec 28 2003
    copyright            : (C) 2003 by Jeroen Wijnhout
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

#ifndef KILEWIDGET_STRUCTURE_H
#define KILEWIDGET_STRUCTURE_H
  
 /**
  * @author Jeroen Wijnhout
  **/

#include <qwidgetstack.h>
#include <qvbox.h>

#include <klistview.h>

#include "kiledocumentinfo.h"

class QString;
class KURL;
class KileInfo;
class QListViewItem;

/**
 * ListView items that can hold some additional information appropriate for the Structure View. The
 * additional information is: line number, title string.
 **/
 
class KileListViewItem : public KListViewItem
{
public:
	KileListViewItem(QListViewItem * parent, QListViewItem * after, const QString &title, const KURL &url, uint line, uint m_column, int type, int level);
	KileListViewItem(QListView * parent, const QString & label);
	KileListViewItem(QListViewItem * parent, const QString & label);

	/** @returns the title of this element (for a label it return the label), without the (line ...) part **/
	const QString& title() const { return m_title; }
	/** @returns the line number of the structure element. **/
	const uint line() const { return m_line; }
	/** @returns the column number of the structure element, right after the { **/
	const uint column() const { return m_column; }
	/** @returns the type of element, see @ref KileStruct **/
	const int type() const { return m_type; }
	/**@returns the file in which this item was found*/
	const KURL & url() const { return m_url; }
	void setURL(const KURL & url) { m_url = url; }

	const int level() const { return m_level; }

private:
	QString		m_title;
	KURL		m_url;
	uint		m_line;
	uint		m_column;
	int			m_type, m_level;
};

namespace KileWidget
{
	class Structure; //forward declaration

	class StructureList : public KListView
	{
		Q_OBJECT

	public:
		StructureList(Structure *stack, KileDocument::Info *docinfo);
		~StructureList();

		void activate();
		void cleanUp();

		const KURL & url() const { return m_docinfo->url(); }

	public slots:
		void addItem(const QString &title, uint line, uint column, int type, int level, const QString & pix, const QString &folder = "root");

	private:
		KileListViewItem *parentFor(int lev, const QString & fldr);

		void init();
		KileListViewItem* createFolder(const QString &folder);
		KileListViewItem* folder(const QString &folder);

		void saveState();
		bool shouldBeOpen(KileListViewItem *item, const QString & folder, int level);

	private:
		Structure							*m_stack;
		KileDocument::Info					*m_docinfo;
		QMap<QString, KileListViewItem *>	m_folders;
		QMap<QString, bool>					m_openByTitle;
		QMap<uint, bool>					m_openByLine;
		KileListViewItem					*m_parent[7], *m_current, *m_root, *m_child, *m_lastChild;
	};

	class Structure : public QWidgetStack
	{
		Q_OBJECT

		public:
			Structure(KileInfo *, QWidget * parent, const char * name = 0);

			int level();
			KileInfo *info() { return m_ki; }

		public slots:
			void slotClicked(QListViewItem *);
			void slotDoubleClicked(QListViewItem *);

			void addDocumentInfo(KileDocument::Info *);
			void closeDocumentInfo(KileDocument::Info *);
			void update(KileDocument::Info *, bool);
            void clean(KileDocument::Info *);

			/**
			* Clears the structure widget and empties the map between KileDocument::Info objects and their structure trees (QListViewItem).
			**/
			void clear();

		signals:
			void setCursor(const KURL &, int, int);
			void fileOpen(const KURL &, const QString &);
			void fileNew(const KURL &);

		private:
			StructureList* viewFor(KileDocument::Info *info);
			bool viewExistsFor(KileDocument::Info *info);

		private:
			KileInfo									*m_ki;
			KileDocument::Info							*m_docinfo;
			QMap<KileDocument::Info *, StructureList *>	m_map;
			StructureList								*m_default;
	};
}

#endif
