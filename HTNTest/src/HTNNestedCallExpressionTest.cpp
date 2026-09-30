// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNFileHelpers.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Domain/Source/HTNSourceText.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Hook/HTNPlanningUnit.h"
#include "Translator/HTNCCodeGenerator.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"
#include "Translator/HTNGeneratedDebugger.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

extern "C" const HTNGeneratedPlannerDefinition* CreateNestedOperatorCallsHTN_GetDefinition(void);

namespace
{
struct Reports
{
    int Count = 0;
    HTNCallTermErrorReason Reason{};
    std::string Name, Domain, File;
    uint32_t Line = 0, Column = 0;
};

void Report(void* inClient, const HTNCallTermErrorInfo* inInfo)
{
    auto& Client = *static_cast<Reports*>(inClient);
    ++Client.Count;
    Client.Reason = inInfo->Reason;
    Client.Name = inInfo->Name ? inInfo->Name : "";
    Client.Domain = inInfo->Source.domain ? inInfo->Source.domain : "";
    Client.File = inInfo->Source.file ? inInfo->Source.file : "";
    Client.Line = inInfo->Source.line;
    Client.Column = inInfo->Source.column;
}

std::string Read(const std::filesystem::path& inPath)
{
    std::ifstream Input(inPath, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

#ifdef HTN_DEBUG_DECOMPOSITION
void CheckSourceCapture(const HTNGeneratedDebugger& inDebugger)
{
    int Roots = 0;
    for (const auto& Node : inDebugger.GetNodes())
    {
        SCOPED_TRACE(Node.DisplayName);
        ASSERT_FALSE(Node.DisplayName.empty());
        EXPECT_EQ(Node.DisplayName.front(), '(');
        EXPECT_EQ(Node.DisplayName.back(), ')');
        EXPECT_EQ(Node.DisplayName.find("$assignment_call_"), std::string::npos);
        EXPECT_EQ(Node.DisplayName.find("__task_call_result_"), std::string::npos);
        if (Node.Started) EXPECT_TRUE(Node.Completed);
        if (Node.ParentEventNodeId == HTN_GENERATED_NO_INDEX) ++Roots;
        for (const auto Child : Node.Children)
        {
            const auto* ChildNode = inDebugger.FindNode(Child);
            ASSERT_NE(ChildNode, nullptr);
            EXPECT_EQ(ChildNode->ParentEventNodeId, Node.EventNodeId);
        }
        std::string Rendered;
        for (const auto& Token : Node.TitleTokens)
        {
            if (!Rendered.empty() && Token.SpaceBefore) Rendered += ' ';
            Rendered += Token.Text;
        }
        if (!Node.TitleTokens.empty()) EXPECT_EQ(Rendered, Node.DisplayName);
        for (const auto* Values : {&Node.VariablesBefore, &Node.VariablesAfter})
            for (const auto& Value : *Values)
            {
                EXPECT_EQ(Value.Name.find("$assignment_call_"), std::string::npos);
                EXPECT_EQ(Value.Name.find("__task_call_result_"), std::string::npos);
            }
    }
    EXPECT_EQ(Roots, 1);
}
#endif
}

TEST(HTNNestedCallExpressionTest, TranslatorEmitsInvocationInsteadOfCallNameValue)
{
    const std::string Source = "(:domain Nested top_level_domain (:method (run) top_level_method "
        "(branch (and (< (call missing_distance_callterm) 0.2)) ((!result)))))";
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("nested.domain", Source, {}, Loaded, Diagnostics));
    HTNCompilerIR IR;
    std::string Error;
    ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
        HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error)) << Error;
    EXPECT_EQ(IR.CallTermStringIds.size(), 1u);
    for (const auto& Value : IR.Values)
    {
        EXPECT_NE(Value.Kind, HTNIRValueKind::Call);
        if (Value.StaticValueIndex != HTN_IR_NO_INDEX)
            EXPECT_NE(IR.Strings.Values[Value.Text], "missing_distance_callterm");
    }
    const auto Path = std::filesystem::temp_directory_path() / "htn_nested_operator_regression.generated.c";
    HTNCCodeGeneratorOptions Options;
    Options.OutputSourcePath = Path.string();
    Options.EntryPointName = "CreateNestedRegressionHTN";
    Options.SourceFilePath = "nested.domain";
    Options.SourceText = Source;
    Options.LinkedSourceFiles = Loaded.SourceFiles;
    ASSERT_TRUE(HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error)) << Error;
    EXPECT_NE(Read(Path).find("HTNCallTermRegistry_InvokeGeneratedCallTermWithSource(context,"), std::string::npos);
    std::error_code Ec;
    std::filesystem::remove(Path, Ec);
}

