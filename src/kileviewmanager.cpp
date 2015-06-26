/**************************************************************************
*   Copyright (C) 2004 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)   *
*             (C) 2006-2014 by Michel Ludwig (michel.ludwig@kdemail.net)  *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kileviewmanager.h"

#include <QClipboard>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLayout>
#include <QPixmap>
#include <QSplitter>
#include <QTimer> //for QTimer::singleShot trick
#include <QMimeData>

#include <QApplication>
#include <QAction>
#include <KActionCollection>
#include <KIconLoader>
#include <kio/pixmaploader.h>
#include <KLocalizedString>
#include <KMessageBox>
#include <KTextEditor/Application>
#include <KTextEditor/CodeCompletionInterface>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <KTextEditor/MainWindow>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>
#include <QMenu>
#include <KAcceleratorManager>

#include <config.h>

#include "editorkeysequencemanager.h"
#include "kileinfo.h"
#include "kileconstants.h"
#include "kileproject.h"
#include "kiledocmanager.h"
#include "kileextensions.h"
#include "kiletool_enums.h"
#include "usermenu/usermenu.h"
#include "livepreview.h"
#include "widgets/projectview.h"
#include "widgets/structurewidget.h"
#include "editorextension.h"
#include "plaintolatexconverter.h"
#include "widgets/previewwidget.h"
#include "quickpreview.h"
#include "codecompletion.h"

#if LIVEPREVIEW_AVAILABLE
#include <okular/interfaces/viewerinterface.h>
#include <KConfigGroup>
#endif

namespace KileView
{

//BEGIN DocumentViewerWindow

DocumentViewerWindow::DocumentViewerWindow(QWidget *parent, Qt::WindowFlags f)
: KMainWindow(parent, f)
{
}

DocumentViewerWindow::~DocumentViewerWindow()
{
}

void DocumentViewerWindow::showEvent(QShowEvent *event)
{
	KMainWindow::showEvent(event);
	emit visibilityChanged(true);
}

void DocumentViewerWindow::closeEvent(QCloseEvent *event)
{
	KMainWindow::closeEvent(event);
	emit visibilityChanged(false);
}

//END DocumentViewerWindow

Manager::Manager(KileInfo *info, KActionCollection *actionCollection, QObject *parent, const char *name) :
	QObject(parent),
	m_ki(info),
// 	m_projectview(Q_NULLPTR),
	m_tabs(Q_NULLPTR),
	m_viewerPartWindow(Q_NULLPTR),
	m_widgetStack(Q_NULLPTR),
	m_emptyDropWidget(Q_NULLPTR),
	m_pasteAsLaTeXAction(Q_NULLPTR),
	m_convertToLaTeXAction(Q_NULLPTR),
	m_quickPreviewAction(Q_NULLPTR)
{
	setObjectName(name);
	createViewerPart(actionCollection);
}


Manager::~Manager()
{
	KILE_DEBUG_MAIN;

	// the parent of the widget might be Q_NULLPTR; see 'destroyDocumentViewerWindow()'
	if(m_viewerPart) {
		delete m_viewerPart->widget();
		delete m_viewerPart;
	}

	destroyDocumentViewerWindow();
}

static inline bool isTextView(QWidget* /*w*/)
{
	return true;
}

static inline KTextEditor::View* toTextView(QWidget* w)
{
	return qobject_cast<KTextEditor::View*>(w);
}

void Manager::setClient(KXMLGUIClient *client)
{
	m_client = client;
	if(Q_NULLPTR == m_client->actionCollection()->action("popup_pasteaslatex")) {
		m_pasteAsLaTeXAction = new QAction(i18n("Paste as LaTe&X"), this);
		connect(m_pasteAsLaTeXAction, SIGNAL(triggered()), this, SLOT(pasteAsLaTeX()));
	}
	if(Q_NULLPTR == m_client->actionCollection()->action("popup_converttolatex")) {
		m_convertToLaTeXAction = new QAction(i18n("Convert Selection to &LaTeX"), this);
		connect(m_convertToLaTeXAction, SIGNAL(triggered()), this, SLOT(convertSelectionToLaTeX()));
	}
	if(Q_NULLPTR == m_client->actionCollection()->action("popup_quickpreview")) {
		m_quickPreviewAction = new QAction(this);
		connect(m_quickPreviewAction, SIGNAL(triggered()), this, SLOT(quickPreviewPopup()));
	}
}

void Manager::readConfig(QSplitter *splitter)
{
	// we might have to change the location of the viewer part
	setupViewerPart(splitter);

	setDocumentViewerVisible(KileConfig::showDocumentViewer());

#if LIVEPREVIEW_AVAILABLE
	Okular::ViewerInterface *viewerInterface = dynamic_cast<Okular::ViewerInterface*>(m_viewerPart.data());
	if(viewerInterface && !m_ki->livePreviewManager()->isLivePreviewActive()) {
		viewerInterface->setWatchFileModeEnabled(KileConfig::watchFileForDocumentViewer());
		// also reload the document; this is necessary for switching back on watch-file mode as otherwise
		// it would only enabled after the document has been reloaded anyway
		if(m_viewerPart->url().isValid()) {
			m_viewerPart->openUrl(m_viewerPart->url());
		}
	}
#endif
}

