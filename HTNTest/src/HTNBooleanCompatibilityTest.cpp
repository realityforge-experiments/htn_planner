// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNCallTermBinding.h"
#include "Core/HTNTask.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Hook/HTNPlanningUnit.h"
#include "Translator/HTNGeneratedDebugger.h"
#include "gtest/gtest.h"

extern "C" const HTNGeneratedPlannerDefinition* CreateBooleanCompatibilityHTN_GetDefinition(void);

namespace
{
struct Flags
{
    static bool Accept(bool inEnabled, bool inDisabled) { return inEnabled && !inDisabled; }
    bool Identity(bool inValue) { ++Calls; return inValue; }
    int Calls = 0;
};

TEST(HTNBooleanCompatibilityTest, EqualityIsSymmetricAndPreservesTypes)
{
    for (const bool Value : {false, true})
    {
        const HTNAtomOwner Boolean(Value);
        const HTNAtomOwner Integer(Value ? 1 : 0);
        EXPECT_EQ(Boolean, Integer);
        EXPECT_EQ(Integer, Boolean);
        EXPECT_TRUE(Boolean.IsType<bool>());
        EXPECT_FALSE(Integer.IsType<bool>());
        EXPECT_TRUE(Integer.IsType<int32>());
        EXPECT_EQ(HTNAtomToString(Boolean, false), Value ? "1" : "0");
        EXPECT_EQ(HTNAtomToString(Integer, false), Value ? "1" : "0");
        EXPECT_EQ(HTNAtomOwner(HTNAtomListOwner{Boolean, HTNAtomListOwner{Integer}}),
                  HTNAtomOwner(HTNAtomListOwner{Integer, HTNAtomListOwner{Boolean}}));
        for (const HTNAtomOwner& Other : {HTNAtomOwner(Value ? 0 : 1), HTNAtomOwner(2), HTNAtomOwner(-1),
                 HTNAtomOwner(Value ? 1.0f : 0.0f), HTNAtomOwner(Value ? "true" : "false"),
                 HTNAtomOwner(HtnSymbol::sGetSymbol(Value ? "true" : "false")), HTNAtomOwner()})
        {
            EXPECT_NE(Boolean, Other);
            EXPECT_NE(Other, Boolean);
        }
    }
    EXPECT_NE(HTNAtomOwner(1), HTNAtomOwner(1.0f));
}

TEST(HTNBooleanCompatibilityTest, DisplayUsesNumbersForBooleansAndPreservesSymbolsAndStrings)
{
    const HTNAtomOwner Values(HTNAtomListOwner{true, false, 1, 0,
        HtnSymbol::sGetSymbol("true"), HtnSymbol::sGetSymbol("false"), "true", "false",
        HTNAtomListOwner{false, true}});
    EXPECT_EQ(Values.ToString(true), "(1 0 1 0 true false \"true\" \"false\" (0 1))");
    EXPECT_EQ(Values.ToString(false), "(1 0 1 0 true false true false (0 1))");
    EXPECT_EQ(Values.GetListElement(0).type, HTN_ATOM_TYPE_BOOL);
    EXPECT_EQ(Values.GetListElement(1).type, HTN_ATOM_TYPE_BOOL);
    EXPECT_EQ(Values.GetListElement(4).type, HTN_ATOM_TYPE_SYMBOL);
    EXPECT_EQ(Values.GetListElement(6).type, HTN_ATOM_TYPE_STRING);
}

TEST(HTNBooleanCompatibilityTest, BoolConversionAcceptsOnlyBooleansAndBinaryIntegers)
{
    for (const bool Value : {false, true})
    {
        for (const HTNAtomOwner& Atom : {HTNAtomOwner(Value), HTNAtomOwner(Value ? 1 : 0)})
        {
            bool Parsed = !Value;
            ASSERT_TRUE(HTNTryParseType(Atom, Parsed));
            EXPECT_EQ(Parsed, Value);
            HTNAtomOwner Output;
            ASSERT_TRUE(HTNTryToAtom(Parsed, Output));
            EXPECT_TRUE(Output.IsType<bool>());
        }
    }
    for (const HTNAtomOwner& Atom : {HTNAtomOwner(-1), HTNAtomOwner(2), HTNAtomOwner(256),
             HTNAtomOwner(0.0f), HTNAtomOwner(1.0f), HTNAtomOwner("true"), HTNAtomOwner(),
             HTNAtomOwner(HtnSymbol::sGetSymbol("true")), HTNAtomOwner(HtnSymbol::sGetSymbol("false")),
             HTNAtomOwner(HTNAtomListOwner{1})})
    {
        bool Parsed = true;
        EXPECT_FALSE(HTNTryParseType(Atom, Parsed));
        EXPECT_TRUE(Parsed);
    }
    int32 Integer = 7;
    EXPECT_FALSE(HTNTryParseType(HTNAtomOwner(true), Integer));
    EXPECT_EQ(Integer, 7);
}

TEST(HTNBooleanCompatibilityTest, TypedMemberBindingsAcceptBinaryIntegersAndReportInvalidArguments)
{
    HTNCallTermRegistry Registry;
    ASSERT_TRUE(HTN_CALLTERM_BIND_MEMBER(Registry, "identity", Flags, Identity));
    HTNCallTermBindingContext Bindings(Registry);
    Flags Daemon;
    ASSERT_TRUE(HTN_CALLTERM_SET_DAEMON(Bindings, Flags, &Daemon));
    struct Report { int Count = 0; HTNCallTermErrorInfo Info{}; } Error;
    HTNPlannerExecutionContext Context{};
    Context.CallTermBindingContext = &Bindings;
    Context.ClientContext = &Error;
    Context.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
    Context.CallTermErrorCallback = [](void* inClient, const HTNCallTermErrorInfo* inInfo) {
        auto& State = *static_cast<Report*>(inClient);
        ++State.Count;
        State.Info = *inInfo;
    };
    for (const int Value : {0, 1})
    {
        const std::vector<HTNAtomOwner> Args{HTNAtomOwner(Value)};
        const auto Result = Registry.Execute("identity", Context, Args);
        ASSERT_TRUE(Result.IsType<bool>());
        EXPECT_EQ(Result.GetValue<bool>(), Value != 0);
    }
    EXPECT_EQ(Daemon.Calls, 2);
    EXPECT_EQ(Error.Count, 0);
    for (const HTNAtomOwner& Value : {HTNAtomOwner(2), HTNAtomOwner(-1), HTNAtomOwner(1.0f), HTNAtomOwner("true"),
             HTNAtomOwner(HtnSymbol::sGetSymbol("true")), HTNAtomOwner(HtnSymbol::sGetSymbol("false"))})
    {
        const std::vector<HTNAtomOwner> Args{Value};
        const int Before = Error.Count;
        EXPECT_FALSE(Registry.Execute("identity", Context, Args).IsBound());
        EXPECT_EQ(Error.Count, Before + 1);
        EXPECT_EQ(Error.Info.Reason, HTNCallTermErrorReason::ArgumentTypeMismatch);
        EXPECT_EQ(Error.Info.ArgumentIndex, 0u);
        EXPECT_EQ(Error.Info.ActualAtomType, static_cast<uint32_t>(Value.Get()->type));
        EXPECT_EQ(Error.Info.ExpectedAtomType, static_cast<uint32_t>(HTN_ATOM_TYPE_BOOL));
    }
    EXPECT_EQ(Daemon.Calls, 2);
}

class HTNBooleanCompatibilityGeneratedTest : public testing::Test
{
protected:
    HTNDatabaseHook Database;
    HTNCallTermRegistry Registry;
    HTNPlannerHook Hook{Database.GetWorldState(), Registry};
    HTNPlanningUnit Unit{Database, Hook, "run"};
    HTNAtomOwner Alive{false};
    int Calls = 0;
#ifdef HTN_DEBUG_DECOMPOSITION
    HTNGeneratedDebugger Debugger;
#endif

