// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#pragma once

#include "Translator/HTNCompilerIR.h"

// Emission masks over validated, resolved IR. Keep its indices and metadata intact.
struct HTNCompilerReachability
{
    std::vector<bool> Methods;
    std::vector<bool> Implementations;
    std::vector<bool> Branches;
    std::vector<bool> Tasks;
    std::vector<bool> Conditions;
    std::vector<bool> Axioms;
    bool NeedsArithmetic = false;
};

HTNCompilerReachability HTNAnalyzeCompilerReachability(const HTNCompilerIR& inIR);