void Manager::writeConfig()
{
	if(m_viewerPart) {
		KileConfig::setShowDocumentViewer(isViewerPartShown());
	}
	if(m_viewerPartWindow) {
		KConfigGroup group(KSharedConfig::openConfig(), "KileDocumentViewerWindow");
		m_viewerPartWindow->saveMainWindowSettings(group);
	}
}

QWidget* Manager::createTabs(QWidget *parent)
{
	m_widgetStack = new QStackedWidget(parent);
	m_emptyDropWidget = new DropWidget(m_widgetStack);
	m_widgetStack->addWidget(m_emptyDropWidget);
	connect(m_emptyDropWidget, SIGNAL(testCanDecode(const QDragEnterEvent*, bool&)), this, SLOT(testCanDecodeURLs(const QDragEnterEvent*, bool&)));
	connect(m_emptyDropWidget, SIGNAL(receivedDropEvent(QDropEvent*)), m_ki->docManager(), SLOT(openDroppedURLs(QDropEvent*)));
	connect(m_emptyDropWidget, SIGNAL(mouseDoubleClick()), m_ki->docManager(), SLOT(fileNew()));
	m_tabs = new QTabWidget(parent);
	KAcceleratorManager::setNoAccel(m_tabs);
	m_widgetStack->addWidget(m_tabs);
	m_tabs->setFocusPolicy(Qt::ClickFocus);

	m_tabs->setMovable(true);
	m_tabs->setTabsClosable(true);
	m_tabs->setUsesScrollButtons(true);
	m_tabs->setFocus();
	m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(currentViewChanged(int)));
	connect(m_tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
	connect(m_tabs, SIGNAL(testCanDecode(const QDragMoveEvent*, bool&)), this, SLOT(testCanDecodeURLs(const QDragMoveEvent*, bool&)));
	connect(m_tabs, SIGNAL(receivedDropEvent(QDropEvent*)), m_ki->docManager(), SLOT(openDroppedURLs(QDropEvent*)));
	connect(m_tabs, SIGNAL(receivedDropEvent(QWidget*, QDropEvent*)), this, SLOT(replaceLoadedURL(QWidget*, QDropEvent*)));
	connect(m_tabs->tabBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(tabContext(const QPoint&)));
	connect(m_tabs, SIGNAL(mouseDoubleClick()), m_ki->docManager(), SLOT(fileNew()));

	m_widgetStack->setCurrentWidget(m_emptyDropWidget); // there are no tabs, so show the DropWidget

	return m_widgetStack;
}

void Manager::closeTab(int index)
{
	QWidget *widget = m_tabs->widget(index);
	if(widget->inherits("KTextEditor::View")) {
		KTextEditor::View *view = static_cast<KTextEditor::View*>(widget);
		m_ki->docManager()->fileClose(view->document());
	}
}

