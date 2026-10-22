/*
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _ABOUTDATA_H_
#define _ABOUTDATA_H_

#include <KAboutData>

#include "core/version.h"

#include <KLocalizedString>

inline KAboutData okularAboutData()
{
    KAboutData about(QStringLiteral("charta"),
                     i18n("Charta"),
                     QStringLiteral(CHARTA_VERSION_STRING),
                     i18n("Charta, a document reader and annotator"),
                     KAboutLicense::GPL,
                     i18n("(C) 2002 Wilco Greven, Christophe Devriese\n"
                          "(C) 2004-2005 Enrico Ros\n"
                          "(C) 2005 Piotr Szymanski\n"
                          "(C) 2004-2017 Albert Astals Cid\n"
                          "(C) 2006-2009 Pino Toscano"),
                     QString(),
                     QString());
    about.setDesktopFileName(QStringLiteral("org.charta.reader"));

    about.addAuthor(i18n("Pino Toscano"), i18n("Former maintainer"), QStringLiteral("pino@kde.org"));
    about.addAuthor(i18n("Tobias Koenig"), i18n("Lots of framework work, FictionBook backend and former ODT backend"), QStringLiteral("tokoe@kde.org"));
    about.addAuthor(i18n("Albert Astals Cid"), i18n("Developer"), QStringLiteral("aacid@kde.org"));
    about.addAuthor(i18n("Piotr Szymanski"), i18n("Created the upstream viewer from the KPDF codebase"), QStringLiteral("djurban@pld-dc.org"));
    about.addAuthor(i18n("Enrico Ros"), i18n("KPDF developer"), QStringLiteral("eros.kde@email.it"));
    about.addCredit(i18n("Eugene Trounev"), i18n("Annotations artwork"), QStringLiteral("eugene.trounev@gmail.com"));
    about.addCredit(i18n("Fabio D'Urso"), i18n("Annotation improvements"), QStringLiteral("fabiodurso@hotmail.it"));

    // Charta maintainer. The code in this project was produced with AI tools; see
    // docs/licensing/ and docs/THIRD-PARTY-NOTICES.md. No authorship claim is made for
    // generated code; the maintainer is named as the party applying the project licence.
    about.addAuthor(i18n("Rupam Dutta"), i18n("Charta maintainer"), QStringLiteral("rupamd.work@gmail.com"));

    return about;
}

#endif
