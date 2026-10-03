// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNCallTermBindingContext.h"
#include "Core/HTNCallTermBinding.h"
#include "Core/HTNCallTermRegistry.h"
#include "Translator/HTNGeneratedPlanner.h"
#include "Core/HTNFileHelpers.h"
#include "Domain/Source/HTNSourceText.h"
#include <fstream>
#include "Core/HTNTask.h"
#include "Hook/HTNDatabaseHook.h"
#include "Hook/HTNPlannerHook.h"
#include "Hook/HTNPlanningUnit.h"
#include "gtest/gtest.h"

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

extern "C" const HTNGeneratedPlannerDefinition* CreateMissingCalltermsHTN_GetDefinition(void);

namespace
{
struct ClientContext
{
    int Reports = 0;
    HTNCallTermErrorReason Reason{};
    std::string Name;
    std::string Daemon;
    std::string Domain;
    std::string File;
    uint32_t Line = 0u;
    uint32_t Column = 0u;
};

void Report(void* inContext, const HTNCallTermErrorInfo* inInfo)
{
    auto& Client = *static_cast<ClientContext*>(inContext);
    ++Client.Reports;
    Client.Reason = inInfo->Reason;
    Client.Name = inInfo->Name ? inInfo->Name : "";
    Client.Daemon = inInfo->DaemonID ? inInfo->DaemonID : "";
    Client.Domain = inInfo->Source.domain ? inInfo->Source.domain : "";
    Client.File = inInfo->Source.file ? inInfo->Source.file : "";
    Client.Line = inInfo->Source.line;
    Client.Column = inInfo->Source.column;
}
}

namespace
{
int InvokeGenerated(const HTNPlannerExecutionContext& inContext, const HTNGeneratedCallTerm& inCall, HTNAtom* outResult)
{
    HTNGeneratedPlannerContext Context{};
    Context.callterm_binding_context = inContext.CallTermBindingContext;
    Context.client_context = inContext.ClientContext;
    Context.callterm_error_policy = inContext.CallTermErrorPolicy;
    Context.callterm_error_callback = inContext.CallTermErrorCallback;
    return HTNCallTermRegistry_InvokeGeneratedCallTermWithSource(&Context, &inCall, nullptr, 0u, outResult, nullptr);
}
}

TEST(HTNCallTermErrorTest, ExecutionOptionsDefaultToUnset)
{
    HTNPlannerExecutionContext Context{};
    HTNGeneratedPlannerContext Generated{};
    EXPECT_EQ(Context.CallTermErrorPolicy, HTNCallTermErrorPolicy::Unset);
    EXPECT_EQ(Generated.callterm_error_policy, HTNCallTermErrorPolicy::Unset);
    EXPECT_EQ(Context.CallTermErrorCallback, nullptr);
    EXPECT_EQ(Generated.callterm_error_callback, nullptr);
}

TEST(HTNCallTermErrorTest, BothInvocationApisUseTheSamePolicyAndReasons)
{
    HTNCallTermRegistry Registry;
    ASSERT_TRUE(Registry.BindMember("empty", "agent", {}, {}));
    ASSERT_TRUE(Registry.BindMember("member", "agent", [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(true); }, {}));
    Registry.Bind("ordinary_failure", [](const HTNCallTermArguments&) { return HTNAtomOwner(); });
    HTNCallTermBindingContext Bindings(Registry);
    HTNPlannerExecutionContext Context{};
    Context.CallTermBindingContext = &Bindings;
    ClientContext Client;
    Context.ClientContext = &Client;
    Context.CallTermErrorCallback = Report;
    const char* Names[] = {"absent", "empty", "member"};
    const HTNCallTermErrorReason Reasons[] = {HTNCallTermErrorReason::NotRegistered,
        HTNCallTermErrorReason::MissingBinding, HTNCallTermErrorReason::MissingInstance};
    const std::vector<HTNAtomOwner> Arguments;
    for (const auto Policy : {HTNCallTermErrorPolicy::FailSilently, HTNCallTermErrorPolicy::Report})
    {
        Context.CallTermErrorPolicy = Policy;
        for (size_t I = 0; I < 3u; ++I)
        {
            const int Before = Client.Reports;
            const auto Call = HTNCallTermRegistry_ResolveGeneratedCallTerm(&Bindings, Names[I]);
            HTNAtomOwner Result;
            testing::internal::CaptureStdout();
            testing::internal::CaptureStderr();
            const bool Bound = Registry.Execute(Names[I], Context, Arguments).IsBound();
            const int Invoked = InvokeGenerated(Context, Call, Result.Get());
            const auto Stderr = testing::internal::GetCapturedStderr();
            const auto Stdout = testing::internal::GetCapturedStdout();
            EXPECT_FALSE(Bound);
            EXPECT_EQ(Invoked, 0);
            EXPECT_TRUE(Stdout.empty());
            EXPECT_TRUE(Stderr.empty());
            EXPECT_EQ(Client.Reports - Before, Policy == HTNCallTermErrorPolicy::Report ? 2 : 0);
            if (Policy == HTNCallTermErrorPolicy::Report)
            {
                EXPECT_EQ(Client.Reason, Reasons[I]);
                EXPECT_EQ(Client.Name, Names[I]);
                EXPECT_EQ(Client.Daemon, I == 0u ? "" : "agent");
            }
        }
    }
    const int Before = Client.Reports;
    EXPECT_FALSE(Registry.Execute("ordinary_failure", Context, Arguments).IsBound());
    EXPECT_EQ(Client.Reports, Before);
}

