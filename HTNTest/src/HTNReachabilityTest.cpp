// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNFileHelpers.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"
#include "Translator/HTNCompilerReachability.h"
#include "Translator/HTNTranslation.h"
#include "gtest/gtest.h"
#include <fstream>

namespace
{
class HTNReachabilityTest : public testing::Test
{
protected:
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    HTNCompilerIR IR;
    std::string Error;

    void LoadFixture()
    {
        ASSERT_TRUE(HTNCompilerDomainLoader().Load(HTNFileHelpers::MakeAbsolutePath(
            "Domains/Test/shared_implementations.domain").string(), Loaded, Diagnostics));
        ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
            HTNGeneratedRuntimeBacktrackingSupport::Enabled, IR, Error)) << Error;
    }
    std::string Generate(HTNGeneratedInstrumentation Mode)
    {
        HTNCCodeGeneratorOptions Options;
        Options.EntryPointName = "CreateReachability";
        Options.LinkedSourceFiles = Loaded.SourceFiles;
        Options.Instrumentation = Mode;
        Options.OutputSourcePath = HTNFileHelpers::MakeAbsolutePath(
            Mode == HTNGeneratedInstrumentation::Full ? "build/logs/reachability-full.generated.c"
                                                      : "build/logs/reachability-none.generated.c").string();
        std::filesystem::create_directories(std::filesystem::path(Options.OutputSourcePath).parent_path());
        if (!HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error))
        {
            ADD_FAILURE() << Error;
            return {};
        }
        std::ifstream Input(Options.OutputSourcePath);
        return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
    }
};

TEST_F(HTNReachabilityTest, RootsAndResolvedEdgesPreserveEntriesWithoutRetainingDeadCycles)
{
    ASSERT_NO_FATAL_FAILURE(LoadFixture());
    const auto Reachable = HTNAnalyzeCompilerReachability(IR);
    const auto Check = [&](const char* Name, uint32 Arity, bool Expected) {
        const int M = IR.FindMethodByStringId(IR.Strings.Find(Name), Arity);
        ASSERT_GE(M, 0) << Name;
        EXPECT_EQ(Reachable.Methods[M], Expected) << Name << "/" << Arity;
    };
    for (const auto& Method : IR.Methods)
        if (Method.IsTopLevel || Method.IsExternallyDecomposable)
            Check(IR.Strings.Values[Method.Id].c_str(), Method.ParameterCount, true);
    Check("SharedBase::choose", 1, true);
    Check("choose", 1, true);
    Check("shared", 1, true);
    Check("shared", 2, false);
    Check("even_step", 1, true);
    Check("odd_step", 1, true);
    Check("SharedImplementations::even_step", 1, false);
    Check("unused", 0, false);
    Check("SharedBase::unused_with_helpers", 0, false);
    Check("dead_cycle_a", 0, false);
    Check("dead_cycle_b", 0, false);
    Check("dead_deferred_source", 0, false);
    Check("deferred_only", 1, true);
    const int Even = IR.FindMethodByStringId(IR.Strings.Find("even_step"), 1);
    const uint32 Body = IR.Methods[Even].ImplementationIndex;
    // The canonical qualified identity is unused, but its shared body must remain.
    EXPECT_NE(static_cast<uint32>(Even), Body);
    EXPECT_FALSE(Reachable.Methods[Body]);
    EXPECT_TRUE(Reachable.Implementations[Body]);
    for (size_t A = 0; A < IR.Axioms.size(); ++A)
    {
        const auto& Axiom = IR.Axioms[A];
        const auto& Name = IR.Strings.Values[Axiom.Id];
        if (Name.find("unused_value") != std::string::npos)
        {
            EXPECT_FALSE(Reachable.Axioms[A]);
            EXPECT_FALSE(Reachable.Conditions[Axiom.Condition]);
        }
        if (Name == "SharedBase::choose_value" || Name == "choose_value" || Name == "candidate")
        {
            EXPECT_TRUE(Reachable.Axioms[A]) << Name;
        }
    }
}

