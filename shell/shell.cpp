/*
    SPDX-FileCopyrightText: 2002 Wilco Greven <greven@kde.org>
    SPDX-FileCopyrightText: 2002 Chris Cheney <ccheney@cheney.cx>
    SPDX-FileCopyrightText: 2003 Benjamin Meyer <benjamin@csh.rit.edu>
    SPDX-FileCopyrightText: 2003-2004 Christophe Devriese <Christophe.Devriese@student.kuleuven.ac.be>
    SPDX-FileCopyrightText: 2003 Laurent Montel <montel@kde.org>
    SPDX-FileCopyrightText: 2003-2004 Albert Astals Cid <aacid@kde.org>
    SPDX-FileCopyrightText: 2003 Luboš Luňák <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2003 Malcolm Hunter <malcolm.hunter@gmx.co.uk>
    SPDX-FileCopyrightText: 2004 Dominique Devriese <devriese@kde.org>
    SPDX-FileCopyrightText: 2004 Dirk Mueller <mueller@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shell.h"

// qt/kde includes
#include <KActionCollection>
#include <KConfigGroup>
#include <KIO/Global>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>
#include <KRecentFilesAction>
#include <KSharedConfig>
#include <KStandardAction>
#if !defined(Q_OS_WIN) && !defined(Q_OS_OSX) && !defined(Q_OS_HAIKU)
#include <KStartupInfo>
#include <KWindowInfo>
#endif
#include <KToggleFullScreenAction>
#include <KToolBar>
#include <KUrlMimeData>
#include <KWindowSystem>
#include <KXMLGUIFactory>
#include <QApplication>
#if HAVE_DBUS
#include <QDBusConnection>
#endif // HAVE_DBUS
#include <QDragMoveEvent>
#include <QFileDialog>
#include <QJsonArray>
#include <QMenuBar>
#include <QMimeData>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

// local includes
#include "../interfaces/viewerinterface.h"
#include "kdocumentviewer.h"
#include "shellutils.h"

static const char *shouldShowMenuBarComingFromFullScreen = "shouldShowMenuBarComingFromFullScreen";

static const char *const SESSION_URL_KEY = "Urls";
static const char *const SESSION_TAB_KEY = "ActiveTab";

static inline QString DesktopEntryGroupKey()
{
    return QStringLiteral("Desktop Entry");
}
static inline QString RecentFilesGroupKey()
{
    return QStringLiteral("Recent Files");
}

class ResizableStackedWidget : public QStackedWidget
{
    Q_OBJECT

public:
    QSize sizeHint() const override
    {
        return currentWidget()->sizeHint();
    }
    QSize minimumSizeHint() const override
    {
        return currentWidget()->minimumSizeHint();
    }
};

Shell::Shell(const QString &serializedOptions)
    : KParts::MainWindow()
    , m_menuBarWasShown(true)
    , m_isValid(true)
{
    setObjectName(QStringLiteral("okular::Shell#"));
    setContextMenuPolicy(Qt::NoContextMenu);
    // otherwise .rc file won't be found by unit test
    setComponentName(QStringLiteral("charta"), QString());
    // set the shell's ui resource file
    setXMLFile(QStringLiteral("shell.rc"));
    m_fileformatsscanned = false;
    m_showMenuBarAction = nullptr;
    // this routine will find and load our Part.  it finds the Part by
    // name which is a bad idea usually.. but it's alright in this
    // case since our Part is made for this Shell

    const auto result = KPluginFactory::loadFactory(KPluginMetaData(QStringLiteral("kf6/parts/okularpart")));

    if (!result) {
        // if we couldn't find our Part, we exit since the Shell by
        // itself can't do anything useful
        m_isValid = false;
        KMessageBox::error(this, i18n("Unable to find the Charta component: %1", result.errorString));
        return;
    } else {
        m_partFactory = result.plugin;
    }

    // now that the Part plugin is loaded, create the part
    KParts::ReadWritePart *const firstPart = m_partFactory->create<KParts::ReadWritePart>(this);
    if (firstPart) {
        // Setup the central widget
        m_centralStackedWidget = new ResizableStackedWidget();
        setCentralWidget(m_centralStackedWidget);

        // Setup the welcome screen
        m_welcomeScreen = new WelcomeScreen(this);
        connect(m_welcomeScreen, &WelcomeScreen::openClicked, this, &Shell::fileOpen);
        connect(m_welcomeScreen, &WelcomeScreen::createClicked, this, &Shell::createNewDocument);
        connect(m_welcomeScreen, &WelcomeScreen::builderClicked, this, &Shell::openDocumentBuilder);
        connect(m_welcomeScreen, &WelcomeScreen::recentItemClicked, this, [this](const QUrl &url) { openUrl(url); });
        connect(m_welcomeScreen, &WelcomeScreen::forgetRecentItem, this, &Shell::forgetRecentItem);
        m_centralStackedWidget->addWidget(m_welcomeScreen);

        m_welcomeScreen->installEventFilter(this);

        // Setup tab bar
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabsClosable(true);
        m_tabWidget->setElideMode(Qt::ElideRight);
        m_tabWidget->tabBar()->hide();
        m_tabWidget->setDocumentMode(true);
        m_tabWidget->setMovable(true);
        m_tabWidget->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { background: #171c24; border: 0; }"
            "QTabBar { background: #171c24; }"
            "QTabBar::tab { color: #9ba8b5; background: #222a34; border: 1px solid #343f4c; border-bottom: 0; border-top-left-radius: 7px; border-top-right-radius: 7px; padding: 6px 12px; margin: 3px 3px 0 0; }"
            "QTabBar::tab:selected { color: #f0f4f7; background: #303a46; border-color: #4a5867; }"
            "QTabBar::tab:hover:!selected { color: #d4dde5; background: #29333e; }"));

        m_tabWidget->setAcceptDrops(true);
        m_tabWidget->tabBar()->installEventFilter(this);

        m_centralStackedWidget->addWidget(m_tabWidget);

        connect(m_tabWidget, &QTabWidget::currentChanged, this, &Shell::setActiveTab);
        connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &Shell::closeTab);
        connect(m_tabWidget->tabBar(), &QTabBar::tabMoved, this, &Shell::moveTabData);

        // then, setup our actions
        setupActions();
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &QObject::deleteLater);
        // and integrate the part's GUI with the shell's
        setupGUI(Keys | ToolBar | Save);
        setStyleSheet(QStringLiteral(
            "QMenuBar { color: #aeb9c4; background: #171c24; border: 0; padding: 2px 5px; spacing: 3px; }"
            "QMenuBar::item { background: transparent; border-radius: 4px; padding: 4px 8px; }"
            "QMenuBar::item:selected { color: #f1f5f8; background: #303a46; }"
            "QMenu { color: #e2e8ee; background: #202630; border: 1px solid #3b4653; padding: 4px; }"
            "QMenu::item { border-radius: 4px; padding: 5px 24px 5px 22px; }"
            "QMenu::item:selected { color: #ffffff; background: #16414c; }"
            "QMenu::separator { height: 1px; margin: 4px 8px; background: #3b4653; }"));

        m_tabs.append(TabState(firstPart));
        m_tabWidget->addTab(firstPart->widget(), QString()); // triggers setActiveTab that calls createGUI( part )

        connectPart(firstPart);

        readSettings();

        m_unique = ShellUtils::unique(serializedOptions);
#if HAVE_DBUS
        if (m_unique) {
            m_unique = QDBusConnection::sessionBus().registerService(QStringLiteral("org.charta.reader"));
            if (!m_unique) {
                KMessageBox::information(this, i18n("There is already a unique Charta instance running. This instance won't be the unique one."));
            }
        } else {
            // TODO When porting to KF7 Remove
            // PID is not unique in containers and "-" in the name violates D-Bus naming conventions.
            // Was left for compatibility with 3rd-party scripts.
            QString serviceName = QStringLiteral("org.charta.reader-") + QString::number(qApp->applicationPid());
            QDBusConnection::sessionBus().registerService(serviceName);

            QDBusConnection::sessionBus().registerService(ShellUtils::currentProcessDbusName());
        }
        if (ShellUtils::noRaise(serializedOptions)) {
            setAttribute(Qt::WA_ShowWithoutActivating);
        }

        {
            const QString editorCmd = ShellUtils::editorCmd(serializedOptions);
            if (!editorCmd.isEmpty()) {
                QMetaObject::invokeMethod(firstPart, "setEditorCmd", Q_ARG(QString, editorCmd));
            }
        }

        QDBusConnection::sessionBus().registerObject(QStringLiteral("/chartashell"), this, QDBusConnection::ExportScriptableSlots);
#endif // HAVE_DBUS

        // Make sure that the welcome scren is visible on startup.
        showWelcomeScreen();
    } else {
        m_isValid = false;
        KMessageBox::error(this, i18n("Unable to find the Charta component."));
    }

    connect(guiFactory(), &KXMLGUIFactory::shortcutsSaved, this, &Shell::reloadAllXML);
}

void Shell::reloadAllXML()
{
    for (const TabState &tab : std::as_const(m_tabs)) {
        tab.part->reloadXML();
    }
}

void Shell::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && window()->isFullScreen()) {
        setFullScreen(false);
    }
}

bool Shell::eventFilter(QObject *obj, QEvent *event)
{
    QDragMoveEvent *dmEvent = dynamic_cast<QDragMoveEvent *>(event);
    if (dmEvent) {
        bool accept = dmEvent->mimeData()->hasUrls();
        event->setAccepted(accept);
        return accept;
    }

    QDropEvent *dEvent = dynamic_cast<QDropEvent *>(event);
    if (dEvent) {
        const QList<QUrl> list = KUrlMimeData::urlsFromMimeData(dEvent->mimeData());
        handleDroppedUrls(list);
        dEvent->setAccepted(true);
        return true;
    }

    // Handle middle button click events on the tab bar
    if (obj == m_tabWidget->tabBar() && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mEvent = static_cast<QMouseEvent *>(event);
        if (mEvent->button() == Qt::MiddleButton) {
            int tabIndex = m_tabWidget->tabBar()->tabAt(mEvent->pos());
            if (tabIndex != -1) {
                closeTab(tabIndex);
                return true;
            }
        }
    }
    return KParts::MainWindow::eventFilter(obj, event);
}

bool Shell::isValid() const
{
    return m_isValid;
}

void Shell::showOpenRecentMenu()
{
    m_recent->menu()->popup(QCursor::pos());
}

Shell::~Shell()
{
    if (!m_tabs.empty()) {
        writeSettings();
        for (const TabState &tab : std::as_const(m_tabs)) {
            tab.part->closeUrl(false);
        }
        m_tabs.clear();
    }
#if HAVE_DBUS
    if (m_unique) {
        QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.charta.reader"));
    }
#endif // HAVE_DBUS

    delete m_tabWidget;
}

// Open a new document if we have space for it
// This can hang if called on a unique instance and openUrl pops a messageBox
bool Shell::openDocument(const QUrl &url, const QString &serializedOptions)
{
    if (m_tabs.size() <= 0) {
        return false;
    }

    hideWelcomeScreen();

    KParts::ReadWritePart *const part = m_tabs[0].part;

    // Return false if we can't open new tabs and the only part is occupied
    if (!qobject_cast<Okular::ViewerInterface *>(part)->openNewFilesInTabs() && !part->url().isEmpty() && !ShellUtils::unique(serializedOptions)) {
        return false;
    }

    openUrl(url, serializedOptions);

    return true;
}

bool Shell::openDocument(const QString &urlString, const QString &serializedOptions)
{
    return openDocument(QUrl(urlString), serializedOptions);
}

void Shell::openNewlySignedFile(const QString &path, int pageNumber)
{
    // for now, this function just applies the "replace current document"
    // strategy for opening.
    // Given signing is slightly closer to annotating and saving, the over
    // all user experience should not be that different
    // The fact that we get a different file out of saving is a bit of a
    // implementation  detail that shouldn't leak that much onto the users
    QUrl url = QUrl::fromLocalFile(path);
    url.setFragment(QStringLiteral("page=%1").arg(pageNumber));

    const int activeTab = m_tabWidget->currentIndex();
    KParts::ReadWritePart *const activePart = m_tabs[activeTab].part;
    activePart->closeUrl(false);
    activePart->openUrl(url);
}

bool Shell::canOpenDocs(int numDocs, int desktop)
{
    if (m_tabs.size() <= 0 || numDocs <= 0 || m_unique) {
        return false;
    }

    KParts::ReadWritePart *const part = m_tabs[0].part;
    const bool allowTabs = qobject_cast<Okular::ViewerInterface *>(part)->openNewFilesInTabs();

    if (!allowTabs && (numDocs > 1 || !part->url().isEmpty())) {
        return false;
    }

#if !defined(Q_OS_WIN) && !defined(Q_OS_OSX) && !defined(Q_OS_HAIKU)
    const KWindowInfo winfo(window()->effectiveWinId(), NET::WMDesktop);
    if (winfo.desktop() != desktop) {
        return false;
    }
#else
    Q_UNUSED(desktop);
#endif

    return true;
}

void Shell::openUrl(const QUrl &url, const QString &serializedOptions)
{
    hideWelcomeScreen();

    const int activeTab = m_tabWidget->currentIndex();
    KParts::ReadWritePart *const activePart = m_tabs[activeTab].part;
    if (!activePart->url().isEmpty()) {
        if (m_unique) {
            applyOptionsToPart(activePart, serializedOptions);
            activePart->openUrl(url);
        } else {
            if (qobject_cast<Okular::ViewerInterface *>(activePart)->openNewFilesInTabs()) {
                openNewTab(url, serializedOptions);
            } else {
                Shell *newShell = new Shell(serializedOptions);
                newShell->show();
                // The new shell performs the open, so the untitled handoff has to travel with it
                // (otherwise a brand-new document would be treated as a file that has a location)
                newShell->m_untitledPath = m_untitledPath;
                m_untitledPath.clear();
                newShell->openUrl(url, serializedOptions);
            }
        }
    } else {
        m_tabWidget->setTabText(activeTab, url.fileName());
        m_tabWidget->setTabToolTip(activeTab, url.fileName());

        const bool untitled = !m_untitledPath.isEmpty(); // read before applyOptionsToPart() clears it
        applyOptionsToPart(activePart, serializedOptions);
        bool openOk = activePart->openUrl(url);
        const bool isstdin = url.fileName() == QLatin1String("-") || url.scheme() == QLatin1String("fd");
        if (!isstdin) {
            if (openOk) {
                // A brand-new document lives in a temporary file: it is not a file the user has, so
                // it must not appear in the recent files
                if (!untitled) {
                    m_recent->addUrl(url);
                }
            } else {
                m_recent->removeUrl(url);
                closeTab(activeTab);
            }
        }
    }
}

void Shell::closeUrl()
{
    closeTab(m_tabWidget->currentIndex());

    // When closing the current tab two things can happen:
    //  * the focus was on the tab
    //  * the focus was somewhere in the toolbar
    // we don't have other places that accept focus
    //  * If it was on the tab, logic says it should go back to the next current tab
    //  * If it was on the toolbar, we could leave it there, but since we redo the menus/toolbars for the new tab, it gets kind of lost
    //    so it's easier to set it to the next current tab which also makes sense as consistency
    if (m_tabWidget->count() >= 0) {
        KParts::ReadWritePart *const newPart = m_tabs[m_tabWidget->currentIndex()].part;
        newPart->widget()->setFocus();
    }
}

void Shell::readSettings()
{
    readRecentFilesSettings();

    const KConfigGroup group = KSharedConfig::openConfig()->group(DesktopEntryGroupKey());
    bool fullScreen = group.readEntry("FullScreen", false);
    setFullScreen(fullScreen);

    if (fullScreen) {
        m_menuBarWasShown = group.readEntry(shouldShowMenuBarComingFromFullScreen, true);
    }
}

void Shell::writeSettings()
{
    saveRecents();

    KConfigGroup group = KSharedConfig::openConfig()->group(DesktopEntryGroupKey());
    group.writeEntry("FullScreen", m_fullScreenAction->isChecked());
    if (m_fullScreenAction->isChecked()) {
        group.writeEntry(shouldShowMenuBarComingFromFullScreen, m_menuBarWasShown);
    }
    KSharedConfig::openConfig()->sync();
}

void Shell::saveRecents()
{
    m_recent->saveEntries(KSharedConfig::openConfig()->group(RecentFilesGroupKey()));
}

void Shell::setupActions()
{
    KStandardAction::open(this, SLOT(fileOpen()), actionCollection());
    KStandardAction::openNew(this, SLOT(createNewDocument()), actionCollection());

    QAction *builderAction = actionCollection()->addAction(QStringLiteral("file_document_builder"));
    builderAction->setText(i18n("Document &Builder…"));
    builderAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    builderAction->setWhatsThis(i18n("Assemble a new document from pages of other documents."));
    actionCollection()->setDefaultShortcut(builderAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(builderAction, &QAction::triggered, this, &Shell::openDocumentBuilder);

    m_recent = KStandardAction::openRecent(this, SLOT(openUrl(QUrl)), actionCollection());
    m_recent->setToolBarMode(KRecentFilesAction::MenuMode);
    connect(m_recent, &QAction::triggered, this, &Shell::showOpenRecentMenu);
    connect(m_recent, &KRecentFilesAction::recentListCleared, this, &Shell::refreshRecentsOnWelcomeScreen);
    connect(m_welcomeScreen, &WelcomeScreen::forgetAllRecents, m_recent, &KRecentFilesAction::clear);
    m_recent->setToolTip(i18n("Click to open a file\nClick and hold to open a recent file"));
    m_recent->setWhatsThis(i18n("<b>Click</b> to open a file or <b>Click and hold</b> to select a recent file"));
    m_printAction = KStandardAction::print(this, SLOT(print()), actionCollection());
    m_printAction->setEnabled(false);
    m_closeAction = KStandardAction::close(this, SLOT(closeUrl()), actionCollection());
    m_closeAction->setEnabled(false);
    KStandardAction::quit(this, SLOT(close()), actionCollection());

    setStandardToolBarMenuEnabled(true);

    m_showMenuBarAction = KStandardAction::showMenubar(this, SLOT(slotShowMenubar()), actionCollection());
    m_fullScreenAction = KStandardAction::fullScreen(this, SLOT(slotUpdateFullScreen()), this, actionCollection());

    actionCollection()->setDefaultShortcuts(m_fullScreenAction, KStandardShortcut::fullScreen() + QList {QKeySequence(Qt::Key_F11)});

    m_nextTabAction = actionCollection()->addAction(QStringLiteral("tab-next"));
    m_nextTabAction->setText(i18n("Next Tab"));
    actionCollection()->setDefaultShortcuts(m_nextTabAction, KStandardShortcut::tabNext());
    m_nextTabAction->setEnabled(false);
    connect(m_nextTabAction, &QAction::triggered, this, &Shell::activateNextTab);

    m_prevTabAction = actionCollection()->addAction(QStringLiteral("tab-previous"));
    m_prevTabAction->setText(i18n("Previous Tab"));
    actionCollection()->setDefaultShortcuts(m_prevTabAction, KStandardShortcut::tabPrev());
    m_prevTabAction->setEnabled(false);
    connect(m_prevTabAction, &QAction::triggered, this, &Shell::activatePrevTab);

    // add shortcuts for Shift+Alt+1 to Shift+Alt+9 to switch tabs(browser logic- Shift+Alt+9 is always going to be last tab)
    for (int i = 1; i <= 9; ++i) {
        QAction *action = actionCollection()->addAction(QStringLiteral("tab-switch-%1").arg(i));
        action->setText(i18n("Switch to Tab %1", i));

        // static cast to Qt::Key satisfies Qt 6 strict type checking for QKeySequence
        actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::SHIFT | Qt::ALT | static_cast<Qt::Key>(Qt::Key_0 + i)));
        connect(action, &QAction::triggered, this, [this, i]() {
            if (m_tabs.isEmpty()) {
                return;
            }
            int index = (i == 9) ? m_tabs.size() - 1 : i - 1;
            if (index >= 0 && index < m_tabs.size()) {
                setActiveTab(index);
            }
        });
    }

    m_undoCloseTab = actionCollection()->addAction(QStringLiteral("undo-close-tab"));
    m_undoCloseTab->setText(i18n("Undo close tab"));
    actionCollection()->setDefaultShortcut(m_undoCloseTab, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    m_undoCloseTab->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    m_undoCloseTab->setEnabled(false);
    connect(m_undoCloseTab, &QAction::triggered, this, &Shell::undoCloseTab);
}

void Shell::saveProperties(KConfigGroup &group)
{
    if (!m_isValid) { // part couldn't be loaded, nothing to save
        return;
    }

    // Gather lists of settings to preserve
    QStringList urls;
    for (const TabState &tab : std::as_const(m_tabs)) {
        urls.append(tab.part->url().url());
    }
    group.writePathEntry(SESSION_URL_KEY, urls);
    group.writeEntry(SESSION_TAB_KEY, m_tabWidget->currentIndex());
}

void Shell::readProperties(const KConfigGroup &group)
{
    // Reopen documents based on saved settings
    QStringList urls = group.readPathEntry(SESSION_URL_KEY, QStringList());
    int desiredTab = group.readEntry<int>(SESSION_TAB_KEY, 0);

    while (!urls.isEmpty()) {
        openUrl(QUrl(urls.takeFirst()));
    }

    if (desiredTab < m_tabs.size()) {
        setActiveTab(desiredTab);
    }
}

void Shell::fileOpen()
{
    // this slot is called whenever the File->Open menu is selected,
    // the Open shortcut is pressed (usually CTRL+O) or the Open toolbar
    // button is clicked
    const int activeTab = m_tabWidget->currentIndex();
    if (!m_fileformatsscanned) {
        const KDocumentViewer *const doc = qobject_cast<KDocumentViewer *>(m_tabs[activeTab].part);
        Q_ASSERT(doc);

        m_fileformats = doc->supportedMimeTypes();

        m_fileformatsscanned = true;
    }

    QUrl startDir;
    const KParts::ReadWritePart *const curPart = m_tabs[activeTab].part;
    if (curPart->url().isLocalFile()) {
        startDir = KIO::upUrl(curPart->url());
    }
    if (startDir.isEmpty() || (startDir == QUrl::fromLocalFile(QDir::rootPath()))) {
        startDir = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    }

    QPointer<QFileDialog> dlg(new QFileDialog(this));
    dlg->setDirectoryUrl(startDir);
    dlg->setAcceptMode(QFileDialog::AcceptOpen);
    dlg->setOption(QFileDialog::HideNameFilterDetails, true);
    dlg->setFileMode(QFileDialog::ExistingFiles); // Allow selection of more than one file

    QMimeDatabase mimeDatabase;
    // Unfortunately non Plasma file dialogs don't support the "All supported files" when using
    // setMimeTypeFilters instead of setNameFilters, so for those use setNameFilters which is a bit
    // worse because doesn't show you pdf files named bla.blo when you say "show me the pdf files", but
    // that's solvable by choosing "All Files" and it's not that common while it's more convenient to
    // only get shown the files that the application can open by default instead of all of them
    const bool useMimeTypeFilters = qgetenv("XDG_CURRENT_DESKTOP").toLower() == "kde";
    if (useMimeTypeFilters) {
        QStringList mimetypes;
        for (const QString &mimeName : std::as_const(m_fileformats)) {
            QMimeType mimeType = mimeDatabase.mimeTypeForName(mimeName);
            mimetypes << mimeType.name();
        }
        mimetypes.prepend(QStringLiteral("application/octet-stream"));
        dlg->setMimeTypeFilters(mimetypes);
    } else {
        QSet<QString> globPatterns;
        QMap<QString, QStringList> namedGlobs;
        for (const QString &mimeName : std::as_const(m_fileformats)) {
            QMimeType mimeType = mimeDatabase.mimeTypeForName(mimeName);
            const QStringList globs(mimeType.globPatterns());
            if (globs.isEmpty()) {
                continue;
            }

            globPatterns.unite(QSet<QString>(globs.begin(), globs.end()));

            namedGlobs[mimeType.comment()].append(globs);
        }
        QStringList namePatterns;
        for (auto it = namedGlobs.cbegin(); it != namedGlobs.cend(); ++it) {
            namePatterns.append(it.key() + QLatin1String(" (") + it.value().join(QLatin1Char(' ')) + QLatin1Char(')'));
        }

        const QStringList allGlobPatterns = globPatterns.values();
        namePatterns.prepend(i18n("All files (*)"));
        namePatterns.prepend(i18n("All supported files (%1)", allGlobPatterns.join(QLatin1Char(' '))));
        dlg->setNameFilters(namePatterns);
    }

    dlg->setWindowTitle(i18n("Open Document")); /* cppcheck-suppress nullPointerRedundantCheck ; QPointer things here is not understood*/
    if (dlg->exec() && dlg) {                   /* cppcheck-suppress nullPointerRedundantCheck ; QPointer things here is not understood*/
        const QList<QUrl> urlList = dlg->selectedUrls();
        for (const QUrl &url : urlList) {
            openUrl(url);
        }
    }

    if (dlg) {
        delete dlg.data();
    }
}

