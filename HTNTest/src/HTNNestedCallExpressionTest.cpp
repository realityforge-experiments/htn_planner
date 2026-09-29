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
    HTNMissingCallTermReason Reason{};
    std::string Name, Domain, File;
    uint32_t Line = 0, Column = 0;
};

void Report(void* inClient, const HTNMissingCallTermInfo* inInfo)
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
        Unit.GetExecutionContext().MissingCallTermPolicy = HTNMissingCallTermPolicy::FailSilently;
        Calls = 0;
        ASSERT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Calls, Test.Calls);
        EXPECT_EQ(Unit.GetCurrentPlan().size(), 1u);
    }
}

TEST(HTNNestedCallExpressionTest, MissingPoliciesReasonsCountAndExpressionSource)
{
    const auto Path = HTNFileHelpers::MakeAbsolutePath("Domains/Test/nested_operator_calls.domain");
    const std::string Text = Read(Path);
    ASSERT_FALSE(Text.empty());
    const HTNSourceText Source(Text);
    for (const auto Reason : {HTNMissingCallTermReason::NotRegistered,
                             HTNMissingCallTermReason::MissingBinding, HTNMissingCallTermReason::MissingInstance})
    {
        HTNDatabaseHook Database;
        HTNCallTermRegistry Registry;
        int OuterCalls = 0;
        Registry.Bind("identity", [&OuterCalls](const HTNCallTermArguments& Args) { ++OuterCalls; return HTNAtomOwner(Args[0]); });
        if (Reason == HTNMissingCallTermReason::MissingBinding)
            ASSERT_TRUE(Registry.BindMember("missing_distance_callterm", "agent", {}, {}));
        if (Reason == HTNMissingCallTermReason::MissingInstance)
            ASSERT_TRUE(Registry.BindMember("missing_distance_callterm", "agent",
                [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(0.1f); }, {}));
        HTNPlannerHook Hook(Database.GetWorldState(), Registry);
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateNestedOperatorCallsHTN_GetDefinition()));
        for (const auto Policy : {HTNMissingCallTermPolicy::FailSilently, HTNMissingCallTermPolicy::Report})
            for (const std::string Entry : {"behave", "missing_right", "missing_arithmetic", "missing_deep", "bound_first", "two_attempts", "short_circuit", "missing_task_arithmetic"})
            {
                SCOPED_TRACE(Entry);
                HTNPlanningUnit Unit(Database, Hook, Entry);
                Reports Client;
                auto& Context = Unit.GetExecutionContext();
                Context.ClientContext = &Client;
                Context.MissingCallTermPolicy = Policy;
                Context.MissingCallTermCallback = Report;
                for (int Attempt = 0; Attempt < 2; ++Attempt)
                {
                    Client.Count = 0;
                    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), Entry == "short_circuit" ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
                    const int ExpectedReports = Policy == HTNMissingCallTermPolicy::FailSilently || Entry == "short_circuit" ? 0 : Entry == "two_attempts" ? 2 : 1;
                    EXPECT_EQ(Client.Count, ExpectedReports);
                    EXPECT_EQ(OuterCalls, 0);
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
    EXPECT_DEATH(Run(), "Configure the missing callterm policy explicitly");
#else
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
#endif
}
