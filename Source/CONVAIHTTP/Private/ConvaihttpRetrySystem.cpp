// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpRetrySystem.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "ConvaihttpModule.h"
#include "Convaihttp.h"
#include "ConvaihttpManager.h"
#include "Stats/Stats.h"

namespace FConvaihttpRetrySystem
{
	TOptional<double> ReadThrottledTimeFromResponseInSeconds(FConvaihttpResponsePtr Response)
	{
		TOptional<double> LockoutPeriod;
		// Check if there was a Retry-After header
		if (Response.IsValid())
		{
			int32 ResponseCode = Response->GetResponseCode();
			if (ResponseCode == EConvaihttpResponseCodes::TooManyRequests || ResponseCode == EConvaihttpResponseCodes::ServiceUnavail)
			{
				FString RetryAfter = Response->GetHeader(TEXT("Retry-After"));
				if (!RetryAfter.IsEmpty())
				{
					if (RetryAfter.IsNumeric())
					{
						// seconds
						LockoutPeriod.Emplace(FCString::Atof(*RetryAfter));
					}
					else
					{
						// convaihttp date
						FDateTime UTCServerTime;
						if (FDateTime::ParseHttpDate(RetryAfter, UTCServerTime))
						{
							const FDateTime UTCNow = FDateTime::UtcNow();
							LockoutPeriod.Emplace((UTCServerTime - UTCNow).GetTotalSeconds());
						}
					}
				}
				else
				{
					FString RateLimitReset = Response->GetHeader(TEXT("X-Rate-Limit-Reset"));
					if (!RateLimitReset.IsEmpty())
					{
						// UTC seconds
						const FDateTime UTCServerTime = FDateTime::FromUnixTimestamp(FCString::Atoi64(*RateLimitReset));
						const FDateTime UTCNow = FDateTime::UtcNow();
						LockoutPeriod.Emplace((UTCServerTime - UTCNow).GetTotalSeconds());
					}
				}
			}
		}
		return LockoutPeriod;
	}
}

FConvaihttpRetrySystem::FRequest::FRequest(
	FManager& InManager,
	const TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>& ConvaihttpRequest, 
	const FConvaihttpRetrySystem::FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FConvaihttpRetrySystem::FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FConvaihttpRetrySystem::FRetryResponseCodes& InRetryResponseCodes,
	const FConvaihttpRetrySystem::FRetryVerbs& InRetryVerbs,
	const FConvaihttpRetrySystem::FRetryDomainsPtr& InRetryDomains
	)
    : FConvaihttpRequestAdapterBase(ConvaihttpRequest)
    , Status(FConvaihttpRetrySystem::FRequest::EStatus::NotStarted)
    , RetryLimitCountOverride(InRetryLimitCountOverride)
    , RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
	, RetryResponseCodes(InRetryResponseCodes)
	, RetryVerbs(InRetryVerbs)
	, RetryDomains(InRetryDomains)
	, RetryManager(InManager)
{
    // if the InRetryTimeoutRelativeSecondsOverride override is being used the value cannot be negative
    check(!(InRetryTimeoutRelativeSecondsOverride.IsSet()) || (InRetryTimeoutRelativeSecondsOverride.GetValue() >= 0.0));

	if (RetryDomains.IsValid())
	{
		if (RetryDomains->Domains.Num() == 0)
		{
			// If there are no domains to cycle through, go through the simpler path
			RetryDomains.Reset();
		}
		else
		{
			// Start with the active index
			RetryDomainsIndex = RetryDomains->ActiveIndex;
			check(RetryDomains->Domains.IsValidIndex(RetryDomainsIndex));
		}
	}
}

bool FConvaihttpRetrySystem::FRequest::ProcessRequest()
{ 
	TSharedRef<FRequest, ESPMode::ThreadSafe> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	OriginalUrl = ConvaihttpRequest->GetURL();
	if (RetryDomains.IsValid())
	{
		SetUrlFromRetryDomains();
	}

	ConvaihttpRequest->OnRequestProgress().BindThreadSafeSP(RetryRequest, &FConvaihttpRetrySystem::FRequest::ConvaihttpOnRequestProgress);

	return RetryManager.ProcessRequest(RetryRequest);
}

