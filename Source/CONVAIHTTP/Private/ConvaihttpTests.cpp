// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpTests.h"
#include "ConvaihttpModule.h"
#include "Convaihttp.h"

// FConvaihttpTest

FConvaihttpTest::FConvaihttpTest(const FString& InVerb, const FString& InPayload, const FString& InUrl, int32 InIterations)
	: Verb(InVerb)
	, Payload(InPayload)
	, Url(InUrl)
	, TestsToRun(InIterations)
{
	
}

void FConvaihttpTest::Run(void)
{
	UE_LOG(LogConvaihttp, Log, TEXT("Starting test [%s] Url=[%s]"), 
		*Verb, *Url);

	for (int Idx=0; Idx < TestsToRun; Idx++)
	{
		TSharedPtr<IConvaihttpRequest, ESPMode::ThreadSafe> Request = FConvaihttpModule::Get().CreateRequest();
		Request->OnProcessRequestComplete().BindRaw(this, &FConvaihttpTest::RequestComplete);
		Request->SetURL(Url);
		if (Payload.Len() > 0)
		{
			Request->SetContentAsString(Payload);
		}
		Request->SetVerb(Verb);
		Request->ProcessRequest();
	}
}

void FConvaihttpTest::RequestComplete(FConvaihttpRequestPtr ConvaihttpRequest, FConvaihttpResponsePtr ConvaihttpResponse, bool bSucceeded)
{
	if (!ConvaihttpResponse.IsValid())
	{
		UE_LOG(LogConvaihttp, Log, TEXT("Test failed. NULL response"));
	}
	else
	{
		UE_LOG(LogConvaihttp, Log, TEXT("Completed test [%s] Url=[%s] Response=[%d] [%s]"), 
			*ConvaihttpRequest->GetVerb(), 
			*ConvaihttpRequest->GetURL(), 
			ConvaihttpResponse->GetResponseCode(), 
			*ConvaihttpResponse->GetContentAsString());
	}
	
	if ((--TestsToRun) <= 0)
	{
		ConvaihttpRequest->OnProcessRequestComplete().Unbind();
		// Done with the test. Delegate should always gets called
		delete this;
	}
}

