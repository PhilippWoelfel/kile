/************************************************************************************************
  Copyright (C) 2004-2007 by Holger Danielsson (holger.danielsson@versanet.de)
                2009-2010 by Michel Ludwig (michel.ludwig@kdemail.net)
 ************************************************************************************************/


/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "widgets/codecompletionconfigwidget.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KConfig>
#include <KDialog>
#include <KLocale>
#include <KMessageBox>
#include <KPushButton>
#include <KStandardDirs>
#include <KTabWidget>

#include "kilelistselector.h"
#include "kileconfig.h"
#include "kiledebug.h"
#include "widgets/logwidget.h"
#include "kiletool_enums.h"

CodeCompletionConfigWidget::CodeCompletionConfigWidget(KConfig *config, KileWidget::LogWidget *logwidget, QWidget *parent, const char *name)
		: QWidget(parent), m_config(config), m_logwidget(logwidget)
{
	setObjectName(name);
	setupUi(this);

	// add three pages: Tex/Latex, Dictionary, Abbreviation
	addPage(m_tabWidget, TexPage, i18n("TeX/LaTeX"), "tex");
	addPage(m_tabWidget, DictionaryPage, i18n("Dictionary"), "dictionary");
	addPage(m_tabWidget, AbbreviationPage, i18n("Abbreviation"), "abbreviation");

	cb_setcursor->setWhatsThis(i18n("Try to place the cursor."));
	cb_setbullets->setWhatsThis(i18n("Insert bullets where the user must input data."));
	cb_closeenv->setWhatsThis(i18n("Also close an environment when an opening command is inserted."));
	cb_autocomplete->setWhatsThis(i18n("Directional or popup-based completion of the TeX/LaTeX commands that are contained in the selected completion files."));
	sp_latexthreshold->setWhatsThis(i18n("Automatically show a completion list of TeX/LaTeX commands when the word has this length."));
	cb_citeoutofbraces->setWhatsThis(i18n("Move cursor out of braces after selecting from a citation keylist."));

	cb_showabbrevview->setWhatsThis(i18n("Show abbreviations of the selected completion files in the sidebar"));
	cb_autocompleteabbrev->setWhatsThis(i18n("Directional or popup-based completion of abbreviations that are contained in the selected completion files."));
	cb_showcwlview->setWhatsThis(i18n("Show LaTeX commands of the selected completion files in the sidebar"));
	sp_cwlthreshold->setWhatsThis(i18n("Maximum number of completion files, which can be shown in the sidebar."));

	connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(showPage(int)));
	connect(m_addFileButton, SIGNAL(clicked()), this, SLOT(addClicked()));
	connect(m_removeFileButton, SIGNAL(clicked()), this, SLOT(removeClicked()));

	// find resource directories for cwl files
	getCwlDirs();
}

CodeCompletionConfigWidget::~CodeCompletionConfigWidget()
{
}

void CodeCompletionConfigWidget::addPage(QTabWidget *tab, CompletionPage page, const QString &title, const QString &dirname)
{
	m_page[page] = new QWidget(tab);

	m_listview[page] = new QTreeWidget(m_page[page]);
	m_listview[page]->setHeaderLabels(QStringList() << i18n("Completion Files")
	                                                << i18n("Local File"));
	m_listview[page]->setAllColumnsShowFocus(true);
	m_listview[page]->setRootIsDecorated(false);
	m_listview[page]->setSelectionMode(QAbstractItemView::ExtendedSelection);

	QGridLayout *grid = new QGridLayout();
	grid->setMargin(0);
	grid->setSpacing(KDialog::spacingHint());
	m_page[page]->setLayout(grid);
	grid->addWidget(m_listview[page], 0, 0);

	// add Tab
	tab->addTab(m_page[page], title);

	// remember directory name
	m_dirname << dirname;

	connect(m_listview[page], SIGNAL(itemSelectionChanged()),
	        this, SLOT(slotSelectionChanged()));
}

//////////////////// read/write configuration ////////////////////

void CodeCompletionConfigWidget::readConfig(void)
{
	// read selected and deselected filenames with wordlists
	m_wordlist[TexPage] = KileConfig::completeTex();
	m_wordlist[DictionaryPage]  = KileConfig::completeDict();
	m_wordlist[AbbreviationPage]  = KileConfig::completeAbbrev();

	// set checkbox status
	cb_setcursor->setChecked(KileConfig::completeCursor());
	cb_setbullets->setChecked(KileConfig::completeBullets());
	cb_closeenv->setChecked(KileConfig::completeCloseEnv());
	cb_showabbrevview->setChecked(KileConfig::completeShowAbbrev());
	cb_showcwlview->setChecked(KileConfig::showCwlCommands());
	cb_citeoutofbraces->setChecked(KileConfig::completeCitationMove());

	cb_autocomplete->setChecked(KileConfig::completeAuto());
	cb_autocompleteabbrev->setChecked(KileConfig::completeAutoAbbrev());

	sp_latexthreshold->setValue(KileConfig::completeAutoThreshold());
	sp_cwlthreshold->setValue(KileConfig::maxCwlFiles());

	// insert filenames into listview
	for (uint i = TexPage; i < NumPages; ++i) {
		setListviewEntries(CompletionPage(i));
	}
}

