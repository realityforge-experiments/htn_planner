// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#pragma once

#include "Core/HTNCallTermError.h"
#include <cstdio>

inline void ReportDemoCallTermError(const char* inBackend, const HTNCallTermErrorInfo& inInfo)
{
    const char* Reason = "unknown reason";
    switch (inInfo.Reason)
    {
    case HTNCallTermErrorReason::NotRegistered: Reason = "not registered"; break;
    case HTNCallTermErrorReason::MissingBinding: Reason = "no callable bound"; break;
    case HTNCallTermErrorReason::MissingInstance: Reason = "daemon instance missing"; break;
    case HTNCallTermErrorReason::ArgumentCountMismatch: Reason = "argument count mismatch"; break;
    case HTNCallTermErrorReason::ArgumentTypeMismatch: Reason = "incompatible argument type"; break;
    case HTNCallTermErrorReason::ArgumentConversionFailed: Reason = "argument conversion failed"; break;
    case HTNCallTermErrorReason::ReturnConversionFailed: Reason = "return conversion failed"; break;
    case HTNCallTermErrorReason::None: break;
    }
    std::fprintf(stderr, "[%s] Callterm '%s' failed: %s (daemon: %s, domain: %s, source: %s:%u:%u)\n",
                 inBackend, inInfo.Name ? inInfo.Name : "<unknown>", Reason,
                 inInfo.DaemonID ? inInfo.DaemonID : "-",
                 inInfo.Source.domain ? inInfo.Source.domain : "<unknown>",
                 inInfo.Source.file ? inInfo.Source.file : "<unavailable>",
                 inInfo.Source.line, inInfo.Source.column);
    if (inInfo.ArgumentIndex != UINT32_MAX)
        std::fprintf(stderr, "  argument %u: expected atom type %u (%s), received %u\n",
            inInfo.ArgumentIndex + 1, inInfo.ExpectedAtomType,
            inInfo.ExpectedTypeName ? inInfo.ExpectedTypeName : "see atom type", inInfo.ActualAtomType);
}

inline void ReportGeneratedDemoCallTermError(void*, const HTNCallTermErrorInfo* inInfo)
{
    ReportDemoCallTermError("Generated", *inInfo);
}
