// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNAtomListOwner.h"
#include "Core/HTNAtomListAllocator.h"
#include "Core/HTNFileHelpers.h"
#include "Core/HTNTypeConversion.h"
#include "Domain/Source/HTNSourceText.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Hook/HTNPlanningUnit.h"
#include "Translator/HTNGeneratedDebugger.h"
#include "../../HTNDemo/src/HTNDemoCallTermReporting.h"
#include "HTNGTest.h"
#include <filesystem>
#include <fstream>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

extern "C" const HTNGeneratedPlannerDefinition* CreateBooleanCalltermsHTN_GetDefinition(void);

struct HTNConditionVector3 { float X, Y, Z; };
template<> struct HTNTypeTraits<HTNConditionVector3> : HTNTypeTraits<HTNAtomList> {};
template<> struct HTNTypeConverter<HTNConditionVector3>
{
    static bool ToAtom(void*, const HTNConditionVector3& inValue, HTNAtom& outAtom)
    {
        const HTNAtomListOwner List{inValue.X, inValue.Y, inValue.Z};
        return HTNAtom_SetListCopy(&outAtom, List.Get()) != 0;
    }
};

namespace
{
std::string Read(const std::filesystem::path& inPath)
{
    std::ifstream Input(inPath, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

HTNAtomOwner MakeValue(int inCase)
{
    switch (inCase)
    {
    case 0:
    {
        HTNAtomOwner Atom;
        EXPECT_TRUE(HTNTryToAtom(HTNConditionVector3{1.0f, 2.0f, 3.0f}, *Atom.Get()));
        return Atom;
    }
    case 1: return HTNAtomOwner(42);
    case 2: return HTNAtomOwner(0.25f);
    case 3: return HTNAtomOwner(HtnSymbol::sGetSymbol("position"));
    case 4: return HTNAtomOwner("A returned string long enough to require owned heap storage");
    default: return HTNAtomOwner(HTNAtomListOwner{HTNAtomOwner("An owned string inside a returned list"), 7});
    }
}

struct ReportData
{
    int Count = 0;
    HTNCallTermErrorInfo Info{};
    std::string Name, Domain, File, ExpectedType;
    static void Record(void* inClient, const HTNCallTermErrorInfo* inInfo)
    {
        auto& Report = *static_cast<ReportData*>(inClient);
        ++Report.Count;
        Report.Info = *inInfo;
        Report.Name = inInfo->Name ? inInfo->Name : "";
        Report.Domain = inInfo->Source.domain ? inInfo->Source.domain : "";
        Report.File = inInfo->Source.file ? inInfo->Source.file : "";
        Report.ExpectedType = inInfo->ExpectedTypeName ? inInfo->ExpectedTypeName : "";
    }
};

class HTNBooleanCallTermTest : public testing::Test
{
protected:
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    HTNPlannerHook Hook{Database.GetWorldState(), Registry};
    HTNPlanningUnit Unit{Database, Hook, "condition"};
    HTNAtomOwner Value{true};
    ReportData Report;
    int Calls = 0;
#ifdef HTN_DEBUG_DECOMPOSITION
    HTNGeneratedDebugger Debugger;
#endif

    void SetUp() override
    {
        Registry.Bind("ppr_finalize_best", [this](const HTNCallTermArguments&) { ++Calls; return Value; });
        Registry.Bind("expected_value", [this](const HTNCallTermArguments&) { return Value; });
        Registry.Bind("accept_value", [](const HTNCallTermArguments&) { return true; });
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateBooleanCalltermsHTN_GetDefinition()));
        auto& Context = Unit.GetExecutionContext();
        Context.ClientContext = &Report;
        Context.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
        Context.CallTermErrorCallback = ReportData::Record;
        Context.BacktrackingMode = HTN_BACKTRACKING_ALL;
#ifdef HTN_DEBUG_DECOMPOSITION
        Debugger.SetEnabled(true);
        Unit.SetGeneratedDebugger(&Debugger);
#endif
    }

    void CheckReport(const char* inMethod, const char* inExpression = "(call ppr_finalize_best)")
    {
        EXPECT_EQ(Report.Info.Reason, HTNCallTermErrorReason::NonBooleanConditionResult);
        EXPECT_EQ(Report.Name, "ppr_finalize_best");
        EXPECT_EQ(Report.Domain, "BooleanCallTerms");
        EXPECT_EQ(Report.Info.ActualAtomType, static_cast<uint32_t>(Value.Get()->type));
        EXPECT_EQ(Report.Info.ExpectedAtomType, static_cast<uint32_t>(HTN_ATOM_TYPE_BOOL));
        EXPECT_EQ(Report.Info.ArgumentIndex, UINT32_MAX);
        EXPECT_EQ(Report.ExpectedType, "bool");
        const auto Path = HTNFileHelpers::MakeAbsolutePath("Domains/Test/boolean_callterms.domain");
        EXPECT_EQ(std::filesystem::path(Report.File).generic_string(), "Domains/Test/boolean_callterms.domain");
        const auto Source = Read(Path);
        const auto Method = Source.find(std::string("(:method (") + inMethod + ")");
        ASSERT_NE(Method, std::string::npos);
        const auto Offset = Source.find(inExpression, Method);
        ASSERT_NE(Offset, std::string::npos);
        const auto Position = HTNSourceText(Source).GetPosition(Offset);
        EXPECT_EQ(Report.Info.Source.line, static_cast<uint32_t>(Position.Line));
        EXPECT_EQ(Report.Info.Source.column, static_cast<uint32_t>(Position.Column));
    }
};

class HTNNonBooleanCallTermTest : public HTNBooleanCallTermTest, public testing::WithParamInterface<int> {};

TEST_P(HTNNonBooleanCallTermTest, ReportsStandaloneNonBooleanResult)
{
    Value = MakeValue(GetParam());
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_TRUE(Unit.GetCurrentPlan().empty());
    EXPECT_EQ(Calls, 1);
    ASSERT_EQ(Report.Count, 1);
    CheckReport("condition");
}

TEST_P(HTNNonBooleanCallTermTest, ValuesRemainValidInAssignmentsComparisonsAndNestedArguments)
{
    Value = MakeValue(GetParam());
    for (const char* Entry : {"assignment", "axiom_assignment", "comparison", "nested_condition"})
    {
        SCOPED_TRACE(Entry);
        Calls = 0;
        ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Entry)), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Calls, 1);
        EXPECT_EQ(Report.Count, 0);
        ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
        if (std::string(Entry).find("assignment") != std::string::npos)
        {
            EXPECT_TRUE(HTNAtom_Equals(&HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 0), Value.Get()));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ReturnTypes, HTNNonBooleanCallTermTest, testing::Range(0, 6));

