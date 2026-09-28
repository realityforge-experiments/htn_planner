// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#pragma once

#include "Domain/Source/HTNSourceText.h"
#include <cstdint>
#include <string>

enum class HTNParserErrorCode : uint8_t
{
    None,
    TokenOutOfBounds,
    UnexpectedToken,
    AxiomPrefixInTaskList,
    UnclosedList,
    IncompleteSyntax,
    ExpectedIdentifier,
    EmptyLiteralList,
    ExpectedLiteral,
    InvalidArithmeticArity,
    ExpectedCondition,
    InvalidNotCondition,
    InvalidComparisonArity,
    InvalidAssignment,
    ImplicitAssignment,
    ExpectedBoundCall,
    UnexpectedBoundCallSyntax,
    QualifiedFact,
    InvalidSplitArity,
    ExpectedConditionBody,
    ExpectedTask,
    QualifiedPrimitiveTask,
    ExpectedBranch,
    ExpectedDeclaration,
    ExpectedConstant,
    ExpectedConstantLiteral,
    ExpectedParameterVariable,
    InvalidAxiomVisibility,
    UnexpectedAxiomSyntax,
    UnknownDeclaration,
    TrailingSource,
    ExpectedDomain,
    LexingFailed,
    ExpectedIncludePath,
    UnterminatedIncludePath,
    ExpectedIncludeEnd,
    MisplacedInclude
};

inline constexpr const char* HTNInvalidAssignmentDiagnostic = "Assignment requires a variable destination and exactly one value expression";
inline constexpr const char* HTNImplicitAssignmentDiagnostic = "Implicit call-result binding is no longer supported; use an explicit assignment condition";

struct HTNParserError
{
    HTNParserErrorCode Code = HTNParserErrorCode::None;
    std::string Message;
    HTNSourceRange Range;

    bool HasError() const { return Code != HTNParserErrorCode::None; }
};
