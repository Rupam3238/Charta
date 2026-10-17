/*
    SPDX-FileCopyrightText: 2004-2006 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnaillist.h"

// qt/kde includes
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include <KActionCollection>
#include <KLocalizedString>
#include <KTitleWidget>

// local includes
#include "core/area.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"
#include "core/generator.h"
#include "core/page.h"
#include "gui/pagepainter.h"
#include "gui/priorities.h"
#include "settings.h"

class ThumbnailWidget;

ThumbnailsBox::ThumbnailsBox(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setSpacing(0);

    KTitleWidget *titleWidget = new KTitleWidget(this);
    titleWidget->setLevel(4);
    titleWidget->setText(i18n("Thumbnails"));
    vbox->addWidget(titleWidget);
    vbox->setAlignment(titleWidget, Qt::AlignHCenter);
}

QSize ThumbnailsBox::sizeHint() const
{
    return QSize();
}

// The bar shown at the bottom of the list while pages are being selected
class SelectionBar : public QWidget
{
public:
    explicit SelectionBar(QWidget *parent)
        : QWidget(parent)
    {
        setAutoFillBackground(true);
        setBackgroundRole(QPalette::Window);

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(6, 4, 6, 4);
        outer->setSpacing(2);

        auto *top = new QHBoxLayout();
        label = new QLabel(this);
        top->addWidget(label, 1);
        done = new QToolButton(this);
        done->setAutoRaise(true);
        done->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));
        done->setText(i18n("Done"));
        done->setToolTip(i18n("Leave selection mode"));
        done->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        top->addWidget(done);
        outer->addLayout(top);

        auto *row = new QHBoxLayout();
        selectAll = makeButton(QStringLiteral("edit-select-all"), i18n("Select all pages"));
        add = makeButton(QStringLiteral("list-add"), i18n("Add page"));
        add->setPopupMode(QToolButton::InstantPopup);
        addMenu = new QMenu(add);
        add->setMenu(addMenu);
        del = makeButton(QStringLiteral("edit-delete"), i18n("Delete the selected pages"));
        row->addWidget(selectAll);
        row->addWidget(add);
        row->addWidget(del);
        row->addStretch();
        outer->addLayout(row);
    }

    QLabel *label;
    QToolButton *done;
    QToolButton *selectAll;
    QToolButton *add;
    QMenu *addMenu;
    QToolButton *del;

private:
    QToolButton *makeButton(const QString &iconName, const QString &toolTip)
    {
        auto *button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setIcon(QIcon::fromTheme(iconName));
        button->setToolTip(toolTip);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        return button;
    }
};

class ThumbnailListPrivate : public QWidget
{
    Q_OBJECT
public:
    ThumbnailListPrivate(ThumbnailList *qq, Okular::Document *document);
    ~ThumbnailListPrivate() override;

    ThumbnailList *q;
    Okular::Document *m_document;
    ThumbnailWidget *m_selected;
    QTimer *m_delayTimer;
    QPixmap m_bookmarkOverlay;
    QList<ThumbnailWidget *> m_thumbnails;
    QList<ThumbnailWidget *> m_visibleThumbnails;
    int m_vectorIndex;

    // Selection mode: pages are ticked, then deleted or moved together
    bool m_selectMode;
    QSet<int> m_selectedPageNumbers;
    SelectionBar *m_selectionBar;

    // Mouse press (click / long press)
    QTimer *m_longPressTimer;
    QPoint m_pressPos;
    int m_pressedPage;
    bool m_pressedOnCheckBox;

    // Drag and drop of pages
    bool m_dragging;
    QList<int> m_dragPages;
    QPixmap m_dragPixmap;
    QPoint m_dragOffset;
    QPoint m_dragPos;
    // the dragged pages are dropped before this page (the number of pages means "at the end"), -1 if none
    int m_dropPage;
    QTimer *m_autoScrollTimer;

    // selection mode helpers
    void createSelectionBar();
    void layoutSelectionBar();
    void setSelectMode(bool enabled);
    void toggleSelected(int page);
    void selectAllPages();
    void updateSelectionHighlights();
    void updateSelectionBar();

    // drag and drop helpers
    void startDrag();
    void updateDrag(const QPoint &pos);
    void finishDrag(bool drop);
    void cancelDrag();
    void autoScrollStep();
    int dropPageFor(const QPoint &pos) const;
    QRect dropIndicatorRect() const;

    // resize thumbnails to fit the width
    void viewportResizeEvent(QResizeEvent *);

    ThumbnailWidget *itemFor(const QPoint p) const;
    void delayedRequestVisiblePixmaps(int delayMs = 0);

    // SLOTS:
    // make requests for generating pixmaps for visible thumbnails
    void slotRequestVisiblePixmaps();
    // delay timeout: resize overlays and requests pixmaps
    void slotDelayTimeout();
    ThumbnailWidget *getPageByNumber(int page) const;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
};

// ThumbnailWidget represents a single thumbnail in the ThumbnailList
class ThumbnailWidget
{
public:
    ThumbnailWidget(ThumbnailListPrivate *parent, const Okular::Page *page);

    // set internal parameters to fit the page in the given width
    void resizeFitWidth(int width);
    // set thumbnail's selected state
    void setSelected(bool selected);
    // set whether the thumbnail is part of the multi page selection
    void setPageSelected(bool selected);
    // set the visible rect of the current page
    void setVisibleRect(const Okular::NormalizedRect &rect);

    // query methods
    int heightHint() const
    {
        return m_pixmapHeight + m_labelHeight + m_margin;
    }
    int pixmapWidth() const
    {
        return m_pixmapWidth;
    }
    int pixmapHeight() const
    {
        return m_pixmapHeight;
    }
    int pageNumber() const
    {
        return m_page->number();
    }
    const Okular::Page *page() const
    {
        return m_page;
    }
    QRect visibleRect() const
    {
        return m_visibleRect.geometry(m_pixmapWidth, m_pixmapHeight);
    }
    // the check box shown in selection mode, in the coordinates of the thumbnail (with some room around it)
    QRect checkBoxRect() const
    {
        return QRect(m_margin / 2 + m_pixmapWidth - 26, m_margin / 2 + m_pixmapHeight - 26, 22, 22).adjusted(-6, -6, 6, 6);
    }

    void paint(QPainter &p, const QRect clipRect);

    static int margin()
    {
        return m_margin;
    }

    // simulating QWidget
    QRect rect() const
    {
        return m_rect;
    }
    int height() const
    {
        return m_rect.height();
    }
    int width() const
    {
        return m_rect.width();
    }
    QPoint pos() const
    {
        return m_rect.topLeft();
    }
    void move(int x, int y)
    {
        m_rect.setTopLeft(QPoint(x, y));
    }
    void update()
    {
        m_parent->update(m_rect);
    }
    void update(const QRect rect)
    {
        m_parent->update(rect.translated(m_rect.topLeft()));
    }

private:
    // the margin around the widget
    static int const m_margin = 16;

    ThumbnailListPrivate *m_parent;
    const Okular::Page *m_page;
    bool m_selected;
    bool m_pageSelected;
    int m_pixmapWidth, m_pixmapHeight;
    int m_labelHeight, m_labelNumber;
    Okular::NormalizedRect m_visibleRect;
    QRect m_rect;
};

ThumbnailListPrivate::ThumbnailListPrivate(ThumbnailList *qq, Okular::Document *document)
    : QWidget()
    , q(qq)
    , m_document(document)
    , m_selected(nullptr)
    , m_delayTimer(nullptr)
    , m_vectorIndex(0)
    , m_selectMode(false)
    , m_selectionBar(nullptr)
    , m_pressedPage(-1)
    , m_pressedOnCheckBox(false)
    , m_dragging(false)
    , m_dropPage(-1)
{
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(350);
    connect(m_longPressTimer, &QTimer::timeout, this, &ThumbnailListPrivate::startDrag);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(30);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &ThumbnailListPrivate::autoScrollStep);
}

ThumbnailWidget *ThumbnailListPrivate::getPageByNumber(int page) const
{
    QList<ThumbnailWidget *>::const_iterator tIt = m_thumbnails.constBegin(), tEnd = m_thumbnails.constEnd();
    for (; tIt != tEnd; ++tIt) {
        if ((*tIt)->pageNumber() == page) {
            return (*tIt);
        }
    }
    return nullptr;
}

ThumbnailListPrivate::~ThumbnailListPrivate()
{
}

ThumbnailWidget *ThumbnailListPrivate::itemFor(const QPoint p) const
{
    QList<ThumbnailWidget *>::const_iterator tIt = m_thumbnails.constBegin(), tEnd = m_thumbnails.constEnd();
    for (; tIt != tEnd; ++tIt) {
        if ((*tIt)->rect().contains(p)) {
            return (*tIt);
        }
    }
    return nullptr;
}

void ThumbnailListPrivate::createSelectionBar()
{
    m_selectionBar = new SelectionBar(q);
    m_selectionBar->hide();
    connect(m_selectionBar->done, &QToolButton::clicked, this, [this]() { setSelectMode(false); });
    connect(m_selectionBar->selectAll, &QToolButton::clicked, this, [this]() { selectAllPages(); });
    connect(m_selectionBar->del, &QToolButton::clicked, this, [this]() { Q_EMIT q->pagesDeleted(q->selectedPageNumbers()); });
    m_selectionBar->addMenu->addAction(QIcon::fromTheme(QStringLiteral("go-up")), i18n("Before the first selected page"), this, [this]() {
        const QList<int> selected = q->selectedPageNumbers();
        if (!selected.isEmpty()) {
            Q_EMIT q->pageAddedBefore(selected.first());
        }
    });
    m_selectionBar->addMenu->addAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("After the last selected page"), this, [this]() {
        const QList<int> selected = q->selectedPageNumbers();
        if (!selected.isEmpty()) {
            Q_EMIT q->pageAddedAfter(selected.last());
        }
    });
}

void ThumbnailListPrivate::layoutSelectionBar()
{
    if (!m_selectionBar) {
        return;
    }
    const int height = m_selectionBar->sizeHint().height();
    m_selectionBar->setGeometry(0, q->height() - height, q->width(), height);
    m_selectionBar->raise();
}

void ThumbnailListPrivate::setSelectMode(bool enabled)
{
    if (m_selectMode == enabled) {
        return;
    }
    cancelDrag();
    m_selectMode = enabled;
    if (!enabled) {
        m_selectedPageNumbers.clear();
    }
    updateSelectionHighlights();
    updateSelectionBar();
    update();
}

void ThumbnailListPrivate::toggleSelected(int page)
{
    if (m_selectedPageNumbers.contains(page)) {
        m_selectedPageNumbers.remove(page);
    } else {
        m_selectedPageNumbers.insert(page);
    }
    updateSelectionHighlights();
    updateSelectionBar();
}

void ThumbnailListPrivate::selectAllPages()
{
    m_selectedPageNumbers.clear();
    for (const ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
        m_selectedPageNumbers.insert(thumb->pageNumber());
    }
    updateSelectionHighlights();
    updateSelectionBar();
}

void ThumbnailListPrivate::updateSelectionHighlights()
{
    for (ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
        thumb->setPageSelected(m_selectedPageNumbers.contains(thumb->pageNumber()));
    }
}

void ThumbnailListPrivate::updateSelectionBar()
{
    if (!m_selectionBar) {
        return;
    }
    const int count = m_selectedPageNumbers.size();
    m_selectionBar->label->setText(i18np("%1 page selected", "%1 pages selected", count));
    m_selectionBar->del->setEnabled(count > 0 && count < static_cast<int>(m_document->pages()) && m_document->canModifyPages());
    m_selectionBar->add->setEnabled(count > 0 && m_document->canModifyPages());
    m_selectionBar->selectAll->setEnabled(!m_thumbnails.isEmpty());

    if (m_selectionBar->isVisible() != m_selectMode) {
        const int barHeight = m_selectMode ? m_selectionBar->sizeHint().height() : 0;
        q->setViewportMargins(0, 0, 0, barHeight);
        m_selectionBar->setVisible(m_selectMode);
    }
    layoutSelectionBar();
}

int ThumbnailListPrivate::dropPageFor(const QPoint &pos) const
{
    for (const ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
        if (pos.y() < thumb->rect().center().y()) {
            return thumb->pageNumber();
        }
    }
    return static_cast<int>(m_document->pages());
}

QRect ThumbnailListPrivate::dropIndicatorRect() const
{
    if (m_thumbnails.isEmpty()) {
        return QRect();
    }
    const int spacing = qMax(4, style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical));
    int y = m_thumbnails.last()->rect().bottom() + spacing / 2;
    for (const ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
        if (thumb->pageNumber() >= m_dropPage) {
            y = thumb->pos().y() - spacing / 2;
            break;
        }
    }
    return QRect(8, y - 2, qMax(0, width() - 16), 4);
}

void ThumbnailListPrivate::startDrag()
{
    ThumbnailWidget *item = getPageByNumber(m_pressedPage);
    if (!item || !m_document->canModifyPages() || !(QApplication::mouseButtons() & Qt::LeftButton)) {
        return;
    }

    // in selection mode the whole selection moves when one of its pages is picked up
    if (m_selectMode && m_selectedPageNumbers.contains(m_pressedPage)) {
        m_dragPages = q->selectedPageNumbers();
    } else {
        m_dragPages = QList<int>() << m_pressedPage;
    }

    // the picked up page: a reduced copy of its thumbnail that follows the cursor
    const qreal scale = 0.6;
    const qreal ratio = devicePixelRatioF();
    const QSize ghostSize(qMax(1, qRound(item->width() * scale)), qMax(1, qRound(item->height() * scale)));
    m_dragPixmap = QPixmap(ghostSize * ratio);
    m_dragPixmap.setDevicePixelRatio(ratio);
    m_dragPixmap.fill(Qt::transparent);
    {
        QPainter painter(&m_dragPixmap);
        painter.scale(scale, scale);
        item->paint(painter, QRect(QPoint(0, 0), item->rect().size()));
    }
    m_dragOffset = QPointF((m_pressPos - item->pos()) * scale).toPoint();

    m_dragging = true;
    m_longPressTimer->stop();
    setCursor(Qt::ClosedHandCursor);
    m_autoScrollTimer->start();
    updateDrag(mapFromGlobal(QCursor::pos()));
}

void ThumbnailListPrivate::updateDrag(const QPoint &pos)
{
    m_dragPos = pos;
    m_dropPage = dropPageFor(pos);
    update();
}

void ThumbnailListPrivate::autoScrollStep()
{
    if (!m_dragging) {
        m_autoScrollTimer->stop();
        return;
    }
    const int edge = 48;
    const int maxStep = 28;
    const int viewportHeight = q->viewport()->height();
    const int y = q->viewport()->mapFromGlobal(QCursor::pos()).y();
    int delta = 0;
    if (y < edge) {
        delta = -qMax(2, (edge - y) * maxStep / edge);
    } else if (y > viewportHeight - edge) {
        delta = qMax(2, (y - (viewportHeight - edge)) * maxStep / edge);
    }
    if (delta != 0) {
        QScrollBar *scrollBar = q->verticalScrollBar();
        scrollBar->setValue(scrollBar->value() + delta);
    }
    // the list may have scrolled under a motionless cursor
    updateDrag(mapFromGlobal(QCursor::pos()));
}

void ThumbnailListPrivate::cancelDrag()
{
    m_longPressTimer->stop();
    m_autoScrollTimer->stop();
    const bool wasDragging = m_dragging;
    m_dragging = false;
    m_dragPages.clear();
    m_dragPixmap = QPixmap();
    m_dropPage = -1;
    m_pressedPage = -1;
    m_pressedOnCheckBox = false;
    if (wasDragging) {
        unsetCursor();
        update();
    }
}

void ThumbnailListPrivate::finishDrag(bool drop)
{
    const QList<int> pages = m_dragPages;
    const int dropPage = m_dropPage;
    cancelDrag();
    if (!drop || pages.isEmpty() || dropPage < 0) {
        return;
    }

    // where the first of the pages ends up once all of them have been taken out of the document
    int before = 0;
    for (const int page : pages) {
        if (page < dropPage) {
            ++before;
        }
    }
    const int to = dropPage - before;

    const bool movedTheSelection = m_selectMode && QSet<int>(pages.begin(), pages.end()) == m_selectedPageNumbers;
    Q_EMIT q->pagesReordered(pages, to);

    // the thumbnails have been rebuilt: keep the moved pages selected
    if (movedTheSelection) {
        m_selectedPageNumbers.clear();
        for (int i = 0; i < pages.size(); ++i) {
            m_selectedPageNumbers.insert(to + i);
        }
        updateSelectionHighlights();
        updateSelectionBar();
    }
}

void ThumbnailListPrivate::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    for (ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
        QRect rect = e->rect().intersected(thumb->rect());
        if (!rect.isNull()) {
            rect.translate(-thumb->pos());
            painter.save();
            painter.translate(thumb->pos());
            if (m_dragging && m_dragPages.contains(thumb->pageNumber())) {
                // the pages that have been picked up are faded where they were
                painter.setOpacity(0.35);
            }
            thumb->paint(painter, rect);
            painter.restore();
        }
    }

    if (m_dragging) {
        painter.setRenderHint(QPainter::Antialiasing);
        // where the pages will be dropped
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(dropIndicatorRect(), 2, 2);

        // the pages under the cursor
        const QPoint topLeft = m_dragPos - m_dragOffset;
        painter.setOpacity(0.8);
        painter.drawPixmap(topLeft, m_dragPixmap);
        painter.setOpacity(1.0);
        if (m_dragPages.size() > 1) {
            QRect badge(0, 0, 26, 26);
            badge.moveCenter(QRect(topLeft, m_dragPixmap.deviceIndependentSize().toSize()).topRight());
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette().color(QPalette::Highlight));
            painter.drawEllipse(badge);
            painter.setPen(palette().color(QPalette::HighlightedText));
            painter.drawText(badge, Qt::AlignCenter, QString::number(m_dragPages.size()));
        }
    }
}

/** ThumbnailList implementation **/