void Shell::tryRaise(const QString &startupId)
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_OSX) && !defined(Q_OS_HAIKU)
    if (KWindowSystem::isPlatformWayland()) {
        KWindowSystem::setCurrentXdgActivationToken(startupId);
    } else if (KWindowSystem::isPlatformX11()) {
        KStartupInfo::setNewStartupId(window()->windowHandle(), startupId.toUtf8());
    }
#else
    Q_UNUSED(startupId);
#endif

    KWindowSystem::activateWindow(window()->windowHandle());
}

// only called when starting the program
void Shell::setFullScreen(bool useFullScreen)
{
    if (useFullScreen) {
        setWindowState(windowState() | Qt::WindowFullScreen); // set
    } else {
        setWindowState(windowState() & ~Qt::WindowFullScreen); // reset
    }
}

void Shell::setCaption(const QString &caption)
{
    bool modified = false;

    const int activeTab = m_tabWidget->currentIndex();
    if (activeTab != -1) {
        KParts::ReadWritePart *const activePart = m_tabs[activeTab].part;
        QString tabCaption = activePart->url().fileName();
        if (tabCaption.isEmpty() && m_tabs[activeTab].builderVisible) {
            // A blank builder has no URL; give the tab something to say.
            tabCaption = i18n("Document Builder");
        }
        if (activePart->isModified()) {
            modified = true;
            if (!tabCaption.isEmpty()) {
                tabCaption.append(QStringLiteral(" *"));
            }
        }

        m_tabWidget->setTabText(activeTab, tabCaption);
    }

    setCaption(caption, modified);
}

