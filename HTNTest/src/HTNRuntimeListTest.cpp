// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNAtomListAllocator.h"
#include "Core/HTNAtomListOwner.h"
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
#include <fstream>

extern "C" const HTNGeneratedPlannerDefinition* CreateRuntimeListsHTN_GetDefinition(void);
extern "C" const HTNGeneratedPlannerDefinition* CreateRuntimeListsNoneHTN_GetDefinition(void);

namespace
{
std::string Text(const HTNAtom& Atom) { return HTNAtomToString(Atom, true); }

std::string Read(const std::string& Path)
{
    std::ifstream Input(Path);
    return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

class HTNRuntimeListTest : public testing::TestWithParam<bool>
{
protected:
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    HTNPlannerHook Hook{Database.GetWorldState(), Registry};
    const HTNGeneratedPlannerDefinition* Definition = CreateRuntimeListsHTN_GetDefinition();
    HTNGeneratedPlannerContext Context{};
    HTNAtomOwner Plan, Seen;
    int Calls = 0, AfterCalls = 0, OuterCalls = 0, Time = 10, Reports = 0;
    HTNCallTermErrorReason Reason{};
    std::string ReportName, ReportFile;
    uint32_t ReportLine = 0, ReportColumn = 0;
#ifdef HTN_DEBUG_DECOMPOSITION
    HTNGeneratedDebugger Debugger;
#endif
    static void Report(void* Client, const HTNCallTermErrorInfo* Info)
    {
        auto& Self = *static_cast<HTNRuntimeListTest*>(Client);
        ++Self.Reports;
        Self.Reason = Info->Reason;
        Self.ReportName = Info->Name;
        Self.ReportFile = Info->Source.file;
        Self.ReportLine = Info->Source.line;
        Self.ReportColumn = Info->Source.column;
    }
    template<typename T> void AddFact(const char* Name, T&& Value)
    {
        Database.GetWorldState().AddFact(Name, std::vector<HTNAtomOwner>{HTNAtomOwner(std::forward<T>(Value))});
    }
    void SetUp() override
    {
        Definition = GetParam() ? CreateRuntimeListsHTN_GetDefinition() : CreateRuntimeListsNoneHTN_GetDefinition();
        Registry.Bind("ppr_start", [this](const HTNCallTermArguments& Args) { ++OuterCalls; Seen = Args[0]; return true; });
        Registry.Bind("get_entity_position", [this](const HTNCallTermArguments& Args) {
            ++Calls;
            return HTNAtomListOwner{HTNAtomGetValue<int32>(Args[0]), 0, 3};
        });
        Registry.Bind("get_time", [this](const HTNCallTermArguments&) { ++Calls; return Time; });
        Registry.Bind("count_after", [this](const HTNCallTermArguments&) { ++AfterCalls; return 1; });
        Registry.Bind("failing_runtime_list", [this](const HTNCallTermArguments&) { ++Calls; return HTNAtomOwner(); });
        Registry.Bind("identity", [](const HTNCallTermArguments& Args) { return HTNAtomOwner(Args[0]); });
        Registry.Bind("add_fact", [this](const HTNCallTermArguments& Args) {
            AddFact("added", HTNAtomOwner(Args[0]));
            return HTNAtomOwner(Args[0]);
        });
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(Definition));
        Context.world_state = &Database.GetWorldState();
        Context.callterm_binding_context = &Hook.GetCallTermBindingContext();
        Context.prepared_storage = Hook.GetGeneratedPreparedStorage();
        Context.execution_storage = ::operator new(Definition->execution_storage_size);
        ASSERT_TRUE(Definition->initialize_execution_storage(Context.execution_storage));
        Context.backtracking_mode = HTN_BACKTRACKING_ALL;
        Context.client_context = this;
        Context.callterm_error_policy = HTNCallTermErrorPolicy::Report;
        Context.callterm_error_callback = Report;
#ifdef HTN_DEBUG_DECOMPOSITION
        Debugger.SetEnabled(true);
        Context.debugger = &Debugger;
#endif
    }
    void TearDown() override
    {
        Definition->destroy_execution_storage(Context.execution_storage);
        ::operator delete(Context.execution_storage);
    }
    template<typename... Args> HTNDecompositionStatus Run(const char* Method, Args&&... Arguments)
    {
        Plan = HTNAtomOwner();
        HTNAtomOwner Call(HTNAtom::sCreateCall(HtnSymbol::sGetSymbol(Method), std::forward<Args>(Arguments)...));
        return Definition->decompose_call(&Context, Call.Get(), 1, Plan.Get());
    }
    const HTNAtom& Result(size_t Argument = 0)
    {
        return HTNAtomGetListElement(HTNAtomGetListElement(*Plan.Get(), 0), static_cast<uint32>(Argument + 1u));
    }
    std::string Error() const
    {
        const auto* Info = Definition->get_execution_info(Context.execution_storage);
        return Info && Info->last_error ? Info->last_error : "";
    }
};

TEST_P(HTNRuntimeListTest, ExactPprReproducerPassesCurrentEntity)
{
    for (int Id : {7, 9})
    {
        ASSERT_EQ(Run("run", Id), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(*Seen.Get()), "(find_closest_location_to_entity " + std::to_string(Id) + ")");
        EXPECT_EQ(HTNAtomGetValue<int32>(Result()), Id);
    }
    EXPECT_EQ(OuterCalls, 2);
}

TEST_P(HTNRuntimeListTest, StaticAndDynamicListsPreserveValuesAndInputOwnership)
{
    ASSERT_EQ(Run("static"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(HTNAtomGetListSize(Result()), 2);
    HTNAtomOwner Position(HTNAtomListOwner{1, 2, 3});
    const auto Before = Text(*Position.Get());
    ASSERT_EQ(Run("variables", 7, std::move(Position)), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(action 7 (1 2 3))");
    EXPECT_EQ(Text(*Position.Get()), Before);
}

TEST_P(HTNRuntimeListTest, EveryArithmeticOperatorWorksAsListElement)
{
    ASSERT_EQ(Run("arithmetic", 6), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(math 8 4 12 3 0 7 5)");
    ASSERT_EQ(Run("arithmetic_call", 6), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(sum 8)");
    EXPECT_EQ(Run("bad_numeric_operand"), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Run("bad_list_operand", 6), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(AfterCalls, 0);
}

TEST_P(HTNRuntimeListTest, CallsAssignmentsAndNestedCallArgumentsOwnResults)
{
    for (const char* Entry : {"calls", "assignment", "nested_call"})
    {
        SCOPED_TRACE(Entry);
        Calls = 0;
        ASSERT_EQ(Run(Entry, 7), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(Result()), "(target 7 (7 0 3))");
        EXPECT_EQ(Calls, 1);
    }
}

TEST_P(HTNRuntimeListTest, NestedListsAndConstantsWork)
{
    ASSERT_EQ(Run("nested", 7), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(HTNAtomGetListSize(Result()), 5);
    EXPECT_EQ(Text(HTNAtomGetListElement(Result(), 1)), "(target 7)");
    EXPECT_EQ(Text(HTNAtomGetListElement(Result(), 3)), "(metadata current_time 10)");
    EXPECT_EQ(Text(HTNAtomGetListElement(Result(), 4)), "(query 7)");
    EXPECT_EQ(Calls, 2);
}

TEST_P(HTNRuntimeListTest, FactsComparisonsAndSplitConsumeEvaluatedList)
{
    AddFact("list_fact", HTNAtomListOwner{HtnSymbol::sGetSymbol("key"), 7});
    ASSERT_EQ(Run("fact", 7), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "key");
    EXPECT_EQ(Text(Result(1)), "(7)");
    EXPECT_EQ(Run("fact", 8), HTN_DECOMPOSITION_NO_PLAN);
}

TEST_P(HTNRuntimeListTest, AxiomCompoundAndPlanMarkerArgumentsWork)
{
    ASSERT_EQ(Run("axiom_list", 7), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(wrapped (key 7))");
    for (const char* Entry : {"compound", "plan_markers"})
    {
        ASSERT_EQ(Run(Entry, 7), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(Result()), "(key 7)");
    }
}

TEST_P(HTNRuntimeListTest, DeferredCapturesArgumentsAndEvaluatesBodyLater)
{
    ASSERT_EQ(Run("deferred", 7), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Calls, 1);
    const auto& Step = HTNAtomGetListElement(*Plan.Get(), 0);
    HTNAtomOwner Call(HTNAtom::sCreateCall(HtnSymbol::sGetSymbol("later"), HTNAtomGetListElement(Step, 1)));
    Time = 20;
    Plan = HTNAtomOwner();
    ASSERT_EQ(Definition->decompose_call(&Context, Call.Get(), 0, Plan.Get()), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(captured (snapshot 7 10) 20)");
    EXPECT_EQ(Calls, 2);
}

TEST_P(HTNRuntimeListTest, BacktrackingRebuildsFromCurrentBindings)
{
    AddFact("entity", 1);
    AddFact("entity", 2);
    AddFact("selected",
        HTNAtomListOwner{HtnSymbol::sGetSymbol("target"), 2, HTNAtomListOwner{2, 0, 3}});
    ASSERT_EQ(Run("backtracking"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(target 2 (2 0 3))");
    EXPECT_EQ(Calls, 2);
    AddFact("list_fact", HTNAtomListOwner{HtnSymbol::sGetSymbol("key"), 2});
    ASSERT_EQ(Run("axiom_backtracking"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(key 2)");
}

TEST_P(HTNRuntimeListTest, UnboundAndFailedElementsStopWithoutPartialLists)
{
    for (const char* Entry : {"unbound", "failed", "failed_task", "failed_arithmetic"})
    {
        SCOPED_TRACE(Entry);
        EXPECT_EQ(Run(Entry), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(HTNAtomGetListSize(*Plan.Get()), 0);
        EXPECT_FALSE(Error().empty());
        EXPECT_NE(Error().find("Domains/Test/runtime_lists.domain:"), std::string::npos);
        EXPECT_NE(Error().find("domain 'RuntimeLists'"), std::string::npos);
        if (std::string(Entry) == "unbound") EXPECT_NE(Error().find("'?io_value' is unbound"), std::string::npos);
        EXPECT_EQ(AfterCalls, 0);
        EXPECT_EQ(OuterCalls, 0);
    }
    EXPECT_EQ(Reports, 0);
    ASSERT_EQ(Run("fallback"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_TRUE(Error().empty());
}

TEST_P(HTNRuntimeListTest, MissingCallUsesPolicyExactlyOnceWithOriginalSource)
{
    const std::string Text = Read(HTNFileHelpers::MakeAbsolutePath("Domains/Test/runtime_lists.domain").string());
    const auto Position = HTNSourceText(Text).GetPosition(Text.find("(call missing_runtime_list)"));
    for (const auto Policy : {HTNCallTermErrorPolicy::Report, HTNCallTermErrorPolicy::FailSilently})
    {
        Context.callterm_error_policy = Policy;
        Reports = 0;
        EXPECT_EQ(Run("missing"), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Reports, Policy == HTNCallTermErrorPolicy::Report ? 1 : 0);
        EXPECT_EQ(AfterCalls, 0);
        EXPECT_EQ(OuterCalls, 0);
        EXPECT_NE(Error().find("Callterm 'missing_runtime_list' failed while constructing a runtime list"), std::string::npos);
        if (Reports)
        {
            EXPECT_EQ(Reason, HTNCallTermErrorReason::NotRegistered);
            EXPECT_EQ(ReportName, "missing_runtime_list");
            EXPECT_EQ(ReportLine, static_cast<uint32_t>(Position.Line));
            EXPECT_EQ(ReportColumn, static_cast<uint32_t>(Position.Column));
        }
    }
    bool Found = false;
    for (uint32_t I = 0; I < Definition->callterm_requirement_count; ++I)
        if (std::string(Definition->callterm_requirements[I].name) == "missing_runtime_list") Found = true;
    EXPECT_TRUE(Found);
}

TEST_P(HTNRuntimeListTest, CallMutationsRefreshFactStorage)
{
    ASSERT_EQ(Run("mutation", 7), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(Result()), "(target 7)");
}

TEST_P(HTNRuntimeListTest, ResultsRemainOwnedAfterExecutionStorageIsDestroyed)
{
    ASSERT_EQ(Run("nested", 7), HTN_DECOMPOSITION_SUCCEEDED);
    const auto Before = Text(Result());
    Definition->destroy_execution_storage(Context.execution_storage);
    ASSERT_TRUE(Definition->initialize_execution_storage(Context.execution_storage));
    EXPECT_EQ(Text(Result()), Before);
#ifdef HTN_DEBUG_DECOMPOSITION
    for (const auto& Node : Debugger.GetNodes())
    {
        EXPECT_EQ(Node.DisplayName.find("__task_list_result_"), std::string::npos);
        EXPECT_EQ(Node.DisplayName.find("$assignment_call_"), std::string::npos);
    }
#endif
}

TEST_P(HTNRuntimeListTest, OwnedStringsAndListsAreReleasedOnSuccessFailureAndReuse)
{
#ifdef HTN_DEBUG_DECOMPOSITION
    Context.debugger = nullptr;
#endif
    Registry.Bind("get_entity_position", [](const HTNCallTermArguments&) {
        return HTNAtomListOwner{HTNAtomOwner("A heap-owned string returned inside a nested runtime list"), 1};
    });
    const auto Before = HTNAtomDebug_GetStats();
    for (int I = 0; I < 10; ++I)
    {
        ASSERT_EQ(Run("nested", I), HTN_DECOMPOSITION_SUCCEEDED);
        ASSERT_EQ(Run("failed_task"), HTN_DECOMPOSITION_NO_PLAN);
        ASSERT_EQ(Run("owned_failure", I), HTN_DECOMPOSITION_NO_PLAN);
    }
    Plan = HTNAtomOwner();
    Definition->destroy_execution_storage(Context.execution_storage);
    ASSERT_TRUE(Definition->initialize_execution_storage(Context.execution_storage));
    const auto After = HTNAtomDebug_GetStats();
    EXPECT_EQ(After.live_heap_strings, Before.live_heap_strings);
    EXPECT_EQ(After.live_list_nodes, Before.live_list_nodes);
#ifdef HTN_MEMORY_ATOM_DIAGNOSTICS
    EXPECT_GT(After.heap_string_allocations, Before.heap_string_allocations);
    EXPECT_GT(After.list_node_allocations, Before.list_node_allocations);
#endif
}

TEST_P(HTNRuntimeListTest, BoundIoAssignmentRejectsBeforeEvaluatingList)
{
    EXPECT_EQ(Run("bound_io"), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(AfterCalls, 0);
}

TEST_P(HTNRuntimeListTest, MissingBindingsAndInstancesInsideListsRetainTheirReasons)
{
    ASSERT_TRUE(Registry.BindMember("missing_runtime_list", "agent", {}, {}));
    EXPECT_EQ(Run("missing"), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Reports, 1);
    EXPECT_EQ(Reason, HTNCallTermErrorReason::MissingBinding);
}

TEST_P(HTNRuntimeListTest, MissingInstanceInsideListIsReportedOnce)
{
    ASSERT_TRUE(Registry.BindMember("missing_runtime_list", "agent",
        [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(1); }, {}));
    EXPECT_EQ(Run("missing"), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Reports, 1);
    EXPECT_EQ(Reason, HTNCallTermErrorReason::MissingInstance);
}

TEST_P(HTNRuntimeListTest, InstrumentationModePreservesAbiAndControlsDebugger)
{
    ASSERT_TRUE(HTNGeneratedPlanner_ValidateDefinition(Definition));
    ASSERT_EQ(Run("run", 7), HTN_DECOMPOSITION_SUCCEEDED);
#ifdef HTN_DEBUG_DECOMPOSITION
    EXPECT_EQ(Definition->debug_metadata != nullptr, GetParam());
    EXPECT_EQ(!Debugger.GetNodes().empty(), GetParam());
#endif
#ifdef HTN_GENERATED_EXECUTION_PROFILING
    ASSERT_NE(Definition->get_execution_profiling, nullptr);
    EXPECT_EQ(Definition->get_execution_profiling(Context.execution_storage) != nullptr, GetParam());
#endif
}

TEST_P(HTNRuntimeListTest, NonBooleanConditionStillReportsItsCallTermError)
{
    Registry.Bind("ppr_start", [](const HTNCallTermArguments&) { return HTNAtomOwner(7); });
    EXPECT_EQ(Run("run", 7), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Reports, 1);
    EXPECT_EQ(Reason, HTNCallTermErrorReason::NonBooleanConditionResult);
    EXPECT_EQ(ReportName, "ppr_start");
    EXPECT_FALSE(ReportFile.empty());
    EXPECT_GT(ReportLine, 0u);
    EXPECT_GT(ReportColumn, 0u);
}

INSTANTIATE_TEST_CASE_P(InstrumentationModes, HTNRuntimeListTest, testing::Values(true, false),
    [](const testing::TestParamInfo<bool>& Info) { return Info.param ? "Full" : "None"; });

TEST(HTNRuntimeListCompilerTest, StaticListsStayPreparedWhileRuntimeListsKeepEvaluableChildren)
{
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    const std::string Source = "(:domain Lists top_level_domain (:method (run ?inp_id) top_level_method "
        "(branch () ((!result (query_extent_y (1.0 1.0 1.0)) (target ?inp_id (call position ?inp_id)))))))";
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("runtime_list_ir.domain", Source, {}, Loaded, Diagnostics));
    HTNCompilerIR IR;
    std::string Error;
    ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
        HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error)) << Error;
    ASSERT_FALSE(IR.Tasks.empty());
    const auto& Literal = IR.Values[IR.Tasks[0].FirstArgument];
    EXPECT_EQ(Literal.Kind, HTNIRValueKind::Literal);
    EXPECT_NE(Literal.StaticValueIndex, HTN_IR_NO_INDEX);
    EXPECT_EQ(Literal.AtomType, HTN_ATOM_TYPE_LIST);
    ASSERT_FALSE(IR.RuntimeExpressions.empty());
    const auto& Elements = IR.RuntimeExpressions[0].Children;
    ASSERT_EQ(Elements.size(), 3u);
    EXPECT_EQ(Elements[1].Kind, HTNIRValueKind::Variable);
    EXPECT_EQ(Elements[2].Kind, HTNIRValueKind::Call);
    EXPECT_EQ(Elements[2].StaticValueIndex, HTN_IR_NO_INDEX);
    const auto Path = HTNFileHelpers::MakeAbsolutePath("build/logs/runtime-list-ir.generated.c");
    std::filesystem::create_directories(Path.parent_path());
    HTNCCodeGeneratorOptions Options;
    Options.OutputSourcePath = Path.string();
    Options.EntryPointName = "CreateRuntimeListIRHTN";
    Options.SourceFilePath = "runtime_list_ir.domain";
    Options.SourceText = Source;
    Options.LinkedSourceFiles = Loaded.SourceFiles;
    ASSERT_TRUE(HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error)) << Error;
    const auto Generated = Read(Path.string());
    EXPECT_NE(Generated.find("HTNAtom_PushBackListElementMove("), std::string::npos);
    EXPECT_NE(Generated.find("HTNCallTermRegistry_InvokeGeneratedCallTermWithSource("), std::string::npos);
}

TEST(HTNRuntimeListCompilerTest, RejectsUnknownNestedVariablesAndReassignmentWithLocatedDiagnostics)
{
    for (const auto* Body : {"(= ?list (target ?unknown))", "(and (= ?list (target ?inp_id)) (= ?inp_id 4))",
                            "(query (call identity (target ?unknown)))", "(#echo (call identity (target ?unknown)))"})
    {
        HTNCompilerDomainLoadResult Loaded;
        HTNDiagnosticSink Diagnostics;
        const std::string Source = "(:domain Invalid top_level_domain (:axiom (echo ?inp_value) (and (== ?inp_value 1))) "
            "(:method (run ?inp_id) top_level_method (branch (and " +
            std::string(Body) + ") ((!result)))))";
        EXPECT_FALSE(HTNCompilerDomainLoader().LoadFromSource("runtime_list_invalid.domain", Source, {}, Loaded, Diagnostics));
        ASSERT_TRUE(Diagnostics.HasErrors());
        EXPECT_GT(Diagnostics.GetDiagnostics()[0].Range.Begin.Line, 0);
        EXPECT_GT(Diagnostics.GetDiagnostics()[0].Range.Begin.Column, 0);
        EXPECT_EQ(Diagnostics.GetDiagnostics()[0].FilePath, "runtime_list_invalid.domain");
        if (std::string(Body).find("?unknown") != std::string::npos)
            EXPECT_NE(Diagnostics.GetDiagnostics()[0].Message.find("Unknown variable"), std::string::npos);
    }
}
}
