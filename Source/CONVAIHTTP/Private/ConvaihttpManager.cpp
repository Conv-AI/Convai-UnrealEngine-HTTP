// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpManager.h"
#include "ConvaihttpModule.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Convaihttp.h"
#include "Misc/Guid.h"
#include "Misc/Fork.h"

#include "ConvaihttpThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

#include "Stats/Stats.h"
#include "Containers/BackgroundableTicker.h"

// FConvaihttpManager

FCriticalSection FConvaihttpManager::RequestLock;

FConvaihttpManager::FConvaihttpManager()
	: FTSTickerObjectBase(0.0f, FTSBackgroundableTicker::GetCoreTicker())
	, Thread(nullptr)
	, CorrelationIdMethod(FConvaihttpManager::GetDefaultCorrelationIdMethod())
{
	bFlushing = false;
}

FConvaihttpManager::~FConvaihttpManager()
{
	if (Thread)
	{
		Thread->StopThread();
		delete Thread;
	}
}

void FConvaihttpManager::Initialize()
{
	if (FPlatformConvaihttp::UsesThreadedConvaihttp())
	{
		Thread = CreateConvaihttpThread();
		Thread->StartThread();
	}

	UpdateConfigs();
}

void FConvaihttpManager::ReloadFlushTimeLimits()
{
	FlushTimeLimitsMap.Reset();

	//Save int values of Default and FullFlush?
	for (EConvaihttpFlushReason Reason : TEnumRange<EConvaihttpFlushReason>())
	{
		double SoftLimitSeconds = 2.0;
		double HardLimitSeconds = 4.0;

		// We default the time limits to generous values, keeping the Hard limits always greater than the soft ones, and -1 for the unlimited
		switch (Reason)
		{
		case EConvaihttpFlushReason::Default:
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushSoftTimeLimitDefault"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushHardTimeLimitDefault"), HardLimitSeconds, GEngineIni);
			break;
		case EConvaihttpFlushReason::Background:
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushSoftTimeLimitBackground"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushHardTimeLimitBackground"), HardLimitSeconds, GEngineIni);
			break;
		case EConvaihttpFlushReason::Shutdown:
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushSoftTimeLimitShutdown"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushHardTimeLimitShutdown"), HardLimitSeconds, GEngineIni);
			
			if ((HardLimitSeconds >= 0) && ((SoftLimitSeconds < 0) || (SoftLimitSeconds >= HardLimitSeconds)))
			{
				UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Soft limit[%.02f] is higher than the hard limit set[%.02f] in file [%s]. Please change the soft limit to a value lower than the hard limit for Flush to work correctly. - 1 is unlimited and therefore the highest possible value."), static_cast<float>(SoftLimitSeconds), static_cast<float>(HardLimitSeconds), *GEngineIni);
				// we need to be absolutely sure that SoftLimitSeconds is always strictly less than HardLimitSeconds so remaining requests (if any) can be canceled before exiting
				if (HardLimitSeconds > 0.0)
				{
					SoftLimitSeconds = HardLimitSeconds / 2.0;	// clamping SoftLimitSeconds to a reasonable value
				}
				else
				{
					// HardLimitSeconds should never be 0.0 while shutting down otherwise we can't cancel the remaining requests
					HardLimitSeconds = 0.05;	// using a non zero value 
					SoftLimitSeconds = 0.0;		// cancelling request immediately
				}
			}

			break;
		case EConvaihttpFlushReason::FullFlush:
			SoftLimitSeconds = -1.0;
			HardLimitSeconds = -1.0;
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushSoftTimeLimitFullFlush"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("CONVAIHTTP"), TEXT("FlushHardTimeLimitFullFlush"), HardLimitSeconds, GEngineIni);
			break;
		}

		FConvaihttpFlushTimeLimit TimeLimit(SoftLimitSeconds, HardLimitSeconds);

		FlushTimeLimitsMap.Add(Reason, TimeLimit);
	}
}

void FConvaihttpManager::SetCorrelationIdMethod(TFunction<FString()> InCorrelationIdMethod)
{
	check(InCorrelationIdMethod);
	CorrelationIdMethod = MoveTemp(InCorrelationIdMethod);
}

FString FConvaihttpManager::CreateCorrelationId() const
{
	return CorrelationIdMethod();
}

bool FConvaihttpManager::IsDomainAllowed(const FString& Url) const
{
#if !UE_BUILD_SHIPPING
#if !(UE_GAME || UE_SERVER)
	// Allowed domain filtering is opt-in in non-shipping non-game/server builds
	static const bool bForceUseAllowList = FParse::Param(FCommandLine::Get(), TEXT("EnableConvaihttpDomainRestrictions"));
	if (!bForceUseAllowList)
	{
		return true;
	}
#else
	// The check is on by default but allow non-shipping game/server builds to disable the filtering
	static const bool bIgnoreAllowList = FParse::Param(FCommandLine::Get(), TEXT("DisableConvaihttpDomainRestrictions"));
	if (bIgnoreAllowList)
	{
		return true;
	}
#endif
#endif // !UE_BUILD_SHIPPING

	// check to see if the Domain is allowed (either on the list or the list was empty)
	const TArray<FString>& AllowedDomains = FConvaihttpModule::Get().GetAllowedDomains();
	if (AllowedDomains.Num() > 0)
	{
		const FString Domain = FPlatformConvaihttp::GetUrlDomain(Url);
		for (const FString& AllowedDomain : AllowedDomains)
		{
			if (Domain.EndsWith(AllowedDomain))
			{
				return true;
			}
		}
		return false;
	}
	return true;
}