KTextEditor::View* Manager::createTextView(KileDocument::TextInfo *info, int index)
{
	KTextEditor::Document *doc = info->getDoc();
	KTextEditor::View *view = static_cast<KTextEditor::View*>(info->createView(m_tabs, Q_NULLPTR));

	if(!view) {
		KMessageBox::error(m_ki->mainWindow(), i18n("Could not create an editor view."), i18n("Fatal Error"));
	}

	//install a key sequence recorder on the view
	view->focusProxy()->installEventFilter(new KileEditorKeySequence::Recorder(view, m_ki->editorKeySequenceManager()));

	// in the case of simple text documents, we mimic the behaviour of LaTeX documents
	if(info->getType() == KileDocument::Text) {
// 		view->focusProxy()->installEventFilter(m_ki->eventFilter());
	}

	//insert the view in the tab widget
	m_tabs->insertTab(index, view, QString());

	connect(view, SIGNAL(cursorPositionChanged(KTextEditor::View*, const KTextEditor::Cursor&)),
	        this, SIGNAL(cursorPositionChanged(KTextEditor::View*, const KTextEditor::Cursor&)));
	connect(view, SIGNAL(viewModeChanged(KTextEditor::View*, KTextEditor::View::ViewMode)),
	        this, SIGNAL(viewModeChanged(KTextEditor::View*, KTextEditor::View::ViewMode)));
	connect(view, SIGNAL(selectionChanged(KTextEditor::View*)),
	        this, SIGNAL(selectionChanged(KTextEditor::View*)));
//TODO KF5
// 	connect(view, SIGNAL(informationMessage(KTextEditor::View*,const QString&)), this, SIGNAL(informationMessage(KTextEditor::View*,const QString&)));
	connect(view, SIGNAL(viewModeChanged(KTextEditor::View*, KTextEditor::View::ViewMode)), this, SIGNAL(updateCaption()));
	connect(view, SIGNAL(viewInputModeChanged(KTextEditor::View*, enum KTextEditor::View::InputMode)), this, SIGNAL(updateModeStatus()));
	connect(view, SIGNAL(dropEventPass(QDropEvent *)), m_ki->docManager(), SLOT(openDroppedURLs(QDropEvent *)));
	connect(doc, SIGNAL(documentNameChanged(KTextEditor::Document*)), this, SLOT(updateTabTexts(KTextEditor::Document*)));
	connect(doc, SIGNAL(documentUrlChanged(KTextEditor::Document*)), this, SLOT(updateTabTexts(KTextEditor::Document*)));

	connect(view, SIGNAL(textInserted(KTextEditor::View*, const KTextEditor::Cursor&, const QString &)),
	        m_ki->codeCompletionManager(), SLOT(textInserted(KTextEditor::View*, const KTextEditor::Cursor&, const QString &)));

	// code completion
	KTextEditor::CodeCompletionInterface *completionInterface = qobject_cast<KTextEditor::CodeCompletionInterface*>(view);
	if(completionInterface) {
		completionInterface->setAutomaticInvocationEnabled(true);
	}

	// install a working text editor part popup dialog thingy
	installContextMenu(view);

	// delete the 'Configure Editor...' action
	delete view->actionCollection()->action("set_confdlg");

	// use Kile's save and save-as functions instead of the text editor's
	QAction *action = view->actionCollection()->action(KStandardAction::name(KStandardAction::Save));
	if(action) {
		KILE_DEBUG_MAIN << "   reconnect action 'file_save'...";
		action->disconnect(SIGNAL(triggered(bool)));
		connect(action, SIGNAL(triggered()), m_ki->docManager(), SLOT(fileSave()));
	}
	action = view->actionCollection()->action(KStandardAction::name(KStandardAction::SaveAs));
	if(action) {
		KILE_DEBUG_MAIN << "   reconnect action 'file_save_as'...";
		action->disconnect(SIGNAL(triggered(bool)));
		connect(action, SIGNAL(triggered()), m_ki->docManager(), SLOT(fileSaveAs()));
	}
	updateTabTexts(doc);
	m_widgetStack->setCurrentWidget(m_tabs); // there is at least one tab, so show the QTabWidget now
	// we do this twice as otherwise the tool tip for the first view did not appear (Qt issue ?)
	// (BUG 205245)
	updateTabTexts(doc);
	m_tabs->setCurrentIndex(m_tabs->indexOf(view));

	//activate the newly created view
	emit(activateView(view, false));
	emit(updateCaption());  //make sure the caption gets updated

	reflectDocumentModificationStatus(view->document(), false, KTextEditor::ModificationInterface::OnDiskUnmodified);

	emit(prepareForPart("Editor"));

	return view;
}

void Manager::installContextMenu(KTextEditor::View *view)
{
	QMenu *popupMenu = view->defaultContextMenu();

	if(popupMenu) {
		connect(popupMenu, SIGNAL(aboutToShow()), this, SLOT(onTextEditorPopupMenuRequest()));

		// install some more actions on it
		popupMenu->addSeparator();
		popupMenu->addAction(m_pasteAsLaTeXAction);
		popupMenu->addAction(m_convertToLaTeXAction);
		popupMenu->addSeparator();
		popupMenu->addAction(m_quickPreviewAction);

		// insert actions from user-defined latex menu
		KileMenu::UserMenu *usermenu = m_ki->userMenu();
		if ( usermenu ) {
			KILE_DEBUG_MAIN << "Insert actions from user-defined latex menu";
			popupMenu->addSeparator();
			foreach ( QAction *action, usermenu->contextMenuActions() ) {
				if ( action ) {
					popupMenu->addAction(action);
				}
				else {
					popupMenu->addSeparator();
				}
			}
		}

		view->setContextMenu(popupMenu);
	}
}

