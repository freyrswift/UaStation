# Third-party notices

Ua includes independently licensed software, skill content, icons, fonts, and
brand assets. This document identifies those components; it does not replace
their original license texts or notices. It is a component inventory, not a
complete license-compliance audit.

## Scope of Ua's noncommercial terms

The new Ua noncommercial license applies only to Ua-owned material covered by
[LICENSE](../LICENSE). That material is for personal study, reference, and
noncommercial use; commercial use, sale, resale, and paid bundling require a
separate grant from the relevant Ua rightsholders.

**Ua's noncommercial terms do not relicense or restrict third-party components.**
The original upstream terms prevail for their respective components, including
MIT-licensed content, Qt, and other separately licensed assets. In particular,
the MIT permissions to use, modify, distribute, sublicense, and sell covered
upstream material remain intact, subject to the MIT notice requirements. Those
permissions do not grant commercial rights to new Ua-owned material alongside it.

**Earlier Ua MIT grants remain unchanged.** Copies and versions of Ua material
already distributed under MIT retain those grants, subject to their original
terms. A later noncommercial license does not retroactively revoke or narrow
them. The historical text is retained as
[licenses/ua-prior-MIT.txt](../licenses/ua-prior-MIT.txt); it is not a new MIT
grant for all current Ua material. Preserve the license and copyright notices
accompanying the version and component being redistributed.

## Component inventory

Package paths below are relative to the directory containing the release's
top-level [LICENSE](../LICENSE). Original license texts must remain with copies
or substantial portions of the covered component, including embedded resources
and redistributed skill bundles.