/*static*/
TFunction<FString()> FConvaihttpManager::GetDefaultCorrelationIdMethod()
{
	return []{ return FGuid::NewGuid().ToString(); };
}

void FConvaihttpManager::OnBeforeFork()
{
	Flush(EConvaihttpFlushReason::Default);
}

void FConvaihttpManager::OnAfterFork()
{

}

void FConvaihttpManager::OnEndFramePostFork()
{
	// nothing
}


void FConvaihttpManager::UpdateConfigs()
{
	ReloadFlushTimeLimits();

	if (Thread)
	{
		Thread->UpdateConfigs();
	}
}

void FConvaihttpManager::AddGameThreadTask(TFunction<void()>&& Task)
{
	if (Task)
	{
		GameThreadQueue.Enqueue(MoveTemp(Task));
	}
}

FConvaihttpThread* FConvaihttpManager::CreateConvaihttpThread()
{
	return new FConvaihttpThread();
}

void FConvaihttpManager::Flush(bool bShutdown)
{
	Flush(bShutdown ? EConvaihttpFlushReason::Shutdown : EConvaihttpFlushReason::Default);
}

void FConvaihttpManager::Flush(EConvaihttpFlushReason FlushReason)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpManager_Flush);

	FScopeLock ScopeLock(&RequestLock);
	
	// This variable is set to indicate that flush is happening.
	// While flushing is in progress, the RequestLock is held and threads are blocked when trying to submit new requests.
	bFlushing = true;

	double FlushTimeSoftLimitSeconds = FlushTimeLimitsMap[FlushReason].SoftLimitSeconds;
	double FlushTimeHardLimitSeconds = FlushTimeLimitsMap[FlushReason].HardLimitSeconds;

	// this specifies how long to sleep between calls to tick.
	// The smaller the value, the more quickly we may find out that all requests have completed, but the more work may be done in the meantime.
	float SecondsToSleepForOutstandingThreadedRequests = 0.5f;
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("RequestCleanupDelaySec"), SecondsToSleepForOutstandingThreadedRequests, GEngineIni);

	// Clear all delegates bound to ongoing Convaihttp requests
	if (FlushReason == EConvaihttpFlushReason::Shutdown)
	{
		if (Requests.Num())
		{
			// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
			UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Convaihttp module shutting down, but needs to wait on %d outstanding Convaihttp requests:"), Requests.Num());
		}

		// Clear delegates since they may point to deleted instances
		for (TArray64<FConvaihttpRequestRef>::TIterator It(Requests); It; ++It)
		{
			FConvaihttpRequestRef& Request = *It;
			Request->OnProcessRequestComplete().Unbind();
			Request->OnRequestProgress().Unbind();
			Request->OnHeaderReceived().Unbind();

			// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
			UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EConvaihttpRequestStatus::ToString(Request->GetStatus()));
		}
	}

	// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
	UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Cleaning up %d outstanding Convaihttp requests."), Requests.Num());

	double BeginWaitTime = FPlatformTime::Seconds();
	double LastFlushTickTime = BeginWaitTime;
	double StallWarnTime = BeginWaitTime + 0.5;
	double AppTime = FPlatformTime::Seconds();

	// For a duration equal to FlushTimeHardLimitSeconds, we wait for ongoing convaihttp requests to complete
	while (Requests.Num() > 0 && (FlushTimeHardLimitSeconds < 0 || (AppTime - BeginWaitTime < FlushTimeHardLimitSeconds)))
	{
		SCOPED_ENTER_BACKGROUND_EVENT(STAT_FConvaihttpManager_Flush_Iteration);

		// If time equal to FlushTimeSoftLimitSeconds has passed and there's still ongoing convaihttp requests, we cancel them (setting FlushTimeSoftLimitSeconds to 0 does this immediately)
		if (FlushTimeSoftLimitSeconds >= 0 && (AppTime - BeginWaitTime >= FlushTimeSoftLimitSeconds))
		{
			// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
			UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Canceling remaining %d CONVAIHTTP requests"), Requests.Num());

			for (TArray64<FConvaihttpRequestRef>::TIterator It(Requests); It; ++It)
			{
				FConvaihttpRequestRef& Request = *It;

				// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
				UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EConvaihttpRequestStatus::ToString(Request->GetStatus()));

				FScopedEnterBackgroundEvent(*Request->GetURL());

				Request->CancelRequest();
			}
		}

		// Process ongoing Convaihttp Requests
		FlushTick(AppTime - LastFlushTickTime);
		LastFlushTickTime = AppTime;

		// Process threaded Convaihttp Requests
		if (Requests.Num() > 0)
		{
			if (Thread)
			{
				if( Thread->NeedsSingleThreadTick() )
				{
					if (AppTime >= StallWarnTime)
					{
						// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
						UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Ticking CONVAIHTTPThread for %d outstanding Convaihttp requests."), Requests.Num());
						StallWarnTime = AppTime + 0.5;
					}
					Thread->Tick();
				}
				else
				{
					// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
					UE_CLOG(!IsRunningCommandlet(), LogConvaihttp, Warning, TEXT("Sleeping %.3fs to wait for %d outstanding Convaihttp requests."), SecondsToSleepForOutstandingThreadedRequests, Requests.Num());
					FPlatformProcess::Sleep(SecondsToSleepForOutstandingThreadedRequests);
				}
			}
			else
			{
				check(!FPlatformConvaihttp::UsesThreadedConvaihttp());
			}
		}

		AppTime = FPlatformTime::Seconds();
	}

	// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
	if (Requests.Num() > 0 && (FlushTimeHardLimitSeconds > 0 && (AppTime - BeginWaitTime > FlushTimeHardLimitSeconds)) && !IsRunningCommandlet())
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("HTTTManager::Flush exceeded hard limit time %.3fs. Current time is %.3fs. These requests are being abandoned without being flushed:"), FlushTimeHardLimitSeconds, AppTime - BeginWaitTime);
		for (TArray64<FConvaihttpRequestRef>::TIterator It(Requests); It; ++It)
		{
			FConvaihttpRequestRef& Request = *It;
			//List the outstanding requests that are being abandoned without being canceled.
			UE_LOG(LogConvaihttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EConvaihttpRequestStatus::ToString(Request->GetStatus()));
		}
	}

	bFlushing = false;
}