TEST(HTNCallTermErrorTest, GeneratedCallsReportProvenanceAndPreserveFailureSemantics)
{
    std::ifstream Input(HTNFileHelpers::MakeAbsolutePath("Domains/Test/missing_callterms.domain"), std::ios::binary);
    ASSERT_TRUE(Input.good());
    const std::string Text((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    const HTNSourceText Source(Text);
    for (const auto Reason : {HTNCallTermErrorReason::NotRegistered, HTNCallTermErrorReason::MissingBinding,
                             HTNCallTermErrorReason::MissingInstance})
    {
        HTNCallTermRegistry Registry;
        if (Reason == HTNCallTermErrorReason::MissingBinding)
        {
            ASSERT_TRUE(Registry.BindMember("probe", "agent", {}, {}));
        }
        if (Reason == HTNCallTermErrorReason::MissingInstance)
        {
            ASSERT_TRUE(Registry.BindMember("probe", "agent", [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(true); }, {}));
        }
        Registry.Bind("identity", [](const HTNCallTermArguments& Args) { return HTNAtomOwner(Args[0]); });
        HTNDatabaseHook Database;
        HTNPlannerHook Hook(Database.GetWorldState(), Registry);
        ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateMissingCalltermsHTN_GetDefinition()));
        HTNPlanningUnit Unit(Database, Hook, "condition");
        ClientContext Client;
        auto& Context = Unit.GetExecutionContext();
        Context.CallTermErrorCallback = Report;
        Unit.SetClientContext(&Client);
        for (const auto Policy : {HTNCallTermErrorPolicy::FailSilently, HTNCallTermErrorPolicy::Report})
        {
            Context.CallTermErrorPolicy = Policy;
            for (const std::string Entry : {"condition", "binding", "primitive", "compound", "nested", "unused"})
            {
                SCOPED_TRACE(Entry);
                const int Before = Client.Reports;
                const bool Succeeds = Entry == "condition" || Entry == "binding" || Entry == "unused";
                EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Entry)),
                          Succeeds ? HTN_DECOMPOSITION_SUCCEEDED : HTN_DECOMPOSITION_NO_PLAN);
                EXPECT_EQ(Client.Reports - Before, Policy == HTNCallTermErrorPolicy::Report && Entry != "unused" ? 1 : 0);
                if (Succeeds)
                {
                    ASSERT_EQ(Unit.GetCurrentPlan().size(), 1u);
                    ASSERT_NE(HTNGetTaskHead(Unit.GetCurrentPlan().front()), nullptr);
                    EXPECT_EQ(HTNGetTaskHead(Unit.GetCurrentPlan().front())->GetString(), Entry == "unused" ? "!safe" : "!fallback");
                }
                if (Policy == HTNCallTermErrorPolicy::Report && Entry != "unused")
                {
                    EXPECT_EQ(Client.Reason, Reason);
                    EXPECT_EQ(Client.Name, "probe");
                    EXPECT_EQ(Client.Domain, "MissingCallTerms");
                    EXPECT_NE(Client.File.find("missing_callterms.domain"), std::string::npos);
                    const size_t Method = Text.find("(:method (" + Entry + ")");
                    ASSERT_NE(Method, std::string::npos);
                    const size_t Call = Text.find("(call probe)", Method);
                    ASSERT_NE(Call, std::string::npos);
                    const auto Position = Source.GetPosition(Call);
                    EXPECT_EQ(Client.Line, static_cast<uint32_t>(Position.Line));
                    EXPECT_EQ(Client.Column, static_cast<uint32_t>(Position.Column));
                }
            }
        }
    }
}

