/*
    SPDX-FileCopyrightText: 2006 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationpopup.h"

#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QDomDocument>
#include <QImage>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QPixmap>

#include "annotationpropertiesdialog.h"

#include "core/annotations.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"
#include "core/page.h"
#include "gui/guiutils.h"
#include "okmenutitle.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QUuid>
#include <QUrl>

Q_DECLARE_METATYPE(AnnotationPopup::AnnotPagePair)

namespace
{
class ClipboardImageStampAnnotation final : public Okular::StampAnnotation
{
public:
    explicit ClipboardImageStampAnnotation(const QImage &image)
        : m_imageFile(QDir::tempPath() + QLatin1String("/charta-clipboard-XXXXXX.png"))
    {
        setStampIconName(QString());
        setUniqueName(QStringLiteral("charta-clipboard-image-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (m_imageFile.open() && image.save(&m_imageFile, "PNG") && m_imageFile.flush()) {
            setStampIconName(m_imageFile.fileName());
        }

        Okular::Annotation::Style &style = this->style();
        style.setWidth(0);
    }

    bool isValid() const
    {
        return !stampIconName().isEmpty() && m_imageFile.isOpen();
    }

private:
    QTemporaryFile m_imageFile;
};

QImage clipboardImage(const QMimeData *mimeData)
{
    if (!mimeData) {
        return {};
    }

    const QVariant imageData = mimeData->imageData();
    QImage image;
    if (imageData.canConvert<QImage>()) {
        image = imageData.value<QImage>();
    }
    if (image.isNull() && imageData.canConvert<QPixmap>()) {
        image = imageData.value<QPixmap>().toImage();
    }
    if (!image.isNull()) {
        return image;
    }

    for (const QString &format : mimeData->formats()) {
        if (format.startsWith(QLatin1String("image/"))) {
            image = QImage::fromData(mimeData->data(format));
            if (!image.isNull()) {
                return image;
            }
        }
    }

    return {};
}

bool clipboardContainsImageFormat(const QMimeData *mimeData)
{
    if (!mimeData) {
        return false;
    }
    if (mimeData->hasImage()) {
        return true;
    }
    for (const QString &format : mimeData->formats()) {
        if (format.startsWith(QLatin1String("image/")) || format == QLatin1String("application/x-qt-image")) {
            return true;
        }
    }
    return false;
}

bool annotationHasFileAttachment(Okular::Annotation *annotation)
{
    return (annotation->subType() == Okular::Annotation::AFileAttachment || annotation->subType() == Okular::Annotation::ARichMedia);
}

Okular::EmbeddedFile *embeddedFileFromAnnotation(Okular::Annotation *annotation)
{
    if (annotation->subType() == Okular::Annotation::AFileAttachment) {
        const Okular::FileAttachmentAnnotation *fileAttachAnnot = static_cast<Okular::FileAttachmentAnnotation *>(annotation);
        return fileAttachAnnot->embeddedFile();
    } else if (annotation->subType() == Okular::Annotation::ARichMedia) {
        const Okular::RichMediaAnnotation *richMediaAnnot = static_cast<Okular::RichMediaAnnotation *>(annotation);
        return richMediaAnnot->embeddedFile();
    } else {
        return nullptr;
    }
}

bool annotationSupportsCopy(const Okular::Annotation *annotation)
{
    if (!annotation) {
        return false;
    }

    switch (annotation->subType()) {
    case Okular::Annotation::AGeom:
    case Okular::Annotation::AInk:
    case Okular::Annotation::ALine:
    case Okular::Annotation::AText:
        return true;
        break;

    default:
        return false;
        break;
    }
    return false;
}

bool clipboardFormatVersionSupported(const QDomElement &root)
{
    const QString versionAttributeName = QStringLiteral("version");
    if (!root.hasAttribute(versionAttributeName)) {
        return false;
    }

    bool versionIsNumber = false;
    const int clipboardFormatVersion = root.attribute(versionAttributeName).toInt(&versionIsNumber);
    return versionIsNumber && clipboardFormatVersion == AnnotationPopup::annotationClipboardFormatVersion;
}

}

AnnotationPopup::AnnotationPopup(Okular::Document *document, MenuMode mode, QWidget *parent)
    : mParent(parent)
    , mDocument(document)
    , mMenuMode(mode)
{
}

void AnnotationPopup::addAnnotation(Okular::Annotation *annotation, int pageNumber)
{
    AnnotPagePair pair(annotation, pageNumber);
    if (!mAnnotations.contains(pair)) {
        mAnnotations.append(pair);
    }
}

void AnnotationPopup::exec(const QPoint point)
{
    if (mAnnotations.isEmpty()) {
        return;
    }

    QMenu menu(mParent);

    addActionsToMenu(&menu);

    menu.exec(point.isNull() ? QCursor::pos() : point);
}

void AnnotationPopup::addActionsToMenu(QMenu *menu)
{
    QAction *action = nullptr;

    if (mMenuMode == SingleAnnotationMode) {
        const bool onlyOne = (mAnnotations.count() == 1);

        const AnnotPagePair &pair = mAnnotations.at(0);

        menu->addAction(new OKMenuTitle(menu, i18ncp("Menu title", "Annotation", "%1 Annotations", mAnnotations.count())));

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("comment")), i18n("&Open Pop-up Note"));
        action->setEnabled(onlyOne);
        connect(action, &QAction::triggered, menu, [this, pair] { doOpenAnnotationWindow(pair); });

        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        bool isBookmarked = mDocument->bookmarkManager()->isBookmarked(vp);

        if (isBookmarked) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-remove")), i18n("Remove Bookmark"));
            action->setEnabled(onlyOne);
            connect(action, &QAction::triggered, menu, [this, pair] { doRemoveAnnotationBookmark(pair); });
        } else {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"));
            action->setEnabled(onlyOne);
            connect(action, &QAction::triggered, menu, [this, pair] { doAddAnnotationBookmark(pair); });
        }

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action", "Copy"));
        action->setEnabled(onlyOne && annotationSupportsCopy(pair.annotation));
        connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotation(pair); });

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste"));
        action->setEnabled(onlyOne && mDocument->isAllowed(Okular::AllowNotes) && clipboardHasAnnotations());
        connect(action, &QAction::triggered, menu, [this, pair] { doPasteAnnotation(pair); });

        if (!pair.annotation->contents().isEmpty()) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Text to Clipboard"));
            const bool copyAllowed = mDocument->isAllowed(Okular::AllowCopy);
            if (!copyAllowed) {
                action->setEnabled(false);
                action->setText(i18n("Copy forbidden by DRM"));
            }
            connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotationText(pair); });
        }

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18n("&Delete"));
        action->setEnabled(mDocument->isAllowed(Okular::AllowNotes));
        connect(action, &QAction::triggered, menu, [this] {
            for (const AnnotPagePair &pair : std::as_const(mAnnotations)) {
                doRemovePageAnnotation(pair);
            }
        });

        for (const AnnotPagePair &annot : std::as_const(mAnnotations)) {
            if (!mDocument->canRemovePageAnnotation(annot.annotation)) {
                action->setEnabled(false);
            }
        }

        const Okular::Page *annotationPage = pair.pageNumber >= 0 ? mDocument->page(pair.pageNumber) : nullptr;
        const int annotationStackIndex = annotationPage ? mDocument->annotationIndex(pair.pageNumber, pair.annotation) : -1;
        const int annotationStackLastIndex = annotationPage ? annotationPage->annotations().size() - 1 : -1;
        const bool canStackAnnotation = onlyOne && annotationStackIndex >= 0 && mDocument->canModifyPageAnnotation(pair.annotation);
        QMenu *stackingMenu = menu->addMenu(i18n("Stacking"));
        action = stackingMenu->addAction(QIcon::fromTheme(QStringLiteral("arrow-up")), i18n("Move Up"));
        action->setEnabled(canStackAnnotation && annotationStackIndex < annotationStackLastIndex);
        connect(action, &QAction::triggered, menu, [this, pair] { mDocument->movePageAnnotationUp(pair.pageNumber, pair.annotation); });

        action = stackingMenu->addAction(QIcon::fromTheme(QStringLiteral("arrow-down")), i18n("Move Down"));
        action->setEnabled(canStackAnnotation && annotationStackIndex > 0);
        connect(action, &QAction::triggered, menu, [this, pair] { mDocument->movePageAnnotationDown(pair.pageNumber, pair.annotation); });

        action = stackingMenu->addAction(QIcon::fromTheme(QStringLiteral("go-top")), i18n("Move to Topmost"));
        action->setEnabled(canStackAnnotation && annotationStackIndex < annotationStackLastIndex);
        connect(action, &QAction::triggered, menu, [this, pair] { mDocument->movePageAnnotationToTop(pair.pageNumber, pair.annotation); });

        action = stackingMenu->addAction(QIcon::fromTheme(QStringLiteral("go-bottom")), i18n("Move to Bottommost"));
        action->setEnabled(canStackAnnotation && annotationStackIndex > 0);
        connect(action, &QAction::triggered, menu, [this, pair] { mDocument->movePageAnnotationToBottom(pair.pageNumber, pair.annotation); });

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("&Properties"));
        action->setEnabled(onlyOne);
        connect(action, &QAction::triggered, menu, [this, pair] { doOpenPropertiesDialog(pair); });

        if (onlyOne && annotationHasFileAttachment(pair.annotation)) {
            const Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
            if (embeddedFile) {
                const QString openText = i18nc("%1 is the name of the file to open", "&Open '%1'…", embeddedFile->name());
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), openText);
                connect(action, &QAction::triggered, menu, [this, pair] { doOpenEmbeddedFile(pair); });

                const QString saveText = i18nc("%1 is the name of the file to save", "&Save '%1'…", embeddedFile->name());
                menu->addSeparator();
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), saveText);
                connect(action, &QAction::triggered, menu, [this, pair] { doSaveEmbeddedFile(pair); });
            }
        }
    } else {
        for (const AnnotPagePair &pair : std::as_const(mAnnotations)) {
            menu->addAction(new OKMenuTitle(menu, GuiUtils::captionForAnnotation(pair.annotation)));

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("comment")), i18n("&Open Pop-up Note"));
            connect(action, &QAction::triggered, menu, [this, pair] { doOpenAnnotationWindow(pair); });

            Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
            bool isBookmarked = mDocument->bookmarkManager()->isBookmarked(vp);

            if (isBookmarked) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-remove")), i18n("Remove Bookmark"));
                connect(action, &QAction::triggered, menu, [this, pair] { doRemoveAnnotationBookmark(pair); });
            } else {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"));
                connect(action, &QAction::triggered, menu, [this, pair] { doAddAnnotationBookmark(pair); });
            }

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action", "Copy"));
            action->setEnabled(annotationSupportsCopy(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotation(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && clipboardHasAnnotations());
            connect(action, &QAction::triggered, menu, [this, pair] { doPasteAnnotation(pair); });

            if (!pair.annotation->contents().isEmpty()) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Text to Clipboard"));
                const bool copyAllowed = mDocument->isAllowed(Okular::AllowCopy);
                if (!copyAllowed) {
                    action->setEnabled(false);
                    action->setText(i18n("Copy forbidden by DRM"));
                }
                connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotationText(pair); });
            }

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18n("&Delete"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doRemovePageAnnotation(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("&Properties"));
            connect(action, &QAction::triggered, menu, [this, pair] { doOpenPropertiesDialog(pair); });

            if (annotationHasFileAttachment(pair.annotation)) {
                const Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
                if (embeddedFile) {
                    const QString openText = i18nc("%1 is the name of the file to open", "&Open '%1'…", embeddedFile->name());
                    action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), openText);
                    connect(action, &QAction::triggered, menu, [this, pair] { doOpenEmbeddedFile(pair); });

                    const QString saveText = i18nc("%1 is the name of the file to save", "&Save '%1'…", embeddedFile->name());
                    menu->addSeparator();
                    action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), saveText);
                    connect(action, &QAction::triggered, menu, [this, pair] { doSaveEmbeddedFile(pair); });
                }
            }
        }
    }
}

void AnnotationPopup::doCopyAnnotation(AnnotPagePair pair)
{
    if (mAnnotations.isEmpty() || !annotationSupportsCopy(pair.annotation)) {
        return;
    }

    doCopyAnnotations();
}

void AnnotationPopup::doCopyAnnotations()
{
    if (mAnnotations.isEmpty() || std::any_of(mAnnotations.cbegin(), mAnnotations.cend(), [](const AnnotPagePair &pair) { return !annotationSupportsCopy(pair.annotation); })) {
        return;
    }

    QDomDocument document(QStringLiteral("okular-annotations"));
    QDomElement root = document.createElement(QStringLiteral("annotations"));
    root.setAttribute(QStringLiteral("version"), AnnotationPopup::annotationClipboardFormatVersion);
    document.appendChild(root);

    for (const AnnotPagePair &pair : std::as_const(mAnnotations)) {
        QDomElement annotationElement = document.createElement(QStringLiteral("annotation"));
        Okular::AnnotationUtils::storeAnnotation(pair.annotation, annotationElement, document);
        root.appendChild(annotationElement);
    }

    auto *mimeData = new QMimeData();
    mimeData->setData(QLatin1String(annotationClipboardMimeType), document.toByteArray());
    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
}

void AnnotationPopup::doPasteAnnotation(AnnotPagePair pair)
{
    pasteAnnotationToPage(pair.pageNumber);
}

void AnnotationPopup::doCopyAnnotationText(AnnotPagePair pair)
{
    const QString text = pair.annotation->contents();
    if (!text.isEmpty()) {
        QClipboard *cb = QApplication::clipboard();
        cb->setText(text, QClipboard::Clipboard);
    }
}

QList<Okular::Annotation *> AnnotationPopup::pasteAnnotationToPage(int pageNumber, const Okular::NormalizedPoint *targetPoint)
{
    if (pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes)) {
        return {};
    }

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasFormat(QLatin1String(annotationClipboardMimeType))) {
        return {};
    }

    QDomDocument document;
    if (!document.setContent(mimeData->data(QLatin1String(annotationClipboardMimeType)))) {
        return {};
    }

    QDomElement root = document.documentElement();
    if (root.tagName() != QLatin1String("annotations")) {
        return {};
    }
    if (!clipboardFormatVersionSupported(root)) {
        return {};
    }

    QList<Okular::Annotation *> annotations;
    Okular::NormalizedRect unionRect;
    bool hasUnionRect = false;
    for (QDomElement element = root.firstChildElement(QStringLiteral("annotation")); !element.isNull(); element = element.nextSiblingElement(QStringLiteral("annotation"))) {
        Okular::Annotation *annotation = Okular::AnnotationUtils::createAnnotation(element);
        if (!annotation) {
            continue;
        }

        // Avoid duplicate uniqueName collisions when pasting annotations.
        annotation->setUniqueName(QString());

        const Okular::NormalizedRect rect = annotation->boundingRectangle();
        if (!rect.isNull()) {
            unionRect = hasUnionRect ? (unionRect | rect) : rect;
            hasUnionRect = true;
        }
        annotations.append(annotation);
    }

    if (annotations.isEmpty()) {
        return {};
    }

    Okular::NormalizedPoint offset(0.02, 0.02);
    if (targetPoint && hasUnionRect) {
        const double centerX = (unionRect.left + unionRect.right) / 2.0;
        const double centerY = (unionRect.top + unionRect.bottom) / 2.0;
        offset = Okular::NormalizedPoint(targetPoint->x - centerX, targetPoint->y - centerY);
    }

    if (annotations.size() > 1) {
        mDocument->beginUndoMacro(i18n("paste annotations"));
    }
    for (Okular::Annotation *annotation : std::as_const(annotations)) {
        annotation->translate(offset);
        mDocument->addPageAnnotation(pageNumber, annotation);
    }
    if (annotations.size() > 1) {
        mDocument->endUndoMacro();
    }

    return annotations;
}

QList<Okular::Annotation *> AnnotationPopup::pasteClipboardImageToPage(int pageNumber, const Okular::NormalizedPoint *targetPoint)
{
    if (pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes)) {
        return {};
    }

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    const QImage image = clipboardImage(mimeData);
    if (image.isNull()) {
        if (clipboardContainsImageFormat(mimeData)) {
            qWarning() << "Clipboard image data is in an unsupported or unreadable format";
        }
        return {};
    }

    const Okular::Page *page = mDocument->page(pageNumber);
    if (!page || page->width() <= 0 || page->height() <= 0) {
        return {};
    }

    const double imageAspectRatio = static_cast<double>(image.width()) / image.height();
    const double targetWidthPoints = qMin(page->width() * 0.25, page->height() * 0.4 * imageAspectRatio);
    const double width = targetWidthPoints / page->width();
    const double height = targetWidthPoints / imageAspectRatio / page->height();
    const Okular::NormalizedPoint center = targetPoint ? *targetPoint : Okular::NormalizedPoint(0.5, 0.5);
    const double centerX = qBound(width / 2.0, center.x, 1.0 - width / 2.0);
    const double centerY = qBound(height / 2.0, center.y, 1.0 - height / 2.0);

    auto *annotation = new ClipboardImageStampAnnotation(image);
    if (!annotation->isValid()) {
        delete annotation;
        qWarning() << "Unable to create a temporary image for clipboard paste";
        return {};
    }

    annotation->setBoundingRectangle(Okular::NormalizedRect(centerX - width / 2.0, centerY - height / 2.0, centerX + width / 2.0, centerY + height / 2.0));
    annotation->setFlags(annotation->flags() & ~Okular::Annotation::DenyWrite);
    mDocument->addPageAnnotation(pageNumber, annotation);
    return {annotation};
}

bool AnnotationPopup::clipboardHasAnnotations()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    return mimeData && mimeData->hasFormat(QLatin1String(annotationClipboardMimeType));
}

void AnnotationPopup::doRemovePageAnnotation(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        mDocument->removePageAnnotation(pair.pageNumber, pair.annotation);
    }
}

void AnnotationPopup::doOpenAnnotationWindow(AnnotPagePair pair)
{
    Q_EMIT openAnnotationWindow(pair.annotation, pair.pageNumber);
}

void AnnotationPopup::doOpenPropertiesDialog(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        AnnotsPropertiesDialog propdialog(mParent, mDocument, pair.pageNumber, pair.annotation);
        propdialog.exec();
    }
}

void AnnotationPopup::doSaveEmbeddedFile(AnnotPagePair pair)
{
    Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
    GuiUtils::saveEmbeddedFile(embeddedFile, mParent);
}

void AnnotationPopup::doOpenEmbeddedFile(AnnotPagePair pair)
{
    Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
    if (!embeddedFile) {
        return;
    }
    // preserve the file extension so the OS knows which app to use
    const QString name = embeddedFile->name();
    const QString extension = QFileInfo(name).suffix();
    const QString templateName = QDir::tempPath() + QLatin1Char('/') + QFileInfo(name).baseName() + QStringLiteral(".XXXXXX.") + extension;
    QTemporaryFile tempFile(templateName);
    tempFile.setAutoRemove(false);

    if (tempFile.open()) {
        tempFile.write(embeddedFile->data());
        tempFile.setPermissions(QFile::ReadOwner);
        const QString fileName = tempFile.fileName();
        tempFile.close();
        auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(fileName));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, mParent));
        job->start();
    }
}

// This code comes from Reviews::activated in side_reviews.cpp
Okular::DocumentViewport AnnotationPopup::calculateAnnotationViewport(AnnotPagePair pair) const
{
    Okular::DocumentViewport vp;

    const Okular::Page *page = mDocument->page(pair.pageNumber);
    if (!page || !pair.annotation) {
        return vp; // Return empty viewport on error
    }

    QRect rect = Okular::AnnotationUtils::annotationGeometry(pair.annotation, page->width(), page->height());
    Okular::NormalizedRect nr(rect, (int)page->width(), (int)page->height());

    vp.pageNumber = pair.pageNumber;
    vp.rePos.enabled = true;
    vp.rePos.pos = Okular::DocumentViewport::Center;
    vp.rePos.normalizedX = (nr.right + nr.left) / 2.0;
    vp.rePos.normalizedY = (nr.bottom + nr.top) / 2.0;

    return vp;
}

void AnnotationPopup::doAddAnnotationBookmark(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        QString title = pair.annotation->contents();
        if (title.isEmpty()) {
            mDocument->bookmarkManager()->addBookmark(mDocument->currentDocument(), vp);
        } else {
            mDocument->bookmarkManager()->addBookmark(mDocument->currentDocument(), vp, title);
        }
    }
}

void AnnotationPopup::doRemoveAnnotationBookmark(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        mDocument->bookmarkManager()->removeBookmark(vp);
    }
}

#include "moc_annotationpopup.cpp"