TEST(HTNNestedCallExpressionTest, ValuesParticipateInBothOperandsAndEveryOperator)
{
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    int Calls = 0;
    Registry.Bind("distance", [&Calls](const HTNCallTermArguments&) { ++Calls; return 0.1f; });
    Registry.Bind("missing_distance_callterm", [&Calls](const HTNCallTermArguments& Args) {
        ++Calls;
        EXPECT_EQ(Args.size(), 2u);
        EXPECT_EQ(HTNAtomGetListSize(Args[0]), 3);
        EXPECT_EQ(HTNAtomGetValue<float>(HTNAtomGetListElement(Args[0], 0u)), 1.0f);
        EXPECT_EQ(HTNAtomGetValue<float>(HTNAtomGetListElement(Args[1], 0u)), 2.0f);
        return 0.1f;
    });
    Registry.Bind("identity", [&Calls](const HTNCallTermArguments& Args) { ++Calls; return HTNAtomOwner(Args[0]); });
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
    struct Case { const char* Entry; int Calls; };
    for (const auto Test : {Case{"valid_left", 1}, Case{"valid_right", 1}, Case{"valid_both", 2},
                            Case{"valid_bound", 1}, Case{"operators", 15}, Case{"behave", 1}, Case{"task_arithmetic", 2}})
    {
        SCOPED_TRACE(Test.Entry);
        HTNPlanningUnit Unit(Database, Hook, Test.Entry);
        Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
#ifdef HTN_DEBUG_DECOMPOSITION
        HTNGeneratedDebugger Debugger;
        Debugger.SetEnabled(true);
        Unit.SetGeneratedDebugger(&Debugger);
#endif
        Calls = 0;
        ASSERT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Calls, Test.Calls);
        EXPECT_EQ(Unit.GetCurrentPlan().size(), 1u);
#ifdef HTN_DEBUG_DECOMPOSITION
        CheckSourceCapture(Debugger);
#endif
    }
}

TEST(HTNNestedCallExpressionTest, MissingPoliciesReasonsCountAndExpressionSource)
{
    const auto Path = HTNFileHelpers::MakeAbsolutePath("Domains/Test/nested_operator_calls.domain");
    const std::string Text = Read(Path);
    ASSERT_FALSE(Text.empty());
    const HTNSourceText Source(Text);
    for (const auto Reason : {HTNCallTermErrorReason::NotRegistered,
                             HTNCallTermErrorReason::MissingBinding, HTNCallTermErrorReason::MissingInstance})
    {
        HTNDatabaseHook Database;
        HTNCallTermRegistry Registry;
        int OuterCalls = 0;
        Registry.Bind("identity", [&OuterCalls](const HTNCallTermArguments& Args) { ++OuterCalls; return HTNAtomOwner(Args[0]); });
        if (Reason == HTNCallTermErrorReason::MissingBinding)
            ASSERT_TRUE(Registry.BindMember("missing_distance_callterm", "agent", {}, {}));
        if (Reason == HTNCallTermErrorReason::MissingInstance)
            ASSERT_TRUE(Registry.BindMember("missing_distance_callterm", "agent",
                [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(0.1f); }, {}));
        HTNPlannerHook Hook(Database.GetWorldState(), Registry);
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
        for (const auto Policy : {HTNCallTermErrorPolicy::FailSilently, HTNCallTermErrorPolicy::Report})
            for (const std::string Entry : {"behave", "missing_right", "missing_arithmetic", "missing_deep", "bound_first", "two_attempts", "short_circuit", "missing_task_arithmetic"})
            {
                SCOPED_TRACE(Entry);
                HTNPlanningUnit Unit(Database, Hook, Entry);
                Reports Client;
                auto& Context = Unit.GetExecutionContext();
                Context.ClientContext = &Client;
                Context.CallTermErrorPolicy = Policy;
                Context.CallTermErrorCallback = Report;
#ifdef HTN_DEBUG_DECOMPOSITION
                HTNGeneratedDebugger Debugger;
                Debugger.SetEnabled(true);
                Unit.SetGeneratedDebugger(&Debugger);
#endif
                for (int Attempt = 0; Attempt < 2; ++Attempt)
                {
                    Client.Count = 0;
                    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), Entry == "short_circuit" ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
                    const int ExpectedReports = Policy == HTNCallTermErrorPolicy::FailSilently || Entry == "short_circuit" ? 0 : Entry == "two_attempts" ? 2 : 1;
                    EXPECT_EQ(Client.Count, ExpectedReports);
                    EXPECT_EQ(OuterCalls, 0);
#ifdef HTN_DEBUG_DECOMPOSITION
                    CheckSourceCapture(Debugger);
#endif
                    if (ExpectedReports == 0) continue;
                    EXPECT_EQ(Client.Reason, Reason);
                    EXPECT_EQ(Client.Name, "missing_distance_callterm");
                    EXPECT_EQ(Client.Domain, "NestedOperatorCalls");
                    EXPECT_EQ(HTNFileHelpers::MakeAbsolutePath(Client.File).lexically_normal().generic_string(), Path.lexically_normal().generic_string());
                    const size_t Method = Text.find("(:method (" + Entry + ")");
                    ASSERT_NE(Method, std::string::npos);
                    size_t Call = Text.find("(call missing_distance_callterm", Method);
                    if (Entry == "two_attempts") Call = Text.find("(call missing_distance_callterm", Call + 1);
                    ASSERT_NE(Call, std::string::npos);
                    const auto Position = Source.GetPosition(Call);
                    EXPECT_EQ(Client.Line, static_cast<uint32_t>(Position.Line));
                    EXPECT_EQ(Client.Column, static_cast<uint32_t>(Position.Column));
                }
            }
    }
}