void Manager::tabContext(const QPoint& pos)
{
	KILE_DEBUG_MAIN << pos;
	const int tabUnderPos = m_tabs->tabBar()->tabAt(pos);
	if(tabUnderPos < 0) {
		KILE_DEBUG_MAIN << tabUnderPos;
		return;
	}

	KTextEditor::View *view = dynamic_cast<KTextEditor::View*>(m_tabs->widget(tabUnderPos));

	if(!view || !view->document()) {
		return;
	}

	QMenu tabMenu;

	tabMenu.addSection(m_ki->getShortName(view->document()));

	// 'action1' can become Q_NULLPTR if it belongs to a view that has been closed, for example
	QPointer<QAction> action1 = m_ki->mainWindow()->action("move_view_tab_left");
	action1->setData(qVariantFromValue(view));
	tabMenu.addAction(action1);

	QPointer<QAction> action2 = m_ki->mainWindow()->action("move_view_tab_right");
	action2->setData(qVariantFromValue(view));
	tabMenu.addAction(action2);

	tabMenu.addSeparator();

	QPointer<QAction> action3;
	if(view->document()->isModified()) {
		action3 = view->actionCollection()->action(KStandardAction::name(KStandardAction::Save));
		action3->setData(qVariantFromValue(view));
		tabMenu.addAction(action3);
	}

	QPointer<QAction> action4 = view->actionCollection()->action(KStandardAction::name(KStandardAction::SaveAs));
	action4->setData(qVariantFromValue(view));
	tabMenu.addAction(action4);

	QPointer<QAction> action5 = m_ki->mainWindow()->action("file_save_copy_as");
	action5->setData(qVariantFromValue(view));
	tabMenu.addAction(action5);

	tabMenu.addSeparator();

	QPointer<QAction> action6 = m_ki->mainWindow()->action("file_close");
	action6->setData(qVariantFromValue(view));
	tabMenu.addAction(action6);

	QPointer<QAction> action7 = m_ki->mainWindow()->action("file_close_all_others");
	action7->setData(qVariantFromValue(view));
	tabMenu.addAction(action7);
/*
	FIXME create proper actions which delete/add the current file without asking stupidly
	QAction* removeAction = m_ki->mainWindow()->action("project_remove");
	QAction* addAction = m_ki->mainWindow()->action("project_add");

	tabMenu.insertSeparator(addAction);
	tabMenu.addAction(addAction);
	tabMenu.addAction(removeAction);*/

	tabMenu.exec(m_tabs->tabBar()->mapToGlobal(pos));

	if(action1) {
		action1->setData(QVariant());
	}
	if(action2) {
		action2->setData(QVariant());
	}
	if(action3) {
		action3->setData(QVariant());
	}
	if(action4) {
		action4->setData(QVariant());
	}
	if(action5) {
		action5->setData(QVariant());
	}
	if(action6) {
		action6->setData(QVariant());
	}
	if(action7) {
		action7->setData(QVariant());
	}
}

void Manager::removeView(KTextEditor::View *view)
{
	if (view) {
		m_client->factory()->removeClient(view);

		const bool isActiveView = (KTextEditor::Editor::instance()->application()->activeMainWindow()->activeView() == view);
		m_tabs->removeTab(m_tabs->indexOf(view));

		emit(updateCaption());  //make sure the caption gets updated
		if (m_tabs->count() == 0) {
			m_ki->structureWidget()->clear();
			m_widgetStack->setCurrentWidget(m_emptyDropWidget); // there are no tabs left, so show
			                                                    // the DropWidget
		}

		emit(textViewClosed(view, isActiveView));
		delete view;
	}
	else{
		KILE_DEBUG_MAIN << "View should be removed but is Q_NULLPTR";
	}

}

KTextEditor::View *Manager::currentTextView() const
{
	KTextEditor::View *view = qobject_cast<KTextEditor::View*>(m_tabs->currentWidget());

	return view;
}

KTextEditor::View* Manager::textView(int i)
{
	return qobject_cast<KTextEditor::View*>(m_tabs->widget(i));
}

KTextEditor::View* Manager::textView(KileDocument::TextInfo *info)
{
	KTextEditor::Document *doc = info->getDoc();
	if(!doc) {
		return Q_NULLPTR;
	}
	for(int i = 0; i < m_tabs->count(); ++i) {
		KTextEditor::View *view = toTextView(m_tabs->widget(i));
		if(!view) {
			continue;
		}

		if(view->document() == doc) {
			return view;
		}
	}

	return Q_NULLPTR;
}

int Manager::textViewCount() const
{
	return m_tabs->count();
}

int Manager::getIndexOf(KTextEditor::View* view) const
{
	return m_tabs->indexOf(view);
}

unsigned int Manager::getTabCount() const {
	return m_tabs->count();
}

KTextEditor::View* Manager::switchToTextView(const QUrl &url, bool requestFocus)
{
	return switchToTextView(m_ki->docManager()->docFor(url), requestFocus);
}

KTextEditor::View* Manager::switchToTextView(KTextEditor::Document *doc, bool requestFocus)
{
	KTextEditor::View *view = Q_NULLPTR;
	if(doc) {
		if (doc->views().count() > 0) {
			view = doc->views().first();
			if(view) {
				switchToTextView(view, requestFocus);
			}
		}
	}
	return view;
}

void Manager::switchToTextView(KTextEditor::View *view, bool requestFocus)
{
	int index = m_tabs->indexOf(view);
	if(index < 0) {
		return;
	}
	m_tabs->setCurrentIndex(index);
	if(requestFocus) {
		focusTextView(view);
	}
}

void Manager::setTabIcon(QWidget *view, const QPixmap& icon)
{
	m_tabs->setTabIcon(m_tabs->indexOf(view), QIcon(icon));
}

void Manager::updateStructure(bool parse /* = false */, KileDocument::Info *docinfo /* = Q_NULLPTR */)
{
	if (!docinfo) {
		docinfo = m_ki->docManager()->getInfo();
	}

	if(docinfo) {
		m_ki->structureWidget()->update(docinfo, parse);
	}

	if(m_tabs->count() == 0) {
		m_ki->structureWidget()->clear();
	}
}

void Manager::gotoNextView()
{
	if(m_tabs->count() < 2) {
		return;
	}

	int cPage = m_tabs->currentIndex() + 1;
	if(cPage >= m_tabs->count()) {
		m_tabs->setCurrentIndex(0);
	}
	else {
		m_tabs->setCurrentIndex(cPage);
	}
}

