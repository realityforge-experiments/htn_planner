// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNAtomListOwner.h"
#include "Core/HTNFileHelpers.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"
#include "Translator/HTNGeneratedDebugger.h"
#include "Translator/HTNTranslation.h"
#include "gtest/gtest.h"
#include <fstream>
#include <set>

extern "C" const HTNGeneratedPlannerDefinition* CreateSharedImplementationsHTN_GetDefinition(void);
extern "C" const HTNGeneratedPlannerDefinition* CreateSharedImplementationsNoneHTN_GetDefinition(void);

namespace
{
TEST(HTNSharedImplementationCompilerTest, LinkedAliasesShareBodiesButOverridesAndOverloadsDoNot)
{
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    ASSERT_TRUE(HTNCompilerDomainLoader().Load(HTNFileHelpers::MakeAbsolutePath(
        "Domains/Test/shared_implementations.domain").string(), Loaded, Diagnostics));
    HTNCompilerIR IR;
    std::string Error;
    ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
        HTNGeneratedRuntimeBacktrackingSupport::Enabled, IR, Error)) << Error;
    const auto Find = [&](const char* Name, uint32 Arity) {
        return IR.FindMethodByStringId(IR.Strings.Find(Name), Arity);
    };
    for (const auto& Names : {std::pair{"shared", "SharedBase::shared"},
                              std::pair{"run", "SharedImplementations::run"},
                              std::pair{"choose", "SharedImplementations::choose"}})
    {
        const uint32 Arity = std::string(Names.first) == "run" ? 0u : 1u;
        const int A = Find(Names.first, Arity), B = Find(Names.second, Arity);
        ASSERT_GE(A, 0); ASSERT_GE(B, 0);
        const auto& Left = IR.Methods[A];
        const auto& Right = IR.Methods[B];
        EXPECT_NE(Left.Id, Right.Id);
        EXPECT_EQ(Left.ImplementationIndex, Right.ImplementationIndex);
        EXPECT_EQ(Left.FirstParameter, Right.FirstParameter);
        EXPECT_EQ(Left.FirstBranch, Right.FirstBranch);
        EXPECT_EQ(Left.VariableSlotMask, Right.VariableSlotMask);
        EXPECT_EQ(Left.Source.FileIndex, Right.Source.FileIndex);
    }
    ASSERT_GE(Find("SharedBase::choose", 1u), 0);
    ASSERT_GE(Find("choose", 2u), 0);
    EXPECT_NE(IR.Methods[Find("choose", 1u)].ImplementationIndex,
              IR.Methods[Find("SharedBase::choose", 1u)].ImplementationIndex);
    EXPECT_NE(IR.Methods[Find("choose", 1u)].ImplementationIndex,
              IR.Methods[Find("choose", 2u)].ImplementationIndex);
    // IR keeps unused declarations/indices; reachability only filters code emission.
    ASSERT_GE(Find("unused", 0u), 0);
    std::set<uint32> Bodies;
    for (const auto& Method : IR.Methods) Bodies.insert(Method.ImplementationIndex);
    EXPECT_LT(Bodies.size(), IR.Methods.size());
    std::unordered_map<std::string, uint32> AxiomBodies;
    for (const auto& Axiom : IR.Axioms) AxiomBodies[IR.Strings.Values[Axiom.Id]] = Axiom.Condition;
    EXPECT_EQ(AxiomBodies.at("choose_value"), AxiomBodies.at("SharedImplementations::choose_value"));
    EXPECT_NE(AxiomBodies.at("choose_value"), AxiomBodies.at("SharedBase::choose_value"));
}

