#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Finds the Qt patch releases that can share a build of the GNOME title bar plugin
# (third_party/qadwaitadecorations), for samePrivateApi in src/app/WindowDecoration.cpp.
#
#   tools/compare-qt-releases.py <work-dir> <release>... [--objects <label>=<dir>]...
#
# Each release is downloaded from qt.io into <work-dir> and the plugin is built against it at -O0
# with full debug info. From that the script reads what the plugin uses of Qt's private API: the
# private functions it calls, the code of private inline functions compiled into it, the layout of
# its classes and the private classes they derive from, and the vtable slots and enum values of
# private classes. It then compares consecutive releases of the same minor version. Releases that
# agree on all of it run each other's build. New enum values count as the same if the existing ones
# keep theirs.
#
# --objects adds plugin objects built elsewhere, such as against a distribution's Qt in a
# container. Build them with the CMake options printed by --cmake-args, pass the folder holding the
# three .o files from build/third_party, and start the label with the release (6.11.2-fedora).
#
# Needs aqtinstall and pyelftools (pip install aqtinstall pyelftools), CMake, Ninja, g++ and
# binutils.

import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

CMAKE_ARGS = [
    "-DBUILD_TESTING=OFF",
    "-DEACHECK_DECORATIONS=ON",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_CXX_FLAGS_RELEASE=-O0 -g -femit-class-debug-always -DNDEBUG -ffunction-sections -fdata-sections",
]
REPO = Path(__file__).resolve().parent.parent


def run(cmd, log):
    with open(log, "w") as f:
        if subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT).returncode != 0:
            sys.exit(f"failed: {' '.join(map(str, cmd))} (see {log})")


def build(work, release):
    """Downloads a qt.io release and builds the plugin's objects against it."""
    qt, dl, out = work / "qt", work / "download", work / "objects" / release
    shutil.rmtree(qt, ignore_errors=True)
    shutil.rmtree(dl, ignore_errors=True)
    arches = subprocess.run([sys.executable, "-m", "aqt", "list-qt", "linux", "desktop", "--arch", release],
                            capture_output=True, text=True).stdout.split()
    arch = next((a for a in arches if "gcc_64" in a), None)
    if arch is None:
        sys.exit(f"qt.io has no Linux desktop build of {release}")
    run([sys.executable, "-m", "aqt", "install-qt", "linux", "desktop", release, arch,
         "--archives", "qtbase", "qtwayland", "qtsvg", "icu", "--outputdir", dl], work / "aqt.log")
    # Every release is built at the same path, so no path ends up in what's compared.
    (dl / release / "gcc_64").rename(qt)
    b = work / "build"
    shutil.rmtree(b, ignore_errors=True)
    run(["cmake", "-S", REPO, "-B", b, "-G", "Ninja", f"-DCMAKE_PREFIX_PATH={qt}", *CMAKE_ARGS], work / "cmake.log")
    run(["cmake", "--build", b, "--target", "qadwaitadecorations"], work / "build.log")
    out.mkdir(parents=True, exist_ok=True)
    for o in (b / "third_party").rglob("*.o"):
        shutil.copy(o, out / o.name)
    shutil.rmtree(qt)
    shutil.rmtree(dl)
    return out


def private(path):
    return path is not None and ("/private/" in path or "/qpa/" in path)


