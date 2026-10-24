/*
    SPDX-FileCopyrightText: 2004-2005 Enrico Ros <eros.kde@email.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "action.h"

// kde includes
#include <KLocalizedString>

// local includes
#include "document.h"
#include "movie.h"
#include "sound.h"
#include "sourcereference_p.h"

using namespace Okular;

class Okular::ActionPrivate
{
public:
    ActionPrivate()
    {
    }

    virtual ~ActionPrivate()
    {
        qDeleteAll(m_nextActions);
    }

    ActionPrivate(const ActionPrivate &) = delete;
    ActionPrivate &operator=(const ActionPrivate &) = delete;

    QVariant m_nativeId;
    std::shared_ptr<const void> m_nativeHandle;
    QList<Action *> m_nextActions;
};

Action::Action(ActionPrivate &dd)
    : d_ptr(&dd)
{
}

Action::~Action()
{
    delete d_ptr;
}

QString Action::actionTip() const
{
    return QLatin1String("");
}

Action *Action::clone() const
{
    // Base implementation: subclasses must override to provide a real deep copy.
    return nullptr;
}

void Action::setNativeId(const QVariant &id)
{
    Q_D(Action);
    d->m_nativeId = id;
}

QVariant Action::nativeId() const
{
    Q_D(const Action);
    return d->m_nativeId;
}

QList<Action *> Action::nextActions() const
{
    Q_D(const Action);
    return d->m_nextActions;
}

void Action::setNativeHandle(std::shared_ptr<const void> handle)
{
    Q_D(Action);
    d->m_nativeHandle = std::move(handle);
}
const void *Action::nativeHandle() const
{
    Q_D(const Action);
    return d->m_nativeHandle.get();
}

void Action::setNextActions(const QList<Action *> &actions)
{
    Q_D(Action);
    qDeleteAll(d->m_nextActions);
    d->m_nextActions = actions;
}

// GotoAction

class Okular::GotoActionPrivate : public Okular::ActionPrivate
{
public:
    GotoActionPrivate(const QString &fileName, const DocumentViewport &viewport)
        : ActionPrivate()
        , m_extFileName(fileName)
        , m_vp(viewport)
    {
    }

    GotoActionPrivate(const QString &fileName, const QString &namedDestination)
        : ActionPrivate()
        , m_extFileName(fileName)
        , m_dest(namedDestination)
    {
    }

    QString m_extFileName;
    DocumentViewport m_vp;
    QString m_dest;
};

GotoAction::GotoAction(const QString &fileName, const DocumentViewport &viewport)
    : Action(*new GotoActionPrivate(fileName, viewport))
{
}

GotoAction::GotoAction(const QString &fileName, const QString &namedDestination)
    : Action(*new GotoActionPrivate(fileName, namedDestination))
{
}

GotoAction::~GotoAction()
{
}

Action *GotoAction::clone() const
{
    Q_D(const GotoAction);
    GotoAction *copy;
    if (!d->m_dest.isEmpty()) {
        copy = new GotoAction(d->m_extFileName, d->m_dest);
    } else {
        copy = new GotoAction(d->m_extFileName, d->m_vp);
    }
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType GotoAction::actionType() const
{
    return Goto;
}

QString GotoAction::actionTip() const
{
    Q_D(const GotoAction);
    return d->m_extFileName.isEmpty() ? (d->m_vp.isValid() ? i18n("Go to page %1", d->m_vp.pageNumber + 1) : QLatin1String("")) : i18n("Open external file");
}

bool GotoAction::isExternal() const
{
    Q_D(const GotoAction);
    return !d->m_extFileName.isEmpty();
}

QString GotoAction::fileName() const
{
    Q_D(const GotoAction);
    return d->m_extFileName;
}

DocumentViewport GotoAction::destViewport() const
{
    Q_D(const GotoAction);
    return d->m_vp;
}

QString GotoAction::destinationName() const
{
    Q_D(const GotoAction);
    return d->m_dest;
}

// ExecuteAction

class Okular::ExecuteActionPrivate : public Okular::ActionPrivate
{
public:
    ExecuteActionPrivate(const QString &file, const QString &parameters)
        : ActionPrivate()
        , m_fileName(file)
        , m_parameters(parameters)
    {
    }

    QString m_fileName;
    QString m_parameters;
};

ExecuteAction::ExecuteAction(const QString &file, const QString &parameters)
    : Action(*new ExecuteActionPrivate(file, parameters))
{
}

ExecuteAction::~ExecuteAction()
{
}

Action *ExecuteAction::clone() const
{
    Q_D(const ExecuteAction);
    ExecuteAction *copy = new ExecuteAction(d->m_fileName, d->m_parameters);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType ExecuteAction::actionType() const
{
    return Execute;
}

QString ExecuteAction::actionTip() const
{
    Q_D(const Okular::ExecuteAction);
    return i18n("Execute '%1'…", d->m_fileName);
}

QString ExecuteAction::fileName() const
{
    Q_D(const Okular::ExecuteAction);
    return d->m_fileName;
}

QString ExecuteAction::parameters() const
{
    Q_D(const Okular::ExecuteAction);
    return d->m_parameters;
}

// BrowseAction

class Okular::BrowseActionPrivate : public Okular::ActionPrivate
{
public:
    explicit BrowseActionPrivate(const QUrl &url)
        : ActionPrivate()
        , m_url(url)
    {
    }

    QUrl m_url;
};

BrowseAction::BrowseAction(const QUrl &url)
    : Action(*new BrowseActionPrivate(url))
{
}

BrowseAction::~BrowseAction()
{
}

Action *BrowseAction::clone() const
{
    Q_D(const BrowseAction);
    BrowseAction *copy = new BrowseAction(d->m_url);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType BrowseAction::actionType() const
{
    return Browse;
}

QString BrowseAction::actionTip() const
{
    Q_D(const Okular::BrowseAction);
    if (auto ref = extractLilyPondSourceReference(d->m_url)) {
        return sourceReferenceToolTip(*ref);
    }
    return d->m_url.toDisplayString();
}

QUrl BrowseAction::url() const
{
    Q_D(const Okular::BrowseAction);
    return d->m_url;
}

// DocumentAction

class Okular::DocumentActionPrivate : public Okular::ActionPrivate
{
public:
    explicit DocumentActionPrivate(enum DocumentAction::DocumentActionType documentActionType)
        : ActionPrivate()
        , m_type(documentActionType)
    {
    }

    DocumentAction::DocumentActionType m_type;
};

DocumentAction::DocumentAction(enum DocumentActionType documentActionType)
    : Action(*new DocumentActionPrivate(documentActionType))
{
}

DocumentAction::~DocumentAction()
{
}

Action *DocumentAction::clone() const
{
    Q_D(const DocumentAction);
    DocumentAction *copy = new DocumentAction(d->m_type);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

DocumentAction::DocumentActionType DocumentAction::documentActionType() const
{
    Q_D(const Okular::DocumentAction);
    return d->m_type;
}

Action::ActionType DocumentAction::actionType() const
{
    return DocAction;
}

QString DocumentAction::actionTip() const
{
    Q_D(const Okular::DocumentAction);
    switch (d->m_type) {
    case PageFirst:
        return i18n("First Page");
    case PagePrev:
        return i18n("Previous Page");
    case PageNext:
        return i18n("Next Page");
    case PageLast:
        return i18n("Last Page");
    case HistoryBack:
        return i18n("Back");
    case HistoryForward:
        return i18n("Forward");
    case Quit:
        return i18n("Quit");
    case Presentation:
        return i18n("Start Presentation");
    case EndPresentation:
        return i18n("End Presentation");
    case Find:
        return i18n("Find…");
    case GoToPage:
        return i18n("Go To Page…");
    case Close:
    default:;
    }

    return QString();
}

// SoundAction

class Okular::SoundActionPrivate : public Okular::ActionPrivate
{
public:
    SoundActionPrivate(double volume, bool sync, bool repeat, bool mix, Okular::Sound *sound)
        : ActionPrivate()
        , m_volume(volume)
        , m_sync(sync)
        , m_repeat(repeat)
        , m_mix(mix)
        , m_sound(sound)
    {
    }

    ~SoundActionPrivate() override
    {
        delete m_sound;
    }

    double m_volume;
    bool m_sync : 1;
    bool m_repeat : 1;
    bool m_mix : 1;
    Okular::Sound *m_sound;
};

SoundAction::SoundAction(double volume, bool sync, bool repeat, bool mix, Okular::Sound *sound)
    : Action(*new SoundActionPrivate(volume, sync, repeat, mix, sound))
{
}

SoundAction::~SoundAction()
{
}

Action *SoundAction::clone() const
{
    Q_D(const SoundAction);
    Okular::Sound *sound = nullptr;
    if (d->m_sound) {
        // Re-create the sound so the clone does not share the original's Sound object.
        if (d->m_sound->soundType() == Okular::Sound::Embedded) {
            sound = new Okular::Sound(d->m_sound->data());
        } else {
            sound = new Okular::Sound(d->m_sound->url());
        }
        sound->setSamplingRate(d->m_sound->samplingRate());
        sound->setChannels(d->m_sound->channels());
        sound->setBitsPerSample(d->m_sound->bitsPerSample());
        sound->setSoundEncoding(d->m_sound->soundEncoding());
    }
    SoundAction *copy = new SoundAction(d->m_volume, d->m_sync, d->m_repeat, d->m_mix, sound);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType SoundAction::actionType() const
{
    return Sound;
}

QString SoundAction::actionTip() const
{
    return i18n("Play sound…");
}

double SoundAction::volume() const
{
    Q_D(const Okular::SoundAction);
    return d->m_volume;
}

bool SoundAction::synchronous() const
{
    Q_D(const Okular::SoundAction);
    return d->m_sync;
}

bool SoundAction::repeat() const
{
    Q_D(const Okular::SoundAction);
    return d->m_repeat;
}

bool SoundAction::mix() const
{
    Q_D(const Okular::SoundAction);
    return d->m_mix;
}

Okular::Sound *SoundAction::sound() const
{
    Q_D(const Okular::SoundAction);
    return d->m_sound;
}

// ScriptAction

class Okular::ScriptActionPrivate : public Okular::ActionPrivate
{
public:
    ScriptActionPrivate(enum ScriptType type, const QString &script)
        : ActionPrivate()
        , m_scriptType(type)
        , m_script(script)
    {
    }

    ScriptType m_scriptType;
    QString m_script;
};

ScriptAction::ScriptAction(enum ScriptType type, const QString &script)
    : Action(*new ScriptActionPrivate(type, script))
{
}

ScriptAction::~ScriptAction()
{
}

Action *ScriptAction::clone() const
{
    Q_D(const ScriptAction);
    ScriptAction *copy = new ScriptAction(d->m_scriptType, d->m_script);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType ScriptAction::actionType() const
{
    return Script;
}

QString ScriptAction::actionTip() const
{
    Q_D(const Okular::ScriptAction);
    switch (d->m_scriptType) {
    case JavaScript:
        return i18n("JavaScript Script");
    }

    return QString();
}

ScriptType ScriptAction::scriptType() const
{
    Q_D(const Okular::ScriptAction);
    return d->m_scriptType;
}

QString ScriptAction::script() const
{
    Q_D(const Okular::ScriptAction);
    return d->m_script;
}

// MovieAction

class Okular::MovieActionPrivate : public Okular::ActionPrivate
{
public:
    explicit MovieActionPrivate(MovieAction::OperationType operation)
        : ActionPrivate()
        , m_operation(operation)
        , m_annotation(nullptr)
    {
    }

    MovieAction::OperationType m_operation;
    MovieAnnotation *m_annotation;
};

MovieAction::MovieAction(OperationType operation)
    : Action(*new MovieActionPrivate(operation))
{
}

MovieAction::~MovieAction()
{
}

Action *MovieAction::clone() const
{
    Q_D(const MovieAction);
    // The associated annotation is a back-pointer owned by the annotation itself: not copied.
    MovieAction *copy = new MovieAction(d->m_operation);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType MovieAction::actionType() const
{
    return Movie;
}

QString MovieAction::actionTip() const
{
    return i18n("Play movie…");
}

MovieAction::OperationType MovieAction::operation() const
{
    Q_D(const Okular::MovieAction);
    return d->m_operation;
}

void MovieAction::setAnnotation(MovieAnnotation *annotation)
{
    Q_D(Okular::MovieAction);
    d->m_annotation = annotation;
}

MovieAnnotation *MovieAction::annotation() const
{
    Q_D(const Okular::MovieAction);
    return d->m_annotation;
}

// RenditionAction

class Okular::RenditionActionPrivate : public Okular::ActionPrivate
{
public:
    RenditionActionPrivate(RenditionAction::OperationType operation, Okular::Movie *movie, enum ScriptType scriptType, const QString &script)
        : ActionPrivate()
        , m_operation(operation)
        , m_movie(movie)
        , m_scriptType(scriptType)
        , m_script(script)
        , m_annotation(nullptr)
    {
    }

    RenditionAction::OperationType m_operation;
    Okular::Movie *m_movie;
    ScriptType m_scriptType;
    QString m_script;
    ScreenAnnotation *m_annotation;
};

RenditionAction::RenditionAction(OperationType operation, Okular::Movie *movie, enum ScriptType scriptType, const QString &script)
    : Action(*new RenditionActionPrivate(operation, movie, scriptType, script))
{
}

RenditionAction::~RenditionAction()
{
}

Action *RenditionAction::clone() const
{
    Q_D(const RenditionAction);
    // The associated movie is owned by the screen annotation: shared, not deep-copied here.
    // The associated annotation is a back-pointer: not copied.
    RenditionAction *copy = new RenditionAction(d->m_operation, d->m_movie, d->m_scriptType, d->m_script);
    QList<Action *> nexts;
    for (Action *a : nextActions()) {
        nexts.append(a->clone());
    }
    copy->setNextActions(nexts);
    return copy;
}

Action::ActionType RenditionAction::actionType() const
{
    return Rendition;
}

QString RenditionAction::actionTip() const
{
    Q_D(const Okular::RenditionAction);

    switch (d->m_operation) {
    default:
    case None:
        switch (d->m_scriptType) {
        case JavaScript:
            return i18n("JavaScript Script");
        default:
            return QString();
        }
    case Play:
        return i18n("Play movie");
    case Stop:
        return i18n("Stop movie");
    case Pause:
        return i18n("Pause movie");
    case Resume:
        return i18n("Resume movie");
    }
}

RenditionAction::OperationType RenditionAction::operation() const
{
    Q_D(const Okular::RenditionAction);
    return d->m_operation;
}

Okular::Movie *RenditionAction::movie() const
{
    Q_D(const Okular::RenditionAction);
    return d->m_movie;
}

ScriptType RenditionAction::scriptType() const
{
    Q_D(const Okular::RenditionAction);
    return d->m_scriptType;
}

QString RenditionAction::script() const
{
    Q_D(const Okular::RenditionAction);
    return d->m_script;
}

void RenditionAction::setAnnotation(ScreenAnnotation *annotation)
{
    Q_D(Okular::RenditionAction);
    d->m_annotation = annotation;
}

ScreenAnnotation *RenditionAction::annotation() const
{
    Q_D(const Okular::RenditionAction);
    return d->m_annotation;
}

BackendOpaqueAction::BackendOpaqueAction()
    : Action(*new ActionPrivate())
{
}

Action *BackendOpaqueAction::clone() const
{
    BackendOpaqueAction *clone = new BackendOpaqueAction();
    QList<Action *> clonedNextActions;
    for (Action *action : nextActions()) {
        if (Action *clonedAction = action ? action->clone() : nullptr) {
            clonedNextActions.append(clonedAction);
        }
    }
    clone->setNextActions(clonedNextActions);
    return clone;
}

Action::ActionType BackendOpaqueAction::actionType() const
{
    return BackendOpaque;
}
