// SPDX-License-Identifier: GPL-3.0-or-later
#include "AllChecks.h"

#include "Checks.h"
#include "PlaceholderCheck.h"

using namespace Qt::StringLiterals;

CheckList createAllChecks()
{
    CheckList checks;
    checks.push_back(std::make_unique<KernelSigningCheck>());
    checks.push_back(std::make_unique<TpmCheck>());
    checks.push_back(std::make_unique<SecureBootCheck>());
    // Phase 4: TCG2 event log parser + PCR replay.
    checks.push_back(std::make_unique<PlaceholderCheck>(u"measured-boot"_s,
                                                        u"Measured boot"_s,
                                                        QList{HostRequest::read(HostPaths::TpmPcrsSha256)},
                                                        QList{HostRequest::read(HostPaths::EventLog)}));
    checks.push_back(std::make_unique<IommuCheck>());
    checks.push_back(std::make_unique<LockdownCheck>());
    checks.push_back(std::make_unique<KexecCheck>());
    checks.push_back(std::make_unique<BpfLsmCheck>());
    return checks;
}