| Component and actual use | Upstream and copyright record | Applicable terms | Source license and retained package notice |
| --- | --- | --- | --- |
| REA skill packs, vendored under [rea/](https://github.com/freyrswift/UaStation/tree/main/resources/skills-upstream/rea) with `rea-` names | [morluto/rea](https://github.com/morluto/rea); Copyright (c) 2026 morluto | MIT | Original [REA LICENSE](https://github.com/freyrswift/UaStation/blob/main/resources/skills-upstream/rea/LICENSE); packaged copy: [licenses/rea-MIT.txt](../licenses/rea-MIT.txt) |
| reverse-skill packs, vendored under [reverse-skill/](https://github.com/freyrswift/UaStation/tree/main/resources/skills-upstream/reverse-skill) with `rs-` names | [zhaoxuya520/reverse-skill](https://github.com/zhaoxuya520/reverse-skill); Copyright (c) 2026 zhaoxuya520 | MIT; separately licensed nested content retains its own notices | Original [reverse-skill LICENSE](https://github.com/freyrswift/UaStation/blob/main/resources/skills-upstream/reverse-skill/LICENSE); packaged copy: [licenses/reverse-skill-MIT.txt](../licenses/reverse-skill-MIT.txt) |
| Nested src-hunter content included with [rs-pentest-tools/src-hunter/](https://github.com/freyrswift/UaStation/tree/main/resources/skills-upstream/reverse-skill/rs-pentest-tools/src-hunter) | [MyuriKanao/src-hunter-skill](https://github.com/MyuriKanao/src-hunter-skill), as identified by its retained [README](https://github.com/freyrswift/UaStation/blob/main/resources/skills-upstream/reverse-skill/rs-pentest-tools/src-hunter/README.en.md); Copyright (c) 2026 MyuriKanao | MIT, with its own attribution in addition to the enclosing pack's notice | Original [src-hunter LICENSE](https://github.com/freyrswift/UaStation/blob/main/resources/skills-upstream/reverse-skill/rs-pentest-tools/src-hunter/LICENSE); packaged copy: [licenses/src-hunter-MIT.txt](../licenses/src-hunter-MIT.txt); the original nested license also accompanies that skill's assets |
| Fluent UI System Icons and the embedded [FluentSystemIcons-Resizable.ttf](https://github.com/freyrswift/UaStation/blob/main/resources/fonts/FluentSystemIcons-Resizable.ttf) icon font | [Microsoft Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons); upstream Copyright (c) 2020 Microsoft Corporation | MIT; the repository also explicitly marks its bundled icon font as MIT | Upstream [MIT license](https://github.com/microsoft/fluentui-system-icons/blob/main/LICENSE); packaged copy: [licenses/fluent-system-icons-MIT.txt](../licenses/fluent-system-icons-MIT.txt) |
| Qt runtime, QML modules, and deployment plugins used by Ua and the skills MCP executable | [Qt](https://www.qt.io/); The Qt Company and other contributors as identified by the exact Qt distribution | Separately licensed: applicable Qt commercial terms or the open-source LGPL/GPL terms for the particular module and version, plus any third-party subcomponent terms | Packaged texts: [LGPL-3.0-only.txt](../licenses/qt/LGPL-3.0-only.txt), [GPL-3.0-only.txt](../licenses/qt/GPL-3.0-only.txt), and [Qt-GPL-exception-1.0.txt](../licenses/qt/Qt-GPL-exception-1.0.txt); see [Qt licensing](https://doc.qt.io/qt-6.8/licensing.html) |
| Copilot logo artwork sourced from Primer Octicons | [primer/octicons](https://github.com/primer/octicons); Copyright (c) 2026 GitHub Inc., as recorded in the copied license | MIT for the icon artwork; trademark rights remain separate | Upstream [LICENSE](https://github.com/primer/octicons/blob/main/LICENSE); packaged copy: [licenses/primer-octicons-MIT.txt](../licenses/primer-octicons-MIT.txt) |
| Cybersecurity skill scripts, examples and Python helpers restored in 2.2.4 | [Masriyan/Claude-Code-CyberSecurity-Skill](https://github.com/Masriyan/Claude-Code-CyberSecurity-Skill), revision `3aa77d84fb64cf07b550f6be4544bb91292db89f`; Copyright (c) 2025-2026 Riyan Pratama (Masriyan / sudo3rs) | Original MIT license and attribution NOTICE | Original LICENSE and NOTICE accompany each restored skill, including [web-security](../resources/skills/web-security/); no Ua license replaces them |
| Unity Game Research companions | [avm-dev/unity-game-research](https://github.com/avm-dev/unity-game-research), revision `675a46af12ccf8e16dbb9cff26181c565df69adf`; Copyright (c) 2026 Aleksey Melnikov | MIT | [Original LICENSE](../resources/skills/unity-game-research/LICENSE) accompanies the skill; source revision is pinned here |
| DMA research reference documents | [gmh5225/awesome-game-security](https://github.com/gmh5225/awesome-game-security), revision `a8fc1efad86fce3c04710f98f04b740dd2e21d6e`; Copyright (c) 2022 gmh | MIT | [Original LICENSE](../resources/skills/dma-attack/LICENSE) accompanies the restored references |
| APK, MCP JS and softseep companion material | [angusdevgo/Seep-Reverse-Lab](https://github.com/angusdevgo/Seep-Reverse-Lab), historical revision `8f3c49c84bbfd7d3e9b9541e49beb36cdf1afa7a` | Preserve individual package/file terms; no repository-wide MIT grant is asserted | [APK LICENSE](../resources/skills/apkseep/LICENSE) and existing MIT metadata are retained where present. That historical root and MCP package do not supply a separate license notice; later repository licensing is not represented as the license of the historical snapshot |
| Product logos and names used to identify supported clients | Respective owners; acquisition URLs, dates, and recorded digests are in [logos/sources.json](https://github.com/freyrswift/UaStation/blob/main/resources/logos/sources.json) | Applicable asset licenses and trademark/brand-use rules; no blanket Ua license grant | Preserve applicable source notices. The source record is provenance, not a license or trademark permission |

## Vendored skill provenance

The vendoring comments in [resources.qrc](https://github.com/freyrswift/UaStation/blob/main/resources.qrc) and
[items.cpp](https://github.com/freyrswift/UaStation/blob/main/src/core/items.cpp) record REA at `833136507` and reverse-skill at
`cab634bd`. These are the short revision identifiers recorded by this repository,
not independently verified full commit IDs or a claim that every vendored file is
byte-for-byte identical to an upstream checkout. Ua uses `rea-` and `rs-` directory
and resource aliases to distinguish these packs from the other bundled skills.

The nested src-hunter [marketplace record](https://github.com/freyrswift/UaStation/blob/main/resources/skills-upstream/reverse-skill/rs-pentest-tools/src-hunter/.claude-plugin/marketplace.json)
identifies its upstream project and MIT license. No separate src-hunter commit
identifier is established here. Its license is a distinct notice and must not be
discarded merely because the enclosing reverse-skill project is also MIT.

The explicit skill manifests are embedded by [resources.qrc](https://github.com/freyrswift/UaStation/blob/main/resources.qrc).
[SkillAssets.cmake](https://github.com/freyrswift/UaStation/blob/main/cmake/SkillAssets.cmake) also collects companion files from
each packaged skill directory, which includes the nested src-hunter license.
The REA and reverse-skill top-level licenses sit outside those per-skill
directories, so the packaged copies at `licenses/rea-MIT.txt` and
`licenses/reverse-skill-MIT.txt` must remain in the release. The nested
src-hunter copy at `licenses/src-hunter-MIT.txt` must remain as well. Repackaging
extracted skills must preserve the applicable upstream notices; the new Ua
license is not a substitute for them.

The 2.2.4 companion recovery restores REA files from full revision
`833136507dd79d959340b09862a53af344e4f1b3` and reverse-skill files from
`cab634bd855fc287f6e420c1f36fd1a6b9245960`. The restored reverse-skill content
matches the pinned Git blobs, allowing the CRLF checkout conversion declared
for PowerShell files by that upstream's `.gitattributes`.
Existing skill manifests are retained except for an explicit repository-root
path clarification in the REA changelog skill. Local companions, external
repository tools, generated outputs and separately installed runtimes are not
interchangeable dependencies. Bundling a reference document does not bundle
every external project it discusses.

## Icons, fonts, and marks

[resources.qrc](https://github.com/freyrswift/UaStation/blob/main/resources.qrc) embeds the Fluent font, and
[IconGlyph.qml](https://github.com/freyrswift/UaStation/blob/main/qml/components/IconGlyph.qml) selects its family for interface
glyphs. The source comments identify Fluent System Icons and MIT licensing. No
upstream font commit is recorded here; the embedded name table's `Version 1.0`
and `http://fontello.com` entry do not establish a Microsoft repository revision
or a complete generation history. Do not infer an exact release or commit from
the font filename.

The client logos in [logos/](https://github.com/freyrswift/UaStation/tree/main/resources/logos) are identification assets, not
Ua-owned artwork. The Copilot source record points to a GitHub Primer Octicons
asset, while the Visual Studio Code record points to Microsoft's branding
download; other records point to the listed vendor assets or an installed
official package. Neither those records nor an asset's software license transfers
ownership of the associated trademarks. Product names and marks remain with their
respective owners. Inclusion does not imply sponsorship, affiliation, endorsement,
or permission to rebrand another product.

## Qt distribution obligations

[CMakeLists.txt](https://github.com/freyrswift/UaStation/blob/main/CMakeLists.txt) requires Qt 6.5 or newer and links Core, Gui,
Qml, Quick, QuickControls2, Svg, Network, Widgets, and Concurrent. The configured
[build workflow](https://github.com/freyrswift/UaStation/blob/main/.github/workflows/build.yml) selects Qt 6.8.3. Platform
deployment uses [windeployqt](https://github.com/freyrswift/UaStation/blob/main/cmake/DeployWindows.cmake),
[macdeployqt](https://github.com/freyrswift/UaStation/blob/main/cmake/DeployMacOS.cmake), or the
[Qt Linux deployment API](https://github.com/freyrswift/UaStation/blob/main/cmake/DeployLinux.cmake), so the exact runtime,
plugins, QML imports, and their notices depend on the build's Qt distribution.

Ua's license grants no rights over Qt. Follow the terms supplied with each
deployed Qt module and its third-party dependencies, preserving all required
license texts and notices. The copied texts above are the Qt notices currently
retained in this repository; this document does not assert that they exhaust
every notice in a particular Qt deployment.

### Qt 6.8.3 corresponding source

The workflow uses unmodified official **Qt 6.8.3** dynamically linked kits:
`win64_msvc2022_64`, macOS `clang_64` for the selected architecture, and Linux
`linux_gcc_64`. No local Qt source patch or Qt rebuild step is configured. The
exact Ua source commit and CI configuration for a publication are those checked
out by its release-tag Actions run, identified by that run's `GITHUB_SHA` and
the tag's commit. Use that commit's workflow, not the changing `main` branch,
when reproducing the build; these docs do not invent a future release commit.

The tag-triggered release job is configured to download the official
[`qt-everywhere-src-6.8.3.tar.xz`](https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz),
verify it against the pinned official
[SHA256](https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz.sha256),
and attach an unchanged copy to the same GitHub Release as the four application
assets. The expected digest is:

```text
cdd3a69967208276bb01af7ace7dba0ba53e679f886a4cbe624225c60fb73f2c
```

The Qt archive is a separate release asset, not inside the Windows ZIP, either
macOS DMG, or the Linux tar archive. Download it from the matching tag's Assets
on [UaStation Releases](https://github.com/freyrswift/UaStation/releases).
The exact URL pattern is:

```text
https://github.com/freyrswift/UaStation/releases/download/<release-tag>/qt-everywhere-src-6.8.3.tar.xz
```

Replace `<release-tag>` with the tag of the Ua package being used. The job also
generates `SHA256SUMS` for the four application archives and this Qt source
archive. This describes configured pipeline behavior, not an assertion that a
particular Release or source asset has already been published. An upstream
download link alone is not treated as fulfilling corresponding-source delivery;
see Qt's [LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations).

For a written corresponding-source request, open a
[project Issue](https://github.com/freyrswift/UaStation/issues/new) identifying the
Ua release tag, CI/source commit, operating system, architecture, and Qt 6.8.3.
Use this contact if the mirrored archive is missing or inaccessible, or to
request the corresponding source and build/installation information needed for
that distribution. A request contact is not a substitute for supplying required
source or for checking that the published source asset remains accessible.

### Library modification, replacement, and relinking

For LGPL-covered Qt components, recipients retain the rights required by
[LGPLv3 section 4](../licenses/qt/LGPL-3.0-only.txt): to modify Qt, reverse engineer
the application for debugging those library modifications, replace or relink
with an interface-compatible modified Qt library, and run the resulting work.
Ua's noncommercial terms do not restrict those rights. A shared-library
mechanism must actually work with the compatible modified library; applicable
installation information must also be supplied. Retaining license files or
using a deployment tool alone does not establish those conditions.

Keep the same target architecture and compatible toolchain/Qt ABI when building
a modified Qt replacement from the corresponding source. In a separate local
copy of the package, replace the appropriate Qt DLLs on Windows, Qt frameworks
on macOS, or shared libraries on Linux, along with affected plugins/QML modules.
Do not mix incompatible Qt versions or module builds. For changes requiring Ua
to be relinked, use the release's exact Ua source commit and its
[build instructions](https://github.com/freyrswift/UaStation/blob/main/docs/building.md)
with the modified Qt installation selected via `CMAKE_PREFIX_PATH`. See
[environment settings](env-vars.md) for the separate build-time `Qt6_DIR` hint;
runtime tool-profile variables do not select the Qt runtime.

If a package's loader paths, signing restrictions, or other installation
conditions prevent running an interface-compatible replacement, the distributor
must supply the applicable installation information or other required relink
materials. No replacement/relink execution or complete compliance audit is
claimed here. The archive identifies upstream source; a changed Qt build must
also retain its patches and the information needed to reproduce that build.

If a distribution uses a GPL-covered Qt module, its module-specific terms
control. The [Qt GPL exception](../licenses/qt/Qt-GPL-exception-1.0.txt) applies
only where the relevant upstream licensing statement grants it; it is not a
blanket exception for all Qt runtime modules. Refer to the
[Qt 6.8 licensing page](https://doc.qt.io/qt-6.8/licensing.html) and each deployed
module's own license and third-party notices.

## Release notice locations

Keep the third-party notice document and retained license texts together with
the release documentation. The documentation base is the Windows package root,
`ua.app/Contents/Resources` on macOS, and `${CMAKE_INSTALL_DOCDIR}` on Linux
(normally `share/doc/Ua` beneath the install prefix). Within that base, this
document is `docs/THIRD_PARTY_NOTICES.md`. The three skill licenses, Fluent MIT,
Primer Octicons MIT, historical Ua MIT, and Qt texts are retained at the
`licenses/` paths linked above. The CMake install rules copy that directory to
the documentation base. The Qt corresponding-source archive is delivered
separately through Release Assets as described above, not through those install
rules. Repository-only provenance links use GitHub so they remain usable from
packaged docs; select the release's exact source commit for historical evidence.
This inventory is not a replacement for original license texts or a complete
audit of a deployed package.
