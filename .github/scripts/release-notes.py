#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Print the release notes for a tag as Markdown.

A pre-release (a version with a hyphen) lists the commits since the previous tag. A final release
lists everything since the previous final release. Commits are grouped by their Conventional Commits
type. Ones that only touch tests, CI or contributor docs go in a collapsed section at the end.

Usage: release-notes.py TAG
"""

import os
import re
import subprocess
import sys

GROUPS = [
    ("feat", "Features"),
    ("fix", "Fixes"),
    ("perf", "Performance"),
    ("docs", "Documentation"),
]
OTHER = "Other changes"
CONTRIBUTOR_TYPES = {"test", "ci"}
# Paths that only matter to people working on the project.
CONTRIBUTOR_PATHS = re.compile(r"^(tests/|\.github/|docs/[^/]+\.md$|CONTRIBUTING\.md$|ruff\.toml$)")
SUBJECT = re.compile(r"^(?P<type>[a-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?: (?P<desc>.+)$")


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def previous_tag(tag):
    args = ["describe", "--tags", "--abbrev=0", "--match", "v*"]
    if "-" not in tag:
        args += ["--exclude", "v*-*"]
    result = subprocess.run(["git", *args, f"{tag}^"], capture_output=True, text=True)
    return result.stdout.strip() or None


def commits(rev_range):
    """(short hash, subject, body, files) for each commit, newest first."""
    out = []
    log = git("log", "--no-merges", "--format=%x1e%h%x1f%s%x1f%b%x1f", "--name-only", rev_range)
    for record in log.split("\x1e")[1:]:
        sha, subject, body, files = record.split("\x1f")
        out.append((sha, subject, body, [f for f in files.splitlines() if f]))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[-1])
    tag = sys.argv[1]
    prev = previous_tag(tag)
    rev_range = f"{prev}..{tag}" if prev else tag

    breaking, contributor = [], []
    grouped = {title: [] for _, title in GROUPS}
    grouped[OTHER] = []
    titles = dict(GROUPS)
    for sha, subject, body, files in commits(rev_range):
        m = SUBJECT.match(subject)
        kind = m["type"] if m else None
        # The section gives the type.
        text = (f"{m['scope']}: " if m["scope"] else "") + m["desc"] if m else subject
        entry = f"- {text} ({sha})"
        if m and (m["breaking"] or re.search(r"^BREAKING[ -]CHANGE: ", body, re.MULTILINE)):
            breaking.append(entry)
        elif kind in CONTRIBUTOR_TYPES or (files and all(CONTRIBUTOR_PATHS.match(f) for f in files)):
            contributor.append(entry)
        else:
            grouped[titles.get(kind, OTHER)].append(entry)

    sections = []
    if breaking:
        sections.append("## Breaking changes\n\n" + "\n".join(breaking))
    for title, entries in grouped.items():
        if entries:
            sections.append(f"## {title}\n\n" + "\n".join(entries))
    if contributor:
        sections.append(
            f"<details>\n<summary>Tests, CI and contributor docs ({len(contributor)})</summary>\n\n"
            + "\n".join(contributor)
            + "\n\n</details>"
        )
    repo = os.environ.get("GITHUB_REPOSITORY")
    if prev and repo:
        sections.append(f"Full changelog: https://github.com/{repo}/compare/{prev}...{tag}")
    print("\n\n".join(sections) if sections else "No changes.")


if __name__ == "__main__":
    main()
