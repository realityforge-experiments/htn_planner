// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNFileHelpers.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Hook/HTNPlanningUnit.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNTranslation.h"
#include "Translator/HTNGeneratedPlanner.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <fstream>

extern "C" const HTNGeneratedPlannerDefinition* CreateRecursionDispatchHTN_GetDefinition(void);
extern "C" const HTNGeneratedPlannerDefinition* CreateRecursionDispatchNoneHTN_GetDefinition(void);

namespace
{
std::string ReadGeneratedSource(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

TEST(HTNGeneratedInstrumentationTest, SourceStatisticsAreExactAndNoneOmitsOptionalCode)
{
    for (const char* Domain : {"runtime_lists", "recursion_dispatch", "nested_axiom_choices"})
    for (const auto Policy : {HTNGeneratedBacktrackingPolicy::FixedWithOverflow, HTNGeneratedBacktrackingPolicy::FixedCapacity})
    for (const auto Backtracking : {HTNGeneratedRuntimeBacktrackingSupport::Disabled, HTNGeneratedRuntimeBacktrackingSupport::Enabled})
    {
        SCOPED_TRACE(Domain);
        HTNTranslationRequest Request;
        Request.DomainPath = HTNFileHelpers::MakeAbsolutePath(std::string("Domains/Test/") + Domain + ".domain");
        Request.OutputDirectory = HTNFileHelpers::MakeAbsolutePath("build/logs/instrumentation-tests");
        Request.EntryPointName = "CreateInstrumentationTest";
        Request.BacktrackingPolicy = Policy;
        Request.RuntimeBacktrackingSupport = Backtracking;
        uint64_t FullBytes = 0;
        for (const auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
        {
            Request.Instrumentation = Mode;
            HTNTranslationResult Result;
            ASSERT_TRUE(HTNTranslateDomain(Request, Result)) << Result.ErrorMessage;
            const auto Source = ReadGeneratedSource(Result.OutputSourcePath);
            const auto& Stats = Result.CodeStatistics;
            EXPECT_EQ(Source.size(), Stats.Logic.Bytes + Stats.Debugger.Bytes + Stats.Profiling.Bytes);
            EXPECT_EQ(static_cast<uint64_t>(std::count(Source.begin(), Source.end(), '\n')),
                Stats.Logic.LineBreaks + Stats.Debugger.LineBreaks + Stats.Profiling.LineBreaks);
            if (Mode == HTNGeneratedInstrumentation::Full)
            {
                FullBytes = Source.size();
                EXPECT_GT(Stats.Debugger.Bytes, 0u);
                EXPECT_GT(Stats.Profiling.Bytes, 0u);
                EXPECT_EQ(Stats.OmittedDebugger.Bytes + Stats.OmittedProfiling.Bytes, 0u);
            }
            else
            {
                EXPECT_LT(Source.size(), FullBytes);
                EXPECT_EQ(Stats.Debugger.Bytes + Stats.Profiling.Bytes, 0u);
                EXPECT_GT(Stats.OmittedDebugger.Bytes, 0u);
                EXPECT_GT(Stats.OmittedProfiling.Bytes, 0u);
                for (const char* Token : {"HTN_GENERATED_EVENT_DEBUG_", "HTN_GENERATED_PROFILE_BEGIN",
                    "HTN_GENERATED_PROFILE_END", "HTN_GENERATED_STRUCTURAL_EVENT", "HTN_GENERATED_PREPARATION_",
                    "_DEBUG_METADATA", "_DEBUG_STRINGS", "HTNGeneratedDebug.h", "HTNGeneratedProfiling.h",
                    "HTNGeneratedProfiling_Create", "HTNGeneratedProfiling_Destroy", "HTNGeneratedProfiling_ResetExecution"})
                    EXPECT_EQ(Source.find(Token), std::string::npos) << Token;
                EXPECT_NE(Source.find("HTNCallTermRegistry_InvokeGeneratedCallTermWithSource"), std::string::npos);
                EXPECT_NE(Source.find("_CALLTERM_REQUIREMENTS"), std::string::npos);
                EXPECT_NE(Source.find("--call-frame-capacity="), std::string::npos);
            }
        }
    }
}

TEST(HTNGeneratedInstrumentationTest, DefaultIsFullAndInvalidModeIsRejected)
{
    HTNTranslationRequest Request;
    Request.DomainPath = HTNFileHelpers::MakeAbsolutePath("Domains/Test/recursion_dispatch.domain");
    Request.OutputDirectory = HTNFileHelpers::MakeAbsolutePath("build/logs/instrumentation-default");
    Request.EntryPointName = "CreateInstrumentationDefault";
    HTNTranslationResult Result;
    ASSERT_TRUE(HTNTranslateDomain(Request, Result));
    const auto Default = ReadGeneratedSource(Result.OutputSourcePath);
    Request.Instrumentation = HTNGeneratedInstrumentation::Full;
    ASSERT_TRUE(HTNTranslateDomain(Request, Result));
    EXPECT_EQ(ReadGeneratedSource(Result.OutputSourcePath), Default);
    Request.Instrumentation = static_cast<HTNGeneratedInstrumentation>(255);
    EXPECT_FALSE(HTNTranslateDomain(Request, Result));
    EXPECT_EQ(Result.Failure, HTNTranslationFailure::InvalidOptions);
    EXPECT_EQ(Result.CodeStatistics.Logic.Bytes, 0u);
    EXPECT_EQ(ReadGeneratedSource(Request.OutputDirectory / "recursion_dispatch.generated.c"), Default);
}

TEST(HTNGeneratedInstrumentationTest, InstrumentationWordsInDomainLiteralsRemainUntouched)
{
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("instrumentation_literal.domain",
        "(:domain Text top_level_domain (:method (run) top_level_method (ok () "
        "((!result \"HTN_GENERATED_PROFILE_BEGIN(context, category);\")))))", {}, Loaded, Diagnostics));
    HTNCCodeGeneratorOptions Options;
    Options.EntryPointName = "CreateLiteral";
    Options.OutputSourcePath = HTNFileHelpers::MakeAbsolutePath("build/logs/instrumentation-literal.c").string();
    Options.Instrumentation = HTNGeneratedInstrumentation::None;
    std::string Error;
    ASSERT_TRUE(HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error)) << Error;
    EXPECT_NE(ReadGeneratedSource(Options.OutputSourcePath).find("HTN_GENERATED_PROFILE_BEGIN(context, category);"), std::string::npos);
}

TEST(HTNGeneratedInstrumentationTest, RecursionAndFailureRollbackMatchAtDepth1000)
{
    HTNDatabaseHook Database;
    Database.GetWorldState().AddFact("depth", std::vector<HTNAtomOwner>{HTNAtomOwner(int32{1000})});
    HTNCallTermRegistry Registry;
    int Visits = 0;
    Registry.Bind("visit", [&](const HTNCallTermArguments&) { ++Visits; return true; });
    HTNPlannerHook FullHook(Database.GetWorldState(), Registry), NoneHook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(FullHook.SetGeneratedPlannerDefinition(CreateRecursionDispatchHTN_GetDefinition()));
    ASSERT_TRUE(NoneHook.SetGeneratedPlannerDefinition(CreateRecursionDispatchNoneHTN_GetDefinition()));
    HTNPlanningUnit Full(Database, FullHook, "non_tail"), None(Database, NoneHook, "non_tail");
    Full.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
    None.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
    for (const char* Entry : {"non_tail", "mutual", "deep_failure"})
    for (int Attempt = 0; Attempt < 2; ++Attempt)
    {
        SCOPED_TRACE(Entry);
        Visits = 0;
        ASSERT_EQ(Full.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Entry)), HTN_DECOMPOSITION_SUCCEEDED);
        const int FullVisits = Visits;
        Visits = 0;
        ASSERT_EQ(None.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Entry)), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(None.GetCurrentPlan(), Full.GetCurrentPlan());
        EXPECT_EQ(Visits, FullVisits);
        if (std::string(Entry) == "deep_failure")
        {
            EXPECT_EQ(Visits, 1001);
            ASSERT_EQ(None.GetCurrentPlan().size(), 1u);
        }
    }
}

TEST(HTNGeneratedInstrumentationTest, NonePreservesCapacityFailureAndStorageReuse)
{
    HTNDatabaseHook Database;
    Database.GetWorldState().AddFact("depth", std::vector<HTNAtomOwner>{HTNAtomOwner(int32{5000})});
    HTNPlannerHook Hook(Database.GetWorldState());
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateRecursionDispatchNoneHTN_GetDefinition()));
    HTNPlanningUnit Unit(Database, Hook, "non_tail");
    Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_CALL_FRAME_CAPACITY_EXCEEDED);
    EXPECT_TRUE(Unit.GetCurrentPlan().empty());
    Database.GetWorldState().RemoveFact("depth", 1u, 0u);
    Database.GetWorldState().AddFact("depth", std::vector<HTNAtomOwner>{HTNAtomOwner(int32{2})});
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Unit.GetCurrentPlan().size(), 5u);
}
}