void Manager::gotoPrevView()
{
	if(m_tabs->count() < 2) {
		return;
	}

	int cPage = m_tabs->currentIndex() - 1;
	if(cPage < 0) {
		m_tabs->setCurrentIndex(m_tabs->count() - 1);
	}
	else {
		m_tabs->setCurrentIndex(cPage);
	}
}

void Manager::moveTabLeft(QWidget *widget)
{
	if(m_tabs->count() < 2) {
		return;
	}

	// the 'data' property can be set by 'tabContext'
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	if(action) {
		QVariant var = action->data();
		if(!widget && var.isValid()) {
			// the action's 'data' property is cleared
			// when the context menu is destroyed
			widget = var.value<QWidget*>();
		}
	}
	if(!widget) {
		widget = currentTextView();
	}
	if(!widget) {
		return;
	}
	int currentIndex = m_tabs->indexOf(widget);
	int newIndex = (currentIndex == 0 ? m_tabs->count() - 1 : currentIndex - 1);
	m_tabs->tabBar()->moveTab(currentIndex, newIndex);
}

void Manager::moveTabRight(QWidget *widget)
{
	if(m_tabs->count() < 2) {
		return;
	}

	// the 'data' property can be set by 'tabContext'
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	if(action) {
		QVariant var = action->data();
		if(!widget && var.isValid()) {
			// the action's 'data' property is cleared
			// when the context menu is destroyed
			widget = var.value<QWidget*>();
		}
	}
	if(!widget) {
		widget = currentTextView();
	}
	if(!widget) {
		return;
	}
	int currentIndex = m_tabs->indexOf(widget);
	int newIndex = (currentIndex == m_tabs->count() - 1 ? 0 : currentIndex + 1);
	m_tabs->tabBar()->moveTab(currentIndex, newIndex);
}

void Manager::reflectDocumentModificationStatus(KTextEditor::Document *doc,
                                                bool isModified,
                                                KTextEditor::ModificationInterface::ModifiedOnDiskReason reason)
{

	QPixmap icon;
	if(reason == KTextEditor::ModificationInterface::OnDiskUnmodified && isModified) { //nothing
		icon = SmallIcon("modified"); // This icon is taken from Kate. Therefore
		                              // our thanks go to the authors of Kate.
	}
	else if(reason == KTextEditor::ModificationInterface::OnDiskModified
	     || reason == KTextEditor::ModificationInterface::OnDiskCreated) { //dirty file
		icon = SmallIcon("modonhd"); // This icon is taken from Kate. Therefore
		                             // our thanks go to the authors of Kate.
	}
	else if(reason == KTextEditor::ModificationInterface::OnDiskDeleted) { //file deleted
		icon = SmallIcon("process-stop");
	}
	else if(m_ki->extensions()->isScriptFile(doc->url())) {
		icon = SmallIcon("js");
	}
	else {
		icon = KIO::pixmapForUrl(doc->url(), 0, KIconLoader::Small);
	}

	const QList<KTextEditor::View*> &viewsList = doc->views();
	for(QList<KTextEditor::View*>::const_iterator i = viewsList.begin(); i != viewsList.end(); ++i) {
		setTabIcon(*i, icon);
	}
}

/**
 * Adds/removes the "Convert to LaTeX" entry in Kate's popup menu according to the selection.
 */
void Manager::onTextEditorPopupMenuRequest()
{
	KTextEditor::View *view = currentTextView();
	if(!view) {
		return;
	}

	const QString quickPreviewSelection = i18n("&QuickPreview Selection");
	const QString quickPreviewEnvironment = i18n("&QuickPreview Environment");
	const QString quickPreviewMath = i18n("&QuickPreview Math");

	// Setting up the "QuickPreview selection" entry
	if(view->selection()) {
		m_quickPreviewAction->setText(quickPreviewSelection);
		m_quickPreviewAction->setEnabled(true);

	}
	else if(m_ki->editorExtension()->hasMathgroup(view)) {
		m_quickPreviewAction->setText(quickPreviewMath);
		m_quickPreviewAction->setEnabled(true);
	}
	else if(m_ki->editorExtension()->hasEnvironment(view)) {
		m_quickPreviewAction->setText(quickPreviewEnvironment);
		m_quickPreviewAction->setEnabled(true);
	}
	else {
		m_quickPreviewAction->setText(quickPreviewSelection);
		m_quickPreviewAction->setEnabled(false);
	}


	// Setting up the "Convert to LaTeX" entry
	m_convertToLaTeXAction->setEnabled(view->selection());

	// Setting up the "Paste as LaTeX" entry
	QClipboard *clipboard = QApplication::clipboard();
	if(clipboard) {
		m_pasteAsLaTeXAction->setEnabled(!clipboard->text().isEmpty());
	}
}