TEST_F(HTNBooleanCallTermTest, TrueAndFalseRemainNormalConditionResults)
{
    for (const bool Result : {true, false})
    {
        Value = HTNAtomOwner(Result);
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(), Result ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Report.Count, 0);
    }
    EXPECT_EQ(Calls, 2);
}

TEST_F(HTNBooleanCallTermTest, FailSilentlyDoesNotReportOrLog)
{
    Value = MakeValue(0);
    Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
    testing::internal::CaptureStderr();
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
    EXPECT_EQ(Calls, 1);
    EXPECT_EQ(Report.Count, 0);
}

TEST_F(HTNBooleanCallTermTest, BinaryIntegersAndBooleanSymbolsStillRequireExplicitComparison)
{
    for (const HTNAtomOwner& Result : {HTNAtomOwner(0), HTNAtomOwner(1),
             HTNAtomOwner(HtnSymbol::sGetSymbol("true")), HTNAtomOwner(HtnSymbol::sGetSymbol("false"))})
    {
        Value = Result;
        const int Before = Report.Count;
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
        ASSERT_EQ(Report.Count, Before + 1);
        CheckReport("condition");
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("comparison")), HTN_DECOMPOSITION_SUCCEEDED);
        EXPECT_EQ(Report.Count, Before + 1);
    }
}

TEST_F(HTNBooleanCallTermTest, DeferredCallsReportAtExecutionWithTheirOwnSource)
{
    Value = MakeValue(0);
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("deferred")), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Calls, 0);
    EXPECT_EQ(Report.Count, 0);
    EXPECT_EQ(Unit.ResolveCurrentPrimitiveTask(), HTNPrimitiveTaskResolution::Failed);
    EXPECT_TRUE(Unit.GetCurrentPlan().empty());
    EXPECT_EQ(Calls, 1);
    ASSERT_EQ(Report.Count, 1);
    CheckReport("later");
    Value = HTNAtomOwner(true);
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("deferred")), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Unit.ResolveCurrentPrimitiveTask(), HTNPrimitiveTaskResolution::TaskReady);
    EXPECT_EQ(Report.Count, 1);
}