TEST(HTNSharedImplementationCompilerTest, MinimalDomainHasOneBodyAndOneTaskInBothModes)
{
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("minimal_shared.domain",
        "(:domain Minimal top_level_domain (:method (run) top_level_method (one () ((!done)))))",
        {}, Loaded, Diagnostics));
    HTNCompilerIR IR;
    std::string Error;
    ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
        HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error));
    ASSERT_EQ(IR.Methods.size(), 2u);
    EXPECT_EQ(IR.Branches.size(), 1u);
    EXPECT_EQ(IR.Tasks.size(), 1u);
    for (const auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
    {
        HTNCCodeGeneratorOptions Options;
        Options.EntryPointName = "CreateMinimalShared";
        Options.Instrumentation = Mode;
        Options.OutputSourcePath = HTNFileHelpers::MakeAbsolutePath("build/logs/shared-minimal.generated.c").string();
        std::filesystem::create_directories(std::filesystem::path(Options.OutputSourcePath).parent_path());
        ASSERT_TRUE(HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error)) << Error;
        std::ifstream Input(Options.OutputSourcePath);
        const std::string Source{std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
        // The qualified alias is internal and unused; only the public run identity remains.
        EXPECT_NE(Source.find("_METHOD_0("), std::string::npos);
        EXPECT_EQ(Source.find("_METHOD_BODY_1("), std::string::npos);
        EXPECT_EQ(Source.find("_TASK_1("), std::string::npos);
        EXPECT_EQ(Source.find("uint32_t method_index"), std::string::npos);
    }
}