TEST(HTNCallTermErrorTest, SharedRegistryKeepsInstancesAndClientContextsIndependent)
{
    HTNCallTermRegistry Registry;
    ASSERT_TRUE(Registry.BindMember("member", "agent", [](void* Instance, const HTNCallTermArguments&) {
        return HTNAtomOwner(*static_cast<int*>(Instance));
    }, {}));
    HTNCallTermBindingContext FirstBindings(Registry), SecondBindings(Registry);
    HTNPlannerExecutionContext First{}, Second{};
    First.CallTermBindingContext = &FirstBindings;
    Second.CallTermBindingContext = &SecondBindings;
    ClientContext FirstClient, SecondClient;
    First.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
    First.CallTermErrorCallback = Report;
    First.ClientContext = &FirstClient;
    Second.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
    Second.CallTermErrorCallback = Report;
    Second.ClientContext = &SecondClient;
    int Instance = 7;
    ASSERT_TRUE(FirstBindings.SetDaemon("agent", &Instance));
    const auto Call = HTNCallTermRegistry_ResolveGeneratedCallTerm(&FirstBindings, "member");
    HTNAtomOwner Result;
    EXPECT_EQ(InvokeGenerated(First, Call, Result.Get()), 1);
    EXPECT_EQ(InvokeGenerated(Second, Call, Result.Get()), 0);
    EXPECT_EQ(FirstClient.Reports, 0);
    EXPECT_EQ(SecondClient.Reports, 1);
    ASSERT_TRUE(SecondBindings.SetDaemon("agent", &Instance));
    EXPECT_EQ(InvokeGenerated(Second, Call, Result.Get()), 1);
    EXPECT_EQ(SecondClient.Reports, 1);
}

TEST(HTNCallTermErrorTest, UnsetAssertsOnMissingInvocation)
{
    HTNCallTermRegistry Registry;
    Registry.Bind("typed", [](const HTNCallTermArguments&) { return HTNAtomOwner(true); },
        {HTNAtomType::HTN_ATOM_TYPE_INT});
    ASSERT_TRUE(Registry.BindMember("empty", "agent", {}, {}));
    ASSERT_TRUE(Registry.BindMember("member", "agent", [](void*, const HTNCallTermArguments&) { return HTNAtomOwner(true); }, {}));
    HTNCallTermBindingContext Bindings(Registry);
    HTNPlannerExecutionContext Context{};
    Context.CallTermBindingContext = &Bindings;
    const std::vector<HTNAtomOwner> Arguments;
    for (const char* Name : {"absent", "empty", "member", "typed"})
    {
        SCOPED_TRACE(Name);
        const auto Call = HTNCallTermRegistry_ResolveGeneratedCallTerm(&Bindings, Name);
        HTNAtomOwner Result;
#ifndef NDEBUG
        const auto Invoke = [&](bool Generated) {
#if defined(_WIN32) && defined(_DEBUG)
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
            if (Generated)
                (void)InvokeGenerated(Context, Call, Result.Get());
            else
                (void)Registry.Execute(Name, Context, Arguments);
        };
        EXPECT_DEATH(Invoke(false), "Configure the callterm error policy explicitly");
        EXPECT_DEATH(Invoke(true), "Configure the callterm error policy explicitly");
#else
        EXPECT_FALSE(Registry.Execute(Name, Context, Arguments).IsBound());
        EXPECT_EQ(InvokeGenerated(Context, Call, Result.Get()), 0);
#endif
    }
}

