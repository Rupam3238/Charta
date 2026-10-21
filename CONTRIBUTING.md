# Contributing to Charta

Charta is a document reader and annotator derived from
[KDE Okular](https://invent.kde.org/graphics/okular). Thank you for helping.

## Licence and copyright

* **Inbound = outbound.** Every contribution is accepted under the **GPL-2.0-or-later**, the
  licence the project already uses per file. Do not add code under an incompatible licence.
* **Sign-off is required.** Every commit must carry a `Signed-off-by:` line (the
  [Developer Certificate of Origin](https://developercertificate.org/)). It certifies that you have
  the right to submit the contribution under the stated licence.
* **New files need SPDX headers** at the top:

  ```c
  /*
      SPDX-FileCopyrightText: 2026 Your Name <you@example.com>

      SPDX-License-Identifier: GPL-2.0-or-later
  */
  ```

  For a non-C++ file use `#` comments. If a file cannot carry a header, add it to `REUSE.toml`
  with a narrow path and a comment explaining why.
* **Do not change the licence of an existing file.** Inherited files keep their upstream terms.
  If you believe a licence is wrong, open an issue — do not edit the header.

## Third-party code

Before a pull request that adds code, artwork, data or fonts from anywhere else:

1. Add a row to `docs/licensing/third-party-inventory.csv` (component, version, source URL,
   licence, linkage, obligation).
2. Add the licence text to `LICENSES/` — copy it from the upstream project or from the SPDX
   licence list; never retype it.
3. Record the provenance in `docs/THIRD-PARTY-NOTICES.md`.
4. Make sure the code is compatible with GPL-2.0-or-later. **GPL-3.0-only, AGPL and proprietary
   code cannot be added** without a maintainer decision and a licence change.

## AI-assisted contributions

* Contributions produced with AI tools are accepted, and the project itself was built that way.
* Say so in the commit message, e.g. `Generated with an AI coding assistant; reviewed by …`.
* Do not paste large amounts of third-party code that you did not write, and do not present
  generated output as your own authorship.
* You keep the copyright in your contribution; by signing off you grant the rights needed to
  license it.

## Before you open a pull request

```sh
cmake -S . -B build -DOKULAR_UI=desktop -DBUILD_TESTING=ON
cmake --build build --parallel $(nproc)
cd build && xvfb-run --server-args="-screen 0 1024x768x24" ctest --output-on-failure
```

* Keep the tree lint-clean: `reuse lint` must pass (see `docs/licensing/`).
* Do not commit build output, binaries, or scratch files. The current tree still carries a few
  tracked artefacts that are scheduled for removal — please do not add more.
* Do not rewrite published Git history.
* Use your real name and email in commits.

## Reporting a licence or provenance problem

Open an issue describing where the material came from and which licence you believe applies.
Provenance questions are tracked with the evidence discipline in `docs/licensing/` — a file path, a
hash, and a command is worth more than an opinion. If you can supply the upstream commit, that is
the most useful thing you can bring.