def view(objects):
    """What the objects use of Qt's private API, as a set of lines."""
    from elftools.elf.elffile import ELFFile
    from elftools.elf.relocation import RelocationSection

    out = set()

    def qualified(die):
        parts = []
        while die is not None and die.tag != "DW_TAG_compile_unit":
            if "DW_AT_name" in die.attributes:
                parts.append(die.attributes["DW_AT_name"].value.decode())
            die = die.get_parent()
        return "::".join(reversed(parts)) or "(unnamed)"

    def type_name(die, depth=0):
        if die is None or depth > 12:
            return "void"
        inner = die.get_DIE_from_attribute("DW_AT_type") if "DW_AT_type" in die.attributes else None
        suffix = {"DW_TAG_pointer_type": "*", "DW_TAG_reference_type": "&", "DW_TAG_rvalue_reference_type": "&",
                  "DW_TAG_array_type": "[]", "DW_TAG_ptr_to_member_type": " ::*"}.get(die.tag)
        if suffix:
            return type_name(inner, depth + 1) + suffix
        if die.tag == "DW_TAG_const_type":
            return "const " + type_name(inner, depth + 1)
        if die.tag == "DW_TAG_volatile_type":
            return "volatile " + type_name(inner, depth + 1)
        if die.tag == "DW_TAG_subroutine_type":
            return "fn(" + type_name(inner, depth + 1) + ")"
        return qualified(die)

    for path in sorted(Path(objects).glob("*.o")):
        with open(path, "rb") as f:
            elf = ELFFile(f)
            dwarf = elf.get_dwarf_info()
            declared_in = {}  # linkage name -> file it's declared in
            for cu in dwarf.iter_CUs():
                lp = dwarf.line_program_for_CU(cu)
                v5 = lp.header["version"] >= 5
                files, dirs = lp["file_entry"], lp["include_directory"]

                def file_name(i):
                    i = i if v5 else i - 1
                    if not 0 <= i < len(files):
                        return None
                    e = files[i]
                    d = dirs[e.dir_index if v5 else e.dir_index - 1].decode() if (v5 or e.dir_index) else ""
                    return f"{d}/{e.name.decode()}"

                def decl_file(die):
                    for _ in range(3):
                        if "DW_AT_decl_file" in die.attributes:
                            return file_name(die.attributes["DW_AT_decl_file"].value)
                        ref = next((a for a in ("DW_AT_specification", "DW_AT_abstract_origin") if a in die.attributes), None)
                        if ref is None:
                            return None
                        die = die.get_DIE_from_attribute(ref)
                    return None

                for die in cu.iter_DIEs():
                    a = die.attributes
                    if "DW_AT_linkage_name" in a:
                        declared_in.setdefault(a["DW_AT_linkage_name"].value.decode(), decl_file(die))
                    if die.tag not in ("DW_TAG_class_type", "DW_TAG_structure_type", "DW_TAG_union_type",
                                       "DW_TAG_enumeration_type") or "DW_AT_declaration" in a:
                        continue
                    df = decl_file(die)
                    own = df is not None and "/third_party/" in df
                    if not (own or private(df)):
                        continue
                    name = qualified(die)
                    if own:
                        out.add(f"plugin {name}")
                    out.add(f"type {name} size {a['DW_AT_byte_size'].value if 'DW_AT_byte_size' in a else '?'}")
                    for c in die.iter_children():
                        ca = c.attributes
                        if c.tag == "DW_TAG_member" and "DW_AT_external" not in ca and "DW_AT_declaration" not in ca:
                            at = ca["DW_AT_data_member_location"].value if "DW_AT_data_member_location" in ca else ""
                            if "DW_AT_data_bit_offset" in ca:
                                at = f"bit {ca['DW_AT_data_bit_offset'].value}"
                            if "DW_AT_bit_size" in ca:
                                at = f"{at}/{ca['DW_AT_bit_size'].value}"
                            member = ca["DW_AT_name"].value.decode() if "DW_AT_name" in ca else "(anon)"
                            out.add(f"type {name} member {member} at {at} : {type_name(c.get_DIE_from_attribute('DW_AT_type'))}")
                        elif c.tag == "DW_TAG_inheritance":
                            at = ca["DW_AT_data_member_location"].value if "DW_AT_data_member_location" in ca else "?"
                            out.add(f"type {name} base {type_name(c.get_DIE_from_attribute('DW_AT_type'))} at {at}")
                        elif c.tag == "DW_TAG_subprogram" and "DW_AT_vtable_elem_location" in ca:
                            slot = ca["DW_AT_vtable_elem_location"].value
                            slot = slot[-1] if isinstance(slot, list) else slot
                            fn = (ca.get("DW_AT_linkage_name") or ca["DW_AT_name"]).value.decode()
                            out.add(f"type {name} virtual {fn} slot {slot}")
                        elif c.tag == "DW_TAG_enumerator":
                            out.add(f"type {name} enumerator {ca['DW_AT_name'].value.decode()} = {ca['DW_AT_const_value'].value}")

            symtab = elf.get_section_by_name(".symtab")
            for sec in elf.iter_sections():
                if isinstance(sec, RelocationSection) and ".debug" not in sec.name:
                    for r in sec.iter_relocations():
                        sym = symtab.get_symbol(r["r_info_sym"])
                        if sym["st_shndx"] == "SHN_UNDEF" and private(declared_in.get(sym.name)):
                            out.add(f"call {sym.name}")
            for sec in elf.iter_sections():
                m = re.match(r"\.text\.(_Z.*)", sec.name)
                if not m or not private(declared_in.get(m.group(1))):
                    continue
                h = hashlib.sha256(sec.data())
                rel = elf.get_section_by_name(".rela" + sec.name)
                for r in rel.iter_relocations() if rel else ():
                    sym = symtab.get_symbol(r["r_info_sym"])
                    target = sym.name or elf.get_section(sym["st_shndx"]).name
                    h.update(f"{r['r_offset']} {r['r_info_type']} {target} {r['r_addend']}".encode())
                out.add(f"code {m.group(1)} {h.hexdigest()[:16]}")
    return out