ThumbnailList::ThumbnailList(QWidget *parent, Okular::Document *document)
    : QScrollArea(parent)
    , d(new ThumbnailListPrivate(this, document))
{
    setObjectName(QStringLiteral("okular::Thumbnails"));
    // set scrollbars
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    verticalScrollBar()->setEnabled(false);

    setAttribute(Qt::WA_StaticContents);

    viewport()->setBackgroundRole(QPalette::Base);

    setWidget(d);
    // widget setup: can be focused by mouse click (not wheel nor tab)
    widget()->setFocusPolicy(Qt::ClickFocus);
    widget()->show();
    widget()->setBackgroundRole(QPalette::Base);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, d, &ThumbnailListPrivate::slotRequestVisiblePixmaps);

    d->createSelectionBar();
}

void ThumbnailList::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    d->layoutSelectionBar();
}

ThumbnailList::~ThumbnailList()
{
    d->m_document->removeObserver(this);
}

// BEGIN DocumentObserver inherited methods
void ThumbnailList::notifySetup(const QList<Okular::Page *> &pages, int setupFlags)
{
    // page numbers change when pages are added, removed or moved: the selection is not valid any more
    const bool pagesChanged = setupFlags & (Okular::DocumentObserver::PagesInserted | Okular::DocumentObserver::PagesRemoved | Okular::DocumentObserver::PagesMoved);
    d->cancelDrag();
    if (setupFlags & Okular::DocumentObserver::DocumentChanged) {
        d->setSelectMode(false);
    }
    if (pagesChanged) {
        d->m_selectedPageNumbers.clear();
    }
    const int previousScrollValue = verticalScrollBar()->value();

    // if there was a widget selected, save its pagenumber to restore
    // its selection (if available in the new set of pages)
    int prevPage = -1;
    if (!(setupFlags & Okular::DocumentObserver::DocumentChanged) && !pagesChanged && d->m_selected) {
        prevPage = d->m_selected->page()->number();
    } else {
        prevPage = d->m_document->viewport().pageNumber;
    }

    // delete all the Thumbnails
    QList<ThumbnailWidget *>::const_iterator tIt = d->m_thumbnails.constBegin(), tEnd = d->m_thumbnails.constEnd();
    for (; tIt != tEnd; ++tIt) {
        delete *tIt;
    }
    d->m_thumbnails.clear();
    d->m_visibleThumbnails.clear();
    d->m_selected = nullptr;

    if (pages.count() < 1) {
        widget()->resize(0, 0);
        return;
    }

    // show pages containing highlighted text or bookmarked ones
    // RESTORE THIS int flags = Okular::Settings::filterBookmarks() ? Okular::Page::Bookmark : Okular::Page::Highlight;

    // if no page matches filter rule, then display all pages
    bool skipCheck = true;
    for (const Okular::Page *pIt : pages) {
        // if ( (*pIt)->attributes() & flags )
        if (pIt->hasHighlights(SW_SEARCH_ID)) {
            skipCheck = false;
        }
    }

    // generate Thumbnails for the given set of pages
    const int width = viewport()->width();
    int height = 0;
    int centerHeight = 0;
    for (const Okular::Page *pIt : pages) {
        // if ( skipCheck || (*pIt)->attributes() & flags )
        if (skipCheck || pIt->hasHighlights(SW_SEARCH_ID)) {
            ThumbnailWidget *t = new ThumbnailWidget(d, pIt);
            t->move(0, height);
            // add to the internal queue
            d->m_thumbnails.push_back(t);
            // update total height (asking widget its own height)
            t->resizeFitWidth(width);
            // restoring the previous selected page, if any
            if (pIt->number() < prevPage) {
                centerHeight = height + t->height() + this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical) / 2;
            }
            if (pIt->number() == prevPage) {
                d->m_selected = t;
                d->m_selected->setSelected(true);
                centerHeight = height + t->height() / 2;
            }
            height += t->height() + this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical);
        }
    }

    // Restore the selection after the thumbnails have been regenerated
    d->updateSelectionHighlights();
    d->updateSelectionBar();

    // update scrollview's contents size (sets scrollbars limits)
    height -= this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical);
    widget()->resize(width, height);

    // enable scrollbar when there's something to scroll
    verticalScrollBar()->setEnabled(viewport()->height() < height);
    // when pages are added, removed or moved keep the list where it is instead of centering the current page
    verticalScrollBar()->setValue(pagesChanged ? previousScrollValue : centerHeight - viewport()->height() / 2);

    // request for thumbnail generation
    d->delayedRequestVisiblePixmaps(200);
}