    void SetUp() override
    {
        HTN_CALLTERM_BIND(Registry, "accept_flags", Flags, Accept);
        Registry.Bind("is_entity_alive", [this](const HTNCallTermArguments&) { ++Calls; return Alive; });
        Registry.Bind("alive_for_candidate", [this](const HTNCallTermArguments& inArguments) {
            ++Calls;
            return HTNAtomOwner(HTNAtomGetValue<int32>(inArguments[0]) == 1);
        });
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateBooleanCompatibilityHTN_GetDefinition()));
        Unit.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
        Unit.GetExecutionContext().BacktrackingMode = HTN_BACKTRACKING_ALL;
#ifdef HTN_DEBUG_DECOMPOSITION
        Debugger.SetEnabled(true);
        Unit.SetGeneratedDebugger(&Debugger);
#endif
        auto& World = Database.GetWorldState();
        World.SetFactRegistry(&Hook.GetFactRegistry());
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("flags"), true, false));
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("numeric_flags"), 1, 0));
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("mixed_flags"),
            HTNAtomListOwner{HtnSymbol::sGetSymbol("true"), true, HtnSymbol::sGetSymbol("false"), false}));
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("candidate"), 1, true));
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("candidate"), 2, true));
        ASSERT_TRUE(World.WriteFact(HtnSymbol::sGetSymbol("selected"), 2));
    }
};

