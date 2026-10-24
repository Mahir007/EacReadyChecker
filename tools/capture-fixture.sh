#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copies the host files EACheck reads into a fixture tree, so a machine's state can be turned into
# a test (tests/fixtures/<name>/) or attached to a bug report.
#
#   tools/capture-fixture.sh <output-dir> [--with-event-log]
#
# --with-event-log also copies the TPM event log, which needs root (asks via sudo).
# Nothing here is secret, but /proc/cmdline shows your root device/UUID, so look the tree over
# before sharing it. Keep the path list in sync with the checks' requests (src/checks/).
#
# For tests, EACHECK_CAPTURE_ROOT reads from a sysroot instead of /.

set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <output-dir> [--with-event-log]" >&2
    exit 1
fi
out=$1
event_log=0
[ "${2-}" = --with-event-log ] && event_log=1
src=${EACHECK_CAPTURE_ROOT:-}

mkdir -p "$out"

# copy <pattern>: copies matching files; recreates matching directories (empty).
copy() {
    for p in $src$1; do
        [ -e "$p" ] || continue
        rel=${p#"$src"}
        if [ -d "$p" ]; then
            mkdir -p "$out$rel"
            # Keep otherwise-empty directories when the tree is committed to git.
            [ -n "$(ls -A "$out$rel")" ] || : > "$out$rel/.gitkeep"
        else
            mkdir -p "$out${rel%/*}"
            cat "$p" > "$out$rel" 2>/dev/null || {
                rm -f "$out$rel"
                echo "warning: couldn't read $rel" >&2
            }
        fi
    done
}

efi_global=8be4df61-93ca-11d2-aa0d-00e098032b8c
shim_lock=605dab50-e046-4300-abb6-3dd810dd8b23

copy /proc/cmdline
copy /proc/sys/kernel/tainted
copy /proc/sys/kernel/kexec_load_disabled

copy /sys/firmware/efi/efivars/SecureBoot-$efi_global
copy /sys/firmware/efi/efivars/SetupMode-$efi_global
copy "/sys/firmware/efi/efivars/MokListRT*-$shim_lock"
[ -d "$src/sys/firmware/efi" ] && mkdir -p "$out/sys/firmware/efi"

copy /sys/kernel/security/lsm
copy /sys/kernel/security/lockdown

copy /sys/class/tpm/tpm0/tpm_version_major
copy '/sys/class/tpm/tpm0/pcr-sha256/*'
copy /sys/class/tpm/tpm0 # after its files, so it only gets a .gitkeep if they're missing

copy /sys/class/dmi/id/chassis_type
copy '/sys/class/iommu/*'
copy '/sys/kernel/iommu_groups/*/type'

if [ "$event_log" = 1 ]; then
    log=/sys/kernel/security/tpm0/binary_bios_measurements
    if [ -n "$src" ]; then
        read_log() { cat "$src$log"; }
    else
        read_log() { sudo cat "$log"; }
    fi
    mkdir -p "$out${log%/*}"
    read_log > "$out$log" || {
        rm -f "$out$log"
        rmdir "$out${log%/*}" 2>/dev/null || true
        echo "warning: couldn't read $log" >&2
    }
fi

# Context for whoever turns this into a test. Not read by the app.
{
    echo "captured: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "kernel: $(uname -r)"
    [ -r /etc/os-release ] && . /etc/os-release && echo "os: ${PRETTY_NAME:-unknown}"
    [ -r /sys/class/dmi/id/sys_vendor ] && echo "vendor: $(cat /sys/class/dmi/id/sys_vendor)"
    [ -r /sys/class/dmi/id/product_name ] && echo "product: $(cat /sys/class/dmi/id/product_name)"
} > "$out/CAPTURE_INFO" 2>/dev/null || true

echo "Captured to $out"