void ThumbnailList::notifyCurrentPageChanged(int previousPage, int currentPage)
{
    Q_UNUSED(previousPage)

    // skip notifies for the current page (already selected)
    if (d->m_selected && d->m_selected->pageNumber() == currentPage) {
        return;
    }

    // deselect previous thumbnail
    if (d->m_selected) {
        d->m_selected->setSelected(false);
    }
    d->m_selected = nullptr;

    // select the page with viewport and ensure it's centered in the view
    d->m_vectorIndex = 0;
    QList<ThumbnailWidget *>::const_iterator tIt = d->m_thumbnails.constBegin(), tEnd = d->m_thumbnails.constEnd();
    for (; tIt != tEnd; ++tIt) {
        if ((*tIt)->pageNumber() == currentPage) {
            d->m_selected = *tIt;
            d->m_selected->setSelected(true);
            if (Okular::Settings::syncThumbnailsViewport()) {
                syncThumbnail();
            }
            break;
        }
        d->m_vectorIndex++;
    }
}

void ThumbnailList::syncThumbnail()
{
    int yOffset = qMax(viewport()->height() / 4, d->m_selected->height() / 2);
    ensureVisible(0, d->m_selected->pos().y() + d->m_selected->height() / 2, 0, yOffset);
}

void ThumbnailList::notifyPageChanged(int pageNumber, int changedFlags)
{
    static const int interestingFlags = DocumentObserver::Pixmap | DocumentObserver::Bookmark | DocumentObserver::Highlights | DocumentObserver::Annotations;
    // only handle change notifications we are interested in
    if (!(changedFlags & interestingFlags)) {
        return;
    }

    // iterate over visible items: if page(pageNumber) is one of them, repaint it
    QList<ThumbnailWidget *>::const_iterator vIt = d->m_visibleThumbnails.constBegin(), vEnd = d->m_visibleThumbnails.constEnd();
    for (; vIt != vEnd; ++vIt) {
        if ((*vIt)->pageNumber() == pageNumber) {
            (*vIt)->update();
            break;
        }
    }
}

