// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNCallTermRegistry.h"
#include "Core/HTNCallTermBindingContext.h"
#include "Translator/HTNCallTermBridge.h"
#include "Translator/HTNGeneratedPlanner.h"
#include <cassert>

bool HTNCallTermRegistry::IsBound(const std::string& inID) const
{
    return Resolve(inID) != nullptr;
}

const HTNCallTermFunction* HTNCallTermRegistry::Resolve(const std::string& inID) const
{
    const auto It = mEntries.find(inID);
    return It == mEntries.end() ? nullptr : &It->second.Function;
}

const HTNCallTermSignature* HTNCallTermRegistry::ResolveSignature(const std::string& inID) const
{
    const auto It = mEntries.find(inID);
    if (It == mEntries.end() || !It->second.Signature)
        return nullptr;

    return &*It->second.Signature;
}

HTNAtomOwner HTNCallTermRegistry::Execute(const std::string& inID,
                                          const HTNPlannerExecutionContext& inContext,
                                          const HTNCallTermArguments& inArguments,
                                          const HTNCallTermSource* inSource) const
{
    const auto It = mEntries.find(inID);
    return InvokeEntry(It == mEntries.end() ? nullptr : &It->second, inID.c_str(), inContext.CallTermBindingContext, inArguments, inSource, inContext.ClientContext,
                       inContext.MissingCallTermPolicy, inContext.MissingCallTermCallback);
}

HTNAtomOwner HTNCallTermRegistry::InvokeEntry(const Entry* inEntry, const char* inName,
                                             const HTNCallTermBindingContext* inContext,
                                             const HTNCallTermArguments& inArguments,
                                             const HTNCallTermSource* inSource, void* inClientContext,
                                             HTNMissingCallTermPolicy inPolicy, HTNMissingCallTermCallback inCallback)
{
    const auto Missing = [&](HTNMissingCallTermReason inReason) {
        assert(inPolicy != HTNMissingCallTermPolicy::Unset &&
               "Configure the missing callterm policy explicitly before invoking callterms");
        assert((inPolicy == HTNMissingCallTermPolicy::Unset ||
                inPolicy == HTNMissingCallTermPolicy::FailSilently ||
                inPolicy == HTNMissingCallTermPolicy::Report) && "Invalid missing callterm policy");
        if (inPolicy == HTNMissingCallTermPolicy::Report)
        {
            assert(inCallback && "Report policy requires a missing callterm callback");
            if (inCallback)
            {
                HTNMissingCallTermInfo Info{};
                Info.Name = inName;
                Info.Reason = inReason;
                Info.DaemonID = inEntry && !inEntry->DaemonID.empty() ? inEntry->DaemonID.c_str() : nullptr;
                if (inSource) Info.Source = *inSource;
                inCallback(inClientContext, &Info);
            }
        }
        return HTNAtomOwner();
    };
    void* Daemon = nullptr;
    if (const auto Reason = CheckEntry(inEntry, inContext, Daemon))
        return Missing(*Reason);
    HTNCallTermArguments Arguments = inArguments;
    Arguments.mClientContext = inClientContext;
    return inEntry->Function(Daemon, Arguments);
}

std::optional<HTNMissingCallTermReason> HTNCallTermRegistry::CheckEntry(
    const Entry* inEntry, const HTNCallTermBindingContext* inContext, void*& outDaemon)
{
    outDaemon = nullptr;
    if (!inEntry) return HTNMissingCallTermReason::NotRegistered;
    if (!inEntry->Function) return HTNMissingCallTermReason::MissingBinding;
    if (inEntry->DaemonSlot != std::numeric_limits<std::size_t>::max())
    {
        outDaemon = inContext ? inContext->GetDaemon(inEntry->DaemonSlot) : nullptr;
        if (!outDaemon) return HTNMissingCallTermReason::MissingInstance;
    }
    return std::nullopt;
}

bool HTNCallTermRegistry::ValidateGeneratedCallTerms(const HTNGeneratedPlannerDefinition& inDefinition,
    const HTNCallTermBindingContext& inContext, HTNMissingCallTermCallback inCallback, void* inClientContext) const
{
    if (&inContext.GetRegistry() != this || !HTNGeneratedPlanner_ValidateDefinition(&inDefinition))
        return false;
    bool Valid = true;
    for (uint32_t Index = 0; Index < inDefinition.callterm_requirement_count; ++Index)
    {
        const auto& Requirement = inDefinition.callterm_requirements[Index];
        const auto It = mEntries.find(Requirement.name);
        const Entry* Binding = It == mEntries.end() ? nullptr : &It->second;
        void* Daemon = nullptr;
        if (const auto Reason = CheckEntry(Binding, &inContext, Daemon))
        {
            Valid = false;
            if (inCallback)
            {
                const HTNMissingCallTermInfo Info{Requirement.name, *Reason,
                    Binding && !Binding->DaemonID.empty() ? Binding->DaemonID.c_str() : nullptr, Requirement.source};
                inCallback(inClientContext, &Info);
            }
        }
    }
    return Valid;
}

