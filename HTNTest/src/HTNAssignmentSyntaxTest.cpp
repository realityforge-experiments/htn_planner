// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"

#include "HTNGTest.h"

#include <string>

namespace
{
class HTNAssignmentSyntaxTest : public testing::Test
{
protected:
    bool Load(const std::string& inCondition, HTNDiagnosticSink& outDiagnostics)
    {
        const std::string Source =
            "(:domain AssignmentSyntax top_level_domain\n"
            "  (:method (run ?inp_entity_id) top_level_method\n"
            "    (ready (and " + inCondition + ") ((!result ?value)))\n"
            "  )\n"
            ")\n";
        HTNCompilerDomainLoadResult Loaded;
        if (!HTNCompilerDomainLoader().LoadFromSource("AssignmentSyntax.domain", Source, {}, Loaded, outDiagnostics))
            return false;

        HTNCompilerIR IR;
        std::string Error;
        const bool Built = HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
            HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error);
        EXPECT_TRUE(Built) << Error;
        return Built;
    }

    std::string FirstError(const HTNDiagnosticSink& inDiagnostics) const
    {
        const HTNDiagnostic* Error = inDiagnostics.GetFirstError();
        return Error ? Error->Message : "No diagnostic";
    }
};

TEST_F(HTNAssignmentSyntaxTest, ExplicitAssignmentBindsCallResult)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(Load("(= ?value (call get_entity_position ?inp_entity_id))", Diagnostics))
        << FirstError(Diagnostics);
}

TEST_F(HTNAssignmentSyntaxTest, ExplicitAssignmentBindsLiteral)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(Load("(= ?value main_threat)", Diagnostics)) << FirstError(Diagnostics);
}

TEST_F(HTNAssignmentSyntaxTest, ExplicitAssignmentBindsArithmetic)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(Load("(= ?value (+ 2 3))", Diagnostics)) << FirstError(Diagnostics);
}

TEST_F(HTNAssignmentSyntaxTest, RejectsImplicitCallResultBinding)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_FALSE(Load("(?value (call get_entity_position ?inp_entity_id))", Diagnostics));
    EXPECT_TRUE(Diagnostics.HasErrors());
}

TEST_F(HTNAssignmentSyntaxTest, EqualityComparisonRemainsSupported)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(Load("(value ?value) (== ?value 3)", Diagnostics)) << FirstError(Diagnostics);
}

TEST_F(HTNAssignmentSyntaxTest, RejectsDestinationUsedByEarlierFact)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_FALSE(Load("(value ?value) (= ?value 3)", Diagnostics));
    ASSERT_TRUE(Diagnostics.HasErrors());
    EXPECT_NE(FirstError(Diagnostics).find("already been declared or used"), std::string::npos);
    EXPECT_GT(Diagnostics.GetFirstError()->Range.Begin.Line, 0);
}

TEST_F(HTNAssignmentSyntaxTest, RejectsRepeatedDeclarationAndSelfReference)
{
    for (const auto* Source : {"(= ?value 1) (= ?value 2)", "(= ?value (+ ?value 1))",
                              "(not (value ?value)) (= ?value 2)",
                              "(= ?inp_entity_id 2) (value ?value)"})
    {
        HTNDiagnosticSink Diagnostics;
        EXPECT_FALSE(Load(Source, Diagnostics)) << Source;
        EXPECT_TRUE(Diagnostics.HasErrors()) << Source;
    }
}

TEST_F(HTNAssignmentSyntaxTest, IndependentAlternativesCanDeclareTheSameName)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(Load("(alt (and (= ?value 1)) (and (= ?value 2)))", Diagnostics)) << FirstError(Diagnostics);
}

TEST_F(HTNAssignmentSyntaxTest, RejectsInvalidDestinationAndArity)
{
    for (const auto* Source : {"(= 3 4)", "(= ?value)", "(= ?value 1 2)"})
    {
        HTNDiagnosticSink Diagnostics;
        EXPECT_FALSE(Load(Source, Diagnostics)) << Source;
        EXPECT_TRUE(Diagnostics.HasErrors()) << Source;
    }
}

}
