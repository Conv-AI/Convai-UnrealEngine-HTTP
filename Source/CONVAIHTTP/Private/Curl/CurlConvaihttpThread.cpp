// Copyright Epic Games, Inc. All Rights Reserved.

#include "Curl/CurlConvaihttpThread.h"
#include "Stats/Stats.h"
#include "Convaihttp.h"
#include "Curl/CurlConvaihttp.h"
#include "Curl/CurlConvaihttpManager.h"

#if WITH_CURL

FCurlConvaihttpThread::FCurlConvaihttpThread()
{
}

void FCurlConvaihttpThread::ConvaihttpThreadTick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlConvaihttpThread_ConvaihttpThreadTick);
	check(FCurlConvaihttpManager::IsInit());

	if (RunningThreadedRequests.Num() > 0)
	{
		int RunningRequests = -1;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlConvaihttpThread_ConvaihttpThreadTick_Perform);
			curl_multi_perform(FCurlConvaihttpManager::GMultiHandle, &RunningRequests);
		}

		// read more info if number of requests changed or if there's zero running
		// (note that some requests might have never be "running" from libcurl's point of view)
		if (RunningRequests == 0 || RunningRequests != RunningThreadedRequests.Num())
		{
			for (;;)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlConvaihttpThread_ConvaihttpThreadTick_Loop);
				int MsgsStillInQueue = 0;	// may use that to impose some upper limit we may spend in that loop
				CURLMsg * Message = curl_multi_info_read(FCurlConvaihttpManager::GMultiHandle, &MsgsStillInQueue);

				if (Message == NULL)
				{
					break;
				}

				// find out which requests have completed
				if (Message->msg == CURLMSG_DONE)
				{
					CURL* CompletedHandle = Message->easy_handle;
					curl_multi_remove_handle(FCurlConvaihttpManager::GMultiHandle, CompletedHandle);

					IConvaihttpThreadedRequest** Request = HandlesToRequests.Find(CompletedHandle);
					if (Request)
					{
						FCurlConvaihttpRequest* CurlRequest = static_cast<FCurlConvaihttpRequest*>(*Request);
						CurlRequest->MarkAsCompleted(Message->data.result);

						UE_LOG(LogConvaihttp, Verbose, TEXT("Request %p (easy handle:%p) has completed (code:%d) and has been marked as such"), CurlRequest, CompletedHandle, (int32)Message->data.result);

						HandlesToRequests.Remove(CompletedHandle);
					}
					else
					{
						UE_LOG(LogConvaihttp, Warning, TEXT("Could not find mapping for completed request (easy handle: %p)"), CompletedHandle);
					}
				}
			}
		}
	}

	FConvaihttpThread::ConvaihttpThreadTick(DeltaSeconds);
}

bool FCurlConvaihttpThread::StartThreadedRequest(IConvaihttpThreadedRequest* Request)
{
	FCurlConvaihttpRequest* CurlRequest = static_cast<FCurlConvaihttpRequest*>(Request);
	CURL* EasyHandle = CurlRequest->GetEasyHandle();
	ensure(!HandlesToRequests.Contains(EasyHandle));

	if (!CurlRequest->SetupRequestConvaihttpThread())
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Could not set libcurl options for easy handle, processing CONVAIHTTP request failed. Increase verbosity for additional information."));
		return false;
	}

	CURLMcode AddResult = curl_multi_add_handle(FCurlConvaihttpManager::GMultiHandle, EasyHandle);
	CurlRequest->SetAddToCurlMultiResult(AddResult);

	if (AddResult != CURLM_OK)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Failed to add easy handle %p to multi handle with code %d"), EasyHandle, (int)AddResult);
		return false;
	}

	HandlesToRequests.Add(EasyHandle, Request);

	return FConvaihttpThread::StartThreadedRequest(Request);
}

void FCurlConvaihttpThread::CompleteThreadedRequest(IConvaihttpThreadedRequest* Request)
{
	FCurlConvaihttpRequest* CurlRequest = static_cast<FCurlConvaihttpRequest*>(Request);
	CURL* EasyHandle = CurlRequest->GetEasyHandle();

	if (HandlesToRequests.Find(EasyHandle))
	{
		curl_multi_remove_handle(FCurlConvaihttpManager::GMultiHandle, EasyHandle);
		HandlesToRequests.Remove(EasyHandle);
	}
}

#endif
