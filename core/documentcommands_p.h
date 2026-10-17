/*
    SPDX-FileCopyrightText: 2013 Jon Mease <jon.mease@gmail.com>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_DOCUMENT_COMMANDS_P_H_
#define _OKULAR_DOCUMENT_COMMANDS_P_H_

#include <QDomNode>
#include <QUndoCommand>

#include "area.h"

namespace Okular
{
class Document;
class Annotation;
class DocumentPrivate;
class FormFieldText;
class FormFieldButton;
class FormFieldChoice;
class Page;

class OkularUndoCommand : public QUndoCommand
{
public:
    virtual bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) = 0;
};

class AddAnnotationCommand : public OkularUndoCommand
{
public:
    AddAnnotationCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, int index);

    ~AddAnnotationCommand() override;

    void undo() override;

    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    int m_index;
    bool m_done;
};

class RemoveAnnotationCommand : public OkularUndoCommand
{
public:
    RemoveAnnotationCommand(Okular::DocumentPrivate *doc, Okular::Annotation *annotation, int pageNumber);
    ~RemoveAnnotationCommand() override;
    void undo() override;
    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    int m_oldIndex;
    bool m_done;
};

class ReorderAnnotationCommand : public OkularUndoCommand
{
public:
    ReorderAnnotationCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, int oldIndex, int newIndex, int oldNativeIndex, int newNativeIndex);
    void undo() override;
    void redo() override;
    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    int m_oldIndex;
    int m_newIndex;
    int m_oldNativeIndex;
    int m_newNativeIndex;
};

class ModifyAnnotationPropertiesCommand : public OkularUndoCommand
{
public:
    ModifyAnnotationPropertiesCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, const QDomNode &oldProperties, const QDomNode &newProperties);

    void undo() override;
    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    QDomNode m_prevProperties;
    QDomNode m_newProperties;
};

class TranslateAnnotationCommand : public OkularUndoCommand
{
public:
    TranslateAnnotationCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, const Okular::NormalizedPoint &delta, bool completeDrag);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *uc) override;
    Okular::NormalizedPoint minusDelta();
    Okular::NormalizedRect translateBoundingRectangle(const Okular::NormalizedPoint &delta);

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    Okular::NormalizedPoint m_delta;
    bool m_completeDrag;
};

class AdjustAnnotationCommand : public OkularUndoCommand
{
public:
    AdjustAnnotationCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, const Okular::NormalizedPoint &delta1, const Okular::NormalizedPoint &delta2, bool completeDrag);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *uc) override;
    Okular::NormalizedRect adjustBoundingRectangle(const Okular::NormalizedPoint &delta1, const Okular::NormalizedPoint &delta2);

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
    Okular::NormalizedPoint m_delta1;
    Okular::NormalizedPoint m_delta2;
    bool m_completeDrag;
};

class EditTextCommand : public OkularUndoCommand
{
public:
    EditTextCommand(const QString &newContents, int newCursorPos, const QString &prevContents, int prevCursorPos, int prevAnchorPos);

    void undo() override = 0;
    void redo() override = 0;
    int id() const override = 0;
    bool mergeWith(const QUndoCommand *uc) override;

private:
    enum EditType {
        CharBackspace, ///< Edit made up of one or more single character backspace operations
        CharDelete,    ///< Edit made up of one or more single character delete operations
        CharInsert,    ///< Edit made up of one or more single character insertion operations
        OtherEdit      ///< All other edit operations (these will not be merged together)
    };

    QString oldContentsLeftOfCursor();
    QString newContentsLeftOfCursor();
    QString oldContentsRightOfCursor();
    QString newContentsRightOfCursor();

protected:
    QString m_newContents;
    int m_newCursorPos;
    QString m_prevContents;
    int m_prevCursorPos;
    int m_prevAnchorPos;
    EditType m_editType;
};

class EditAnnotationContentsCommand : public EditTextCommand
{
public:
    EditAnnotationContentsCommand(Okular::DocumentPrivate *docPriv, Okular::Annotation *annotation, int pageNumber, const QString &newContents, int newCursorPos, const QString &prevContents, int prevCursorPos, int prevAnchorPos);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *uc) override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::Annotation *m_annotation;
    int m_pageNumber;
};

class EditFormTextCommand : public EditTextCommand
{
public:
    EditFormTextCommand(Okular::DocumentPrivate *docPriv, Okular::FormFieldText *form, int pageNumber, const QString &newContents, int newCursorPos, const QString &prevContents, int prevCursorPos, int prevAnchorPos);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *uc) override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    Okular::FormFieldText *m_form;
    int m_pageNumber;
};

class EditFormListCommand : public OkularUndoCommand
{
public:
    EditFormListCommand(Okular::DocumentPrivate *docPriv, FormFieldChoice *form, int pageNumber, const QList<int> &newChoices, const QList<int> &prevChoices);

    void undo() override;
    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    FormFieldChoice *m_form;
    int m_pageNumber;
    QList<int> m_newChoices;
    QList<int> m_prevChoices;
};

class EditFormComboCommand : public EditTextCommand
{
public:
    EditFormComboCommand(Okular::DocumentPrivate *docPriv, FormFieldChoice *form, int pageNumber, const QString &newContents, int newCursorPos, const QString &prevContents, int prevCursorPos, int prevAnchorPos);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *uc) override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    Okular::DocumentPrivate *m_docPriv;
    FormFieldChoice *m_form;
    int m_pageNumber;
    int m_newIndex;
    int m_prevIndex;
};

/**
 * Undoable change of the page layout of the document: pages inserted, removed or moved.
 *
 * It keeps the whole page order before and after the change. The pages that are not part of the document in the
 * current state (deleted pages when the command is applied, inserted pages when it is undone) are kept alive here,
 * so that undoing brings back the very same pages, annotations included, and are deleted with the command, that is
 * when the history is cleared (the document is closed or reloaded) or the redo branch is discarded.
 */
class PageStructureCommand : public OkularUndoCommand
{
public:
    PageStructureCommand(Okular::DocumentPrivate *docPriv, const QList<Okular::Page *> &before, const QList<Okular::Page *> &after, const QString &text);
    ~PageStructureCommand() override;

    void undo() override;
    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

    bool hasDeletedOriginalPages() const;

private:
    Okular::DocumentPrivate *m_docPriv;
    QList<Okular::Page *> m_before;
    QList<Okular::Page *> m_after;
    bool m_applied;
};

class EditFormButtonsCommand : public OkularUndoCommand
{
public:
    EditFormButtonsCommand(Okular::DocumentPrivate *docPriv, int pageNumber, const QList<FormFieldButton *> &formButtons, const QList<bool> &newButtonStates);

    void undo() override;
    void redo() override;

    bool refreshInternalPageReferences(const QList<Okular::Page *> &newPagesVector) override;

private:
    void clearFormButtonStates();

private:
    Okular::DocumentPrivate *m_docPriv;
    int m_pageNumber;
    QList<FormFieldButton *> m_formButtons;
    QList<int> m_pageNumbers;
    QList<bool> m_newButtonStates;
    QList<bool> m_prevButtonStates;
};

}
#endif
