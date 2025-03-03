// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpThread.h"
#include "IConvaihttpThreadedRequest.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Fork.h"
#include "Misc/Parse.h"
#include "ConvaihttpModule.h"
#include "Convaihttp.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("CONVAIHTTP Thread"), STATGROUP_CONVAIHTTPThread, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Process"), STAT_CONVAIHTTPThread_Process, STATGROUP_CONVAIHTTPThread);
DECLARE_CYCLE_STAT(TEXT("TickThreadedRequest"), STAT_CONVAIHTTPThread_TickThreadedRequest, STATGROUP_CONVAIHTTPThread);
DECLARE_CYCLE_STAT(TEXT("StartThreadedRequest"), STAT_CONVAIHTTPThread_StartThreadedRequest, STATGROUP_CONVAIHTTPThread);
DECLARE_CYCLE_STAT(TEXT("ConvaihttpThreadTick"), STAT_CONVAIHTTPThread_ConvaihttpThreadTick, STATGROUP_CONVAIHTTPThread);
DECLARE_CYCLE_STAT(TEXT("IsThreadedRequestComplete"), STAT_CONVAIHTTPThread_IsThreadedRequestComplete, STATGROUP_CONVAIHTTPThread);
DECLARE_CYCLE_STAT(TEXT("CompleteThreadedRequest"), STAT_CONVAIHTTPThread_CompleteThreadedRequest, STATGROUP_CONVAIHTTPThread);

// FConvaihttpThread

FConvaihttpThread::FConvaihttpThread()
	:	Thread(nullptr)
	,	bIsSingleThread(false)
	,	bIsStopped(true)
{
	ConvaihttpThreadActiveFrameTimeInSeconds = FConvaihttpModule::Get().GetConvaihttpThreadActiveFrameTimeInSeconds();
	ConvaihttpThreadActiveMinimumSleepTimeInSeconds = FConvaihttpModule::Get().GetConvaihttpThreadActiveMinimumSleepTimeInSeconds();
	ConvaihttpThreadIdleFrameTimeInSeconds = FConvaihttpModule::Get().GetConvaihttpThreadIdleFrameTimeInSeconds();
	ConvaihttpThreadIdleMinimumSleepTimeInSeconds = FConvaihttpModule::Get().GetConvaihttpThreadIdleMinimumSleepTimeInSeconds();

	UE_LOG(LogConvaihttp, Log, TEXT("CONVAIHTTP thread active frame time %.1f ms. Minimum active sleep time is %.1f ms. CONVAIHTTP thread idle frame time %.1f ms. Minimum idle sleep time is %.1f ms."), ConvaihttpThreadActiveFrameTimeInSeconds * 1000.0, ConvaihttpThreadActiveMinimumSleepTimeInSeconds * 1000.0, ConvaihttpThreadIdleFrameTimeInSeconds * 1000.0, ConvaihttpThreadIdleMinimumSleepTimeInSeconds * 1000.0);
}

FConvaihttpThread::~FConvaihttpThread()
{
	StopThread();
}

void FConvaihttpThread::StartThread()
{
	bIsSingleThread = false;

	const bool bDisableForkedCONVAIHTTPThread = FParse::Param(FCommandLine::Get(), TEXT("DisableForkedCONVAIHTTPThread"));

	if (FForkProcessHelper::IsForkedMultithreadInstance() && bDisableForkedCONVAIHTTPThread == false)
	{
		// We only create forkable threads on the forked instance since the CONVAIHTTPManager cannot safely transition from fake to real seamlessly
		Thread = FForkProcessHelper::CreateForkableThread(this, TEXT("ConvaihttpManagerThread"), 128 * 1024, TPri_Normal);
	}
	else
	{
		// If the runnable thread is fake.
		if (FGenericPlatformProcess::SupportsMultithreading() == false)
		{
			bIsSingleThread = true;
		}

		Thread = FRunnableThread::Create(this, TEXT("ConvaihttpManagerThread"), 128 * 1024, TPri_Normal);
	}

	bIsStopped = false;
}

void FConvaihttpThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	bIsStopped = true;
	bIsSingleThread = true;
}

