#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for .github/scripts/release-notes.py, each in a new repository with its own commits and tags.

The repositories ignore the user's and the system's git config, so a signing or hook setting can't
change them.
"""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / ".github" / "scripts" / "release-notes.py"
REPO = "owner/repo"
URL = "https://github.com/owner/repo"
ENV = {
    **{k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "GITHUB_SERVER_URL"},
    "GIT_CONFIG_GLOBAL": os.devnull,
    "GIT_CONFIG_NOSYSTEM": "1",
    "GIT_AUTHOR_NAME": "Test",
    "GIT_AUTHOR_EMAIL": "test@example.com",
    "GIT_COMMITTER_NAME": "Test",
    "GIT_COMMITTER_EMAIL": "test@example.com",
}
WARNING = (
    "> [!WARNING]\n"
    "> This is a pre-release, for trying things out before 1.1.0 is final. "
    f"Please report problems in [Issues]({URL}/issues)."
)
README = "# App\n\n```sh\n## Install in a code block\n```\n\n## Requirements\n\n## Quick start\n\n## Install\n"
APPIMAGE = "eac-ready-checker-1.1.0-x86_64.AppImage"
FLATPAK = "eac-ready-checker-1.1.0-x86_64.flatpak"


class ReleaseNotesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.git("init", "-q", "-b", "main")

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args):
        return subprocess.run(
            ["git", *args], cwd=self.dir, env=ENV, check=True, capture_output=True, text=True
        ).stdout.strip()

    def commit(self, subject, body="", readme=None):
        """Commits a change and returns its full hash."""
        if readme is not None:
            (self.dir / "README.md").write_text(readme)
            self.git("add", "README.md")
        message = subject + (f"\n\n{body}" if body else "")
        self.git("commit", "-q", "--allow-empty", "-m", message)
        return self.git("rev-parse", "HEAD")

    def tag(self, name, *messages):
        """An annotated tag with the messages as paragraphs, or a lightweight one without any."""
        if messages:
            self.git("tag", "-a", name, *(arg for m in messages for arg in ("-m", m)))
        else:
            self.git("tag", name)

    def sums(self, *names):
        paths = []
        for name in names:
            path = self.dir / f"{name}.sha256"
            path.write_text(f"{'0' * 64}  {name}\n")
            paths.append(str(path))
        return paths

    def run_script(self, tag, *sums):
        return subprocess.run(
            [sys.executable, str(SCRIPT), tag, REPO, *sums],
            cwd=self.dir, env=ENV, capture_output=True, text=True,
        )

    def notes(self, tag, *sums):
        result = self.run_script(tag, *sums)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def link(self, sha):
        return f"([`{sha[:7]}`]({URL}/commit/{sha}))"

    def test_whole_notes(self):
        self.commit("feat: first", readme=README)
        self.tag("v1.0.0", "v1.0.0")
        feat = self.commit("feat(ui): add a dark theme")
        chore = self.commit("chore: tidy up")
        self.tag("v1.1.0-beta.1", "v1.1.0-beta.1", "Try the dark theme.")
        notes = self.notes("v1.1.0-beta.1", *self.sums(FLATPAK, APPIMAGE))
        download = f"{URL}/releases/download/v1.1.0-beta.1"
        self.assertEqual(
            notes,
            f"""{WARNING}

Try the dark theme.

## Download

| System | Download |
| --- | --- |
| Linux | [AppImage]({download}/{APPIMAGE}) · [Flatpak]({download}/{FLATPAK}) |

