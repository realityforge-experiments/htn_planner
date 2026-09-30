// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#pragma once

#include "Domain/Source/HTNSourceText.h"
#include <string>
#include <unordered_set>
#include <vector>

// Neutral validation input shared by both frontends; no runtime or AST ownership.
struct HTNAssignmentScopeNode
{
    enum class Kind { Leaf, Sequence, Alternatives, Negation };
    Kind Type = Kind::Leaf;
    std::string Destination;
    HTNSourceRange Range;
    std::vector<std::string> Uses;
    std::vector<HTNAssignmentScopeNode> Children;
};

// Axiom output parameters declare slots, but their first use may initialize them.
// Input/output parameters additionally require an unbound value at runtime.
inline bool HTNAssignmentParameterIsInitiallyUsed(const std::string& inName, bool inAxiom)
{
    return !inAxiom || (!inName.starts_with("out_") && !inName.starts_with("io_"));
}

template<typename Report>
bool HTNValidateAssignmentScope(const HTNAssignmentScopeNode& inNode,
                                std::unordered_set<std::string>& ioSeen, Report&& inReport)
{
    bool Valid = true;
    for (const auto& Use : inNode.Uses) ioSeen.insert(Use);
    if (!inNode.Destination.empty())
    {
        if (!ioSeen.insert(inNode.Destination).second)
        {
            inReport(inNode.Range, "Assignment destination '?" + inNode.Destination +
                "' has already been declared or used; assignment must declare a fresh variable");
            Valid = false;
        }
    }
    const auto Entry = ioSeen;
    for (const auto& Child : inNode.Children)
    {
        if (inNode.Type == HTNAssignmentScopeNode::Kind::Alternatives ||
            inNode.Type == HTNAssignmentScopeNode::Kind::Negation)
        {
            auto Branch = Entry;
            Valid = HTNValidateAssignmentScope(Child, Branch, inReport) && Valid;
            // A later declaration cannot reuse a name mentioned in any alternative.
            ioSeen.insert(Branch.begin(), Branch.end());
        }
        else Valid = HTNValidateAssignmentScope(Child, ioSeen, inReport) && Valid;
    }
    return Valid;
}