TEST_F(HTNBooleanCallTermTest, BacktrackingReportsEachAttemptAndAllowsFallback)
{
    Value = MakeValue(5);
    Database.GetWorldState().AddFact("candidate", std::vector<HTNAtomOwner>{HTNAtomOwner(1)});
    Database.GetWorldState().AddFact("candidate", std::vector<HTNAtomOwner>{HTNAtomOwner(2)});
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("alternatives")), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(Calls, 2);
    ASSERT_EQ(Report.Count, 2);
    CheckReport("alternatives", "(call ppr_finalize_best ?value)");
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    EXPECT_EQ(HTNGetTaskHead(Unit.GetCurrentPlan().front())->GetString(), "!fallback");
}

TEST_F(HTNBooleanCallTermTest, UnboundResultsAndInvocationErrorsAreNotReportedTwice)
{
    Value = HTNAtomOwner();
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Calls, 1);
    EXPECT_EQ(Report.Count, 0);
    Registry.Bind("ppr_finalize_best", [](const HTNCallTermArguments&) { return true; }, {HTN_ATOM_TYPE_INT});
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Report.Count, 1);
    EXPECT_EQ(Report.Info.Reason, HTNCallTermErrorReason::ArgumentCountMismatch);
    Registry.Bind("ppr_finalize_best", [](const HTNCallTermArguments& Args) {
        Args.SetError(HTNCallTermErrorReason::ReturnConversionFailed);
        return MakeValue(4);
    });
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_EQ(Report.Count, 2);
    EXPECT_EQ(Report.Info.Reason, HTNCallTermErrorReason::ReturnConversionFailed);
}

TEST_F(HTNBooleanCallTermTest, DemoDiagnosticExplainsTheResultTypeAndCorrection)
{
    Value = MakeValue(0);
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
    ASSERT_EQ(Report.Count, 1);
    // The error info itself is borrowed; reconstruct its string pointers from
    // the copies before invoking the demo formatter after the report callback.
    auto Info = Report.Info;
    Info.Name = Report.Name.c_str();
    Info.Source.domain = Report.Domain.c_str();
    Info.Source.file = Report.File.c_str();
    Info.ExpectedTypeName = Report.ExpectedType.c_str();
    testing::internal::CaptureStderr();
    ReportDemoCallTermError("Generated", Info);
    const auto Message = testing::internal::GetCapturedStderr();
    EXPECT_NE(Message.find("ppr_finalize_best"), std::string::npos);
    EXPECT_NE(Message.find("returned list where bool was required"), std::string::npos);
    EXPECT_NE(Message.find("standalone callterm is a condition"), std::string::npos);
    EXPECT_NE(Message.find("Bind or compare"), std::string::npos);
    EXPECT_NE(Message.find("(= ?result (call ppr_finalize_best"), std::string::npos);
    EXPECT_NE(Message.find(Report.File + ":" + std::to_string(Info.Source.line) + ":" + std::to_string(Info.Source.column)), std::string::npos);
}

TEST_F(HTNBooleanCallTermTest, RejectedOwnedResultsReleaseTheirListNodes)
{
    HTNPooledAtomListAllocator Allocator(8);
    Registry.Bind("ppr_finalize_best", [&Allocator](const HTNCallTermArguments&) {
        HTNAtomListOwner List(Allocator, {HTNAtomOwner("Owned string released with the rejected result"), 7});
        return HTNAtomOwner(std::move(List));
    });
    for (int Attempt = 0; Attempt < 3; ++Attempt)
    {
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Allocator.GetAllocatedNodeCount(), 0u);
        EXPECT_EQ(Report.Count, Attempt + 1);
    }
}

TEST_F(HTNBooleanCallTermTest, InvalidPolicyConfigurationAssertsOrFailsSafely)
{
    Value = MakeValue(0);
    for (const auto Policy : {HTNCallTermErrorPolicy::Unset, HTNCallTermErrorPolicy::Report,
                             static_cast<HTNCallTermErrorPolicy>(99)})
    {
        auto& Context = Unit.GetExecutionContext();
        Context.CallTermErrorPolicy = Policy;
        Context.CallTermErrorCallback = nullptr;
#ifndef NDEBUG
        const auto Invoke = [&] {
#if defined(_WIN32) && defined(_DEBUG)
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
            (void)Unit.DecomposeTopLevelMethod();
        };
        const char* Message = Policy == HTNCallTermErrorPolicy::Unset ? "Configure the callterm error policy explicitly" :
            Policy == HTNCallTermErrorPolicy::Report ? "Report policy requires" : "Invalid callterm error policy";
        EXPECT_DEATH(Invoke(), Message);
#else
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_TRUE(Unit.GetCurrentPlan().empty());
#endif
        EXPECT_EQ(Report.Count, 0);
    }
}
}