For how to install it, see [Quick start]({URL}/blob/v1.1.0-beta.1/README.md#quick-start) in the README.

## What's changed

### ✨ Features

- **ui:** Add a dark theme {self.link(feat)}

<details>
<summary>🧰 Other changes (1)</summary>

- chore: tidy up {self.link(chore)}

</details>

**Full changelog:** [v1.0.0...v1.1.0-beta.1]({URL}/compare/v1.0.0...v1.1.0-beta.1)

<details>
<summary>🔒 SHA-256</summary>

Download the file's `.sha256` too, and run its line in the folder with both. It prints `OK` or `True` if the file is intact.

Linux:

```sh
sha256sum -c '{APPIMAGE}.sha256'
sha256sum -c '{FLATPAK}.sha256'
```

</details>
""",
        )

    def test_warning_only_for_a_pre_release(self):
        self.commit("feat: first")
        self.tag("v1.1.0-rc.1")
        self.tag("v1.1.0")
        self.assertTrue(self.notes("v1.1.0-rc.1").startswith(WARNING + "\n\n## What's changed"))
        self.assertTrue(self.notes("v1.1.0").startswith("## What's changed"))

    def test_tag_message(self):
        self.commit("feat: first")
        self.tag("v1.0.0", "v1.0.0", "First paragraph.\nSame paragraph.", "- a list")
        self.tag("v1.0.1", "v1.0.1")
        self.tag("v1.0.2")
        self.assertTrue(
            self.notes("v1.0.0").startswith("First paragraph.\nSame paragraph.\n\n- a list\n\n## What's changed")
        )
        self.assertTrue(self.notes("v1.0.1").startswith("## What's changed"))
        # A lightweight tag's contents are the commit's message.
        self.assertTrue(self.notes("v1.0.2").startswith("## What's changed"))

    def test_download_links(self):
        self.commit("feat: first", readme="# App\n\n## Usage\n")
        self.tag("v1.0.0-alpha.1")
        name = "eac-ready-checker-1.0.0-alpha.1+abc1234-x86_64.AppImage"
        notes = self.notes("v1.0.0-alpha.1", *self.sums(name))
        self.assertIn(
            "| Linux | [AppImage](https://github.com/owner/repo/releases/download/v1.0.0-alpha.1/"
            "eac-ready-checker-1.0.0-alpha.1%2Babc1234-x86_64.AppImage) |",
            notes,
        )
        self.assertIn(f"For how to install it, see the [README]({URL}/blob/v1.0.0-alpha.1/README.md).", notes)
        self.assertIn(f"sha256sum -c '{name}.sha256'", notes)

    def test_install_heading(self):
        self.commit("feat: first", readme="# App\n\n## Usage\n\n### Installing `the app`\n")
        self.tag("v1.0.0")
        self.assertIn(
            f"see [Installing `the app`]({URL}/blob/v1.0.0/README.md#installing-the-app) in the README.",
            self.notes("v1.0.0", *self.sums(APPIMAGE)),
        )

    def test_unknown_file(self):
        self.commit("feat: first")
        self.tag("v1.0.0")
        result = self.run_script("v1.0.0", *self.sums(APPIMAGE, "app.zip"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("app.zip", result.stderr)

    def test_no_files(self):
        self.commit("feat: first")
        self.tag("v1.0.0")
        notes = self.notes("v1.0.0")
        self.assertNotIn("## Download", notes)
        self.assertNotIn("SHA-256", notes)

    def test_headings(self):
        commits = {
            subject: self.commit(subject, body)
            for subject, body in [
                ("fix: a crash", ""),
                ("docs(contributing): the build", ""),
                ("feat!: drop the old flag", ""),
                ("perf: faster start", ""),
                ("refactor: move things", "BREAKING CHANGE: the config moved to\n~/.config.\nRefs: #1"),
                ("Fix a typo", ""),
                ("docs: the usage", ""),
                ("feat(cli): add --json", ""),
                ("test: more tests", ""),
            ]
        }
        self.tag("v1.0.0")
        notes = self.notes("v1.0.0")

        def link(subject):
            return self.link(commits[subject])

        self.assertEqual(
            notes,
            f"""## What's changed

### ⚠️ Breaking changes

- Drop the old flag {link("feat!: drop the old flag")}
- The config moved to ~/.config. {link("refactor: move things")}

### ✨ Features

- **cli:** Add --json {link("feat(cli): add --json")}

### 🐛 Fixes

- A crash {link("fix: a crash")}

### ⚡ Performance

- Faster start {link("perf: faster start")}

### 📖 Documentation

- The usage {link("docs: the usage")}

<details>
<summary>🧰 Other changes (3)</summary>

- docs(contributing): the build {link("docs(contributing): the build")}
- Fix a typo {link("Fix a typo")}
- test: more tests {link("test: more tests")}

</details>

**Full changelog:** [all commits]({URL}/commits/v1.0.0)
""",
        )

    def test_no_user_facing_changes(self):
        self.commit("feat: first")
        self.tag("v1.0.0")
        self.commit("ci: faster")
        self.tag("v1.0.1")
        self.commit("Not conventional")
        self.tag("v1.0.2")
        self.assertIn("## What's changed\n\nNo user-facing changes.\n\n<details>", self.notes("v1.0.1"))
        self.assertIn("## What's changed\n\n<details>", self.notes("v1.0.2"))
        self.commit("ci: slower")
        self.git("tag", "v1.0.3")
        self.assertNotIn("Not conventional", self.notes("v1.0.3"))

    def test_reverts(self):
        self.commit("feat: first")
        self.tag("v1.0.0")
        feat = self.commit("feat: a change")
        self.commit("revert: feat: a change", f"This reverts commit {feat}.")
        fix = self.commit("fix: a bug")
        revert = self.commit("revert: fix: a bug", f"This reverts commit {fix}.")
        self.commit("revert: revert: fix: a bug", f"This reverts commit {revert}.")
        self.tag("v1.1.0")
        notes = self.notes("v1.1.0")
        self.assertNotIn("A change", notes)
        self.assertNotIn("revert", notes)
        self.assertIn(f"### 🐛 Fixes\n\n- A bug {self.link(fix)}\n\n**Full changelog:**", notes)

    def test_revert_of_a_release(self):
        feat = self.commit("feat: a change")
        self.tag("v1.0.0")
        revert = self.commit("revert: feat: a change", f"This reverts commit {feat}.")
        self.tag("v1.0.1")
        self.assertIn(f"- revert: feat: a change {self.link(revert)}", self.notes("v1.0.1"))

    def test_ranges(self):
        self.commit("feat: one")
        self.tag("v1.0.0")
        self.commit("feat: two")
        self.tag("v1.1.0-beta.1")
        self.commit("feat: three")
        self.tag("v1.1.0-beta.2")
        self.commit("Merge-like commit")
        self.commit("feat: four")
        self.tag("v1.1.0")

        beta = self.notes("v1.1.0-beta.2")
        self.assertIn("- Three", beta)
        self.assertNotIn("- Two", beta)
        self.assertIn(f"[v1.1.0-beta.1...v1.1.0-beta.2]({URL}/compare/v1.1.0-beta.1...v1.1.0-beta.2)", beta)

        final = self.notes("v1.1.0")
        self.assertNotIn("- One", final)
        # Oldest first.
        self.assertIn("- Two", final)
        self.assertLess(final.index("- Two"), final.index("- Three"))
        self.assertLess(final.index("- Three"), final.index("- Four"))
        self.assertIn(f"[v1.0.0...v1.1.0]({URL}/compare/v1.0.0...v1.1.0)", final)

    def test_merges_left_out(self):
        self.commit("feat: one")
        self.git("checkout", "-q", "-b", "topic")
        self.commit("feat: two")
        self.git("checkout", "-q", "main")
        self.commit("feat: three")
        self.git("merge", "-q", "--no-ff", "-m", "Merge branch 'topic'", "topic")
        self.tag("v1.0.0")
        notes = self.notes("v1.0.0")
        self.assertNotIn("Merge", notes)
        self.assertIn("- Two", notes)


if __name__ == "__main__":
    unittest.main()