TEST_F(HTNBooleanCompatibilityGeneratedTest, FactsComparisonsBindingsAndPlanKeepOriginalTypes)
{
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    const auto& Task = Unit.GetCurrentPlan().front();
    ASSERT_EQ(HTNGetTaskArgumentCount(Task), 9u);
    const HTNAtomType Types[] = {HTN_ATOM_TYPE_BOOL, HTN_ATOM_TYPE_BOOL, HTN_ATOM_TYPE_INT,
        HTN_ATOM_TYPE_INT, HTN_ATOM_TYPE_SYMBOL, HTN_ATOM_TYPE_SYMBOL, HTN_ATOM_TYPE_INT,
        HTN_ATOM_TYPE_INT, HTN_ATOM_TYPE_INT};
    const int Values[] = {1, 0, 1, 0, 1, 0, 1, 0, 2};
    for (uint32_t I = 0; I < 9u; ++I)
    {
        const auto& Value = HTNGetTaskArgument(Task, I);
        EXPECT_EQ(Value.type, Types[I]);
        if (I == 4u || I == 5u)
            EXPECT_EQ(HTNAtomGetValue<const HtnSymbol*>(Value), HtnSymbol::sGetSymbol(I == 4u ? "true" : "false"));
        else
            EXPECT_EQ(HTNAtomOwner(Value), HTNAtomOwner(Values[I]));
    }
    EXPECT_TRUE(Database.GetWorldState().ContainsFactArguments("flags", std::vector<HTNAtomOwner>{1, 0}));
    EXPECT_TRUE(Database.GetWorldState().ContainsFactArguments("numeric_flags", std::vector<HTNAtomOwner>{true, false}));
    const auto& Stored = Database.GetWorldState().FindFactArgumentsTables(HtnSymbol::sGetSymbol("flags"))->at(2).GetFactArgumentsCollection();
    EXPECT_TRUE(Stored.front()[0].IsType<bool>());
#ifdef HTN_DEBUG_DECOMPOSITION
    bool FoundEnabled = false;
    bool FoundDisabled = false;
    for (const auto& Node : Debugger.GetNodes())
        for (const auto& Variable : Node.VariablesAfter)
        {
            if (Variable.Name == "enabled")
            {
                EXPECT_TRUE(Variable.Value.IsType<bool>());
                EXPECT_EQ(Variable.Value.ToString(true), "1");
                FoundEnabled = true;
            }
            else if (Variable.Name == "disabled")
            {
                EXPECT_TRUE(Variable.Value.IsType<bool>());
                EXPECT_EQ(Variable.Value.ToString(true), "0");
                FoundDisabled = true;
            }
        }
    EXPECT_TRUE(FoundEnabled);
    EXPECT_TRUE(FoundDisabled);
#endif
}

TEST_F(HTNBooleanCompatibilityGeneratedTest, BacktrackingFindsSecondCompatibleRow)
{
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("backtrack")), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    const auto& Task = Unit.GetCurrentPlan().front();
    EXPECT_EQ(HTNAtomGetValue<int32>(HTNGetTaskArgument(Task, 0)), 2);
    EXPECT_EQ(HTNGetTaskArgument(Task, 1).type, HTN_ATOM_TYPE_BOOL);
    EXPECT_EQ(HTNGetTaskArgument(Task, 2).type, HTN_ATOM_TYPE_BOOL);
    EXPECT_FALSE(HTNAtomGetValue<bool>(HTNGetTaskArgument(Task, 2)));
    EXPECT_EQ(Calls, 2);
}

TEST_F(HTNBooleanCompatibilityGeneratedTest, DeferredArgumentsUseBooleanCompatibility)
{
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("deferred")), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Unit.ResolveCurrentPrimitiveTask(), HTNPrimitiveTaskResolution::TaskReady);
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    EXPECT_EQ(HTNGetTaskHead(Unit.GetCurrentPlan().front())->GetString(), "!later");
    EXPECT_EQ(HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 0).type, HTN_ATOM_TYPE_INT);
    EXPECT_EQ(HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 1).type, HTN_ATOM_TYPE_BOOL);
    EXPECT_FALSE(HTNAtomGetValue<bool>(HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 1)));
    EXPECT_EQ(Calls, 1);
}

TEST_F(HTNBooleanCompatibilityGeneratedTest, FalseCallResultFailsConditionButBindsAndContinuesAssignment)
{
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("condition")), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_TRUE(Unit.GetCurrentPlan().empty());
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("assignment")), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    const auto& Value = HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 0);
    EXPECT_EQ(Value.type, HTN_ATOM_TYPE_BOOL);
    EXPECT_FALSE(HTNAtomGetValue<bool>(Value));
    EXPECT_EQ(Calls, 2);
    Alive = HTNAtomOwner(true);
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("condition")), HTN_DECOMPOSITION_SUCCEEDED);
    Alive = HTNAtomOwner();
    EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("assignment")), HTN_DECOMPOSITION_NO_PLAN);
    EXPECT_TRUE(Unit.GetCurrentPlan().empty());
}

TEST_F(HTNBooleanCompatibilityGeneratedTest, BareBooleanNamesAreSymbolsNotNumericAliases)
{
    for (const char* Entry : {"symbol_equals_zero", "symbol_equals_one", "invalid_bool_argument"})
    {
        SCOPED_TRACE(Entry);
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Entry)), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_TRUE(Unit.GetCurrentPlan().empty());
    }
    ASSERT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol("symbol_assignment")), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
    const auto& Value = HTNGetTaskArgument(Unit.GetCurrentPlan().front(), 0);
    ASSERT_EQ(Value.type, HTN_ATOM_TYPE_SYMBOL);
    EXPECT_EQ(HTNAtomGetValue<const HtnSymbol*>(Value), HtnSymbol::sGetSymbol("false"));
}
}