TEST(HTNCallTermErrorTest, ReportsUseExecutionContextWithSharedBindings)
{
    HTNCallTermRegistry Registry;
    HTNDatabaseHook Database;
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateMissingCalltermsHTN_GetDefinition()));
    HTNPlanningUnit First(Database, Hook, "condition"), Second(Database, Hook, "condition");
    ClientContext FirstClient, SecondClient;
    First.GetExecutionContext().ClientContext = &FirstClient;
    First.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
    First.GetExecutionContext().CallTermErrorCallback = Report;
    Second.GetExecutionContext().ClientContext = &SecondClient;
    Second.GetExecutionContext().CallTermErrorPolicy = HTNCallTermErrorPolicy::FailSilently;
    ASSERT_EQ(First.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
    ASSERT_EQ(Second.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(FirstClient.Reports, 1);
    EXPECT_EQ(SecondClient.Reports, 0);
    First.GetExecutionContext().ClientContext = &SecondClient;
    ASSERT_EQ(First.DecomposeTopLevelMethod(), HTN_DECOMPOSITION_SUCCEEDED);
    EXPECT_EQ(FirstClient.Reports, 1);
    EXPECT_EQ(SecondClient.Reports, 1);
}

TEST(HTNCallTermErrorTest, InvalidRuntimePolicyAndMissingReportCallbackAssert)
{
    HTNCallTermRegistry Registry;
    HTNCallTermBindingContext Bindings(Registry);
    HTNPlannerExecutionContext Context{};
    Context.CallTermBindingContext = &Bindings;
    const auto Call = HTNCallTermRegistry_ResolveGeneratedCallTerm(&Bindings, "absent");
    const std::vector<HTNAtomOwner> Arguments;
    for (const auto Policy : {HTNCallTermErrorPolicy::Report, static_cast<HTNCallTermErrorPolicy>(99)})
    {
        Context.CallTermErrorPolicy = Policy;
        HTNAtomOwner Result;
#ifndef NDEBUG
        const auto Invoke = [&](bool Generated) {
#if defined(_WIN32) && defined(_DEBUG)
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
            if (Generated) (void)InvokeGenerated(Context, Call, Result.Get());
            else (void)Registry.Execute("absent", Context, Arguments);
        };
        const char* Message = Policy == HTNCallTermErrorPolicy::Report
            ? "Report policy requires" : "Invalid callterm error policy";
        EXPECT_DEATH(Invoke(false), Message);
        EXPECT_DEATH(Invoke(true), Message);
#else
        EXPECT_FALSE(Registry.Execute("absent", Context, Arguments).IsBound());
        EXPECT_EQ(InvokeGenerated(Context, Call, Result.Get()), 0);
#endif
    }
}

TEST(HTNCallTermErrorTest, InitializationValidationSharesRuntimeChecksWithoutInvoking)
{
    const auto& Definition = *CreateMissingCalltermsHTN_GetDefinition();
    ASSERT_EQ(Definition.callterm_requirement_count, 7u);
    HTNCallTermRegistry Registry;
    HTNCallTermBindingContext Bindings(Registry);
    int Executions = 0;
    Registry.Bind("identity", [&Executions](const HTNCallTermArguments&) { ++Executions; return true; });
    ClientContext Client;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings, Report, &Client));
    EXPECT_EQ(Client.Reports, 6);
    EXPECT_EQ(Client.Reason, HTNCallTermErrorReason::NotRegistered);
    EXPECT_EQ(Client.Name, "probe");
    EXPECT_EQ(Client.Domain, "MissingCallTerms");
    EXPECT_FALSE(Client.File.empty());
    EXPECT_GT(Client.Line, 0u);
    EXPECT_GT(Client.Column, 0u);

    ASSERT_TRUE(Registry.BindMember("probe", "agent", {}, {}));
    Client.Reports = 0;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings, Report, &Client));
    EXPECT_EQ(Client.Reports, 6);
    EXPECT_EQ(Client.Reason, HTNCallTermErrorReason::MissingBinding);
    EXPECT_EQ(Client.Daemon, "agent");

    ASSERT_TRUE(Registry.BindMember("probe", "agent",
        [&Executions](void*, const HTNCallTermArguments&) { ++Executions; return HTNAtomOwner(true); }, {}));
    Client.Reports = 0;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings, Report, &Client));
    EXPECT_EQ(Client.Reports, 6);
    EXPECT_EQ(Client.Reason, HTNCallTermErrorReason::MissingInstance);
    ASSERT_TRUE(Bindings.SetDaemon("agent", &Executions));
    Client.Reports = 0;
    EXPECT_TRUE(Registry.ValidateGeneratedCallTerms(Definition, Bindings, Report, &Client));
    EXPECT_EQ(Client.Reports, 0);
    EXPECT_EQ(Executions, 0);
    ASSERT_TRUE(Bindings.SetDaemon("agent", nullptr));
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings));
}