void FConvaihttpRetrySystem::FRequest::SetUrlFromRetryDomains()
{
	check(RetryDomains.IsValid());
	FString OriginalUrlDomainAndPort = FPlatformConvaihttp::GetUrlDomainAndPort(OriginalUrl);
	if (!OriginalUrlDomainAndPort.IsEmpty())
	{
		const FString Url(OriginalUrl.Replace(*OriginalUrlDomainAndPort, *RetryDomains->Domains[RetryDomainsIndex]));
		ConvaihttpRequest->SetURL(Url);
	}
}

void FConvaihttpRetrySystem::FRequest::MoveToNextRetryDomain()
{
	check(RetryDomains.IsValid());
	const int32 NextDomainIndex = (RetryDomainsIndex + 1) % RetryDomains->Domains.Num();
	if (RetryDomains->ActiveIndex.CompareExchange(RetryDomainsIndex, NextDomainIndex))
	{
		RetryDomainsIndex = NextDomainIndex;
	}
	SetUrlFromRetryDomains();
}

void FConvaihttpRetrySystem::FRequest::CancelRequest() 
{ 
	TSharedRef<FRequest, ESPMode::ThreadSafe> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	RetryManager.CancelRequest(RetryRequest);
}

void FConvaihttpRetrySystem::FRequest::ConvaihttpOnRequestProgress(FConvaihttpRequestPtr InConvaihttpRequest, int64 BytesSent, int64 BytesRcv)
{
	OnRequestProgress().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
}

FConvaihttpRetrySystem::FManager::FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault)
    : RandomFailureRate(FRandomFailureRateSetting())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
	, RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{}

TSharedRef<FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe> FConvaihttpRetrySystem::FManager::CreateRequest(
	const FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FRetryResponseCodes& InRetryResponseCodes,
	const FRetryVerbs& InRetryVerbs,
	const FRetryDomainsPtr& InRetryDomains)
{
	return MakeShareable(new FRequest(
		*this,
		FConvaihttpModule::Get().CreateRequest(),
		InRetryLimitCountOverride,
		InRetryTimeoutRelativeSecondsOverride,
		InRetryResponseCodes,
		InRetryVerbs,
		InRetryDomains
		));
}

