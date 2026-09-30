// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"
#include "gtest/gtest.h"

namespace
{
bool LoadAxiom(const std::string& Parameter, const std::string& Body, HTNDiagnosticSink& Diagnostics)
{
    const std::string Source = "(:domain AxiomAssignment top_level_domain\n"
        "(:axiom (select_position ?inp_entity ?" + Parameter + ")\n    " + Body + ")\n"
        "(:method (run) top_level_method (ready () ((!ready))))\n)";
    HTNCompilerDomainLoadResult Loaded;
    if (!HTNCompilerDomainLoader().LoadFromSource("axiom_assignment.domain", Source, {}, Loaded, Diagnostics)) return false;
    HTNCompilerIR IR;
    std::string Error;
    const bool Built = HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles,
        HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error);
    EXPECT_TRUE(Built) << Error;
    return Built;
}
}

TEST(HTNAxiomAssignmentTest, OutputAndIoCanBeInitialized)
{
    for (const std::string Parameter : {"out_position", "io_position"})
        for (const std::string Expression : {"7", "(+ 2 5)", "(call get_entity_position ?inp_entity)"})
        {
            SCOPED_TRACE(Parameter + Expression);
            HTNDiagnosticSink Diagnostics;
            EXPECT_TRUE(LoadAxiom(Parameter, "(and (= ?" + Parameter + " " + Expression + "))", Diagnostics))
                << (Diagnostics.GetFirstError() ? Diagnostics.GetFirstError()->Message : "");
        }
}

TEST(HTNAxiomAssignmentTest, RejectsInputsRepeatedOutputsAndPriorUsesWithLocation)
{
    for (const auto* Body : {"(and (= ?inp_entity 7))", "(and (= ?out_position 1) (= ?out_position 2))",
         "(and (position ?out_position) (= ?out_position 2))", "(and (= ?out_position (+ ?out_position 1)))",
         "(and (= ?local 1) (= ?local 2))", "(and (not (position ?out_position)) (= ?out_position 2))"})
    {
        SCOPED_TRACE(Body);
        HTNDiagnosticSink Diagnostics;
        EXPECT_FALSE(LoadAxiom("out_position", Body, Diagnostics));
        ASSERT_NE(Diagnostics.GetFirstError(), nullptr);
        EXPECT_EQ(Diagnostics.GetFirstError()->FilePath, "axiom_assignment.domain");
        EXPECT_EQ(Diagnostics.GetFirstError()->Range.Begin.Line, 3);
        EXPECT_GT(Diagnostics.GetFirstError()->Range.Begin.Column, 0);
    }
}

TEST(HTNAxiomAssignmentTest, IndependentAlternativesCanInitializeOutput)
{
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(LoadAxiom("out_position", "(alt (= ?out_position 1) (= ?out_position 2))", Diagnostics));
}

TEST(HTNAxiomAssignmentTest, RejectsIoRedeclarationPriorUsesAndSelfReference)
{
    for (const auto* Body : {
        "(and (= ?io_position 1) (= ?io_position 2))",
        "(and (position ?io_position) (= ?io_position 2))",
        "(and (= ?io_position (+ ?io_position 1)))",
        "(and (not (position ?io_position)) (= ?io_position 2))",
        "(and (alt (= ?io_position 1) (= ?io_position 2)) (= ?io_position 3))"})
    {
        SCOPED_TRACE(Body);
        HTNDiagnosticSink Diagnostics;
        EXPECT_FALSE(LoadAxiom("io_position", Body, Diagnostics));
        ASSERT_NE(Diagnostics.GetFirstError(), nullptr);
        EXPECT_NE(Diagnostics.GetFirstError()->Message.find("?io_position"), std::string::npos);
        EXPECT_EQ(Diagnostics.GetFirstError()->FilePath, "axiom_assignment.domain");
        EXPECT_EQ(Diagnostics.GetFirstError()->Range.Begin.Line, 3);
        EXPECT_GT(Diagnostics.GetFirstError()->Range.Begin.Column, 0);
    }
}

TEST(HTNAxiomAssignmentTest, IndependentAlternativesCanInitializeIo)
{
    for (const auto* Body : {"(alt (= ?io_position 1) (= ?io_position 2))",
                             "(or (= ?io_position 1) (= ?io_position 2))"})
    {
        HTNDiagnosticSink Diagnostics;
        EXPECT_TRUE(LoadAxiom("io_position", Body, Diagnostics));
    }
}

TEST(HTNAxiomAssignmentTest, RejectsUnknownCallInputsWithSourceLocation)
{
    for (const auto* Body : {
        "(and (= ?out_position (call get_entity_position ?inp_entity_id)))",
        "(and (call consume ?inp_entity_id))",
        "(and (= ?out_position (+ 1 (call get_entity_position ?inp_entity_id))))",
        "(and (< (call get_entity_position ?inp_entity_id) 2))",
        "(or (call consume ?inp_entity_id) (call consume ?inp_entity))",
        "(and (not (call consume ?inp_entity_id)))"})
    {
        SCOPED_TRACE(Body);
        HTNDiagnosticSink Diagnostics;
        EXPECT_FALSE(LoadAxiom("out_position", Body, Diagnostics));
        ASSERT_NE(Diagnostics.GetFirstError(), nullptr);
        EXPECT_NE(Diagnostics.GetFirstError()->Message.find("inp_entity_id"), std::string::npos);
        EXPECT_EQ(Diagnostics.GetFirstError()->FilePath, "axiom_assignment.domain");
        EXPECT_EQ(Diagnostics.GetFirstError()->Range.Begin.Line, 3);
        EXPECT_GT(Diagnostics.GetFirstError()->Range.Begin.Column, 0);
    }
}

TEST(HTNAxiomAssignmentTest, AcceptsDeclaredInputsAndLocalsInCallExpressions)
{
    for (const auto* Body : {
        "(and (= ?out_position (call get_entity_position ?inp_entity)))",
        "(and (entity ?local) (= ?out_position (call get_entity_position ?local)))",
        "(and (= ?local 1) (= ?out_position (call get_entity_position ?local)))",
        "(and (or (entity ?local) (other ?local)) (= ?out_position (call get_entity_position ?local)))"})
    {
        SCOPED_TRACE(Body);
        HTNDiagnosticSink Diagnostics;
        EXPECT_TRUE(LoadAxiom("out_position", Body, Diagnostics));
    }
    HTNDiagnosticSink Diagnostics;
    EXPECT_TRUE(LoadAxiom("io_position", "(and (call consume ?io_position))", Diagnostics));
}