void CodeCompletionConfigWidget::writeConfig(void)
{
	// default: no changes in configuration
	bool changed = false;

	// get listview entries
	for (uint i = TexPage; i < NumPages; ++i) {
		changed |= getListviewEntries(CompletionPage(i));
	}

	// Konfigurationslisten abspeichern
	KileConfig::setCompleteTex(m_wordlist[TexPage]);
	KileConfig::setCompleteDict(m_wordlist[DictionaryPage]);
	KileConfig::setCompleteAbbrev(m_wordlist[AbbreviationPage]);

	// save checkbox status
	KileConfig::setCompleteCursor(cb_setcursor->isChecked());
	KileConfig::setCompleteBullets(cb_setbullets->isChecked());
	KileConfig::setCompleteCloseEnv(cb_closeenv->isChecked());
	KileConfig::setCompleteShowAbbrev(cb_showabbrevview->isChecked());
	KileConfig::setShowCwlCommands(cb_showcwlview->isChecked());
	KileConfig::setCompleteCitationMove(cb_citeoutofbraces->isChecked());

	// read autocompletion settings
	bool autoModeLatex = cb_autocomplete->isChecked();
	bool autoModeAbbrev = cb_autocompleteabbrev->isChecked();

	// save settings for Kile autocompletion modes
	KileConfig::setCompleteAuto(autoModeLatex);
	KileConfig::setCompleteAutoAbbrev(autoModeAbbrev);
	KileConfig::setCompleteAutoThreshold(sp_latexthreshold->value());
	KileConfig::setMaxCwlFiles(sp_cwlthreshold->value());
	
	// save changed wordlists?
	KileConfig::setCompleteChangedLists(changed);
}

//////////////////// listview ////////////////////

// ListView fr den Konfigurationsdialog einstellen

void CodeCompletionConfigWidget::setListviewEntries(CompletionPage page)
{
	QString listname = m_dirname[page];
	QString localdir = m_localCwlDir + listname + '/';
	QString globaldir = m_globalCwlDir + listname + '/';

	// Daten aus der Konfigurationsliste in das ListView-Widget eintragen
	m_listview[page]->setUpdatesEnabled(false);
	m_listview[page]->clear();
	QStringList::ConstIterator it;
	for (it = m_wordlist[page].constBegin(); it != m_wordlist[page].constEnd(); ++it) {
		QString basename = (*it).right((*it).length() - 2);
		bool localExists = QFileInfo(localdir + basename + ".cwl").exists();

		QTreeWidgetItem *item = new QTreeWidgetItem(m_listview[page], QStringList(basename));
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		if (localExists) {
			item->setCheckState(0, (*it).at(0) == '1' ? Qt::Checked : Qt::Unchecked);
			item->setText(1, "+");
		}
		else {
			if (QFileInfo(globaldir + basename + ".cwl").exists()) {
				item->setCheckState(0, (*it).at(0) == '1' ? Qt::Checked : Qt::Unchecked);
			}
			else {
				item->setCheckState(0, Qt::Unchecked);
				item->setText(1, i18n("File not found"));
			}
		}
	}

	updateColumnWidth(m_listview[page]);
	m_listview[page]->setUpdatesEnabled(true);
}

void CodeCompletionConfigWidget::updateColumnWidth(QTreeWidget *listview)
{
	listview->setColumnWidth(0, listview->columnWidth(0) + 60);
}

bool CodeCompletionConfigWidget::getListviewEntries(CompletionPage page)
{
	KILE_DEBUG() << "===bool CodeCompletionConfigWidget::getListviewEntries(CompletionPage" << page << ")";
	
	bool changed = false;

	// count number of entries
	int n = m_listview[page]->topLevelItemCount();

	// there are changes if this number has changed
	if(n != m_wordlist[page].count()) {
		changed = true;
	}

	// clear all stringlist with files, if there are no entries
	if (n == 0) {
		m_wordlist[page].clear();
		return changed;
	}

	// now check all entries if they have changed
	QStringList newfiles;
	int index = 0;
	QTreeWidgetItemIterator it(m_listview[page]);
	while (*it) {
		QString s = ((*it)->checkState(0) == Qt::Checked) ? "1-" : "0-";
		s += (*it)->text(0);
		newfiles.append(s);

		// check for a change
		if (index >= m_wordlist[page].count() || m_wordlist[page][index] != s) {
			changed = true;
		}

		// go on
		++it;
		index++;
	}

	// only update if there are changes
	if (changed) {
		m_wordlist[page] = newfiles;
	}

	return changed;
}