void Shell::showEvent(QShowEvent *e)
{
    if (!menuBar()->isNativeMenuBar() && m_showMenuBarAction) {
        m_showMenuBarAction->setChecked(menuBar()->isVisible());
    }

    KParts::MainWindow::showEvent(e);
}

void Shell::slotUpdateFullScreen()
{
    if (m_fullScreenAction->isChecked()) {
        m_menuBarWasShown = !menuBar()->isHidden();
        menuBar()->hide();

        KToggleFullScreenAction::setFullScreen(this, true);
    } else {
        if (m_menuBarWasShown) {
            menuBar()->show();
        }
        KToggleFullScreenAction::setFullScreen(this, false);
    }
}

void Shell::slotShowMenubar()
{
    if (menuBar()->isHidden()) {
        menuBar()->show();
    } else {
        menuBar()->hide();
    }
}

QSize Shell::sizeHint() const
{
    const QSize baseSize = QApplication::primaryScreen()->availableSize() * 0.6;
    // Set an arbitrary yet sensible sane minimum size for very small screens;
    // for example we don't want people using 1366x768 screens to get a tiny
    // default window size of 820 x 460 which will elide most of the toolbar buttons.
    return baseSize.expandedTo(QSize(1000, 700));
}

bool Shell::queryClose()
{
    if (m_tabs.count() > 1) {
        const QString dontAskAgainName = QStringLiteral("ShowTabWarning");
        KMessageBox::ButtonCode dummy = KMessageBox::PrimaryAction;
        if (KMessageBox::shouldBeShownTwoActions(dontAskAgainName, dummy)) {
            QDialog *dialog = new QDialog(this);
            dialog->setWindowTitle(i18n("Confirm Close"));

            QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
            buttonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
            KGuiItem::assign(buttonBox->button(QDialogButtonBox::Yes), KGuiItem(i18n("Close Tabs"), QStringLiteral("tab-close")));
            KGuiItem::assign(buttonBox->button(QDialogButtonBox::No), KStandardGuiItem::cancel());

            bool checkboxResult = true;
            const int result = KMessageBox::createKMessageBox(dialog,
                                                              buttonBox,
                                                              QMessageBox::Question,
                                                              i18n("You are about to close %1 tabs. Are you sure you want to continue?", m_tabs.count()),
                                                              QStringList(),
                                                              i18n("Warn me when I attempt to close multiple tabs"),
                                                              &checkboxResult,
                                                              KMessageBox::Notify);

            if (!checkboxResult) {
                KMessageBox::saveDontShowAgainTwoActions(dontAskAgainName, dummy);
            }

            if (result != QDialogButtonBox::Yes) {
                return false;
            }
        }
    }

    for (int i = 0; i < m_tabs.size(); ++i) {
        KParts::ReadWritePart *const part = m_tabs[i].part;

        // To resolve confusion about multiple modified docs, switch to relevant tab
        if (part->isModified()) {
            setActiveTab(i);
        }

        if (!part->queryClose()) {
            return false;
        }
    }
    return true;
}

