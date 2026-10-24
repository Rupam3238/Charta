/*
    SPDX-FileCopyrightText: 2012 Fabio D 'Urso <fabiodurso@hotmail.it>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_GENERATOR_PDF_ANNOTS_H_
#define _OKULAR_GENERATOR_PDF_ANNOTS_H_

#include <poppler-annotation.h>
#include <poppler-qt6.h>
#include <poppler-version.h>

#include <QMutex>
#include <QString>

#include <map>
#include <memory>

#include "core/annotations.h"

extern Okular::Annotation *createAnnotationFromPopplerAnnotation(Poppler::Annotation *popplerAnnotation, const Poppler::Page &popplerPage, bool *doDelete);
extern Poppler::Annotation *createPopplerAnnotationFromOkularAnnotation(const Okular::Annotation *okl_ann, const Poppler::Page *page, Poppler::Document *doc);

// Restores the oriented frame from the patched Poppler metadata. Must be
// called after the Okular annotation has been attached to its page.
extern void applyPopplerUserRotationToOkularAnnotation(Okular::Annotation *okularAnnotation, Poppler::Annotation *popplerAnnotation, const Okular::Page *page, const Poppler::Page *popplerPage);

class PopplerAnnotationProxy : public Okular::AnnotationProxy, public Okular::AnnotationOrderProxy
{
public:
    PopplerAnnotationProxy(Poppler::Document *doc, QMutex *userMutex, QHash<Okular::Annotation *, Poppler::Annotation *> *annotsOnOpenHash);
    ~PopplerAnnotationProxy() override;

    bool supports(Capability capability) const override;
    void notifyAddition(Okular::Annotation *okl_ann, int page) override;
    void notifyModification(const Okular::Annotation *okl_ann, int page, bool appearanceChanged) override;
    void notifyRemoval(Okular::Annotation *okl_ann, int page) override;
    int annotationIndex(int page, const Okular::Annotation *annotation) const override;
    int annotationCount(int page) const override;
    bool moveAnnotationToIndex(int page, Okular::Annotation *annotation, int index) override;

private:
    Poppler::Document *ppl_doc;
    QMutex *mutex;
    QHash<Okular::Annotation *, Poppler::Annotation *> *annotationsOnOpenHash;
    // Appearances of removed annotations, so that undoing the removal brings back the exact same look.
    // Keyed by the annotation's unique name and NOT by its address: an Okular::Annotation that is removed and then
    // never restored gets deleted, and a stroke drawn later can be allocated at the very same address. With a pointer
    // key the new stroke found the stale entry and was given the appearance of the deleted one.
    std::map<QString, std::unique_ptr<Poppler::AnnotationAppearance>> deletedAnnotationAppearances;
};

#endif