void Manager::convertSelectionToLaTeX(void)
{
	KTextEditor::View *view = currentTextView();

	if(Q_NULLPTR == view)
		return;

	KTextEditor::Document *doc = view->document();

	if(Q_NULLPTR == doc)
		return;

	// Getting the selection
	KTextEditor::Range range = view->selectionRange();
	uint selStartLine = range.start().line(), selStartCol = range.start().column();
	uint selEndLine = range.end().line(), selEndCol = range.start().column();

	/* Variable to "restore" the selection after replacement: if {} was selected,
	   we increase the selection of two characters */
	uint newSelEndCol;

	PlainToLaTeXConverter cvt;

	// "Notifying" the editor that what we're about to do must be seen as ONE operation
	KTextEditor::Document::EditingTransaction transaction(doc);

	// Processing the first line
	int firstLineLength;
	if(selStartLine != selEndLine)
		firstLineLength = doc->lineLength(selStartLine);
	else
		firstLineLength = selEndCol;
	QString firstLine = doc->text(KTextEditor::Range(selStartLine, selStartCol, selStartLine, firstLineLength));
	QString firstLineCvt = cvt.ConvertToLaTeX(firstLine);
	doc->removeText(KTextEditor::Range(selStartLine, selStartCol, selStartLine, firstLineLength));
	doc->insertText(KTextEditor::Cursor(selStartLine, selStartCol), firstLineCvt);
	newSelEndCol = selStartCol + firstLineCvt.length();

	// Processing the intermediate lines
	for(uint nLine = selStartLine + 1 ; nLine < selEndLine ; ++nLine) {
		QString line = doc->line(nLine);
		QString newLine = cvt.ConvertToLaTeX(line);
		doc->removeLine(nLine);
		doc->insertLine(nLine, newLine);
	}

	// Processing the final line
	if(selStartLine != selEndLine) {
		QString lastLine = doc->text(KTextEditor::Range(selEndLine, 0, selEndLine, selEndCol));
		QString lastLineCvt = cvt.ConvertToLaTeX(lastLine);
		doc->removeText(KTextEditor::Range(selEndLine, 0, selEndLine, selEndCol));
		doc->insertText(KTextEditor::Cursor(selEndLine, 0), lastLineCvt);
		newSelEndCol = lastLineCvt.length();
	}

	// End of the "atomic edit operation"
	transaction.finish();

	view->setSelection(KTextEditor::Range(selStartLine, selStartCol, selEndLine, newSelEndCol));
}

/**
 * Pastes the clipboard's contents as LaTeX (ie. % -> \%, etc.).
 */
void Manager::pasteAsLaTeX(void)
{
	KTextEditor::View *view = currentTextView();

	if(!view) {
		return;
	}

	KTextEditor::Document *doc = view->document();

	if(!doc) {
		return;
	}

	// Getting a proper text insertion point BEFORE the atomic editing operation
	uint cursorLine, cursorCol;
	if(view->selection()) {
		KTextEditor::Range range = view->selectionRange();
		cursorLine = range.start().line();
		cursorCol = range.start().column();
	} else {
		KTextEditor::Cursor cursor = view->cursorPosition();
		cursorLine = cursor.line();
		cursorCol = cursor.column();
	}

	// "Notifying" the editor that what we're about to do must be seen as ONE operation
	KTextEditor::Document::EditingTransaction transaction(doc);

	// If there is a selection, one must remove it
	if(view->selection()) {
		doc->removeText(view->selectionRange());
	}

	PlainToLaTeXConverter cvt;
	QString toPaste = cvt.ConvertToLaTeX(QApplication::clipboard()->text());
	doc->insertText(KTextEditor::Cursor(cursorLine, cursorCol), toPaste);

	// End of the "atomic edit operation"
	transaction.finish();
}

void Manager::quickPreviewPopup()
{
	KTextEditor::View *view = currentTextView();
	if(!view) {
		return;
	}

	if(view->selection()) {
		emit(startQuickPreview(KileTool::qpSelection));
	}
	else if(m_ki->editorExtension()->hasMathgroup(view)) {
		emit(startQuickPreview(KileTool::qpMathgroup));
	}
	else if(m_ki->editorExtension()->hasEnvironment(view)) {
		emit(startQuickPreview(KileTool::qpEnvironment));
	}
}

void Manager::testCanDecodeURLs(const QDragEnterEvent *e, bool &accept)
{
	accept = e->mimeData()->hasUrls(); // only accept URL drops
}

void Manager::testCanDecodeURLs(const QDragMoveEvent *e, bool &accept)
{
	accept = e->mimeData()->hasUrls(); // only accept URL drops
}

//TODO KF5
void Manager::replaceLoadedURL(QWidget *w, QDropEvent *e)
{
	QList<QUrl> urls = e->mimeData()->urls();
	if (urls.isEmpty()) {
		return;
	}
	int index = m_tabs->indexOf(w);
	KileDocument::Extensions *extensions = m_ki->extensions();
	bool hasReplacedTab = false;
	for(QList<QUrl>::iterator i = urls.begin(); i != urls.end(); ++i) {
		QUrl url = *i;
		if(extensions->isProjectFile(url)) {
			m_ki->docManager()->projectOpen(url);
		}
		else if(!hasReplacedTab) {
			closeTab(index);
			m_ki->docManager()->fileOpen(url, QString(), index);
			hasReplacedTab = true;
		}
		else {
			m_ki->docManager()->fileOpen(url);
		}
	}
}