bool FConvaihttpRetrySystem::FManager::ShouldRetry(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry)
{
    bool bResult = false;

	FConvaihttpResponsePtr Response = ConvaihttpRetryRequestEntry.Request->GetResponse();
	// invalid response means connection or network error but we need to know which one
	if (!Response.IsValid())
	{
		// ONLY retry bad responses if they are connection errors (NOT protocol errors or unknown) otherwise request may be sent (and processed!) twice
		EConvaihttpRequestStatus::Type Status = ConvaihttpRetryRequestEntry.Request->GetStatus();
		if (Status == EConvaihttpRequestStatus::Failed_ConnectionError)
		{
			bResult = true;
		}
		else if (Status == EConvaihttpRequestStatus::Failed)
		{
			const FName Verb = FName(*ConvaihttpRetryRequestEntry.Request->GetVerb());

			// Be default, we will also allow retry for GET and HEAD requests even if they may duplicate on the server
			static const TSet<FName> DefaultRetryVerbs(TArray<FName>({ FName(TEXT("GET")), FName(TEXT("HEAD")) }));

			const bool bIsRetryVerbsEmpty = ConvaihttpRetryRequestEntry.Request->RetryVerbs.Num() == 0;
			if (bIsRetryVerbsEmpty && DefaultRetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
			// If retry verbs are specified, only allow retrying the specified list of verbs
			else if (ConvaihttpRetryRequestEntry.Request->RetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
		}
	}
	else
	{
		// this may be a successful response with one of the explicitly listed response codes we want to retry on
		if (ConvaihttpRetryRequestEntry.Request->RetryResponseCodes.Contains(Response->GetResponseCode()))
		{
			bResult = true;
		}
	}

    return bResult;
}

bool FConvaihttpRetrySystem::FManager::CanRetry(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry)
{
    bool bResult = false;

    bool bShouldTestCurrentRetryCount = false;
    double RetryLimitCount = 0;
    if (ConvaihttpRetryRequestEntry.Request->RetryLimitCountOverride.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = ConvaihttpRetryRequestEntry.Request->RetryLimitCountOverride.GetValue();
    }
    else if (RetryLimitCountDefault.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = RetryLimitCountDefault.GetValue();
    }

    if (bShouldTestCurrentRetryCount)
    {
        if (ConvaihttpRetryRequestEntry.CurrentRetryCount < RetryLimitCount)
        {
            bResult = true;
        }
    }

    return bResult;
}

bool FConvaihttpRetrySystem::FManager::HasTimedOut(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry, const double NowAbsoluteSeconds)
{
    bool bResult = false;

    bool bShouldTestRetryTimeout = false;
    double RetryTimeoutAbsoluteSeconds = ConvaihttpRetryRequestEntry.RequestStartTimeAbsoluteSeconds;
    if (ConvaihttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += ConvaihttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.GetValue();
    }
    else if (RetryTimeoutRelativeSecondsDefault.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += RetryTimeoutRelativeSecondsDefault.GetValue();
    }

    if (bShouldTestRetryTimeout)
    {
        if (NowAbsoluteSeconds >= RetryTimeoutAbsoluteSeconds)
        {
            bResult = true;
        }
    }

    return bResult;
}

float FConvaihttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry)
{
	float LockoutPeriod = 0.0f;
	TOptional<double> ResponseLockoutPeriod = FConvaihttpRetrySystem::ReadThrottledTimeFromResponseInSeconds(ConvaihttpRetryRequestEntry.Request->GetResponse());
	if (ResponseLockoutPeriod.IsSet())
	{
		LockoutPeriod = static_cast<float>(ResponseLockoutPeriod.GetValue());
	}

	if (ConvaihttpRetryRequestEntry.CurrentRetryCount >= 1)
	{
		if (LockoutPeriod <= 0.0f)
		{
			const bool bFailedToConnect = ConvaihttpRetryRequestEntry.Request->GetStatus() == EConvaihttpRequestStatus::Failed_ConnectionError;
			const bool bHasRetryDomains = ConvaihttpRetryRequestEntry.Request->RetryDomains.IsValid();
			// Skip the lockout period if we failed to connect to a domain and we have other domains to try
			const bool bSkipLockoutPeriod = (bFailedToConnect && bHasRetryDomains);
			if (!bSkipLockoutPeriod)
			{
				constexpr const float LockoutPeriodMinimumSeconds = 5.0f;
				constexpr const float LockoutPeriodEscalationSeconds = 2.5f;
				constexpr const float LockoutPeriodMaxSeconds = 30.0f;
				LockoutPeriod = LockoutPeriodMinimumSeconds + LockoutPeriodEscalationSeconds * (ConvaihttpRetryRequestEntry.CurrentRetryCount - 1);
				LockoutPeriod = FMath::Min(LockoutPeriod, LockoutPeriodMaxSeconds);
			}
		}
	}

	return LockoutPeriod;
}

static FRandomStream TempRandomStream(4435261);

bool FConvaihttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_Update);
	LLM_SCOPE(ELLMTag::Networking);

	bool bIsGreen = true;

	if (FileCount != nullptr)
	{
		*FileCount = RequestList.Num();
	}

	const double NowAbsoluteSeconds = FPlatformTime::Seconds();

	// Basic algorithm
	// for each managed item
	//    if the item hasn't timed out
	//       if the item's retry state is NotStarted
	//          if the item's request's state is not NotStarted
	//             move the item's retry state to Processing
	//          endif
	//       endif
	//       if the item's retry state is Processing
	//          if the item's request's state is Failed
	//             flag return code to false
	//             if the item can be retried
	//                increment FailingCount if applicable
	//                retry the item's request
	//                increment the item's retry count
	//             else
	//                increment FailedCount if applicable
	//                set the item's retry state to FailedRetry
	//             endif
	//          else if the item's request's state is Succeeded
	//          endif
	//       endif
	//    else
	//       flag return code to false
	//       set the item's retry state to FailedTimeout
	//       increment FailedCount if applicable
	//    endif
	//    if the item's retry state is FailedRetry
	//       do stuff
	//    endif
	//    if the item's retry state is FailedTimeout
	//       do stuff
	//    endif
	//    if the item's retry state is Succeeded
	//       do stuff
	//    endif
	// endfor

	int32 index = 0;
	while (index < RequestList.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_Update_RequestListItem);

		FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry = RequestList[index];
		TSharedRef<FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe>& ConvaihttpRetryRequest = ConvaihttpRetryRequestEntry.Request;

		const EConvaihttpRequestStatus::Type RequestStatus = ConvaihttpRetryRequest->GetStatus();

		if (ConvaihttpRetryRequestEntry.bShouldCancel)
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Request cancelled on %s"), *(ConvaihttpRetryRequest->GetURL()));
			ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::Cancelled;
		}
		else
		{
			if (!HasTimedOut(ConvaihttpRetryRequestEntry, NowAbsoluteSeconds))
			{
				if (ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::NotStarted)
				{
					if (RequestStatus != EConvaihttpRequestStatus::NotStarted)
					{
						ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::Processing;
					}
				}

				if (ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::Processing)
				{
					bool forceFail = false;

					// Code to simulate request failure
					if (RequestStatus == EConvaihttpRequestStatus::Succeeded && RandomFailureRate.IsSet())
					{
						float random = TempRandomStream.GetFraction();
						if (random < RandomFailureRate.GetValue())
						{
							forceFail = true;
						}
					}

					// If we failed to connect, try the next domain in the list
					if (RequestStatus == EConvaihttpRequestStatus::Failed_ConnectionError)
					{
						if (ConvaihttpRetryRequest->RetryDomains.IsValid())
						{
							ConvaihttpRetryRequest->MoveToNextRetryDomain();
						}
					}
					// Save these for failure case retry checks if we hit a completion state
					bool bShouldRetry = false;
					bool bCanRetry = false;
					if (RequestStatus == EConvaihttpRequestStatus::Failed || RequestStatus == EConvaihttpRequestStatus::Failed_ConnectionError || RequestStatus == EConvaihttpRequestStatus::Succeeded)
					{
						bShouldRetry = ShouldRetry(ConvaihttpRetryRequestEntry);
						bCanRetry = CanRetry(ConvaihttpRetryRequestEntry);
					}

					if (RequestStatus == EConvaihttpRequestStatus::Failed || RequestStatus == EConvaihttpRequestStatus::Failed_ConnectionError || forceFail || (bShouldRetry && bCanRetry))
					{
						bIsGreen = false;

						if (forceFail || (bShouldRetry && bCanRetry))
						{
							float LockoutPeriod = GetLockoutPeriodSeconds(ConvaihttpRetryRequestEntry);

							if (LockoutPeriod > 0.0f)
							{
								UE_LOG(LogConvaihttp, Warning, TEXT("Lockout of %fs on %s"), LockoutPeriod, *(ConvaihttpRetryRequest->GetURL()));
							}

							ConvaihttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds = NowAbsoluteSeconds + LockoutPeriod;
							ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::ProcessingLockout;
							
							QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_Update_OnRequestWillRetry);
							ConvaihttpRetryRequest->OnRequestWillRetry().ExecuteIfBound(ConvaihttpRetryRequest, ConvaihttpRetryRequest->GetResponse(), LockoutPeriod);
						}
						else
						{
							UE_LOG(LogConvaihttp, Warning, TEXT("Retry exhausted on %s"), *(ConvaihttpRetryRequest->GetURL()));
							if (FailedCount != nullptr)
							{
								++(*FailedCount);
							}
							ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::FailedRetry;
						}
					}
					else if (RequestStatus == EConvaihttpRequestStatus::Succeeded)
					{
						if (ConvaihttpRetryRequestEntry.CurrentRetryCount > 0)
						{
							UE_LOG(LogConvaihttp, Warning, TEXT("Success on %s"), *(ConvaihttpRetryRequest->GetURL()));
						}

						if (CompletedCount != nullptr)
						{
							++(*CompletedCount);
						}

						ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::Succeeded;
					}
				}

				if (ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::ProcessingLockout)
				{
					if (NowAbsoluteSeconds >= ConvaihttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds)
					{
						// if this fails the ConvaihttpRequest's state will be failed which will cause the retry logic to kick(as expected)
						bool success = ConvaihttpRetryRequest->ConvaihttpRequest->ProcessRequest();
						if (success)
						{
							UE_LOG(LogConvaihttp, Warning, TEXT("Retry %d on %s"), ConvaihttpRetryRequestEntry.CurrentRetryCount + 1, *(ConvaihttpRetryRequest->GetURL()));

							++ConvaihttpRetryRequestEntry.CurrentRetryCount;
							ConvaihttpRetryRequest->Status = FRequest::EStatus::Processing;
						}
					}

					if (FailingCount != nullptr)
					{
						++(*FailingCount);
					}
				}
			}
			else
			{
				UE_LOG(LogConvaihttp, Warning, TEXT("Timeout on retry %d: %s"), ConvaihttpRetryRequestEntry.CurrentRetryCount + 1, *(ConvaihttpRetryRequest->GetURL()));
				bIsGreen = false;
				ConvaihttpRetryRequest->Status = FConvaihttpRetrySystem::FRequest::EStatus::FailedTimeout;
				if (FailedCount != nullptr)
				{
					++(*FailedCount);
				}
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::Cancelled ||
            ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::FailedRetry ||
            ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = ConvaihttpRetryRequest->Status == FConvaihttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (bWasCompleted)
		{
			if (bWasSuccessful)
			{
				ConvaihttpRetryRequest->BroadcastResponseHeadersReceived();
			}
			
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_Update_OnProcessRequestComplete);
			ConvaihttpRetryRequest->OnProcessRequestComplete().ExecuteIfBound(ConvaihttpRetryRequest, ConvaihttpRetryRequest->GetResponse(), bWasSuccessful);
		}

        if(bWasSuccessful)
        {
            if(CompletedCount != nullptr)
            {
                ++(*CompletedCount);
            }
        }

		if (bWasCompleted)
		{
			RequestList.RemoveAtSwap(index);
		}
		else
		{
			++index;
		}
	}

	return bIsGreen;
}