def relevant(lines):
    """The lines the plugin depends on, and the enumerators among them."""
    types = {l.split(" ", 1)[1] for l in lines if l.startswith("plugin ")}
    known = {l.split(" ")[1] for l in lines if l.startswith("type ")}
    # The plugin's classes contain their bases and members, so their layout counts too.
    while True:
        more = set()
        for l in lines:
            m = re.match(r"type (\S+) (?:base (.*) at \S+|member \S+ at \S* : (.*))$", l)
            if m and m.group(1) in types:
                t = re.sub(r"^const |\[\]$", "", m.group(2) or m.group(3))
                if t in known and not t.endswith(("*", "&")):
                    more.add(t)
        if more <= types:
            break
        types |= more
    keep, enums = set(), {}
    for l in lines:
        name = l.split(" ")[1]
        if name.startswith("QtPrivate_6_"):
            continue  # QObjectPrivateVersion: named after the release, only matters through a call
        m = re.match(r"type (\S+) enumerator (\S+) = (\S+)", l)
        if m:
            enums[(m.group(1), m.group(2))] = m.group(3)
        elif l.startswith(("call ", "code ", "plugin ")) or " virtual " in l or name in types:
            keep.add(l)
    return keep, enums


def demangle(text):
    return subprocess.run(["c++filt"], input=text, capture_output=True, text=True).stdout


def compare(x, y, vx, vy):
    kx, ex = relevant(vx)
    ky, ey = relevant(vy)
    diff = [f"  - {l}" for l in sorted(kx - ky)] + [f"  + {l}" for l in sorted(ky - kx)]
    for (t, e), v in sorted(ex.items()):
        if (t, e) not in ey:
            diff.append(f"  - type {t} enumerator {e} = {v}")
        elif ey[(t, e)] != v:
            diff.append(f"  ~ type {t} enumerator {e} = {v} -> {ey[(t, e)]}")
    added = sorted(f"{t}::{e}" for (t, e) in ey if (t, e) not in ex)
    note = f" (added enum values: {', '.join(added)})" if added else ""
    print(f"{x} -> {y}: {'same' if not diff else 'differs'}{note}")
    if diff:
        print(demangle("\n".join(diff) + "\n"), end="")
    return not diff


def main():
    args = sys.argv[1:]
    if args == ["--cmake-args"]:
        print(" ".join(f"'{a}'" for a in CMAKE_ARGS))
        return
    if len(args) < 2:
        sys.exit("usage: compare-qt-releases.py <work-dir> <release>... [--objects <label>=<dir>]...")
    work = Path(args.pop(0)).resolve()
    work.mkdir(parents=True, exist_ok=True)
    views = {}
    while args:
        a = args.pop(0)
        if a == "--objects":
            label, folder = args.pop(0).split("=", 1)
            views[label] = view(folder)
        else:
            views[a] = view(build(work, a))
            print(f"{a}: built", file=sys.stderr)
    labels = list(views)
    def minor(label):
        return ".".join(label.split(".")[:2])

    groups = [[labels[0]]]
    for x, y in zip(labels, labels[1:]):
        if minor(x) == minor(y) and compare(x, y, views[x], views[y]):
            groups[-1].append(y)
        else:
            groups.append([y])
    print("Groups:", ", ".join(g[0] if len(g) == 1 else f"{g[0]} to {g[-1]}" for g in groups))


main()