TEST(HTNCallTermErrorTest, InitializationMetadataIncludesEveryCallSiteWithExactSource)
{
    const auto& Definition = *CreateMissingCalltermsHTN_GetDefinition();
    const auto Path = HTNFileHelpers::MakeAbsolutePath("Domains/Test/missing_callterms.domain");
    std::ifstream Input(Path);
    const std::string Text{std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
    ASSERT_FALSE(Text.empty());
    const HTNSourceText Source(Text);
    size_t Count = 0;
    for (size_t Offset = Text.find("(call "); Offset != std::string::npos; Offset = Text.find("(call ", Offset + 1))
    {
        const auto Position = Source.GetPosition(Offset);
        uint32_t Matches = 0;
        for (uint32_t Index = 0; Index < Definition.callterm_requirement_count; ++Index)
        {
            const auto& Required = Definition.callterm_requirements[Index];
            if (Required.source.line != static_cast<uint32_t>(Position.Line) || Required.source.column != static_cast<uint32_t>(Position.Column)) continue;
            ++Matches;
            EXPECT_EQ(Text.compare(Offset + 6, std::string(Required.name).size(), Required.name), 0);
            EXPECT_EQ(HTNFileHelpers::MakeAbsolutePath(Required.source.file).lexically_normal().generic_string(),
                Path.lexically_normal().generic_string());
        }
        EXPECT_EQ(Matches, 1u);
        ++Count;
    }
    EXPECT_EQ(Count, Definition.callterm_requirement_count);
}

TEST(HTNCallTermErrorTest, InitializationRejectsInvalidDescriptorsAndForeignRegistry)
{
    auto Definition = *CreateMissingCalltermsHTN_GetDefinition();
    HTNCallTermRegistry Registry, Other;
    HTNCallTermBindingContext Bindings(Registry), Foreign(Other);
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Foreign));
    Definition.callterm_requirement_count = 0;
    Definition.callterm_requirements = nullptr;
    EXPECT_TRUE(Registry.ValidateGeneratedCallTerms(Definition, Bindings));
    Definition.callterm_requirement_count = 1;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings));
    const HTNGeneratedCallTermRequirement Invalid{nullptr, {}};
    Definition.callterm_requirements = &Invalid;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings));
    --Definition.abi_version;
    EXPECT_FALSE(Registry.ValidateGeneratedCallTerms(Definition, Bindings));
}

namespace
{
struct ErrorConversionValue {};
}
template<> struct HTNTypeTraits<ErrorConversionValue>
{
    static constexpr bool IsSupported = true;
    static constexpr bool HasFixedAtomType = true;
    static constexpr HTNAtomType AtomType = HTNAtomType::HTN_ATOM_TYPE_INT;
    static constexpr const char* Name = "ErrorConversionValue";
};
template<> struct HTNTypeConverter<ErrorConversionValue>
{
    static bool FromAtom(void*, const HTNAtom&, ErrorConversionValue&) { return false; }
    static bool ToAtom(void*, const ErrorConversionValue&, HTNAtom&) { return false; }
};
namespace
{
struct ErrorFunctions
{
    static inline int Calls = 0;
    static int32 Integer(int32) { ++Calls; return 1; }
    static int32 Second(int32, ErrorConversionValue) { ++Calls; return 1; }
    static int32 Converted(ErrorConversionValue) { ++Calls; return 1; }
    static ErrorConversionValue Result() { ++Calls; return {}; }
    int32 Member(int32) { ++Calls; return 1; }
    int32 ConvertedMember(ErrorConversionValue) { ++Calls; return 1; }
};
struct InvocationReport
{
    int Count = 0;
    HTNCallTermErrorInfo Info{};
    std::string Name;
};
void CaptureInvocationError(void* Context, const HTNCallTermErrorInfo* Info)
{
    auto& Report = *static_cast<InvocationReport*>(Context);
    ++Report.Count;
    Report.Info = *Info;
    Report.Name = Info->Name ? Info->Name : "";
}
}

