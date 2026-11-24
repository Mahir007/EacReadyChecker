#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Print the metainfo's <releases> element, built from the repository's version tags.

Each v* tag reachable from HEAD gives a release, newest first, with the tag's date and its message
as the description. A message is plain paragraphs and "- " lists, with `code` spans. A pre-release
is written with "~" in place of its first "-" (1.0.0~rc.1), because AppStream sorts 1.0.0-rc.1
after 1.0.0. Without any tags, as in a shallow clone or outside git, the release is VERSION, dated
SOURCE_DATE_EPOCH or the HEAD commit. VERSION's build metadata, such as the commit in
1.2.0-beta.1+abc1234, is left out.

Usage: metainfo-releases.py VERSION
"""

import datetime
import os
import re
import subprocess
import sys
from xml.sax.saxutils import escape

REPO_URL = "https://github.com/Mahir007/EacReadyChecker"
TAG = re.compile(r"^v(?P<version>[0-9]+\.[0-9]+\.[0-9]+(?:-(?:alpha|beta|rc)\.[1-9][0-9]*)?)$")


def git(*args):
    try:
        result = subprocess.run(["git", *args], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout


def date(epoch):
    return datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc).strftime("%Y-%m-%d")


def inline(text):
    parts = re.split(r"`([^`]+)`", text)
    return "".join(escape(p) if i % 2 == 0 else f"<code>{escape(p)}</code>" for i, p in enumerate(parts))


def description(message):
    """The tag message as AppStream description markup, one element per line."""
    out = []
    for block in re.split(r"\n\s*\n", message.strip()):
        lines = block.splitlines()
        if all(line.startswith("- ") or line.startswith("  ") for line in lines) and lines[0].startswith("- "):
            items = []
            for line in lines:
                if line.startswith("- "):
                    items.append(line[2:].strip())
                else:
                    items[-1] += " " + line.strip()
            out.append("<ul>")
            out += [f"  <li>{inline(item)}</li>" for item in items]
            out.append("</ul>")
        else:
            out.append(f"<p>{inline(' '.join(line.strip() for line in lines))}</p>")
    return out


def release(version, day, message=None, url=True):
    kind = "development" if "-" in version else "stable"
    attrs = f'version="{escape(version.replace("-", "~", 1))}" date="{day}" type="{kind}"'
    body = []
    if url:
        body.append(f'<url type="details">{REPO_URL}/releases/tag/v{escape(version)}</url>')
    if message and message.strip():
        body += ["<description>", *("  " + line for line in description(message)), "</description>"]
    if not body:
        return [f"<release {attrs}/>"]
    return [f"<release {attrs}>", *("  " + line for line in body), "</release>"]


def tagged_releases():
    # versionsort.suffix puts 1.0.0-rc.1 before 1.0.0, so the newest is first.
    out = git(
        "-c", "versionsort.suffix=-", "for-each-ref", "--merged", "HEAD", "--sort=-v:refname",
        "--format=%(refname:strip=2)%1f%(objecttype)%1f%(creatordate:unix)%1f%(contents:subject)%1f%(contents:body)%1e",
        "refs/tags/v*",
    )
    versions, entries = [], []
    for record in (out or "").split("\x1e"):
        if not record.strip():
            continue
        name, kind, epoch, subject, body = record.lstrip("\n").split("\x1f")
        m = TAG.match(name)
        if not m:
            continue
        # A lightweight tag's contents are the commit's message, not release notes.
        message = f"{subject}\n\n{body}" if kind == "tag" else None
        versions.append(m["version"])
        entries += release(m["version"], date(epoch), message)
    return versions, entries


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[-1])
    version = sys.argv[1].split("+")[0]
    versions, entries = tagged_releases()
    if versions and TAG.match(f"v{version}") and version not in versions:
        sys.exit(f"metainfo-releases.py: {version} looks like a release, but HEAD has no v{version} tag")
    if not entries:
        epoch = os.environ.get("SOURCE_DATE_EPOCH") or (git("log", "-1", "--format=%ct") or "").strip()
        day = date(epoch) if epoch else datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
        entries = release(version, day, url=False)
    print("  <releases>")
    print("\n".join("    " + line for line in entries))
    print("  </releases>")


if __name__ == "__main__":
    main()