void Shell::setActiveTab(int tab)
{
    m_tabWidget->setCurrentIndex(tab);

    createGUI(m_tabs[tab].part);

    m_printAction->setEnabled(m_tabs[tab].printEnabled);
    m_closeAction->setEnabled(m_tabs[tab].closeEnabled);
}

void Shell::closeTab(int tab)
{
    KParts::ReadWritePart *const part = m_tabs[tab].part;
    QUrl url = part->url();
    bool closeSuccess = part->closeUrl();
    if (closeSuccess && m_tabs.count() > 1) {
        if (part->factory()) {
            part->factory()->removeClient(part);
        }
        part->disconnect(this);

        part->deleteLater();
        m_tabs.removeAt(tab);
        m_tabWidget->removeTab(tab);
        m_undoCloseTab->setEnabled(true);
        m_closedTabUrls.append(url);

        if (m_tabWidget->count() == 1) {
            m_tabWidget->tabBar()->hide();
            m_nextTabAction->setEnabled(false);
            m_prevTabAction->setEnabled(false);
        }
    } else if (closeSuccess && m_tabs.count() == 1) {
        // Show welcome screen when the last tab is closed.

        showWelcomeScreen();
    }
}

void Shell::openNewTab(const QUrl &url, const QString &serializedOptions)
{
    const int previousActiveTab = m_tabWidget->currentIndex();
    KParts::ReadWritePart *const activePart = m_tabs[previousActiveTab].part;

    hideWelcomeScreen();

    bool activateTabIfAlreadyOpen;
    QMetaObject::invokeMethod(activePart, "activateTabIfAlreadyOpenFile", Q_RETURN_ARG(bool, activateTabIfAlreadyOpen));

    if (activateTabIfAlreadyOpen) {
        const int tabIndex = findTabIndex(url);

        if (tabIndex >= 0) {
            setActiveTab(tabIndex);
            m_recent->addUrl(url);
            return;
        }
    }

    // Tabs are hidden when there's only one, so show it
    if (m_tabs.size() == 1) {
        m_tabWidget->tabBar()->show();
        m_nextTabAction->setEnabled(true);
        m_prevTabAction->setEnabled(true);
    }

    const int newIndex = m_tabs.size();

    // Make new part
    m_tabs.append(TabState(m_partFactory->create<KParts::ReadWritePart>(this)));
    connectPart(m_tabs[newIndex].part);

    // Update GUI
    KParts::ReadWritePart *const part = m_tabs[newIndex].part;
    m_tabWidget->addTab(part->widget(), url.fileName());
    m_tabWidget->setTabToolTip(newIndex, url.fileName());

    const bool untitled = !m_untitledPath.isEmpty();

    applyOptionsToPart(part, serializedOptions);

    setActiveTab(m_tabs.size() - 1);

    if (part->openUrl(url)) {
        if (!untitled) {
            m_recent->addUrl(url);
        }
    } else {
        setActiveTab(previousActiveTab);
        closeTab(m_tabs.size() - 1);
        m_recent->removeUrl(url);
    }
}