void ThumbnailList::notifyContentsCleared(int changedFlags)
{
    // if pixmaps were cleared, re-ask them
    if (changedFlags & DocumentObserver::Pixmap) {
        d->slotRequestVisiblePixmaps();
    }
}

void ThumbnailList::notifyVisibleRectsChanged()
{
    const QList<Okular::VisiblePageRect *> &visibleRects = d->m_document->visiblePageRects();
    QList<ThumbnailWidget *>::const_iterator tIt = d->m_thumbnails.constBegin(), tEnd = d->m_thumbnails.constEnd();
    QList<Okular::VisiblePageRect *>::const_iterator vEnd = visibleRects.end();
    for (; tIt != tEnd; ++tIt) {
        bool found = false;
        QList<Okular::VisiblePageRect *>::const_iterator vIt = visibleRects.begin();
        for (; (vIt != vEnd) && !found; ++vIt) {
            if ((*tIt)->pageNumber() == (*vIt)->pageNumber) {
                (*tIt)->setVisibleRect((*vIt)->rect);
                found = true;
            }
        }
        if (!found) {
            (*tIt)->setVisibleRect(Okular::NormalizedRect());
        }
    }
}

bool ThumbnailList::canUnloadPixmap(int pageNumber) const
{
    // if the thumbnail 'pageNumber' is one of the visible ones, forbid unloading
    QList<ThumbnailWidget *>::const_iterator vIt = d->m_visibleThumbnails.constBegin(), vEnd = d->m_visibleThumbnails.constEnd();
    for (; vIt != vEnd; ++vIt) {
        if ((*vIt)->pageNumber() == pageNumber) {
            return false;
        }
    }
    // if hidden permit unloading
    return true;
}
// END DocumentObserver inherited methods

