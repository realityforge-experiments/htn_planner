// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Translator/HTNCompilerReachability.h"

HTNCompilerReachability HTNAnalyzeCompilerReachability(const HTNCompilerIR& inIR)
{
    HTNCompilerReachability Result;
    Result.Methods.resize(inIR.Methods.size());
    Result.Implementations.resize(inIR.Methods.size());
    Result.Branches.resize(inIR.Branches.size());
    Result.Tasks.resize(inIR.Tasks.size());
    Result.Conditions.resize(inIR.Conditions.size());
    Result.Axioms.resize(inIR.Axioms.size());
    std::vector<uint32> PendingMethods, PendingConditions;
    std::vector<const HTNIRValue*> PendingValues;
    const auto AddMethod = [&](uint32 Index) {
        if (!Result.Methods[Index]) { Result.Methods[Index] = true; PendingMethods.push_back(Index); }
    };
    const auto AddCondition = [&](uint32 Index) {
        if (Index != HTN_IR_NO_INDEX && !Result.Conditions[Index])
        {
            Result.Conditions[Index] = true;
            PendingConditions.push_back(Index);
        }
    };
    const auto AddArguments = [&](uint32 First, uint32 Count) {
        for (uint32 I = 0; I < Count; ++I) PendingValues.push_back(&inIR.Values[First + I]);
    };

    // Every top_level_method is a client entry, even with no domain callers.
    // Preserve all existing deferred dispatch entries, including targets mentioned
    // in otherwise unreachable methods: clients may already hold those calls.
    for (uint32 M = 0; M < inIR.Methods.size(); ++M)
        if (inIR.Methods[M].IsTopLevel || inIR.Methods[M].IsExternallyDecomposable) AddMethod(M);

    // Iterative traversal handles self/mutual recursion and shared implementations.
    while (!PendingMethods.empty())
    {
        const uint32 M = PendingMethods.back();
        PendingMethods.pop_back();
        const auto& Method = inIR.Methods[M];
        if (Result.Implementations[Method.ImplementationIndex]) continue;
        Result.Implementations[Method.ImplementationIndex] = true;
        for (uint32 BI = 0; BI < Method.BranchCount; ++BI)
        {
            const uint32 B = Method.FirstBranch + BI;
            const auto& Branch = inIR.Branches[B];
            Result.Branches[B] = true;
            AddCondition(Branch.Condition);
            for (uint32 TI = 0; TI < Branch.TaskCount; ++TI)
            {
                const uint32 T = Branch.FirstTask + TI;
                const auto& Task = inIR.Tasks[T];
                Result.Tasks[T] = true;
                AddArguments(Task.FirstArgument, Task.ArgumentCount);
                if (T < inIR.TaskCallExpressions.size())
                    for (const auto& Call : inIR.TaskCallExpressions[T])
                    {
                        if (Call.IsRuntimeValue) PendingValues.push_back(&Call.RuntimeValue);
                        for (const auto& Argument : Call.Arguments) PendingValues.push_back(&Argument);
                    }
                if (Task.Kind == HTN_TASK_COMPOUND || Task.Kind == HTN_TASK_DEFERRED)
                {
                    const int Target = inIR.FindMethodByStringId(Task.Id, Task.ArgumentCount);
                    if (Target >= 0) AddMethod(static_cast<uint32>(Target));
                }
            }
        }
    }
    while (!PendingConditions.empty())
    {
        const uint32 C = PendingConditions.back();
        PendingConditions.pop_back();
        const auto& Condition = inIR.Conditions[C];
        AddArguments(Condition.FirstArgument, Condition.ArgumentCount);
        for (uint32 I = 0; I < Condition.ChildCount; ++I)
            AddCondition(inIR.ConditionChildRefs[Condition.FirstChildRef + I]);
        if (Condition.Kind == HTN_CONDITION_AXIOM)
        {
            Result.Axioms[Condition.ResolvedIndex] = true;
            AddCondition(inIR.Axioms[Condition.ResolvedIndex].Condition);
        }
    }
    std::vector<bool> VisitedExpressions(inIR.RuntimeExpressions.size());
    while (!PendingValues.empty())
    {
        const auto& Value = *PendingValues.back();
        PendingValues.pop_back();
        if (Value.Kind == HTNIRValueKind::Arithmetic) Result.NeedsArithmetic = true;
        if (Value.RuntimeExpression != HTN_IR_NO_INDEX && !VisitedExpressions[Value.RuntimeExpression])
        {
            VisitedExpressions[Value.RuntimeExpression] = true;
            for (const auto& Child : inIR.RuntimeExpressions[Value.RuntimeExpression].Children)
                PendingValues.push_back(&Child);
        }
    }
    return Result;
}
