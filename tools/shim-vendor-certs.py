#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prints the SHA-256 fingerprint and subject of every certificate a shim binary trusts (the
vendor_authorized part of its .vendor_cert section: one DER certificate, or an EFI signature list
when built with VENDOR_DB_FILE). Used to collect and re-check src/data/well-known-keys.json.

  tools/shim-vendor-certs.py <shimx64.efi>...

Needs objcopy (binutils) and openssl.
"""

import hashlib
import struct
import subprocess
import sys
import tempfile
import uuid

X509 = uuid.UUID("a5c059a1-94e4-4aa7-87b5-ab155c2bf072")


def vendor_cert_section(path):
    with tempfile.NamedTemporaryFile() as out:
        subprocess.run(["objcopy", "-O", "binary", "--only-section=.vendor_cert", path, out.name], check=True)
        return out.read()


def certificates(authorized):
    if authorized[:1] == b"\x30":  # a single DER certificate
        yield authorized
        return
    offset = 0
    while offset + 28 <= len(authorized):
        kind = uuid.UUID(bytes_le=authorized[offset:offset + 16])
        list_size, header_size, entry_size = struct.unpack_from("<III", authorized, offset + 16)
        if list_size < 28 or entry_size <= 16:
            sys.exit("malformed signature list")
        entry = offset + 28 + header_size
        while entry + entry_size <= offset + list_size:
            if kind == X509:
                yield authorized[entry + 16:entry + entry_size]
            else:
                print(f"  (skipped {kind} entry)")
            entry += entry_size
        offset += list_size


def main():
    if len(sys.argv) < 2:
        sys.exit(f"usage: {sys.argv[0]} <shimx64.efi>...")
    for path in sys.argv[1:]:
        section = vendor_cert_section(path)
        if len(section) < 16:
            print(f"{path}: no .vendor_cert section")
            continue
        size, _, offset, _ = struct.unpack_from("<IIII", section, 0)
        print(f"{path}:")
        for der in certificates(section[offset:offset + size]):
            fingerprint = hashlib.sha256(der).hexdigest().upper()
            subject = subprocess.run(
                ["openssl", "x509", "-inform", "DER", "-noout", "-subject", "-nameopt", "sep_comma_plus_space,dn_rev,utf8"],
                input=der, capture_output=True, check=True).stdout.decode().strip().removeprefix("subject=")
            print("  " + ":".join(fingerprint[i:i + 2] for i in range(0, 64, 2)))
            print(f"    {subject}")


if __name__ == "__main__":
    main()