QList<int> ThumbnailList::selectedPageNumbers() const
{
    QList<int> result;
    for (int page : d->m_selectedPageNumbers) {
        result.append(page);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ThumbnailList::updateWidgets()
{
    // Update all visible widgets
    QList<ThumbnailWidget *>::const_iterator vIt = d->m_visibleThumbnails.constBegin(), vEnd = d->m_visibleThumbnails.constEnd();
    for (; vIt != vEnd; ++vIt) {
        ThumbnailWidget *t = *vIt;
        t->update();
    }
}

void ThumbnailList::slotFilterBookmarks(bool filterOn)
{
    // save state
    Okular::Settings::setFilterBookmarks(filterOn);
    Okular::Settings::self()->save();
    // ask for the 'notifySetup' with a little trick (on reinsertion the
    // document sends the list again)
    d->m_document->removeObserver(this);
    d->m_document->addObserver(this);
}

// BEGIN widget events
void ThumbnailList::keyPressEvent(QKeyEvent *keyEvent)
{
    if (d->m_thumbnails.count() < 1) {
        keyEvent->ignore();
        return;
    }

    if (keyEvent->key() == Qt::Key_Escape) {
        if (d->m_dragging) {
            d->cancelDrag();
            keyEvent->accept();
            return;
        }
        if (d->m_selectMode) {
            d->setSelectMode(false);
            keyEvent->accept();
            return;
        }
    }

    if (keyEvent->key() == Qt::Key_Delete && d->m_selectMode && d->m_document->canModifyPages() && !d->m_selectedPageNumbers.isEmpty()) {
        Q_EMIT pagesDeleted(selectedPageNumbers());
        return;
    }

    int nextPage = -1;
    if (keyEvent->key() == Qt::Key_Up) {
        if (!d->m_selected) {
            nextPage = 0;
        } else if (d->m_vectorIndex > 0) {
            nextPage = d->m_thumbnails[d->m_vectorIndex - 1]->pageNumber();
        }
    } else if (keyEvent->key() == Qt::Key_Down) {
        if (!d->m_selected) {
            nextPage = 0;
        } else if (d->m_vectorIndex < (int)d->m_thumbnails.count() - 1) {
            nextPage = d->m_thumbnails[d->m_vectorIndex + 1]->pageNumber();
        }
    } else if (keyEvent->key() == Qt::Key_PageUp) {
        verticalScrollBar()->triggerAction(QScrollBar::SliderPageStepSub);
    } else if (keyEvent->key() == Qt::Key_PageDown) {
        verticalScrollBar()->triggerAction(QScrollBar::SliderPageStepAdd);
    } else if (keyEvent->key() == Qt::Key_Home) {
        nextPage = d->m_thumbnails[0]->pageNumber();
    } else if (keyEvent->key() == Qt::Key_End) {
        nextPage = d->m_thumbnails[d->m_thumbnails.count() - 1]->pageNumber();
    }

    if (nextPage == -1) {
        keyEvent->ignore();
        return;
    }

    keyEvent->accept();
    if (d->m_selected) {
        d->m_selected->setSelected(false);
    }
    d->m_selected = nullptr;
    d->m_document->setViewportPage(nextPage);
}

bool ThumbnailList::viewportEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::Resize: {
        d->viewportResizeEvent(static_cast<QResizeEvent *>(e));
        break;
    }
    default:;
    }
    return QScrollArea::viewportEvent(e);
}