bool HTNCallTermRegistry::BindMember(const std::string& inID,
                                     const std::string& inDaemonID,
                                     HTNCallTermFunction inFunction,
                                     HTNCallTermSignature inSignature)
{
    auto DaemonIt = mDaemonSlots.find(inDaemonID);
    if (DaemonIt == mDaemonSlots.end())
    {
        if (mDaemonSlots.size() >= HTN_MAX_CALLTERM_DAEMON_TYPES)
        {
            HTN_LOG_ERROR("Cannot register callterm daemon type [{}]: capacity [{}] has been reached",
                          inDaemonID,
                          HTN_MAX_CALLTERM_DAEMON_TYPES);
            return false;
        }

        DaemonIt = mDaemonSlots.emplace(inDaemonID, mDaemonSlots.size()).first;
    }

    Entry NewEntry;
    NewEntry.Function = std::move(inFunction);
    NewEntry.Signature = std::move(inSignature);
    NewEntry.DaemonSlot = DaemonIt->second;
    NewEntry.DaemonID = inDaemonID;
    mEntries[inID] = std::move(NewEntry);
    return true;
}

std::size_t HTNCallTermRegistry::FindDaemonSlot(const std::string& inID) const
{
    const auto It = mDaemonSlots.find(inID);
    return It == mDaemonSlots.end() ? std::numeric_limits<std::size_t>::max() : It->second;
}

bool HTNCallTermRegistry::ValidateArguments([[maybe_unused]]const std::string& inID,
                                            const HTNCallTermSignature& inSignature,
                                            const HTNCallTermArguments& inArguments)
{
    if (inArguments.size() != inSignature.size())
    {
        HTN_LOG_ERROR("Callterm [{}] expects [{}] argument(s), received [{}]",
                      inID,
                      inSignature.size(),
                      inArguments.size());
        return false;
    }

    for (size_t Index = 0u; Index < inSignature.size(); ++Index)
    {
        const std::optional<HTNAtomType>& ExpectedType = inSignature[Index];
        if (ExpectedType && HTNAtomGetType(inArguments[Index]) != *ExpectedType)
        {
            HTN_LOG_ERROR("Callterm [{}] argument [{}] has incompatible HTN atom type",
                          inID,
                          Index);
            return false;
        }
    }

    return true;
}

extern "C" HTNGeneratedCallTerm HTNCallTermRegistry_ResolveGeneratedCallTerm(
    const HTNCallTermBindingContext* inContext,
    const char* inName)
{
    HTNGeneratedCallTerm Result = {nullptr, inName};
    if (!inContext || !inName)
        return Result;

    const HTNCallTermRegistry* inRegistry = &inContext->mRegistry;
    const auto It = inRegistry->mEntries.find(inName);
    if (It != inRegistry->mEntries.end())
    {
        Result.registry_entry = &It->second;
        Result.name = It->first.c_str();
    }
    return Result;
}

extern "C" int HTNCallTermRegistry_InvokeGeneratedCallTerm(
    const HTNGeneratedPlannerContext* inContext,
    const HTNGeneratedCallTerm* inCallTerm,
    const HTNAtom* const* inArguments,
    const std::uint32_t inArgumentCount,
    HTNAtom* outResult)
{
    return HTNCallTermRegistry_InvokeGeneratedCallTermWithSource(
        inContext, inCallTerm, inArguments, inArgumentCount, outResult, nullptr);
}

extern "C" int HTNCallTermRegistry_InvokeGeneratedCallTermWithSource(
    const HTNGeneratedPlannerContext* inContext,
    const HTNGeneratedCallTerm* inCallTerm,
    const HTNAtom* const* inArguments,
    const std::uint32_t inArgumentCount,
    HTNAtom* outResult,
    const HTNCallTermSource* inSource)
{
    if (!inContext) return 0;
    const auto* Entry = inCallTerm ? static_cast<const HTNCallTermRegistry::Entry*>(inCallTerm->registry_entry) : nullptr;
    const HTNCallTermArguments Arguments(inArguments, inArgumentCount);
    HTNAtomOwner Result = HTNCallTermRegistry::InvokeEntry(Entry, inCallTerm ? inCallTerm->name : nullptr,
                                                         inContext->callterm_binding_context, Arguments, inSource, inContext->client_context,
                                                         inContext->missing_callterm_policy, inContext->missing_callterm_callback);
    if (!Result.IsBound())
        return 0;
    HTNAtom_AssignMove(outResult, Result.Get());
    return 1;
}