void Shell::applyOptionsToPart(QObject *part, const QString &serializedOptions)
{
    KDocumentViewer *const doc = qobject_cast<KDocumentViewer *>(part);
    const QString find = ShellUtils::find(serializedOptions);
    if (ShellUtils::startInPresentation(serializedOptions)) {
        doc->startPresentation();
    }
    if (ShellUtils::showPrintDialog(serializedOptions)) {
        QMetaObject::invokeMethod(part, "enableStartWithPrint");
    }
    if (ShellUtils::showPrintDialogAndExit(serializedOptions)) {
        QMetaObject::invokeMethod(part, "enableExitAfterPrint");
    }
    if (!find.isEmpty()) {
        QMetaObject::invokeMethod(part, "enableStartWithFind", Q_ARG(QString, find));
    }

    // A brand-new document: the part needs to know so Ctrl+S asks for a location and autosave stays
    // off. Always invoked (with an empty path for ordinary opens) so opening a real file in the same
    // part clears the state again. Must happen before the open (the callers guarantee that).
    const bool ok = QMetaObject::invokeMethod(part, "setUntitledDocument", Q_ARG(QString, m_untitledPath));
    Q_ASSERT(ok);
    Q_UNUSED(ok)
    m_untitledPath.clear();
}

void Shell::connectPart(const KParts::ReadWritePart *part)
{
    // NOLINTBEGIN(clazy-old-style-connect);
    // We're abusing the fact we know the part is our part here
    connect(this, SIGNAL(moveSplitter(int)), part, SLOT(moveSplitter(int)));
    connect(part, SIGNAL(enablePrintAction(bool)), this, SLOT(setPrintEnabled(bool)));
    connect(part, SIGNAL(enableCloseAction(bool)), this, SLOT(setCloseEnabled(bool)));
    connect(part, SIGNAL(mimeTypeChanged(QMimeType)), this, SLOT(setTabIcon(QMimeType)));
    connect(part, SIGNAL(urlsDropped(QList<QUrl>)), this, SLOT(handleDroppedUrls(QList<QUrl>)));
    connect(part, SIGNAL(maxRecentItemsChanged(int)), this, SLOT(triggerUpdateRecentItems(int)));

    // clang-format off
    // Formatting disabled to keep signature normalized
    connect(part, SIGNAL(requestOpenNewlySignedFile(QString,int)), this, SLOT(openNewlySignedFile(QString,int)));
    connect(part, SIGNAL(fitWindowToPage(QSize,QSize)), this, SLOT(slotFitWindowToPage(QSize,QSize)));
    const bool newDocOk = connect(part, SIGNAL(requestOpenNewUntitledDocument(QString)), this, SLOT(openNewUntitledDocument(QString)));
    const bool recentOk = connect(part, SIGNAL(requestAddRecentFile(QString)), this, SLOT(addRecentFile(QString)));
    const bool exportedOk = connect(part, SIGNAL(openExportedDocument(QString)), this, SLOT(openExportedDocument(QString)));
    const bool builderOk = connect(part, SIGNAL(requestDocumentBuilderVisibilityChanged(bool)), this, SLOT(setDocumentBuilderVisible(bool)));
    const bool builderCloseOk = connect(part, SIGNAL(requestCloseEmptyDocumentBuilder()), this, SLOT(closeEmptyDocumentBuilder()));
    // clang-format on
    // String-based connects fail silently on a typo: assert in debug builds
    Q_ASSERT(newDocOk);
    Q_ASSERT(recentOk);
    Q_ASSERT(exportedOk);
    Q_ASSERT(builderOk);
    Q_ASSERT(builderCloseOk);
    Q_UNUSED(newDocOk)
    Q_UNUSED(recentOk)
    Q_UNUSED(exportedOk)
    Q_UNUSED(builderOk)
    Q_UNUSED(builderCloseOk)
    // NOLINTEND(clazy-old-style-connect);
}