void ThumbnailListPrivate::viewportResizeEvent(QResizeEvent *e)
{
    if (m_thumbnails.count() < 1 || width() < 1) {
        return;
    }

    // if width changed resize all the Thumbnails, reposition them to the
    // right place and recalculate the contents area
    if (e->size().width() != e->oldSize().width()) {
        // runs the timer avoiding a thumbnail regeneration by 'contentsMoving'
        delayedRequestVisiblePixmaps(2000);

        // resize and reposition items
        const int newWidth = q->viewport()->width();
        int newHeight = 0;
        QList<ThumbnailWidget *>::const_iterator tIt = m_thumbnails.constBegin(), tEnd = m_thumbnails.constEnd();
        for (; tIt != tEnd; ++tIt) {
            ThumbnailWidget *t = *tIt;
            t->move(0, newHeight);
            t->resizeFitWidth(newWidth);
            newHeight += t->height() + this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical);
        }

        // update scrollview's contents size (sets scrollbars limits)
        newHeight -= this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical);
        const int oldHeight = q->widget()->height();
        const int oldYCenter = q->verticalScrollBar()->value() + q->viewport()->height() / 2;
        q->widget()->resize(newWidth, newHeight);

        // enable scrollbar when there's something to scroll
        q->verticalScrollBar()->setEnabled(q->viewport()->height() < newHeight);

        // ensure that what was visible before remains visible now
        q->ensureVisible(0, int((qreal)oldYCenter * q->widget()->height() / oldHeight), 0, q->viewport()->height() / 2);
    } else if (e->size().height() <= e->oldSize().height()) {
        return;
    }

    // invalidate the bookmark overlay
    m_bookmarkOverlay = QPixmap {};

    // update Thumbnails since width has changed or height has increased
    delayedRequestVisiblePixmaps(500);
}
// END widget events

// BEGIN internal SLOTS
void ThumbnailListPrivate::slotRequestVisiblePixmaps()
{
    // if an update is already scheduled or the widget is hidden, don't proceed
    if ((m_delayTimer && m_delayTimer->isActive()) || q->isHidden()) {
        return;
    }

    // scroll from the top to the last visible thumbnail
    m_visibleThumbnails.clear();
    QList<Okular::PixmapRequest *> requestedPixmaps;
    QList<ThumbnailWidget *>::const_iterator tIt = m_thumbnails.constBegin(), tEnd = m_thumbnails.constEnd();
    const QRect viewportRect = q->viewport()->rect().translated(q->horizontalScrollBar()->value(), q->verticalScrollBar()->value());
    for (; tIt != tEnd; ++tIt) {
        ThumbnailWidget *t = *tIt;
        const QRect thumbRect = t->rect();
        if (!thumbRect.intersects(viewportRect)) {
            continue;
        }
        // add ThumbnailWidget to visible list
        m_visibleThumbnails.push_back(t);
        // if pixmap not present add it to requests
        if (!t->page()->hasPixmap(q, t->pixmapWidth(), t->pixmapHeight())) {
            Okular::PixmapRequest *p = new Okular::PixmapRequest(q, t->pageNumber(), t->pixmapWidth(), t->pixmapHeight(), devicePixelRatioF(), THUMBNAILS_PRIO, Okular::PixmapRequest::Asynchronous);
            requestedPixmaps.push_back(p);
        }
    }

    // actually request pixmaps
    if (!requestedPixmaps.isEmpty()) {
        m_document->requestPixmaps(requestedPixmaps);
    }
}