FConvaihttpRetrySystem::FManager::FConvaihttpRetryRequestEntry::FConvaihttpRetryRequestEntry(TSharedRef<FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe>& InRequest)
    : bShouldCancel(false)
    , CurrentRetryCount(0)
	, RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, Request(InRequest)
{}

bool FConvaihttpRetrySystem::FManager::ProcessRequest(TSharedRef<FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe>& ConvaihttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_ProcessRequest);

	bool bResult = ConvaihttpRetryRequest->ConvaihttpRequest->ProcessRequest();

	if (bResult)
	{
		RequestList.Add(FConvaihttpRetryRequestEntry(ConvaihttpRetryRequest));
	}

	return bResult;
}

void FConvaihttpRetrySystem::FManager::CancelRequest(TSharedRef<FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe>& ConvaihttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRetrySystem_FManager_CancelRequest);

	// Find the existing request entry if is was previously processed.
	bool bFound = false;
	for (int32 i = 0; i < RequestList.Num(); ++i)
	{
		FConvaihttpRetryRequestEntry& EntryRef = RequestList[i];

		if (EntryRef.Request == ConvaihttpRetryRequest)
		{
			EntryRef.bShouldCancel = true;
			bFound = true;
		}
	}
	// If we did not find the entry, likely auth failed for the request, in which case ProcessRequest does not get called.
	// Adding it to the list and flagging as cancel will process it on next tick.
	if (!bFound)
	{
		FConvaihttpRetryRequestEntry RetryRequestEntry(ConvaihttpRetryRequest);
		RetryRequestEntry.bShouldCancel = true;
		RequestList.Add(RetryRequestEntry);
	}
	ConvaihttpRetryRequest->ConvaihttpRequest->CancelRequest();
}

/* This should only be used when shutting down or suspending, to make sure 
	all pending CONVAIHTTP requests are flushed to the network */
void FConvaihttpRetrySystem::FManager::BlockUntilFlushed(float InTimeoutSec)
{
	const float SleepInterval = 0.016;
	float TimeElapsed = 0.0f;
	uint32 FileCount, FailingCount, FailedCount, CompleteCount;
	while (RequestList.Num() > 0 && TimeElapsed < InTimeoutSec)
	{
		FConvaihttpModule::Get().GetConvaihttpManager().Tick(SleepInterval);
		Update(&FileCount, &FailingCount, &FailedCount, &CompleteCount);
		FPlatformProcess::Sleep(SleepInterval);
		TimeElapsed += SleepInterval;
	}
}