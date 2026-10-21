# Trademark and naming (Phase 7)

**Decision:** D12 — rename away from the Okular identity before a public release.
**Evidence class:** `E1` (read from the tree).
**Date:** 2026-10-21

**This is not a copyright question and it is not legal advice.** It is the part of the identity
work that the licence plan cannot decide, so it is written down for a human.

## 1. What is in the tree today

| Kind | Identifier | Count |
| --- | --- | --- |
| D-Bus service | `org.kde.okular` (+ per-process `org.kde.okular-<pid>`, `org.kde.okular.Instance_`) | 8 code/config sites |
| D-Bus paths/interfaces | `/okularshell`, interface `org.kde.okular` | 3 sites |
| Component ids | `org.kde.okular-{comicbook,djvu,dvi,epub,fax,fb,kimgio,md,mobipocket,poppler,spectre,tiff,txt,xps}` | 15 metainfo files |
| MIME type | `application/vnd.kde.okular-archive` | 1 desktop file |
| Plugin namespace | `okular_generators` -> **renamed to `charta_generators`** in this pass | 5 sites |
| Translation domain | `okular` | `CMakeLists.txt:745`, `shell/main.cpp:98` |
| Words | "Okular" in the README, in notices, in comments, in 1 441 files overall | — |

## 2. What KDE's trademark guidance asks for

The relevant references are KDE's trademark policy and the KDE e.V. brand guidelines
(<https://www.kde.org/trademarks/> and the "Trademarks" page of the KDE wiki). The practical
consensus in those documents, which is what matters here:

- The words "KDE" and the KDE logo are **not** free to use as a project name or logo. "KDE
  Okular" and the Okular name itself belong to the Okular project / KDE e.V.
- **Attribution in the factual sense is fine**: saying "based on KDE Okular", "derived from KDE
  Okular", or "contains code from KDE Okular (GPL-2.0-or-later)" is how a GPL derivative is
  supposed to credit its origin. This is a statement of fact about the source, and it is what
  `docs/THIRD-PARTY-NOTICES.md` does.
- **Implying endorsement is not fine**: a distribution called "KDE Charta", using KDE's logo, or
  presented in the KDE store as a KDE product, would be an endorsement claim.
- A project that is not a KDE project should not use KDE's marks, and should not use "KDE" in its
  own product name or visual identity.

## 3. What this means for Charta, concretely

| Item | Position | Action |
| --- | --- | --- |
| Product name "Charta" | fine, subject to a name/ trademark search before any commercial use | yours |
| Icon and logo | AI-generated; a mark whose authorship is unclear is also hard to register or police | yours; keep the AI provenance recorded |
| "based on KDE Okular" in README/notices | correct and required | **done** — `docs/THIRD-PARTY-NOTICES.md` §1 |
| D-Bus service named `org.kde.okular` | a runtime claim to another project's identity; also a functional conflict with a real Okular | **deferred** — coordinated rename, see `packaging.md` §2 |
| Component ids `org.kde.okular-*` | namespace collision with upstream components | deferred |
| `application/vnd.kde.okular-archive` | a MIME type name; renaming needs a new XML type and desktop updates | deferred, low risk |
| "KDE" in the product name/icon | not used | — |
| Store listing copy | must not imply KDE endorsement | yours, at release time |

## 4. The decisions (2026-10-21)

1. **"Charta" is the final public application name.** The identity migration is therefore
   approved in principle.
2. **The migration is deferred, not cancelled.** The D-Bus service/interface rename (Q2) and the
   core library / header path / AppStream component id rename (Q9) are **not applied yet**. The
   complete migration has been prepared as a reviewable patch instead:
   * `docs/licensing/identity-migration.patch` — a unified diff that applies to the current tree
   * `docs/licensing/identity-migration.md` — every site, the risks, the user-facing and
     packager-facing migration notes, the rollback note, and the ready-to-paste footer note
3. **The footer note** (to be added by the maintainer to the README or the release notes) is in
   `docs/licensing/identity-migration.md`, section "Footer note".
4. **KDE trademark guidance** still has to be reviewed before a store listing; "KDE" should
   appear only in factual attribution ("based on KDE Okular"), never in the product name or in a
   way that implies endorsement.
5. **The brand assets are AI-generated**, which is a separate question from the name: a mark whose
   authorship is unclear is also hard to police or register. See
   `docs/licensing/legal-open-questions.md` C-4.