bool CodeCompletionConfigWidget::isListviewEntry(QTreeWidget *listview, const QString &filename)
{
	return listview->findItems(filename, Qt::MatchExactly).count() > 0;
}

//////////////////// tabpages parameter ////////////////////

QTreeWidget *CodeCompletionConfigWidget::getListview(QWidget *page)
{
	for (uint i = TexPage; i < NumPages; ++i) {
		if (page == m_page[i]) {
			return m_listview[i];
		}
	}
	return 0;
}

QString CodeCompletionConfigWidget::getListname(QWidget *page)
{
	for (uint i = TexPage; i < NumPages; ++i) {
		if(page == m_page[i]) {
			return m_dirname[i];
		}
	}
	return QString();
}

//////////////////// shwo tabpages ////////////////////

void CodeCompletionConfigWidget::showPage(QWidget *page)
{
	QTreeWidget *listview = getListview(page);
	if(listview) {
		m_removeFileButton->setEnabled(listview->selectedItems().count() > 0);
	}
}

void CodeCompletionConfigWidget::showPage(int index)
{
	showPage(m_tabWidget->widget(index));
}

//////////////////// add/remove new wordlists ////////////////////

// find local and global resource directories

void CodeCompletionConfigWidget::getCwlDirs()
{
	m_localCwlDir = KStandardDirs::locateLocal("appdata", "complete/");
	m_globalCwlDir.clear();

	const QStringList dirs = KGlobal::dirs()->findDirs("appdata", "complete/");
	for(QStringList::ConstIterator it = dirs.constBegin(); it != dirs.constEnd(); ++it) {
		if((*it) != m_localCwlDir) {
			m_globalCwlDir = (*it);
			break;
		}
	}
}

// find local and global cwl files: global files are not added,
// if there is already a local file with this name. We fill a map
// with filename as key and filepath as value. Additionally all
// filenames are added to a stringlist.

void CodeCompletionConfigWidget::getCwlFiles(QMap<QString, QString> &map, QStringList &list, const QString &dir)
{
	QStringList files = QDir(dir, "*.cwl").entryList();
	for (QStringList::ConstIterator it = files.constBegin(); it != files.constEnd(); ++it) {
		QString filename = QFileInfo(*it).fileName();
		if(! map.contains(filename)) {
			map[filename] = dir + '/' + (*it);
			list << filename;
		}
	}
}

void CodeCompletionConfigWidget::addClicked()
{
	// determine current subdirectory for current tab page
	QString listname = getListname(m_tabWidget->currentWidget());


	// get a sorted list of all cwl files from both directories
	QMap<QString, QString> filemap;
	QStringList filelist;
	getCwlFiles(filemap, filelist, m_localCwlDir + listname);
	getCwlFiles(filemap, filelist, m_globalCwlDir + listname);
	filelist.sort();

	// dialog to add cwl files
	KileListSelectorMultiple *dlg  = new KileListSelectorMultiple(filelist, i18n("Complete Files"), i18n("Select Files"), this);
	if (dlg->exec()) {
		if (dlg->currentItem() >= 0) {
			QTreeWidget *listview = getListview(m_tabWidget->currentWidget());     // get current page
			QStringList filenames = dlg->selected();                   // get selected files
			for (QStringList::ConstIterator it = filenames.constBegin(); it != filenames.constEnd(); ++it) {
				QString filename = *it;
				// could we accept the wordlist?
				QFileInfo fi(filemap[filename]);
				if (!filename.isEmpty() && fi.exists() && fi.isReadable()) {
					QString basename = filename.left(filename.length() - 4);

					// check if this entry already exists
					if (isListviewEntry(listview, basename)) {
						m_logwidget->printMessage(KileTool::Info, i18n("Wordlist '%1' is already used.", basename), i18n("Complete"));
						continue;
					}

					// add new entry
					QTreeWidgetItem *item = new QTreeWidgetItem(listview, QStringList(basename));
					item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
					item->setCheckState(0, Qt::Checked);
					item->setSelected(true);
					if (filemap[filename].left(m_localCwlDir.length()) == m_localCwlDir) {
						item->setText(1, "+");
					}
				}
			}
			updateColumnWidth(listview);
		}
	}
	delete dlg;

}

// delete a selected entry

void CodeCompletionConfigWidget::removeClicked()
{
	QWidget *page = m_tabWidget->currentWidget();
	QTreeWidget *list = getListview(page);                              // determine page

	foreach(QTreeWidgetItem *item, list->selectedItems()) {
		delete item;
	}

	showPage(page);
}

void CodeCompletionConfigWidget::slotSelectionChanged()
{
	QTreeWidget *listview = getListview(m_tabWidget->currentWidget());     // get current page
	m_removeFileButton->setEnabled(listview->selectedItems().count() > 0);
}

#include "codecompletionconfigwidget.moc"