void ThumbnailListPrivate::slotDelayTimeout()
{
    // resize the bookmark overlay
    const int expectedWidth = q->viewport()->width() / 4;
    if (expectedWidth > 10) {
        m_bookmarkOverlay = QPixmap(QIcon::fromTheme(QStringLiteral("bookmarks")).pixmap(expectedWidth));
    } else {
        m_bookmarkOverlay = QPixmap {};
    }

    // request pixmaps
    slotRequestVisiblePixmaps();
}
// END internal SLOTS

void ThumbnailListPrivate::delayedRequestVisiblePixmaps(int delayMs)
{
    if (!m_delayTimer) {
        m_delayTimer = new QTimer(q);
        m_delayTimer->setSingleShot(true);
        connect(m_delayTimer, &QTimer::timeout, this, &ThumbnailListPrivate::slotDelayTimeout);
    }
    m_delayTimer->start(delayMs);
}

/** ThumbnailWidget implementation **/

ThumbnailWidget::ThumbnailWidget(ThumbnailListPrivate *parent, const Okular::Page *page)
    : m_parent(parent)
    , m_page(page)
    , m_selected(false)
    , m_pageSelected(false)
    , m_pixmapWidth(10)
    , m_pixmapHeight(10)
{
    m_labelNumber = m_page->number() + 1;
    m_labelHeight = QFontMetrics(m_parent->font()).height();
}

void ThumbnailWidget::resizeFitWidth(int width)
{
    m_pixmapWidth = width - m_margin;
    m_pixmapHeight = qRound(m_page->ratio() * (double)m_pixmapWidth);
    m_rect.setSize(QSize(width, heightHint()));
}

void ThumbnailWidget::setSelected(bool selected)
{
    // update selected state
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void ThumbnailWidget::setPageSelected(bool selected)
{
    if (m_pageSelected != selected) {
        m_pageSelected = selected;
        update();
    }
}

void ThumbnailWidget::setVisibleRect(const Okular::NormalizedRect &rect)
{
    if (rect == m_visibleRect) {
        return;
    }

    m_visibleRect = rect;
    update();
}

void ThumbnailListPrivate::mousePressEvent(QMouseEvent *e)
{
    cancelDrag();

    ThumbnailWidget *item = itemFor(e->pos());
    if (!item) { // mouse on the spacing between items
        e->ignore();
        return;
    }
    if (e->button() != Qt::LeftButton) {
        return;
    }

    m_pressPos = e->pos();
    m_pressedPage = item->pageNumber();

    // in selection mode the check box of a page ticks it
    if (m_selectMode && item->checkBoxRect().contains(e->pos() - item->pos())) {
        m_pressedOnCheckBox = true;
        toggleSelected(m_pressedPage);
        return;
    }

    // holding the page picks it up
    if (m_document->canModifyPages()) {
        m_longPressTimer->start();
    }
}

void ThumbnailListPrivate::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }

    m_longPressTimer->stop();
    if (m_dragging) {
        finishDrag(true);
        return;
    }

    const int pressedPage = m_pressedPage;
    const bool onCheckBox = m_pressedOnCheckBox;
    m_pressedPage = -1;
    m_pressedOnCheckBox = false;
    if (pressedPage < 0 || onCheckBox) {
        return;
    }

    // a plain click shows the page
    const ThumbnailWidget *item = itemFor(e->pos());
    if (item && item->pageNumber() == pressedPage) {
        Q_EMIT q->pageActivated(pressedPage);
    }
}

void ThumbnailListPrivate::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging) {
        updateDrag(e->pos());
        return;
    }

    // moving before the page has been picked up is neither a click nor a drag
    if (m_pressedPage >= 0 && (e->buttons() & Qt::LeftButton) && (e->pos() - m_pressPos).manhattanLength() > QApplication::startDragDistance()) {
        m_longPressTimer->stop();
        m_pressedPage = -1;
    }
}

void ThumbnailListPrivate::contextMenuEvent(QContextMenuEvent *e)
{
    // never let the event bubble up to the parents
    e->accept();

    const ThumbnailWidget *item = itemFor(e->pos());
    if (!item) {
        return;
    }

    cancelDrag();

    if (!m_document->canModifyPages()) {
        // pages cannot be edited in this document: only the generic page menu
        Q_EMIT q->rightClick(item->page(), e->globalPos());
        return;
    }

    const int clickedPage = item->pageNumber();
    const Okular::Page *clickedPagePtr = item->page();
    const int pageCount = static_cast<int>(m_document->pages());

    QMenu menu;
    if (m_selectMode) {
        const QList<int> selectedPages = q->selectedPageNumbers();
        QAction *deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18np("Delete %1 selected page", "Delete %1 selected pages", selectedPages.size()), [this, selectedPages]() { Q_EMIT q->pagesDeleted(selectedPages); });
        deleteAction->setEnabled(!selectedPages.isEmpty() && selectedPages.size() < pageCount);
        menu.addAction(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), i18n("Done selecting"), [this]() { setSelectMode(false); });
    } else {
        QAction *deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete this page"), [this, clickedPage]() { Q_EMIT q->pagesDeleted(QList<int>() << clickedPage); });
        deleteAction->setEnabled(pageCount > 1);
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-select")), i18n("Select"), [this, clickedPage]() {
            setSelectMode(true);
            toggleSelected(clickedPage);
        });
    }

    // let the owner add the generic page actions (bookmarks, fit width, ...)
    Q_EMIT q->menuRequested(&menu, clickedPagePtr, e->globalPos());

    // the thumbnails may be rebuilt by the chosen action: do not use them after this point
    menu.exec(e->globalPos());
}