bool FConvaihttpManager::Tick(float DeltaSeconds)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpManager_Tick);

	// Run GameThread tasks
	TFunction<void()> Task = nullptr;
	while (GameThreadQueue.Dequeue(Task))
	{
		check(Task);
		Task();
	}

	FScopeLock ScopeLock(&RequestLock);

	// Tick each active request
	for (TArray64<FConvaihttpRequestRef>::TIterator It(Requests); It; ++It)
	{
		FConvaihttpRequestRef Request = *It;
		Request->Tick(DeltaSeconds);
	}

	if (Thread)
	{
		TArray64<IConvaihttpThreadedRequest*> CompletedThreadedRequests;
		Thread->GetCompletedRequests(CompletedThreadedRequests);

		// Finish and remove any completed requests
		for (IConvaihttpThreadedRequest* CompletedRequest : CompletedThreadedRequests)
		{
			FConvaihttpRequestRef CompletedRequestRef = CompletedRequest->AsShared();
			Requests.Remove(CompletedRequestRef);
			CompletedRequest->FinishRequest();
		}
	}
	// keep ticking
	return true;
}

void FConvaihttpManager::FlushTick(float DeltaSeconds)
{
	Tick(DeltaSeconds);
}

void FConvaihttpManager::AddRequest(const FConvaihttpRequestRef& Request)
{
	FScopeLock ScopeLock(&RequestLock);
	check(!bFlushing);
	Requests.Add(Request);
}

void FConvaihttpManager::RemoveRequest(const FConvaihttpRequestRef& Request)
{
	FScopeLock ScopeLock(&RequestLock);

	Requests.Remove(Request);
}

void FConvaihttpManager::AddThreadedRequest(const TSharedRef<IConvaihttpThreadedRequest, ESPMode::ThreadSafe>& Request)
{
	check(Thread);
	{
		FScopeLock ScopeLock(&RequestLock);
		check(!bFlushing);
		Requests.Add(Request);
	}
	Thread->AddRequest(&Request.Get());
}

void FConvaihttpManager::CancelThreadedRequest(const TSharedRef<IConvaihttpThreadedRequest, ESPMode::ThreadSafe>& Request)
{
	check(Thread);
	Thread->CancelRequest(&Request.Get());
}

bool FConvaihttpManager::IsValidRequest(const IConvaihttpRequest* RequestPtr) const
{
	FScopeLock ScopeLock(&RequestLock);

	bool bResult = false;
	for (const FConvaihttpRequestRef& Request : Requests)
	{
		if (&Request.Get() == RequestPtr)
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

void FConvaihttpManager::DumpRequests(FOutputDevice& Ar) const
{
	FScopeLock ScopeLock(&RequestLock);

	Ar.Logf(TEXT("------- (%d) Convaihttp Requests"), Requests.Num());
	for (const FConvaihttpRequestRef& Request : Requests)
	{
		Ar.Logf(TEXT("	verb=[%s] url=[%s] status=%s"),
			*Request->GetVerb(), *Request->GetURL(), EConvaihttpRequestStatus::ToString(Request->GetStatus()));
	}
}

bool FConvaihttpManager::SupportsDynamicProxy() const
{
	return false;
}