TEST(HTNCallTermErrorTest, ArgumentAndConversionErrorsSharePolicyForBothInvocationApis)
{
    HTNCallTermRegistry Registry;
    HTN_CALLTERM_BIND(Registry, "integer", ErrorFunctions, Integer);
    HTN_CALLTERM_BIND(Registry, "converted", ErrorFunctions, Converted);
    HTN_CALLTERM_BIND(Registry, "second", ErrorFunctions, Second);
    HTN_CALLTERM_BIND(Registry, "result", ErrorFunctions, Result);
    ASSERT_TRUE(HTN_CALLTERM_BIND_MEMBER(Registry, "member", ErrorFunctions, Member));
    ASSERT_TRUE(HTN_CALLTERM_BIND_MEMBER(Registry, "converted_member", ErrorFunctions, ConvertedMember));
    ErrorFunctions Instance;
    HTNCallTermBindingContext Bindings(Registry);
    ASSERT_TRUE(HTN_CALLTERM_SET_DAEMON(Bindings, ErrorFunctions, &Instance));
    const HTNCallTermSource Source{"Domain", "test.domain", 12, 9};
    struct Case { const char* Name; std::vector<HTNAtomOwner> Arguments; HTNCallTermErrorReason Reason; uint32_t Index; };
    const Case Cases[] = {
        {"integer", {}, HTNCallTermErrorReason::ArgumentCountMismatch, UINT32_MAX},
        {"integer", {HTNAtomOwner(int32{1}), HTNAtomOwner(int32{2})}, HTNCallTermErrorReason::ArgumentCountMismatch, UINT32_MAX},
        {"integer", {HTNAtomOwner(1.0f)}, HTNCallTermErrorReason::ArgumentTypeMismatch, 0},
        {"member", {HTNAtomOwner(1.0f)}, HTNCallTermErrorReason::ArgumentTypeMismatch, 0},
        {"member", {}, HTNCallTermErrorReason::ArgumentCountMismatch, UINT32_MAX},
        {"converted", {HTNAtomOwner(int32{1})}, HTNCallTermErrorReason::ArgumentConversionFailed, 0},
        {"converted_member", {HTNAtomOwner(int32{1})}, HTNCallTermErrorReason::ArgumentConversionFailed, 0},
        {"second", {HTNAtomOwner(int32{1}), HTNAtomOwner(int32{2})}, HTNCallTermErrorReason::ArgumentConversionFailed, 1},
        {"result", {}, HTNCallTermErrorReason::ReturnConversionFailed, UINT32_MAX}};
    for (const auto Policy : {HTNCallTermErrorPolicy::Report, HTNCallTermErrorPolicy::FailSilently})
        for (bool Generated : {false, true})
            for (const auto& Test : Cases)
            {
                SCOPED_TRACE(Test.Name);
                InvocationReport Report;
                ErrorFunctions::Calls = 0;
                HTNPlannerExecutionContext Context{};
                Context.CallTermBindingContext = &Bindings;
                Context.ClientContext = &Report;
                Context.CallTermErrorPolicy = Policy;
                Context.CallTermErrorCallback = CaptureInvocationError;
                testing::internal::CaptureStdout();
                testing::internal::CaptureStderr();
                if (Generated)
                {
                    HTNGeneratedPlannerContext Runtime{};
                    Runtime.callterm_binding_context = &Bindings;
                    Runtime.client_context = &Report;
                    Runtime.callterm_error_policy = Policy;
                    Runtime.callterm_error_callback = CaptureInvocationError;
                    const auto Call = HTNCallTermRegistry_ResolveGeneratedCallTerm(&Bindings, Test.Name);
                    std::vector<const HTNAtom*> Arguments;
                    for (const auto& Argument : Test.Arguments) Arguments.push_back(Argument.Get());
                    HTNAtomOwner Result;
                    EXPECT_EQ(HTNCallTermRegistry_InvokeGeneratedCallTermWithSource(&Runtime, &Call,
                        Arguments.data(), static_cast<uint32_t>(Arguments.size()), Result.Get(), &Source), 0);
                }
                else EXPECT_FALSE(Registry.Execute(Test.Name, Context, Test.Arguments, &Source).IsBound());
                EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
                EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
                EXPECT_EQ(ErrorFunctions::Calls, Test.Reason == HTNCallTermErrorReason::ReturnConversionFailed ? 1 : 0);
                EXPECT_EQ(Report.Count, Policy == HTNCallTermErrorPolicy::Report ? 1 : 0);
                if (Report.Count)
                {
                    EXPECT_EQ(Report.Info.Reason, Test.Reason);
                    EXPECT_EQ(Report.Info.ArgumentIndex, Test.Index);
                    EXPECT_EQ(Report.Name, Test.Name);
                    EXPECT_STREQ(Report.Info.Source.file, Source.file);
                    EXPECT_STREQ(Report.Info.Source.domain, Source.domain);
                    EXPECT_EQ(Report.Info.Source.line, Source.line);
                    EXPECT_EQ(Report.Info.Source.column, Source.column);
                    EXPECT_EQ(Report.Info.ActualArgumentCount, Test.Arguments.size());
                    EXPECT_EQ(Report.Info.ExpectedArgumentCount, std::string(Test.Name) == "result" ? 0u : (std::string(Test.Name) == "second" ? 2u : 1u));
                    if (Test.Index != UINT32_MAX)
                    {
                        EXPECT_EQ(Report.Info.ExpectedAtomType, static_cast<uint32_t>(HTNAtomType::HTN_ATOM_TYPE_INT));
                        EXPECT_EQ(Report.Info.ActualAtomType, static_cast<uint32_t>(HTNAtomGetType(*Test.Arguments[Test.Index].Get())));
                    }
                    if (Test.Reason == HTNCallTermErrorReason::ArgumentConversionFailed)
                    {
                        EXPECT_STREQ(Report.Info.ExpectedTypeName, "ErrorConversionValue");
                    }
                }
            }
}

