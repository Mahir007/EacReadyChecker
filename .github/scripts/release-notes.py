#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Print the release notes for a tag as Markdown.

The notes have a warning for a pre-release, the tag's message without its first line, a table of
the files to download, the commits grouped by their Conventional Commits type, and the commands
that check each file's SHA-256. A pre-release lists the commits since the previous tag, and a final
release those since the previous final release. Run it in the repository, with the release's
.sha256 files.

Usage: release-notes.py TAG OWNER/REPO [FILE.sha256 ...]
"""

import fnmatch
import os
import re
import subprocess
import sys

# The files a release has, in the order of the download table: a glob, the system, and the link text
# (None shows the file's name).
FILES = [
    ("eac-ready-checker-*-x86_64.AppImage", "Linux", "AppImage"),
    ("eac-ready-checker-*-x86_64.flatpak", "Linux", "Flatpak"),
]
# Longer names for a system's row, such as "macOS (Apple silicon and Intel)".
ROW_NAMES = {}
# How to check a file's SHA-256 on each system: the label, the code block's language and the
# command.
CHECKS = {
    "Windows": (
        "Windows, in PowerShell:",
        "powershell",
        "$hash, $file = (Get-Content '{}') -split '\\s+'; (Get-FileHash $file).Hash -eq $hash",
    ),
    "macOS": ("macOS:", "sh", "shasum -a 256 -c '{}'"),
    "Linux": ("Linux:", "sh", "sha256sum -c '{}'"),
    "Android": ("Android:", "sh", "sha256sum -c '{}'"),
}

TYPES = {"feat", "fix", "perf", "docs", "refactor", "style", "test", "ci", "build", "chore", "revert"}
HEADINGS = [
    ("feat", "✨ Features"),
    ("fix", "🐛 Fixes"),
    ("perf", "⚡ Performance"),
    ("docs", "📖 Documentation"),
]
SUBJECT = re.compile(r"^(?P<type>[a-z]+)(?:\((?P<scope>[^()]+)\))?(?P<breaking>!)?: (?P<desc>\S.*)$")
# A BREAKING CHANGE footer runs until the next footer.
BREAKING = re.compile(
    r"^BREAKING[ -]CHANGE: (?P<text>.*(?:\n(?!BREAKING[ -]CHANGE: |[\w-]+(?:: | #)).*)*)", re.MULTILINE
)
REVERTS = re.compile(r"^This reverts commit (?P<sha>[0-9a-f]{40})", re.MULTILINE)
INSTALL_HEADING = re.compile(r"quick ?start|install|download", re.IGNORECASE)


def git(*args, check=True):
    result = subprocess.run(["git", *args], capture_output=True, text=True, check=check)
    return result.stdout if result.returncode == 0 else None


def warning(version, url):
    final = version.split("-")[0]
    return (
        "> [!WARNING]\n"
        f"> This is a pre-release, for trying things out before {final} is final. "
        f"Please report problems in [Issues]({url}/issues)."
    )


def tag_message(tag):
    """The annotated tag's message without its first line. A lightweight tag has none."""
    ref = f"refs/tags/{tag}"
    if git("cat-file", "-t", ref).strip() != "tag":
        return ""
    contents = git("for-each-ref", "--format=%(contents)", ref)
    signature = git("for-each-ref", "--format=%(contents:signature)", ref).rstrip("\n")
    if signature and signature in contents:
        contents = contents[: contents.index(signature)]
    return "\n".join(contents.splitlines()[1:]).strip()