void Shell::print()
{
    QMetaObject::invokeMethod(m_tabs[m_tabWidget->currentIndex()].part, "slotPrint");
}

void Shell::createNewDocument()
{
    const int activeTab = m_tabWidget->currentIndex();
    if (activeTab < 0 || activeTab >= m_tabs.size()) {
        return;
    }

    // The dialog and the blank-page writer live in the part (it is loaded as a plugin), so ask the
    // active part to run the flow; it comes back through requestOpenNewUntitledDocument(). The
    // invocation is by name because the shell must not link against the part.
    const bool ok = QMetaObject::invokeMethod(m_tabs[activeTab].part, "slotCreateNewDocument");
    Q_ASSERT(ok);
    Q_UNUSED(ok)
}

void Shell::openNewUntitledDocument(const QString &path)
{
    // Consumed by applyOptionsToPart() so the part knows Ctrl+S has to ask for a location
    m_untitledPath = path;
    openUrl(QUrl::fromLocalFile(path));
}

void Shell::openDocumentBuilder()
{
    // The builder lives in the part (it is loaded as a plugin), so ask the active part to show it.
    // The invocation is by name because the shell must not link against the part.
    const int activeTab = m_tabWidget->currentIndex();
    if (activeTab < 0 || activeTab >= m_tabs.size() || !m_tabs[activeTab].part) {
        return;
    }
    KParts::ReadWritePart *const part = m_tabs[activeTab].part;
    if (part->metaObject()->indexOfMethod("slotOpenDocumentBuilder()") < 0) {
        return;
    }

    // Only switch off the welcome screen once the part confirmed it took the request.
    const bool ok = QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder");
    if (!ok) {
        return;
    }
    bool visible = false;
    QMetaObject::invokeMethod(part, "isDocumentBuilderVisible", Q_RETURN_ARG(bool, visible));
    if (!visible) {
        return;
    }

    // A document that never had a URL (welcomed blank builder) would otherwise leave the tab blank.
    if (part->url().isEmpty()) {
        m_tabWidget->setTabText(activeTab, i18n("Document Builder"));
    }

    hideWelcomeScreen();
}

