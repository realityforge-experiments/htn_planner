// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNFileHelpers.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "Parser/HTNToken.h"
#include "Translator/HTNCCodeGenerator.h"
#include "Translator/HTNCompilerDomainLexer.h"
#include "Translator/HTNCompilerDomainLexerContext.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Translator/HTNCompilerIRBuilder.h"
#include "WorldState/Parser/HTNWorldStateLexer.h"
#include "WorldState/Parser/HTNWorldStateLexerContext.h"
#include "WorldState/Parser/HTNWorldStateParser.h"
#include "WorldState/Parser/HTNWorldStateParserContext.h"
#include "WorldState/HTNWorldState.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

TEST(HTNBooleanSymbolSyntaxTest, LexerProducesIdentifiersWithSourceLocations)
{
    const std::string Source = "\n  true false @true @false";
    std::vector<HTNToken> Tokens;
    HTNCompilerDomainLexerContext Context(Source, Tokens);
    ASSERT_TRUE(HTNCompilerDomainLexer().Lex(Context));
    ASSERT_EQ(Tokens.size(), 7u);
    for (size_t Index : {0u, 1u, 3u, 5u})
        EXPECT_EQ(Tokens[Index].GetType(), HTNTokenType::IDENTIFIER);
    EXPECT_EQ(Tokens[0].GetSourceRange().Begin.Line, 2);
    EXPECT_EQ(Tokens[0].GetSourceRange().Begin.Column, 3);
    EXPECT_EQ(Tokens[0].GetSourceRange().Begin.Offset, 3u);
    EXPECT_EQ(Tokens[1].GetSourceRange().Begin.Column, 8);
}

TEST(HTNBooleanSymbolSyntaxTest, AstIrAndGeneratedStorageKeepSymbolsDistinctFromStringsAndConstants)
{
    const std::string Source = R"((:domain Symbols top_level_domain
    (:constants (true 1) (false 0))
    (:method (run) top_level_method
        (branch () ((!values true false @true @false (true false) "true" "false"))))
))";
    HTNCompilerDomainLoadResult Loaded;
    HTNDiagnosticSink Diagnostics;
    ASSERT_TRUE(HTNCompilerDomainLoader().LoadFromSource("boolean_symbols.domain", Source, {}, Loaded, Diagnostics));
    const auto& Arguments = Loaded.Domain.Methods[0]->Branches[0]->Tasks[0]->Arguments;
    ASSERT_EQ(Arguments.size(), 7u);
    for (size_t Index : {0u, 1u})
    {
        EXPECT_EQ(Arguments[Index]->Kind, HTNCompilerAST::ValueKind::Literal);
        ASSERT_EQ(Arguments[Index]->GetValue().type, HTN_ATOM_TYPE_SYMBOL);
        EXPECT_EQ(HTNAtomGetValue<const HtnSymbol*>(Arguments[Index]->GetValue()), HtnSymbol::sGetSymbol(Index == 0 ? "true" : "false"));
    }
    EXPECT_EQ(Arguments[2]->Kind, HTNCompilerAST::ValueKind::Constant);
    EXPECT_EQ(Arguments[3]->Kind, HTNCompilerAST::ValueKind::Constant);
    ASSERT_EQ(Arguments[4]->GetValue().type, HTN_ATOM_TYPE_LIST);
    EXPECT_EQ(HTNAtomGetListElement(Arguments[4]->GetValue(), 0).type, HTN_ATOM_TYPE_SYMBOL);
    EXPECT_EQ(Arguments[5]->GetValue().type, HTN_ATOM_TYPE_STRING);
    EXPECT_EQ(Arguments[6]->GetValue().type, HTN_ATOM_TYPE_STRING);
    HTNCompilerIR IR;
    std::string Error;
    ASSERT_TRUE(HTNBuildCompilerIR(Loaded.Domain, Loaded.SourceFiles, HTNGeneratedRuntimeBacktrackingSupport::Disabled, IR, Error)) << Error;
    const auto& Symbol = IR.Values[IR.Tasks[0].FirstArgument];
    EXPECT_EQ(Symbol.AtomType, HTN_ATOM_TYPE_SYMBOL);
    EXPECT_EQ(IR.Strings.Values[Symbol.DebugText], "true");
    EXPECT_EQ(Symbol.Source.Range.Begin.Line, 4);
    EXPECT_EQ(Symbol.Source.Range.Begin.Offset, Source.find("!values true") + 8u);
    EXPECT_EQ(IR.StaticValues[Symbol.StaticValueIndex].AtomType, HTN_ATOM_TYPE_SYMBOL);
    for (auto Mode : {HTNGeneratedInstrumentation::Full, HTNGeneratedInstrumentation::None})
    {
        const auto Path = HTNFileHelpers::MakeAbsolutePath("build/logs/boolean-symbols/generated.c");
        std::filesystem::create_directories(Path.parent_path());
        HTNCCodeGeneratorOptions Options;
        Options.OutputSourcePath = Path.string();
        Options.EntryPointName = "CreateBooleanSymbols";
        Options.SourceFilePath = "boolean_symbols.domain";
        Options.SourceText = Source;
        Options.LinkedSourceFiles = Loaded.SourceFiles;
        Options.Instrumentation = Mode;
        ASSERT_TRUE(HTNCCodeGenerator().Generate(Loaded.Domain, Options, Error)) << Error;
        std::ifstream Input(Path);
        const std::string Generated{std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
        EXPECT_NE(Generated.find("HtnSymbol_InternGenerated(\"true\")"), std::string::npos);
        EXPECT_NE(Generated.find("HtnSymbol_InternGenerated(\"false\")"), std::string::npos);
        EXPECT_EQ(Generated.find("atom->type = HTN_ATOM_TYPE_BOOL"), std::string::npos);
    }
}

TEST(HTNBooleanSymbolSyntaxTest, WorldStateFilesUseTheSameSymbolSemantics)
{
    const std::string Source = "flags true false 1 0 \"true\"\n";
    std::vector<HTNToken> Tokens;
    HTNWorldStateLexerContext Lexer(Source, Tokens);
    ASSERT_TRUE(HTNWorldStateLexer().Lex(Lexer));
    HTNWorldState World;
    HTNWorldStateParserContext Parser(Tokens, World);
    ASSERT_TRUE(HTNWorldStateParser().Parse(Parser));
    const auto* Tables = World.FindFactArgumentsTables(HtnSymbol::sGetSymbol("flags"));
    ASSERT_NE(Tables, nullptr);
    ASSERT_EQ(Tables->at(5).GetFactArgumentsCollectionSize(), 1u);
    const auto& Row = Tables->at(5).GetFactArgumentsCollection().front();
    EXPECT_EQ(Row[0].GetType(), HTN_ATOM_TYPE_SYMBOL);
    EXPECT_EQ(Row[0].GetValue<const HtnSymbol*>(), HtnSymbol::sGetSymbol("true"));
    EXPECT_EQ(Row[1].GetValue<const HtnSymbol*>(), HtnSymbol::sGetSymbol("false"));
    EXPECT_EQ(Row[2].GetType(), HTN_ATOM_TYPE_INT);
    EXPECT_EQ(Row[3].GetType(), HTN_ATOM_TYPE_INT);
    EXPECT_EQ(Row[4].GetType(), HTN_ATOM_TYPE_STRING);
}
