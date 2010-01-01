/***************************************************************************
    date                 : Mar 30 2007
    version              : 0.24
    copyright            : (C) 2004-2007 by Holger Danielsson
    email                : holger.danielsson@versanet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CODECOMPLETIONCONFIGWIDGET_H
#define CODECOMPLETIONCONFIGWIDGET_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

class KConfig;
class KPushButton;
class KTabWidget;

/**
  *@author Holger Danielsson
  */

namespace KileWidget {
class LogWidget;
}

class CodeCompletionConfigWidget : public QWidget
{
		Q_OBJECT
	public:
		CodeCompletionConfigWidget(KConfig *config, KileWidget::LogWidget *logwidget, QWidget *parent = 0, const char *name = 0);
		~CodeCompletionConfigWidget();

		void readConfig(void);
		void writeConfig(void);

	private:
		enum CompletionPage { TexPage = 0, DictionaryPage = 1, AbbreviationPage = 2, NumPages = 3 };

		KConfig *m_config;
		KileWidget::LogWidget *m_logwidget;

		// tabs, views, pages, wordlists
		KTabWidget *tab;
		QTreeWidget *m_listview[NumPages];
		QWidget *m_page[NumPages];
		QStringList m_wordlist[NumPages];
		QStringList m_dirname;

		// button
		KPushButton *add, *remove;

		// Checkboxes/Spinboxes
		QCheckBox *cb_autocomplete;
		QCheckBox *cb_setcursor, *cb_setbullets;
		QCheckBox *cb_closeenv;
		QSpinBox *sp_latexthreshold, *sp_cwlthreshold;
		QLabel *lb_latexthreshold, *lb_cwlthreshold;
		QCheckBox *cb_autocompleteabbrev;
		QCheckBox *cb_showabbrevview;
		QCheckBox *cb_showcwlview;
		QCheckBox *cb_citeoutofbraces;

		QTreeWidget *getListview(QWidget *page);
		QString getListname(QWidget *page);
		void addPage(QTabWidget *tab, CompletionPage page, const QString &title, const QString &dirname);

		void setListviewEntries(CompletionPage page);
		bool getListviewEntries(CompletionPage page);
		bool isListviewEntry(QTreeWidget *listview, const QString &filename);
		void updateColumnWidth(QTreeWidget *listview);

		QString m_localCwlDir, m_globalCwlDir;
		void getCwlFiles(QMap<QString, QString> &map, QStringList &list, const QString &dir);
		void getCwlDirs();

	private Q_SLOTS:
		void showPage(QWidget *page);
		void showPage(int index);
		void addClicked();
		void removeClicked();
		void slotSelectionChanged();
};

#endif