def release_files(sums):
    """(system, link text, file name) for each file named in the .sha256 files, in FILES order."""
    names = []
    for path in sums:
        with open(path, encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    names.append(line.split(maxsplit=1)[1].strip().lstrip("*"))
    found = []
    for name in names:
        index = next((i for i, (glob, _, _) in enumerate(FILES) if fnmatch.fnmatchcase(name, glob)), None)
        if index is None:
            sys.exit(f"release-notes.py: {name} matches nothing in FILES")
        found.append((index, name))
    return [(FILES[i][1], FILES[i][2] or name, name) for i, name in sorted(found)]


def slug(heading):
    """GitHub's anchor for a heading."""
    return re.sub(r"[^\w\- ]", "", heading.strip().lower()).replace(" ", "-")


def install_link(tag, url):
    readme = f"{url}/blob/{tag}/README.md"
    fenced = False
    for line in (git("show", f"{tag}:README.md", check=False) or "").splitlines():
        if line.startswith("```"):
            fenced = not fenced
        m = re.match(r"#{2,3} (.+)", line)
        if m and not fenced and INSTALL_HEADING.search(m[1]):
            heading = m[1].strip()
            return f"For how to install it, see [{heading}]({readme}#{slug(heading)}) in the README."
    return f"For how to install it, see the [README]({readme})."


def download(files, tag, url):
    rows = {}
    for system, text, name in files:
        link = f"{url}/releases/download/{tag}/{name.replace('+', '%2B')}"
        rows.setdefault(system, []).append(f"[{text}]({link})")
    table = ["| System | Download |", "| --- | --- |"]
    table += [f"| {ROW_NAMES.get(system, system)} | {' · '.join(links)} |" for system, links in rows.items()]
    return "## Download\n\n" + "\n".join(table) + "\n\n" + install_link(tag, url)


def previous_tag(tag):
    args = ["describe", "--tags", "--abbrev=0", "--match", "v*"]
    if "-" not in tag:
        args += ["--exclude", "v*-*"]
    return (git(*args, f"{tag}^", check=False) or "").strip() or None


def commits(rev_range):
    """(sha, subject, body) for each commit, oldest first, without merges, reverts or what they
    revert."""
    log = git("log", "--reverse", "--no-merges", "--format=%H%x1f%s%x1f%b%x1e", rev_range)
    out = [tuple(record.lstrip("\n").split("\x1f")) for record in log.split("\x1e")[:-1]]
    shas = {sha for sha, _, _ in out}
    dropped = set()
    # Newest first, so a revert of a revert brings the first commit back.
    for sha, _, body in reversed(out):
        m = REVERTS.search(body)
        if sha not in dropped and m and m["sha"] in shas:
            dropped |= {sha, m["sha"]}
    return [c for c in out if c[0] not in dropped]


def changes(tag, prev, url):
    rev_range = f"{prev}..{tag}" if prev else tag
    groups = {kind: [] for kind, _ in HEADINGS}
    breaking, other = [], []
    typed = True
    for sha, subject, body in commits(rev_range):
        link = f"([`{sha[:7]}`]({url}/commit/{sha}))"
        m = SUBJECT.match(subject)
        if not m or m["type"] not in TYPES:
            typed = False
            other.append(f"- {subject} {link}")
            continue
        footer = BREAKING.search(body)
        if m["breaking"] or footer:
            text = " ".join(footer["text"].split()) if footer else m["desc"]
        elif m["type"] in groups and (m["type"], m["scope"]) != ("docs", "contributing"):
            text = m["desc"]
        else:
            other.append(f"- {subject} {link}")
            continue
        scope = f"**{m['scope']}:** " if m["scope"] else ""
        entry = f"- {scope}{text[0].upper()}{text[1:]} {link}"
        (breaking if m["breaking"] or footer else groups[m["type"]]).append(entry)

    parts = ["## What's changed"]
    if breaking:
        parts.append("### ⚠️ Breaking changes\n\n" + "\n".join(breaking))
    for kind, title in HEADINGS:
        if groups[kind]:
            parts.append(f"### {title}\n\n" + "\n".join(groups[kind]))
    if len(parts) == 1 and typed:
        parts.append("No user-facing changes.")
    if other:
        parts.append(
            f"<details>\n<summary>🧰 Other changes ({len(other)})</summary>\n\n" + "\n".join(other) + "\n\n</details>"
        )
    if prev:
        parts.append(f"**Full changelog:** [{prev}...{tag}]({url}/compare/{prev}...{tag})")
    else:
        parts.append(f"**Full changelog:** [all commits]({url}/commits/{tag})")
    return "\n\n".join(parts)


def checksums(files):
    blocks = {}
    for system, _, name in files:
        blocks.setdefault(system, []).append(CHECKS[system][2].format(f"{name}.sha256"))
    out = [
        "<details>\n<summary>🔒 SHA-256</summary>",
        "Download the file's `.sha256` too, and run its line in the folder with both. "
        "It prints `OK` or `True` if the file is intact.",
    ]
    for system, lines in blocks.items():
        label, lang, _ = CHECKS[system]
        out.append(f"{label}\n\n```{lang}\n" + "\n".join(lines) + "\n```")
    out.append("</details>")
    return "\n\n".join(out)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    tag, repo, sums = sys.argv[1], sys.argv[2], sys.argv[3:]
    url = f"{os.environ.get('GITHUB_SERVER_URL', 'https://github.com')}/{repo}"
    files = release_files(sums)

    parts = []
    if "-" in tag:
        parts.append(warning(tag[1:], url))
    message = tag_message(tag)
    if message:
        parts.append(message)
    if files:
        parts.append(download(files, tag, url))
    parts.append(changes(tag, previous_tag(tag), url))
    if files:
        parts.append(checksums(files))
    print("\n\n".join(parts))


if __name__ == "__main__":
    main()
