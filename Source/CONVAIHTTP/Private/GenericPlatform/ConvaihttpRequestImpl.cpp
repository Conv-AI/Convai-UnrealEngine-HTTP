// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/ConvaihttpRequestImpl.h"
#include "Stats/Stats.h"
#include "Convaihttp.h"

FConvaihttpRequestCompleteDelegate& FConvaihttpRequestImpl::OnProcessRequestComplete()
{
	UE_LOG(LogConvaihttp, VeryVerbose, TEXT("FConvaihttpRequestImpl::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FConvaihttpRequestProgressDelegate& FConvaihttpRequestImpl::OnRequestProgress() 
{
	UE_LOG(LogConvaihttp, VeryVerbose, TEXT("FConvaihttpRequestImpl::OnRequestProgress()"));
	return RequestProgressDelegate;
}

FConvaihttpRequestHeaderReceivedDelegate& FConvaihttpRequestImpl::OnHeaderReceived()
{
	UE_LOG(LogConvaihttp, VeryVerbose, TEXT("FConvaihttpRequestImpl::OnHeaderReceived()"));
	return HeaderReceivedDelegate;
}

FConvaihttpRequestWillRetryDelegate& FConvaihttpRequestImpl::OnRequestWillRetry()
{
	UE_LOG(LogConvaihttp, VeryVerbose, TEXT("FConvaihttpRequestImpl::OnRequestWillRetry()"));
	return OnRequestWillRetryDelegate;
}

void FConvaihttpRequestImpl::SetTimeout(float InTimeoutSecs)
{
	TimeoutSecs = InTimeoutSecs;
}

void FConvaihttpRequestImpl::ClearTimeout()
{
	TimeoutSecs.Reset();
}

TOptional<float> FConvaihttpRequestImpl::GetTimeout() const
{
	return TimeoutSecs;
}

float FConvaihttpRequestImpl::GetTimeoutOrDefault() const
{
	return GetTimeout().Get(FConvaihttpModule::Get().GetConvaihttpTimeout());
}

void FConvaihttpRequestImpl::BroadcastResponseHeadersReceived()
{
	if (OnHeaderReceived().IsBound())
	{
		const FConvaihttpResponsePtr Response = GetResponse();
		if (Response.IsValid())
		{
			const FConvaihttpRequestPtr ThisPtr(SharedThis(this));
			const TArray64<FString> AllHeaders = Response->GetAllHeaders();
			for (const FString& Header : AllHeaders)
			{
				FString HeaderName;
				FString HeaderValue;
				if (Header.Split(TEXT(":"), &HeaderName, &HeaderValue))
				{
					HeaderValue.TrimStartInline();

					QUICK_SCOPE_CYCLE_COUNTER(STAT_FConvaihttpRequestImpl_BroadcastResponseHeadersReceived_OnHeaderReceived);
					OnHeaderReceived().ExecuteIfBound(ThisPtr, HeaderName, HeaderValue);
				}
			}
		}
	}
}