TEST_F(HTNReachabilityTest, EmissionOmitsDeadBodiesTasksAndHelpersInBothModes)
{
    ASSERT_NO_FATAL_FAILURE(LoadFixture());
    const auto Reachable = HTNAnalyzeCompilerReachability(IR);
    for (const auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
    {
        const auto Source = Generate(Mode);
        ASSERT_FALSE(Source.empty());
        for (size_t M = 0; M < IR.Methods.size(); ++M)
        {
            const auto Suffix = std::to_string(M) + "(";
            if (!Reachable.Methods[M] && !Reachable.Implementations[M])
            {
                EXPECT_EQ(Source.find("static int HTN_SHAREDIMPLEMENTATIONS_METHOD_" + Suffix), std::string::npos);
            }
            if (!Reachable.Implementations[M])
            {
                EXPECT_EQ(Source.find("static int HTN_SHAREDIMPLEMENTATIONS_METHOD_BODY_" + Suffix), std::string::npos);
            }
        }
        for (size_t T = 0; T < IR.Tasks.size(); ++T)
            EXPECT_EQ(Source.find("static int HTN_SHAREDIMPLEMENTATIONS_TASK_" + std::to_string(T) + "(") != std::string::npos,
                      Reachable.Tasks[T]);
        for (size_t B = 0; B < IR.Branches.size(); ++B)
            EXPECT_EQ(Source.find("static int HTN_SHAREDIMPLEMENTATIONS_PUSH_BRANCH_CONTINUATIONS_" + std::to_string(B) + "(") != std::string::npos,
                      Reachable.Branches[B] && IR.Branches[B].TaskCount != 0);
        for (size_t C = 0; C < IR.Conditions.size(); ++C)
        {
            if (Reachable.Conditions[C]) continue;
            for (const char* Helper : {"FACT_CHOICE_", "BEGIN_AXIOM_", "END_AXIOM_"})
                EXPECT_EQ(Source.find("HTN_SHAREDIMPLEMENTATIONS_DOMAIN_" + std::string(Helper) + std::to_string(C) + "("), std::string::npos);
        }
        // Validation still sees callsites in the pruned include.
        EXPECT_NE(Source.find("{\"unused_condition\", {"), std::string::npos);
        EXPECT_NE(Source.find("{\"unused_argument\", {"), std::string::npos);
    }
}

TEST_F(HTNReachabilityTest, ArithmeticAndComparisonHelpersAreOmittedWhenOnlyDeadCodeNeedsThem)
{
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("dead_helpers.domain",
        "(:domain DeadHelpers top_level_domain "
        "(:method (run) top_level_method (one () ((!done)))) "
        "(:method (unused ?inp_x) (one (and (> ?inp_x 0)) ((!done (payload (+ ?inp_x 1)))))))",
        {}, Loaded, Diagnostics));
    for (const auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
    {
        const auto Source = Generate(Mode);
        ASSERT_FALSE(Source.empty());
        EXPECT_EQ(Source.find("_EVALUATE_ARITHMETIC("), std::string::npos);
        EXPECT_EQ(Source.find("_COMPARE_ATOMS("), std::string::npos);
    }
}

TEST_F(HTNReachabilityTest, NoPublicRootsOmitsEvenMutuallyReferencingMethods)
{
    HTNDomainLoadOptions Options;
    Options.RequireTopLevelRoot = false;
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("no_roots.domain",
        "(:domain NoRoots (:method (a) (one () ((b)))) (:method (b) (one () ((a)))))",
        {}, Loaded, Diagnostics, Options));
    for (const auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
    {
        const auto Source = Generate(Mode);
        ASSERT_FALSE(Source.empty());
        EXPECT_EQ(Source.find("static int HTN_NOROOTS_METHOD_"), std::string::npos);
        EXPECT_EQ(Source.find("static int HTN_NOROOTS_TASK_"), std::string::npos);
        EXPECT_NE(Source.find("CreateReachability_GetDefinition("), std::string::npos);
    }
}
}
