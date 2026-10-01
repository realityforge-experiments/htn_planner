// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#pragma once

#include <assert.h>
#include <stdint.h>

#define HTN_CALLTERM_ERROR_UNSET UINT32_C(0)
#define HTN_CALLTERM_ERROR_FAIL_SILENTLY UINT32_C(1)
#define HTN_CALLTERM_ERROR_REPORT UINT32_C(2)

#ifdef __cplusplus
enum class HTNCallTermErrorPolicy : uint32_t
{
    Unset = HTN_CALLTERM_ERROR_UNSET,
    FailSilently = HTN_CALLTERM_ERROR_FAIL_SILENTLY,
    Report = HTN_CALLTERM_ERROR_REPORT
};
enum class HTNCallTermErrorReason : uint32_t
{
    NotRegistered,
    MissingBinding,
    MissingInstance,
    ArgumentCountMismatch,
    ArgumentTypeMismatch,
    ArgumentConversionFailed,
    ReturnConversionFailed,
    NonBooleanConditionResult,
    None = UINT32_MAX
};
#else
typedef uint32_t HTNCallTermErrorPolicy;
typedef uint32_t HTNCallTermErrorReason;
#define HTN_CALLTERM_ERROR_NOT_REGISTERED UINT32_C(0)
#define HTN_CALLTERM_ERROR_MISSING_BINDING UINT32_C(1)
#define HTN_CALLTERM_ERROR_MISSING_INSTANCE UINT32_C(2)
#define HTN_CALLTERM_ERROR_ARGUMENT_COUNT_MISMATCH UINT32_C(3)
#define HTN_CALLTERM_ERROR_ARGUMENT_TYPE_MISMATCH UINT32_C(4)
#define HTN_CALLTERM_ERROR_ARGUMENT_CONVERSION_FAILED UINT32_C(5)
#define HTN_CALLTERM_ERROR_RETURN_CONVERSION_FAILED UINT32_C(6)
#define HTN_CALLTERM_ERROR_NON_BOOLEAN_CONDITION_RESULT UINT32_C(7)
#define HTN_CALLTERM_ERROR_NONE UINT32_MAX
#endif

/* Borrowed invocation provenance, independent of debug instrumentation. */
typedef struct HTNCallTermSource
{
    const char* domain;
    const char* file;
    uint32_t line;
    uint32_t column;
} HTNCallTermSource;

/* Borrowed data valid only during the callback; source strings may be null. */
typedef struct HTNCallTermErrorInfo
{
    const char* Name;
    HTNCallTermErrorReason Reason;
    const char* DaemonID;
    HTNCallTermSource Source;
    /* Zero-based index; UINT32_MAX when the error is not tied to an argument. */
    uint32_t ArgumentIndex;
    uint32_t ExpectedArgumentCount;
    uint32_t ActualArgumentCount;
    /* HTNAtomType values, or UINT32_MAX when not available. */
    uint32_t ExpectedAtomType;
    uint32_t ActualAtomType;
    const char* ExpectedTypeName;
} HTNCallTermErrorInfo;

/* The client owns the context. If this returns, invocation fails normally. */
typedef void (*HTNCallTermErrorCallback)(void* client_context, const HTNCallTermErrorInfo* info);

/* Shared by registry invocation and generated condition checks. No bridge export
 * or layout change: the caller supplies borrowed metadata and handles failure. */
static inline void HTNCallTerm_ReportError(HTNCallTermErrorPolicy policy,
                                         HTNCallTermErrorCallback callback,
                                         void* client_context,
                                         const HTNCallTermErrorInfo* info)
{
    const uint32_t policy_value = (uint32_t)policy;
    assert(policy_value != HTN_CALLTERM_ERROR_UNSET && "Configure the callterm error policy explicitly");
    assert((policy_value == HTN_CALLTERM_ERROR_UNSET || policy_value == HTN_CALLTERM_ERROR_FAIL_SILENTLY ||
            policy_value == HTN_CALLTERM_ERROR_REPORT) && "Invalid callterm error policy");
    if (policy_value == HTN_CALLTERM_ERROR_REPORT)
    {
        assert(callback && "Report policy requires a callterm error callback");
        if (callback) callback(client_context, info);
    }
}