void Manager::updateTabTexts(KTextEditor::Document* changedDoc)
{
	const QList<KTextEditor::View*> &viewsList = changedDoc->views();
	for(QList<KTextEditor::View*>::const_iterator i = viewsList.begin(); i != viewsList.end(); ++i) {
		QString documentName = changedDoc->documentName();
		if(documentName.isEmpty()) {
			documentName = i18n("Untitled");
		}
		const int viewIndex = m_tabs->indexOf(*i);
		m_tabs->setTabText(viewIndex, documentName);
		m_tabs->setTabToolTip(viewIndex, changedDoc->url().toString());
	}
}

void Manager::currentViewChanged(int index)
{
	QWidget *activatedWidget = m_tabs->widget(index);
	if(!activatedWidget) {
		return;
	}
	emit currentViewChanged(activatedWidget);
	KTextEditor::View *view = dynamic_cast<KTextEditor::View*>(activatedWidget);
	if(view) {
		emit textViewActivated(view);
	}
}

DropWidget::DropWidget(QWidget *parent, const char *name, Qt::WindowFlags f) : QWidget(parent, f)
{
	setObjectName(name);
	setAcceptDrops(true);
}

DropWidget::~DropWidget()
{
}

void DropWidget::dragEnterEvent(QDragEnterEvent *e)
{
	bool b;
	emit testCanDecode(e, b);
	if(b) {
		e->acceptProposedAction();
	}
}

void DropWidget::dropEvent(QDropEvent *e)
{
	emit receivedDropEvent(e);
}

void DropWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
	Q_UNUSED(e);
	emit mouseDoubleClick();
}

void Manager::installEventFilter(KTextEditor::View *view, QObject *eventFilter)
{
	QWidget *focusProxy = view->focusProxy();
	if(focusProxy) {
		focusProxy->installEventFilter(eventFilter);
	}
	else {
		view->installEventFilter(eventFilter);
	}
}

void Manager::removeEventFilter(KTextEditor::View *view, QObject *eventFilter)
{
	QWidget *focusProxy = view->focusProxy();
	if(focusProxy) {
		focusProxy->removeEventFilter(eventFilter);
	}
	else {
		view->removeEventFilter(eventFilter);
	}
}

//BEGIN ViewerPart methods

void Manager::createViewerPart(KActionCollection *actionCollection)
{
	m_viewerPart = Q_NULLPTR;
#if LIVEPREVIEW_AVAILABLE
	KPluginLoader pluginLoader(OKULAR_LIBRARY_NAME);
	KPluginFactory *factory = pluginLoader.factory();
	if (!factory) {
		KILE_DEBUG_MAIN << "Could not find the Okular library.";
		m_viewerPart = Q_NULLPTR;
		return;
	}
	else {
		QVariantList argList;
		argList << "ViewerWidget" << "ConfigFileName=kile-livepreview-okularpartrc";
		m_viewerPart = factory->create<KParts::ReadOnlyPart>(this, argList);
		Okular::ViewerInterface *viewerInterface = dynamic_cast<Okular::ViewerInterface*>(m_viewerPart.data());
		if(!viewerInterface) {
			// OkularPart doesn't provide the ViewerInterface
			delete m_viewerPart;
			m_viewerPart = Q_NULLPTR;
			return;
		}
		viewerInterface->setWatchFileModeEnabled(false);
		viewerInterface->setShowSourceLocationsGraphically(true);
		connect(m_viewerPart, SIGNAL(openSourceReference(const QString&, int, int)), this, SLOT(handleActivatedSourceReference(const QString&, int, int)));

		QAction *paPrintCompiledDocument = actionCollection->addAction(KStandardAction::Print, "print_compiled_document", m_viewerPart, SLOT(slotPrint()));
		paPrintCompiledDocument->setText(i18n("Print Compiled Document..."));
		paPrintCompiledDocument->setShortcut(QKeySequence());
		paPrintCompiledDocument->setEnabled(false);
		connect(m_viewerPart, SIGNAL(enablePrintAction(bool)), paPrintCompiledDocument, SLOT(setEnabled(bool)));
		QAction *printPreviewAction = m_viewerPart->actionCollection()->action("file_print_preview");
		if(printPreviewAction) {
			printPreviewAction->setText(i18n("Print Preview For Compiled Document..."));
		}
	}
#else
	Q_UNUSED(actionCollection);
#endif
}