void Shell::setDocumentBuilderVisible(bool visible)
{
    const int tab = findTabIndex(sender());
    if (tab >= 0) {
        m_tabs[tab].builderVisible = visible;
    }
}

void Shell::closeEmptyDocumentBuilder()
{
    const int tab = findTabIndex(sender());
    if (tab < 0) {
        return;
    }

    // Closing the last tab shows the welcome screen again; closeTab() already handles that.
    closeTab(tab);
}

void Shell::addRecentFile(const QString &path)
{
    m_recent->addUrl(QUrl::fromLocalFile(path));
    refreshRecentsOnWelcomeScreen();
}

void Shell::openExportedDocument(const QString &path)
{
    // openUrl() switches to the tab if the document is already open, otherwise opens a new one.
    openUrl(QUrl::fromLocalFile(path));
}

void Shell::setPrintEnabled(bool enabled)
{
    int i = findTabIndex(sender());
    if (i != -1) {
        m_tabs[i].printEnabled = enabled;
        if (i == m_tabWidget->currentIndex()) {
            m_printAction->setEnabled(enabled);
        }
    }
}

void Shell::setCloseEnabled(bool enabled)
{
    int i = findTabIndex(sender());
    if (i != -1) {
        m_tabs[i].closeEnabled = enabled;
        if (i == m_tabWidget->currentIndex()) {
            m_closeAction->setEnabled(enabled);
        }
    }
}