TEST(HTNNestedCallExpressionTest, UnsetKeepsSdkContractForNestedInvocation)
{
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
    HTNPlanningUnit Unit(Database, Hook, "behave");
#ifndef NDEBUG
    const auto Run = [&]() {
#if defined(_WIN32) && defined(_DEBUG)
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
        (void)Unit.DecomposeTopLevelMethod();
    };
    EXPECT_DEATH(Run(), "Configure the callterm error policy explicitly");
#else
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
#endif
}

TEST(HTNNestedCallExpressionTest, ContinueMoveDebuggerPreservesSourceAndExecution)
{
    HTNDatabaseHook Database;
    Database.GetWorldState().AddFact("active_plan", std::vector<HTNAtomOwner>{
        HTNAtomOwner(int32{1}), HTNAtomOwner(HtnSymbol::sGetSymbol("moving_to_seen_entity")),
        HTNAtomOwner(int32{42}), HTNAtomOwner(int32{10})});
    HTNCallTermRegistry Registry;
    int PositionCalls = 0, DistanceCalls = 0;
    float Distance = 0.1f;
    Registry.Bind("get_entity_position", [&](const HTNCallTermArguments& Arguments) {
        ++PositionCalls;
        EXPECT_EQ(Arguments.size(), 1u);
        EXPECT_EQ(HTNAtomGetValue<int32>(Arguments[0]), 42);
        return int32{20};
    });
    Registry.Bind("get_distance_from_to", [&](const HTNCallTermArguments& Arguments) {
        ++DistanceCalls;
        EXPECT_EQ(Arguments.size(), 2u);
        EXPECT_EQ(HTNAtomGetValue<int32>(Arguments[0]), 10);
        EXPECT_EQ(HTNAtomGetValue<int32>(Arguments[1]), 20);
        return Distance;
    });
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
    HTNPlanningUnit Unit(Database, Hook, "continue_move");
    Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
#ifdef HTN_DEBUG_DECOMPOSITION
    HTNGeneratedDebugger Debugger;
    Debugger.SetEnabled(true);
    Unit.SetGeneratedDebugger(&Debugger);
    const auto File = HTNFileHelpers::MakeAbsolutePath("Domains/Test/nested_operator_calls.domain");
    const HTNSourceText Source(Read(File));
    const std::string Assignment = "(= ?new_entity_position (call get_entity_position ?entity_id))";
    const std::string Comparison = "(< (call get_distance_from_to ?old_entity_position ?new_entity_position) 0.2)";
#endif
    for (const bool ShouldContinue : {true, false, true})
    {
        SCOPED_TRACE(ShouldContinue);
        PositionCalls = DistanceCalls = 0;
        Distance = ShouldContinue ? 0.1f : 0.3f;
#ifdef HTN_DEBUG_DECOMPOSITION
        Debugger.Reset();
#endif
        ASSERT_EQ(Unit.DecomposeTopLevelMethod(), ShouldContinue ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(PositionCalls, 1);
        EXPECT_EQ(DistanceCalls, 1);
        EXPECT_EQ(Unit.GetCurrentPlan().size(), ShouldContinue ? 1u : 0u);
#ifdef HTN_DEBUG_DECOMPOSITION
        CheckSourceCapture(Debugger);
        int Assignments = 0, Comparisons = 0;
        for (const auto& Node : Debugger.GetNodes())
        {
            EXPECT_EQ(Node.DisplayName.find("$assignment_call_"), std::string::npos) << Node.DisplayName;
            for (const auto* Values : {&Node.VariablesBefore, &Node.VariablesAfter})
                for (const auto& Value : *Values)
                    EXPECT_EQ(Value.Name.find("$assignment_call_"), std::string::npos) << Value.Name;
            if (Node.Started) EXPECT_TRUE(Node.Completed) << Node.DisplayName;
            if (!Node.Started || (Node.DisplayName != Assignment && Node.DisplayName != Comparison)) continue;
            const bool IsComparison = Node.DisplayName == Comparison;
            IsComparison ? ++Comparisons : ++Assignments;
            EXPECT_EQ(Node.Succeeded, !IsComparison || ShouldContinue);
            EXPECT_EQ(Node.Kind, IsComparison ? HTNGeneratedDebugger::NodeKind::BuiltinComparison : HTNGeneratedDebugger::NodeKind::CallBind);
            // Comparison AST ranges begin at the operator; assignment ranges
            // begin at the opening parenthesis. Preserve both source contracts.
            const auto Position = Source.GetPosition(Source.GetText().find(Node.DisplayName) + (IsComparison ? 1u : 0u));
            EXPECT_EQ(Node.Source.Line, static_cast<uint32_t>(Position.Line));
            EXPECT_EQ(Node.Source.Column, static_cast<uint32_t>(Position.Column));
            EXPECT_TRUE(Node.Children.empty()) << Node.DisplayName;
        }
        EXPECT_EQ(Assignments, 1);
        EXPECT_EQ(Comparisons, 1);
#endif
    }
}

#ifdef HTN_DEBUG_DECOMPOSITION
TEST(HTNNestedCallExpressionTest, DebuggerKeepsRetriesSkippedExpressionsAndNestedAssignments)
{
    HTNDatabaseHook Database;
    Database.GetWorldState().AddFact("candidate", std::vector<HTNAtomOwner>{HTNAtomOwner(int32{3})});
    Database.GetWorldState().AddFact("candidate", std::vector<HTNAtomOwner>{HTNAtomOwner(int32{1})});
    HTNCallTermRegistry Registry;
    int Calls = 0;
    Registry.Bind("identity", [&](const HTNCallTermArguments& Arguments) { ++Calls; return HTNAtomOwner(Arguments[0]); });
    Registry.Bind("distance", [&](const HTNCallTermArguments&) { ++Calls; return 0.1f; });
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
    struct Case { const char* Entry; const char* Expression; int Calls; int Attempts; int Successes; };
    for (const auto Test : {
        Case{"debugger_backtracking", "(< (call identity ?entity) 2)", 2, 2, 1},
        Case{"debugger_skipped", "(< (call distance) 0.2)", 0, 0, 0},
        Case{"debugger_assignment", "(= ?value (call identity (+ 1 (call identity 2))))", 2, 1, 1}})
    {
        SCOPED_TRACE(Test.Entry);
        HTNGeneratedDebugger Debugger;
        Debugger.SetEnabled(true);
        HTNPlanningUnit Unit(Database, Hook, Test.Entry);
        Unit.SetGeneratedDebugger(&Debugger);
        Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
        Calls = 0;
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(), Test.Successes ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Calls, Test.Calls);
        CheckSourceCapture(Debugger);
        int Rows = 0, Attempts = 0, Successes = 0;
        for (const auto& Node : Debugger.GetNodes())
        {
            if (Node.DisplayName != Test.Expression) continue;
            ++Rows;
            Attempts += Node.Started ? 1 : 0;
            Successes += Node.Succeeded ? 1 : 0;
            EXPECT_TRUE(Node.Children.empty());
            const auto* Parent = Debugger.FindNode(Node.ParentEventNodeId);
            ASSERT_NE(Parent, nullptr);
            EXPECT_EQ(Parent->Kind, HTNGeneratedDebugger::NodeKind::And);
            if (std::string(Test.Entry) == "debugger_assignment")
            {
                bool HasOutput = false;
                for (const auto& Value : Node.VariablesAfter)
                    if (Value.Name == "value")
                    {
                        HasOutput = true;
                        EXPECT_EQ(HTNAtomGetValue<int32>(*Value.Value.Get()), 3);
                    }
                EXPECT_TRUE(HasOutput);
            }
        }
        EXPECT_GT(Rows, 0);
        EXPECT_EQ(Attempts, Test.Attempts);
        EXPECT_EQ(Successes, Test.Successes);
    }
}
#endif
