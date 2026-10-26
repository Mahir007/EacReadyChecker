// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "HostPaths.h"
#include "core/Check.h"

// One class per published requirement. Pass criteria live in the .cpp files.

class KernelSigningCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("kernel-signing"); }
    QString title() const override { return QStringLiteral("Signed kernel & kernel modules"); }
    QList<HostRequest> requests() const override;
    QList<HostRequest> privilegedRequests() const override { return {HostRequest::read(HostPaths::EventLog)}; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class TpmCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("tpm"); }
    QString title() const override { return QStringLiteral("TPM 2.0"); }
    QList<HostRequest> requests() const override;
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class SecureBootCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("secure-boot"); }
    QString title() const override { return QStringLiteral("Secure Boot"); }
    QList<HostRequest> requests() const override;
    QList<HostRequest> privilegedRequests() const override { return {HostRequest::read(HostPaths::EventLog)}; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class MeasuredBootCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("measured-boot"); }
    QString title() const override { return QStringLiteral("Measured boot"); }
    QList<HostRequest> requests() const override;
    QList<HostRequest> privilegedRequests() const override { return {HostRequest::read(HostPaths::EventLog)}; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class IommuCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("iommu"); }
    QString title() const override { return QStringLiteral("IOMMU"); }
    QList<HostRequest> requests() const override;
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class LockdownCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("lockdown"); }
    QString title() const override { return QStringLiteral("Kernel lockdown (integrity)"); }
    QList<HostRequest> requests() const override;
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class KexecCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("kexec"); }
    QString title() const override { return QStringLiteral("Kexec disabled"); }
    QList<HostRequest> requests() const override { return {HostRequest::read(HostPaths::KexecLoadDisabled)}; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};

class BpfLsmCheck : public Check
{
public:
    QString id() const override { return QStringLiteral("bpf-lsm"); }
    QString title() const override { return QStringLiteral("BPF LSM"); }
    QList<HostRequest> requests() const override { return {HostRequest::read(HostPaths::Lsm)}; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;
};