void Manager::setupViewerPart(QSplitter *splitter)
{
#if LIVEPREVIEW_AVAILABLE
	if(!m_viewerPart) {
		return;
	}
	if(KileConfig::showDocumentViewerInExternalWindow()) {
		if(m_viewerPartWindow && m_viewerPart->widget()->window() == m_viewerPartWindow) { // nothing to be done
			return;
		}
		m_viewerPartWindow = new DocumentViewerWindow();
		m_viewerPartWindow->setObjectName("KileDocumentViewerWindow");
		m_viewerPartWindow->setCentralWidget(m_viewerPart->widget());
		m_viewerPartWindow->setAttribute(Qt::WA_DeleteOnClose, false);
		m_viewerPartWindow->setAttribute(Qt::WA_QuitOnClose, false);
		connect(m_viewerPartWindow, SIGNAL(visibilityChanged(bool)),
		        this, SIGNAL(documentViewerWindowVisibilityChanged(bool)));

		m_viewerPartWindow->setWindowTitle(i18n("Document Viewer"));
		m_viewerPartWindow->applyMainWindowSettings(KSharedConfig::openConfig()->group("KileDocumentViewerWindow"));
	}
	else {
		if(m_viewerPart->widget()->parent() && m_viewerPart->widget()->parent() != m_viewerPartWindow) { // nothing to be done
			return;
		}
		splitter->addWidget(m_viewerPart->widget()); // remove it from the window first!
		destroyDocumentViewerWindow();
	}
#else
	Q_UNUSED(splitter);
#endif
}

void Manager::destroyDocumentViewerWindow()
{
	if(!m_viewerPartWindow) {
		return;
	}

	KConfigGroup group(KSharedConfig::openConfig(), "KileDocumentViewerWindow");
	m_viewerPartWindow->saveMainWindowSettings(group);
	// we don't want it to influence the document viewer visibility setting as
	// this is a forced close
	disconnect(m_viewerPartWindow, SIGNAL(visibilityChanged(bool)),
	           this, SIGNAL(documentViewerWindowVisibilityChanged(bool)));
	m_viewerPartWindow->hide();
	delete m_viewerPartWindow;
	m_viewerPartWindow = Q_NULLPTR;
}

void Manager::handleActivatedSourceReference(const QString& absFileName, int line, int col)
{
	KILE_DEBUG_MAIN << "absFileName:" << absFileName << "line:" << line << "column:" << col;
	QString fileName;
	KileDocument::TextInfo *textInfo = m_ki->docManager()->textInfoFor(absFileName);
	if(!textInfo) {
		m_ki->docManager()->fileOpen(absFileName);
		textInfo = m_ki->docManager()->textInfoFor(absFileName);
		if(!textInfo) {
			return;
		}
	}
	KTextEditor::View *view = textView(textInfo);
	if(!view) {
		return;
	}
	view->setCursorPosition(KTextEditor::Cursor(line, col));
	switchToTextView(view, true);
}

void Manager::setDocumentViewerVisible(bool b)
{
	if(!m_viewerPart) {
		return;
	}
	KileConfig::setShowDocumentViewer(b);
	if(m_viewerPartWindow) {
		m_viewerPartWindow->setVisible(b);
	}
	m_viewerPart->widget()->setVisible(b);
}

bool Manager::isViewerPartShown() const
{
	if(!m_viewerPart) {
		return false;
	}

	if(m_viewerPartWindow) {
		return !m_viewerPartWindow->isHidden();
	}
	else {
		return !m_viewerPart->widget()->isHidden();
	}
}

bool Manager::openInDocumentViewer(const QUrl &url)
{
#if LIVEPREVIEW_AVAILABLE
	Okular::ViewerInterface *v = dynamic_cast<Okular::ViewerInterface*>(m_viewerPart.data());
	if(!v) {
		return false;
	}
	bool r = m_viewerPart->openUrl(url);
	v->clearLastShownSourceLocation();
	return r;
#else
	Q_UNUSED(url);
	return false;
#endif
}

void Manager::showSourceLocationInDocumentViewer(const QString& fileName, int line, int column)
{
#if LIVEPREVIEW_AVAILABLE
	Okular::ViewerInterface *v = dynamic_cast<Okular::ViewerInterface*>(m_viewerPart.data());
	if(v) {
		v->showSourceLocation(fileName, line, column, true);
	}
#else
	Q_UNUSED(fileName);
	Q_UNUSED(line);
	Q_UNUSED(column);
#endif
}

void Manager::setLivePreviewModeForDocumentViewer(bool b)
{
#if LIVEPREVIEW_AVAILABLE
	Okular::ViewerInterface *viewerInterface = dynamic_cast<Okular::ViewerInterface*>(m_viewerPart.data());
	if(viewerInterface) {
		if(b) {
			viewerInterface->setWatchFileModeEnabled(false);
		}
		else {
			viewerInterface->setWatchFileModeEnabled(KileConfig::watchFileForDocumentViewer());

		}
	}
#else
	Q_UNUSED(b);
#endif
}

//END ViewerPart methods

bool Manager::viewForLocalFilePresent(const QString& localFileName)
{
	for(int i = 0; i < m_tabs->count(); ++i) {
		KTextEditor::View *view = toTextView(m_tabs->widget(i));
		if(!view) {
			continue;
		}
		if(view->document()->url().toLocalFile() == localFileName) {
			return true;
		}
	}
	return false;
}

}

void focusTextView(KTextEditor::View *view)
{
	// we work around a potential Qt bug here which can result in dead keys
	// being treated as 'alive' keys in some circumstances, probably when 'setFocus'
	// is called when the widget hasn't been shown yet (see bug 269590)
	QTimer::singleShot(0, view, SLOT(setFocus()));
}