TEST(HTNCallTermErrorTest, NestedGeneratedCallReportsCountMismatchOnceWithRealSource)
{
    HTNCallTermRegistry Registry;
    HTN_CALLTERM_BIND(Registry, "probe", ErrorFunctions, Integer);
    int OuterCalls = 0;
    Registry.Bind("identity", [&](const HTNCallTermArguments&) { ++OuterCalls; return HTNAtomOwner(int32{1}); });
    HTNDatabaseHook Database;
    HTNPlannerHook Hook(Database.GetWorldState(), Registry);
    ASSERT_TRUE(Hook.SetGeneratedPlannerDefinition(CreateMissingCalltermsHTN_GetDefinition()));
    HTNPlanningUnit Unit(Database, Hook, "nested");
    ClientContext Client;
    auto& Context = Unit.GetExecutionContext();
    Context.ClientContext = &Client;
    Context.CallTermErrorCallback = Report;
    Context.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
    ErrorFunctions::Calls = 0;
    for (const char* Method : {"nested", "primitive", "compound"})
    {
        SCOPED_TRACE(Method);
        Client.Reports = 0;
        EXPECT_EQ(Unit.DecomposeTopLevelMethod(HtnSymbol::sGetSymbol(Method)), HTN_DECOMPOSITION_NO_PLAN);
        EXPECT_EQ(Client.Reports, 1);
        EXPECT_EQ(Client.Reason, HTNCallTermErrorReason::ArgumentCountMismatch);
        EXPECT_EQ(Client.Name, "probe");
        EXPECT_EQ(Client.Domain, "MissingCallTerms");
        EXPECT_NE(Client.File.find("missing_callterms.domain"), std::string::npos);
        EXPECT_GT(Client.Line, 0u);
        EXPECT_GT(Client.Column, 0u);
        EXPECT_EQ(ErrorFunctions::Calls, 0);
        EXPECT_EQ(OuterCalls, 0);
    }
}