void ThumbnailWidget::paint(QPainter &p, const QRect _clipRect)
{
    const int itemWidth = m_pixmapWidth + m_margin;
    QRect clipRect = _clipRect;
    const QPalette pal = m_parent->palette();

    // draw the bottom label + highlight mark
    const QColor baseColor = pal.color(QPalette::Active, QPalette::Base);
    const QColor highlightColor = pal.color(QPalette::Active, QPalette::Highlight);
    QColor fillColor = m_selected ? highlightColor : baseColor;
    if (m_pageSelected && !m_selected) {
        // pages of the multi selection get a softer version of the highlight color
        fillColor = QColor((baseColor.red() + highlightColor.red()) / 2, (baseColor.green() + highlightColor.green()) / 2, (baseColor.blue() + highlightColor.blue()) / 2);
    }
    p.fillRect(clipRect, fillColor);
    p.setPen(m_selected ? pal.color(QPalette::Active, QPalette::HighlightedText) : pal.color(QPalette::Active, QPalette::Text));
    p.drawText(0, m_pixmapHeight + (m_margin - 3), itemWidth, m_labelHeight, Qt::AlignCenter, QString::number(m_labelNumber));

    // draw page outline and pixmap
    if (clipRect.top() < m_pixmapHeight + m_margin) {
        // if page is bookmarked draw a colored border
        const bool isBookmarked = m_parent->m_document->bookmarkManager()->isBookmarked(pageNumber());
        // draw the inner rect
        p.setPen(isBookmarked ? QColor(0xFF8000) : Qt::black);
        p.drawRect(m_margin / 2 - 1, m_margin / 2 - 1, m_pixmapWidth + 1, m_pixmapHeight + 1);
        // draw the clear rect
        p.setPen(isBookmarked ? QColor(0x804000) : pal.color(QPalette::Active, QPalette::Base));
        // draw the bottom and right shadow edges
        if (!isBookmarked) {
            int left, right, bottom, top;
            left = m_margin / 2 + 1;
            right = m_margin / 2 + m_pixmapWidth + 1;
            bottom = m_pixmapHeight + m_margin / 2 + 1;
            top = m_margin / 2 + 1;
            p.setPen(Qt::gray);
            p.drawLine(left, bottom, right, bottom);
            p.drawLine(right, top, right, bottom);
        }

        // draw the page using the shared PagePainter class
        p.translate(m_margin / 2.0, m_margin / 2.0);
        clipRect.translate(-m_margin / 2, -m_margin / 2);
        clipRect = clipRect.intersected(QRect(0, 0, m_pixmapWidth, m_pixmapHeight));
        if (clipRect.isValid()) {
            int flags = PagePainter::Accessibility | PagePainter::Highlights | PagePainter::Annotations;
            PagePainter::paintPageOnPainter(&p, m_page, m_parent->q, flags, m_pixmapWidth, m_pixmapHeight, clipRect);
        }

        // in selection mode every page has a check box at its bottom right corner
        if (m_parent->m_selectMode) {
            const QRect box(m_pixmapWidth - 26, m_pixmapHeight - 26, 22, 22);
            p.save();
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(m_pageSelected ? highlightColor : QColor(110, 110, 110), 2));
            p.setBrush(m_pageSelected ? highlightColor : QColor(255, 255, 255, 225));
            p.drawRoundedRect(box, 4, 4);
            if (m_pageSelected) {
                QPainterPath tick;
                tick.moveTo(box.left() + 5, box.center().y() + 1);
                tick.lineTo(box.left() + 9, box.bottom() - 6);
                tick.lineTo(box.right() - 5, box.top() + 6);
                p.setPen(QPen(pal.color(QPalette::Active, QPalette::HighlightedText), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                p.drawPath(tick);
            }
            p.restore();
        }

        // draw the bookmark overlay on the top-right corner
        const QPixmap bookmarkPixmap = m_parent->m_bookmarkOverlay;
        if (isBookmarked && !bookmarkPixmap.isNull()) {
            int pixW = bookmarkPixmap.width(), pixH = bookmarkPixmap.height();
            clipRect = clipRect.intersected(QRect(m_pixmapWidth - pixW, 0, pixW, pixH));
            if (clipRect.isValid()) {
                p.drawPixmap(m_pixmapWidth - pixW, -pixH / 8, bookmarkPixmap);
            }
        }
    }
}

/** ThumbnailsController implementation **/

#define FILTERB_ID 1

ThumbnailController::ThumbnailController(QWidget *parent, ThumbnailList *list)
    : QToolBar(parent)
{
    setObjectName(QStringLiteral("ThumbsControlBar"));
    // change toolbar appearance
    setIconSize(QSize(16, 16));
    setMovable(false);
    QSizePolicy sp = sizePolicy();
    sp.setVerticalPolicy(QSizePolicy::Minimum);
    setSizePolicy(sp);

    // insert a togglebutton [show only bookmarked pages]
    // insertSeparator();
    QAction *showBoomarkOnlyAction = addAction(QIcon::fromTheme(QStringLiteral("bookmarks")), i18n("Show bookmarked pages only"));
    showBoomarkOnlyAction->setCheckable(true);
    connect(showBoomarkOnlyAction, &QAction::toggled, list, &ThumbnailList::slotFilterBookmarks);
    showBoomarkOnlyAction->setChecked(Okular::Settings::filterBookmarks());
    // insertLineSeparator();
}

#include "moc_thumbnaillist.cpp"
#include "thumbnaillist.moc"
