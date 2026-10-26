// SPDX-License-Identifier: GPL-3.0-or-later
#include "AllChecks.h"

#include "Checks.h"

CheckList createAllChecks()
{
    CheckList checks;
    checks.push_back(std::make_unique<KernelSigningCheck>());
    checks.push_back(std::make_unique<TpmCheck>());
    checks.push_back(std::make_unique<SecureBootCheck>());
    checks.push_back(std::make_unique<MeasuredBootCheck>());
    checks.push_back(std::make_unique<IommuCheck>());
    checks.push_back(std::make_unique<LockdownCheck>());
    checks.push_back(std::make_unique<KexecCheck>());
    checks.push_back(std::make_unique<BpfLsmCheck>());
    return checks;
}