class HTNSharedImplementationTest : public testing::TestWithParam<bool>
{
protected:
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    HTNPlannerHook Hook{Database.GetWorldState(), Registry};
    const HTNGeneratedPlannerDefinition* Definition = nullptr;
    HTNGeneratedPlannerContext Context{};
    HTNAtomOwner Plan;
    int Calls = 0, Reports = 0;
    std::string ErrorFile, ErrorName;
    uint32_t ErrorLine = 0, ErrorColumn = 0;
    HTNCallTermErrorReason Reason{};
#ifdef HTN_DEBUG_DECOMPOSITION
    HTNGeneratedDebugger Debugger;
#endif
    static void Report(void* Client, const HTNCallTermErrorInfo* Info)
    {
        auto& Self = *static_cast<HTNSharedImplementationTest*>(Client);
        ++Self.Reports;
        Self.ErrorName = Info->Name;
        Self.ErrorFile = Info->Source.file;
        Self.ErrorLine = Info->Source.line;
        Self.ErrorColumn = Info->Source.column;
        Self.Reason = Info->Reason;
    }
    void SetUp() override
    {
        Definition = GetParam() ? CreateSharedImplementationsHTN_GetDefinition() : CreateSharedImplementationsNoneHTN_GetDefinition();
        Registry.Bind("echo", [this](const HTNCallTermArguments& Args) { ++Calls; return HTNAtomOwner(Args[0]); });
        Database.GetWorldState().AddFact("candidate_value", std::vector<HTNAtomOwner>{HTNAtomOwner(1)});
        Database.GetWorldState().AddFact("candidate_value", std::vector<HTNAtomOwner>{HTNAtomOwner(2)});
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
    template<typename... Args> HTNDecompositionStatus Run(const char* Name, Args&&... Arguments)
    {
        HTNAtomOwner Call(HTNAtom::sCreateCall(HtnSymbol::sGetSymbol(Name), std::forward<Args>(Arguments)...));
        Plan = HTNAtomOwner();
        return Definition->decompose_call(&Context, Call.Get(), 1, Plan.Get());
    }
    std::string Text() const { return HTNAtomToString(*Plan.Get(), true); }
};

TEST_P(HTNSharedImplementationTest, QualifiedCallsOverridesOverloadsAndAxiomOutputs)
{
    ASSERT_EQ(Run("run"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(), "((!shared (payload 1 1)) (!shared (payload 2 2)) (!base_zero) (!base_zero) (!derived 3) (!base_one 4) (!pair 5 6) (!outputs 105 5 106))");
    EXPECT_EQ(Calls, 2);
}

TEST_P(HTNSharedImplementationTest, DeferredNamesAndAritiesSurviveLaterExpansion)
{
    ASSERT_EQ(Run("deferred"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Calls, 0);
    EXPECT_EQ(Text(), "((&shared 7) (&SharedBase::shared 8) (&choose 9) (&SharedImplementations::choose 10) (&choose 11 12))");
    const char* Names[] = {"shared", "SharedBase::shared", "choose", "SharedImplementations::choose", "choose"};
    const char* Expected[] = {"((!shared (payload 7 7)))", "((!shared (payload 8 8)))", "((!derived 9))", "((!derived 10))", "((!pair 11 12))"};
    for (int I = 0; I < 5; ++I)
    {
        HTNAtomOwner Call(I < 4 ? HTNAtom::sCreateCall(HtnSymbol::sGetSymbol(Names[I]), 7 + I)
                              : HTNAtom::sCreateCall(HtnSymbol::sGetSymbol(Names[I]), 11, 12));
        Plan = HTNAtomOwner();
        EXPECT_EQ(Definition->decompose_call(&Context, Call.Get(), 1, Plan.Get()), HTN_DECOMPOSITION_INVALID_CALL);
        Plan = HTNAtomOwner();
        ASSERT_EQ(Definition->decompose_call(&Context, Call.Get(), 0, Plan.Get()), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(), Expected[I]);
    }
    EXPECT_EQ(Calls, 2);
}

TEST_P(HTNSharedImplementationTest, NonTailRecursionReturnsToBothCallableIdentities)
{
#ifdef HTN_DEBUG_DECOMPOSITION
    Context.debugger = nullptr;
#endif
    ASSERT_EQ(Run("recursive", 1000), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(HTNAtom_GetListSize(Plan.Get()), 4002);
    EXPECT_EQ(HTNAtomToString(*HTNAtom_GetListElement(Plan.Get(), 2000u), true), "(!return 0)");
    EXPECT_EQ(HTNAtomToString(*HTNAtom_GetListElement(Plan.Get(), 2001u), true), "(!visit 0)");
    EXPECT_EQ(HTNAtomToString(*HTNAtom_GetListElement(Plan.Get(), 4001u), true), "(!return 0)");
}

TEST_P(HTNSharedImplementationTest, ClientOnlyEntriesKeepEveryPublicNameAndArity)
{
    ASSERT_EQ(Run("client_entry"), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(), "((!client_zero))");
    ASSERT_EQ(Run("client_entry", 17), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(), "((!client_one 17))");
    EXPECT_EQ(Run("client_entry", 17, 18), HTN_DECOMPOSITION_INVALID_CALL);
    EXPECT_EQ(Run("client_empty"), HTN_DECOMPOSITION_NO_PLAN); // An entry with no branches still exists.
    // Qualified aliases are internal unless also explicitly used as deferred targets.
    EXPECT_EQ(Run("SharedImplementations::client_entry"), HTN_DECOMPOSITION_INVALID_CALL);
    EXPECT_EQ(Run("unused"), HTN_DECOMPOSITION_INVALID_CALL);
}

TEST_P(HTNSharedImplementationTest, MutualRecursionAndSingleAliasDebuggerIdentity)
{
    ASSERT_EQ(Run("client_cycle", 4), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(), "((!even_zero) (!odd 1) (!even 2) (!odd 3) (!even 4))");
#ifdef HTN_DEBUG_DECOMPOSITION
    if (!GetParam()) { EXPECT_TRUE(Debugger.GetNodes().empty()); return; }
    std::set<std::string> Names;
    const auto* Metadata = Definition->debug_metadata;
    for (const auto& Node : Debugger.GetNodes())
    {
        EXPECT_TRUE(Node.Completed);
        if (Node.Kind == HTNGeneratedDebugger::NodeKind::Method)
            Names.insert(Metadata->strings[Metadata->methods[Node.MetadataIndex].id]);
    }
    EXPECT_EQ(Names, (std::set<std::string>{"client_cycle", "even_step", "odd_step"}));
#endif
}

TEST_P(HTNSharedImplementationTest, DeferredTargetRemainsExternallyCallableWithoutReachableProducer)
{
    HTNAtomOwner Call(HTNAtom::sCreateCall(HtnSymbol::sGetSymbol("deferred_only"), 42));
    EXPECT_EQ(Definition->decompose_call(&Context, Call.Get(), 1, Plan.Get()), HTN_DECOMPOSITION_INVALID_CALL);
    ASSERT_EQ(Definition->decompose_call(&Context, Call.Get(), 0, Plan.Get()), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Text(), "((!deferred_only 42))");
    Call = HTNAtomOwner(HTNAtom::sCreateCall(HtnSymbol::sGetSymbol("dead_deferred_source")));
    Plan = HTNAtomOwner();
    EXPECT_EQ(Definition->decompose_call(&Context, Call.Get(), 0, Plan.Get()), HTN_DECOMPOSITION_INVALID_CALL);
}

TEST_P(HTNSharedImplementationTest, PruningPreservesValidationOfUnusedCallterms)
{
    int Executions = 0;
    Registry.Bind("missing_shared_callterm", [&Executions](const HTNCallTermArguments&) { ++Executions; return true; });
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(*Definition, Hook.GetCallTermBindingContext(), Report, this));
    EXPECT_EQ(Reports, 2);
    EXPECT_EQ(Reason, HTNCallTermErrorReason::NotRegistered);
    EXPECT_EQ(ErrorName, "unused_argument");
    EXPECT_NE(ErrorFile.find("shared_implementation_base.domain"), std::string::npos);
    EXPECT_GT(ErrorColumn, 0u);
    for (const char* Name : {"unused_condition", "unused_argument"})
        Registry.Bind(Name, [&Executions](const HTNCallTermArguments&) { ++Executions; return true; });
    EXPECT_TRUE(Registry.ValidateGeneratedCallTerms(*Definition, Hook.GetCallTermBindingContext(), Report, this));
    EXPECT_EQ(Executions, 0);
}

TEST_P(HTNSharedImplementationTest, AxiomAlternativesRollbackAndOwnedInputReuse)
{
#ifdef HTN_DEBUG_DECOMPOSITION
    Context.debugger = nullptr;
#endif
    for (int Attempt = 0; Attempt < 3; ++Attempt)
    {
        ASSERT_EQ(Run("retry"), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(), "((!shared (payload 2 2)))");
        HTNAtomOwner Owned(HTNAtomListOwner{HTNAtomOwner("A long owned string passed through both callable names"), 3});
        const auto Before = HTNAtomToString(*Owned.Get(), true);
        ASSERT_EQ(Run("rollback", Owned), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Text(), "((!fallback " + Before + "))");
        ASSERT_EQ(Run("shared_entry", Owned), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(HTNAtomToString(*Owned.Get(), true), Before);
        EXPECT_EQ(HTNAtom_GetListSize(Plan.Get()), 2);
    }
    EXPECT_EQ(Calls, 12);
}

TEST_P(HTNSharedImplementationTest, ErrorsPreserveIncludedCallsiteAndExactlyOneReport)
{
    for (const char* Entry : {"error_plain", "error_qualified"})
    {
        Reports = 0;
        EXPECT_EQ(Run(Entry), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Reports, 1);
        EXPECT_EQ(Reason, HTNCallTermErrorReason::NotRegistered);
        EXPECT_EQ(ErrorName, "missing_shared_callterm");
        EXPECT_NE(ErrorFile.find("shared_implementation_base.domain"), std::string::npos);
        EXPECT_EQ(ErrorLine, 11u);
        EXPECT_EQ(ErrorColumn, 19u);
    }
}

TEST_P(HTNSharedImplementationTest, DebuggerPreservesCallableNamesParametersAndSource)
{
#ifdef HTN_DEBUG_DECOMPOSITION
    ASSERT_EQ(Run("run"), HTN_DECOMPOSITION_SUCCEEDED);
    if (!GetParam()) { EXPECT_TRUE(Debugger.GetNodes().empty()); return; }
    const auto* Metadata = Definition->debug_metadata;
    ASSERT_NE(Metadata, nullptr);
    std::set<std::string> Seen;
    for (const auto& Node : Debugger.GetNodes())
    {
        EXPECT_TRUE(Node.Completed) << Node.DisplayName;
        if (Node.Kind != HTNGeneratedDebugger::NodeKind::Method) continue;
        const std::string Name = Metadata->strings[Metadata->methods[Node.MetadataIndex].id];
        Seen.insert(Name);
        if (Name != "shared" && Name != "SharedBase::shared") continue;
        EXPECT_NE(Node.Source.DomainPath.find("shared_implementation_base.domain"), std::string::npos);
        EXPECT_EQ(Node.Source.Line, 7u);
        bool Found = false;
        for (const auto& Variable : Node.VariablesBefore)
            if (Variable.Name == "inp_value" || Variable.Name == "?inp_value")
            {
                Found = true;
                EXPECT_EQ(HTNAtomGetValue<int32>(*Variable.Value.Get()), Name == "shared" ? 1 : 2);
            }
        EXPECT_TRUE(Found);
    }
    for (const char* Name : {"run", "shared", "SharedBase::shared", "choose", "SharedBase::choose"})
        EXPECT_EQ(Seen.count(Name), 1u) << Name;
#endif
}

INSTANTIATE_TEST_CASE_P(InstrumentationModes, HTNSharedImplementationTest, testing::Values(true, false),
    [](const testing::TestParamInfo<bool>& Info) { return Info.param ? "Full" : "None"; });
}