void Shell::activateNextTab()
{
    if (m_tabs.size() < 2) {
        return;
    }

    const int activeTab = m_tabWidget->currentIndex();
    const int nextTab = (activeTab == m_tabs.size() - 1) ? 0 : activeTab + 1;

    setActiveTab(nextTab);
}

void Shell::activatePrevTab()
{
    if (m_tabs.size() < 2) {
        return;
    }

    const int activeTab = m_tabWidget->currentIndex();
    const int prevTab = (activeTab == 0) ? m_tabs.size() - 1 : activeTab - 1;

    setActiveTab(prevTab);
}

void Shell::undoCloseTab()
{
    if (m_closedTabUrls.isEmpty()) {
        return;
    }

    const QUrl lastTabUrl = m_closedTabUrls.takeLast();

    if (m_closedTabUrls.isEmpty()) {
        m_undoCloseTab->setEnabled(false);
    }

    openUrl(lastTabUrl);
}

void Shell::setTabIcon(const QMimeType &mimeType)
{
    int i = findTabIndex(sender());
    if (i != -1) {
        m_tabWidget->setTabIcon(i, QIcon::fromTheme(mimeType.iconName()));
    }
}

int Shell::findTabIndex(QObject *sender) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].part == sender) {
            return i;
        }
    }
    return -1;
}

int Shell::findTabIndex(const QUrl &url) const
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [&url](const TabState state) { return state.part->url() == url; });
    return (it != m_tabs.end()) ? std::distance(m_tabs.begin(), it) : -1;
}

void Shell::handleDroppedUrls(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        openUrl(url);
    }
}

void Shell::moveTabData(int from, int to)
{
    m_tabs.move(from, to);
}

void Shell::slotFitWindowToPage(const QSize pageViewSize, const QSize pageSize)
{
    const int xOffset = pageViewSize.width() - pageSize.width();
    const int yOffset = pageViewSize.height() - pageSize.height();
    showNormal();
    resize(width() - xOffset, height() - yOffset);
    Q_EMIT moveSplitter(pageSize.width());
}

void Shell::hideWelcomeScreen()
{
    m_centralStackedWidget->setCurrentWidget(m_tabWidget);
}

void Shell::showWelcomeScreen()
{
    m_centralStackedWidget->setCurrentWidget(m_welcomeScreen);

    refreshRecentsOnWelcomeScreen();
}

void Shell::refreshRecentsOnWelcomeScreen()
{
    saveRecents();
    m_welcomeScreen->loadRecents();
}

void Shell::forgetRecentItem(QUrl const &url)
{
    if (m_recent != nullptr) {
        m_recent->removeUrl(url);
        saveRecents();
        refreshRecentsOnWelcomeScreen();
    }
}

void Shell::triggerUpdateRecentItems(const int maxItems)
{
    m_recent->setMaxItems(maxItems);
    m_welcomeScreen->setMaxRecentItems(m_recent->maxItems());
    // saveRecents() dumps the recent files in correct order to KConfigGroup, respecting the allowed no. of recent items, discarding older items if needed
    refreshRecentsOnWelcomeScreen();
    m_recent->loadEntries(KSharedConfig::openConfig()->group(RecentFilesGroupKey()));
}

void Shell::readRecentFilesSettings()
{
    // Read no. of max. recent items from chartapartrc, populate File->Open Recent menu-item as well as recentsListView on welcome screen
    QString configFilePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1Char('/') + QLatin1String("chartapartrc");
    const KConfigGroup confgrp = KSharedConfig::openConfig(configFilePath).data()->group(QStringLiteral("General"));
    const int defaultMaxRecentItems = 10;
    int maxRecentItems = confgrp.readEntry<int>("MaxRecentItems", defaultMaxRecentItems);
    m_recent->setMaxItems(maxRecentItems);
    m_welcomeScreen->setMaxRecentItems(m_recent->maxItems());
    m_welcomeScreen->loadRecents();
    m_recent->loadEntries(KSharedConfig::openConfig()->group(RecentFilesGroupKey()));
}

#include "moc_shell.cpp"
#include "shell.moc"