void FConvaihttpThread::AddRequest(IConvaihttpThreadedRequest* Request)
{
	NewThreadedRequests.Enqueue(Request);
}

void FConvaihttpThread::CancelRequest(IConvaihttpThreadedRequest* Request)
{
	CancelledThreadedRequests.Enqueue(Request);
}

void FConvaihttpThread::GetCompletedRequests(TArray64<IConvaihttpThreadedRequest*>& OutCompletedRequests)
{
	check(IsInGameThread());
	IConvaihttpThreadedRequest* Request = nullptr;
	while (CompletedThreadedRequests.Dequeue(Request))
	{
		OutCompletedRequests.Add(Request);
	}
}

bool FConvaihttpThread::Init()
{
	LastTime = FPlatformTime::Seconds();
	ExitRequest.Set(false);

	UpdateConfigs();

	return true;
}

uint32 FConvaihttpThread::Run()
{
	// Arrays declared outside of loop to re-use memory
	TArray64<IConvaihttpThreadedRequest*> RequestsToCancel;
	TArray64<IConvaihttpThreadedRequest*> RequestsToComplete;
	while (!ExitRequest.GetValue())
	{
		if (ensureMsgf(!bIsSingleThread, TEXT("CONVAIHTTP Thread was set to singlethread mode while it was running autonomously!")))
		{
			const double OuterLoopBegin = FPlatformTime::Seconds();
			double OuterLoopEnd = 0.0;
			bool bKeepProcessing = true;
			while (bKeepProcessing)
			{
				const double InnerLoopBegin = FPlatformTime::Seconds();
			
				Process(RequestsToCancel, RequestsToComplete);
			
				if (RunningThreadedRequests.Num() == 0)
				{
					bKeepProcessing = false;
				}

				const double InnerLoopEnd = FPlatformTime::Seconds();
				if (bKeepProcessing)
				{
					double InnerLoopTime = InnerLoopEnd - InnerLoopBegin;
					double InnerSleep = FMath::Max(ConvaihttpThreadActiveFrameTimeInSeconds - InnerLoopTime, ConvaihttpThreadActiveMinimumSleepTimeInSeconds);
					FPlatformProcess::SleepNoStats(InnerSleep);
				}
				else
				{
					OuterLoopEnd = InnerLoopEnd;
				}
			}
			double OuterLoopTime = OuterLoopEnd - OuterLoopBegin;
			double OuterSleep = FMath::Max(ConvaihttpThreadIdleFrameTimeInSeconds - OuterLoopTime, ConvaihttpThreadIdleMinimumSleepTimeInSeconds);
			FPlatformProcess::SleepNoStats(OuterSleep);
		}
		else
		{
			break;
		}
	}
	return 0;
}


void FConvaihttpThread::Tick()
{
	if (ensure(bIsSingleThread))
	{
		TArray64<IConvaihttpThreadedRequest*> RequestsToCancel;
		TArray64<IConvaihttpThreadedRequest*> RequestsToComplete;
		Process(RequestsToCancel, RequestsToComplete);
	}
}

bool FConvaihttpThread::NeedsSingleThreadTick() const
{
	return bIsSingleThread;
}

void FConvaihttpThread::UpdateConfigs()
{
	GConfig->GetInt(TEXT("CONVAIHTTP.ConvaihttpThread"), TEXT("RunningThreadedRequestLimit"), RunningThreadedRequestLimit, GEngineIni);
	if (RunningThreadedRequestLimit < 1)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("RunningThreadedRequestLimit must be configured as a number greater than 0. Current value is %d."), RunningThreadedRequestLimit);
		RunningThreadedRequestLimit = INT_MAX;
	}
}

void FConvaihttpThread::ConvaihttpThreadTick(float DeltaSeconds)
{
	// empty
}

bool FConvaihttpThread::StartThreadedRequest(IConvaihttpThreadedRequest* Request)
{
	return Request->StartThreadedRequest();
}

void FConvaihttpThread::CompleteThreadedRequest(IConvaihttpThreadedRequest* Request)
{
	// empty
}

void FConvaihttpThread::Process(TArray64<IConvaihttpThreadedRequest*>& RequestsToCancel, TArray64<IConvaihttpThreadedRequest*>& RequestsToComplete)
{
	SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_Process);

	// cache all cancelled and new requests
	{
		IConvaihttpThreadedRequest* Request = nullptr;

		RequestsToCancel.Reset();
		while (CancelledThreadedRequests.Dequeue(Request))
		{
			RequestsToCancel.Add(Request);
		}

		while (NewThreadedRequests.Dequeue(Request))
		{
			RateLimitedThreadedRequests.Add(Request);
		}
	}

	// Cancel any pending cancel requests
	for (IConvaihttpThreadedRequest* Request : RequestsToCancel)
	{
		if (RunningThreadedRequests.Remove(Request) > 0)
		{
			RequestsToComplete.AddUnique(Request);
		}
		else if (RateLimitedThreadedRequests.Remove(Request) > 0)
		{
			RequestsToComplete.AddUnique(Request);
		}
		else
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Unable to find request (%p) in ConvaihttpThread"), Request);
		}
	}

	const double AppTime = FPlatformTime::Seconds();
	const double ElapsedTime = AppTime - LastTime;
	LastTime = AppTime;

	// Tick any running requests
	// as long as they properly finish in ConvaihttpThreadTick below they are unaffected by a possibly large ElapsedTime above
	for (IConvaihttpThreadedRequest* Request : RunningThreadedRequests)
	{
		SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_TickThreadedRequest);

		Request->TickThreadedRequest(ElapsedTime);
	}

	// We'll start rate limited requests until we hit the limit
	// Tick new requests separately from existing RunningThreadedRequests so they get a chance 
	// to send unaffected by possibly large ElapsedTime above
	int32 RunningThreadedRequestsCounter = RunningThreadedRequests.Num();
	if (RunningThreadedRequestsCounter < RunningThreadedRequestLimit)
	{
		while(RunningThreadedRequestsCounter < RunningThreadedRequestLimit && RateLimitedThreadedRequests.Num())
		{
			SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_StartThreadedRequest);

			IConvaihttpThreadedRequest* ReadyThreadedRequest = RateLimitedThreadedRequests[0];
			RateLimitedThreadedRequests.RemoveAt(0);

			if (StartThreadedRequest(ReadyThreadedRequest))
			{
				RunningThreadedRequestsCounter++;
				RunningThreadedRequests.Add(ReadyThreadedRequest);
				ReadyThreadedRequest->TickThreadedRequest(0.0f);
				UE_LOG(LogConvaihttp, Verbose, TEXT("Started running threaded request (%p). Running threaded requests (%d) Rate limited threaded requests (%d)"), ReadyThreadedRequest, RunningThreadedRequests.Num(), RateLimitedThreadedRequests.Num());
			}
			else
			{
				RequestsToComplete.AddUnique(ReadyThreadedRequest);
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_ConvaihttpThreadTick);

		// Every valid request in RunningThreadedRequests gets at least two calls to ConvaihttpThreadTick
		// Blocking loads still can affect things if the network stack can't keep its connections alive
		ConvaihttpThreadTick(ElapsedTime);
	}

	// Move any completed requests
	for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
	{
		SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_IsThreadedRequestComplete);

		IConvaihttpThreadedRequest* Request = RunningThreadedRequests[Index];

		if (Request->IsThreadedRequestComplete())
		{
			RequestsToComplete.AddUnique(Request);
			RunningThreadedRequests.RemoveAtSwap(Index);
			--Index;
			UE_LOG(LogConvaihttp, Verbose, TEXT("Threaded request (%p) completed. Running threaded requests (%d)"), Request, RunningThreadedRequests.Num());
		}
	}

	if (RequestsToComplete.Num() > 0)
	{
		for (IConvaihttpThreadedRequest* Request : RequestsToComplete)
		{
			SCOPE_CYCLE_COUNTER(STAT_CONVAIHTTPThread_CompleteThreadedRequest);

			CompleteThreadedRequest(Request);
			CompletedThreadedRequests.Enqueue(Request);
		}
		RequestsToComplete.Reset();
	}
}

void FConvaihttpThread::Stop()
{
	ExitRequest.Set(true);
}
	
void FConvaihttpThread::Exit()
{
	// empty
}
