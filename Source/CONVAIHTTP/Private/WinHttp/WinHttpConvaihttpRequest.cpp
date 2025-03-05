// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpConvaihttpRequest.h"
#include "WinHttp/WinHttpConvaihttpManager.h"
#include "WinHttp/WinHttpConvaihttpResponse.h"
#include "WinHttp/Support/WinHttpConnectionConvaihttp.h"
#include "GenericPlatform/ConvaihttpRequestPayload.h"
#include "Convaihttp.h"
#include "ConvaihttpModule.h"

#include "HAL/PlatformTime.h"
#include "Containers/StringView.h"
#include "HAL/FileManager.h"

FCH_WinHttpConvaihttpRequest::FCH_WinHttpConvaihttpRequest()
{

}

FCH_WinHttpConvaihttpRequest::~FCH_WinHttpConvaihttpRequest()
{
	// Make sure we either didn't start, or we finished before destructing
	check(!RequestStartTimeSeconds.IsSet() || RequestFinishTimeSeconds.IsSet());
}

FString FCH_WinHttpConvaihttpRequest::GetURL() const
{
	return RequestData.Url;
}

FString FCH_WinHttpConvaihttpRequest::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;

	if (TOptional<FString> OptionalParameterValue = FGenericPlatformConvaihttp::GetUrlParameter(RequestData.Url, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}

	return ReturnValue;
}

FString FCH_WinHttpConvaihttpRequest::GetHeader(const FString& HeaderName) const
{
	const FString* const ExistingHeader = RequestData.Headers.Find(HeaderName);
	return ExistingHeader ? *ExistingHeader : FString();
}

TArray64<FString> FCH_WinHttpConvaihttpRequest::GetAllHeaders() const
{
	TArray64<FString> AllHeaders;

	for (const TPair<FString, FString>& Header : RequestData.Headers)
	{
		AllHeaders.Add(FString::Printf(TEXT("%s: %s"), *Header.Key, *Header.Value));
	}

	return AllHeaders;
}
	
FString FCH_WinHttpConvaihttpRequest::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

uint64 FCH_WinHttpConvaihttpRequest::GetContentLength() const
{
	return RequestData.Payload.IsValid() ? RequestData.Payload->GetContentLength() : 0;
}

const TArray64<uint8>& FCH_WinHttpConvaihttpRequest::GetContent() const
{
	static const TArray64<uint8> EmptyContent;
	return RequestData.Payload.IsValid() ? RequestData.Payload->GetContent() : EmptyContent;
}

FString FCH_WinHttpConvaihttpRequest::GetVerb() const
{
	return RequestData.Verb;
}

void FCH_WinHttpConvaihttpRequest::SetVerb(const FString& InVerb)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set verb on a request that is inflight"));
		return;
	}

	RequestData.Verb = InVerb.ToUpper();
}

void FCH_WinHttpConvaihttpRequest::SetURL(const FString& InURL)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set URL on a request that is inflight"));
		return;
	}

	RequestData.Url = InURL;
}

void FCH_WinHttpConvaihttpRequest::SetContent(const TArray64<uint8>& ContentPayload)
{
	SetContent(CopyTemp(ContentPayload));
}

void FCH_WinHttpConvaihttpRequest::SetContent(TArray64<uint8>&& ContentPayload)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return;
	}

	RequestData.Payload = MakeShared<FCH_RequestPayloadInMemory, ESPMode::ThreadSafe>(MoveTemp(ContentPayload));
}

void FCH_WinHttpConvaihttpRequest::SetContentAsString(const FString& ContentString)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return;
	}

	const FTCHARToUTF8 Converter(*ContentString, ContentString.Len());

	TArray64<uint8> Content;
	Content.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());

	RequestData.Payload = MakeShared<FCH_RequestPayloadInMemory, ESPMode::ThreadSafe>(MoveTemp(Content));
}

bool FCH_WinHttpConvaihttpRequest::SetContentAsStreamedFile(const FString& Filename)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return false;
	}

	if (FArchive* File = IFileManager::Get().CreateFileReader(*Filename))
	{
		RequestData.Payload = MakeShared<FCH_RequestPayloadInFileStream, ESPMode::ThreadSafe>(MakeShareable(File));
		return true;
	}
	else
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Failed to open '%s' for reading"), *Filename);
		RequestData.Payload.Reset();
		return false;
	}
}

bool FCH_WinHttpConvaihttpRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return false;
	}

	RequestData.Payload = MakeShared<FCH_RequestPayloadInFileStream, ESPMode::ThreadSafe>(Stream);
	return true;
}

void FCH_WinHttpConvaihttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set a header on a request that is inflight"));
		return;
	}

	if (HeaderName.IsEmpty())
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to set an empty header name"));
		return;
	}

	RequestData.Headers.Add(HeaderName, HeaderValue);
}

void FCH_WinHttpConvaihttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to append a header on a request that is inflight"));
		return;
	}
	
	if (HeaderName.IsEmpty())
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to append an empty header name"));
		return;
	}

	if (const FString* ExistingHeaderValue = RequestData.Headers.Find(HeaderName))
	{
		RequestData.Headers.Add(HeaderName, FString::Printf(TEXT("%s, %s"), **ExistingHeaderValue, *AdditionalHeaderValue));
	}
	else
	{
		RequestData.Headers.Add(HeaderName, AdditionalHeaderValue);
	}
}

bool FCH_WinHttpConvaihttpRequest::ProcessRequest()
{
	UE_LOG(LogConvaihttp, Verbose, TEXT("FCH_WinHttpConvaihttpRequest::ProcessRequest() FCH_WinHttpConvaihttpRequest=[%p]"), this);

	if (State == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to start request while it is still in inflight"));
		return false;
	}

	FCH_WinHttpConvaihttpManager* ConvaihttpManager = FCH_WinHttpConvaihttpManager::GetManager();
	if (!ConvaihttpManager)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to start request with no CONVAIHTTP manager"));
		return false;
	}

	Response.Reset();
	TotalBytesSent = 0;
	TotalBytesReceived = 0;
	RequestStartTimeSeconds.Reset();
	RequestFinishTimeSeconds.Reset();
	bRequestCancelled = false;

	CompletionStatus = EConvaihttpRequestStatus::Processing;
	State = EConvaihttpRequestStatus::Processing;

	TSharedRef<FCH_WinHttpConvaihttpRequest, ESPMode::ThreadSafe> LocalStrongThis = StaticCastSharedRef<FCH_WinHttpConvaihttpRequest>(AsShared());
	ConvaihttpManager->QuerySessionForUrl(RequestData.Url, FCH_WinHttpQuerySessionComplete::CreateLambda([LocalWeakThis = TWeakPtr<FCH_WinHttpConvaihttpRequest, ESPMode::ThreadSafe>(LocalStrongThis)](FCH_WinHttpSession* SessionPtr)
	{
		// Validate state
		TSharedPtr<FCH_WinHttpConvaihttpRequest, ESPMode::ThreadSafe> StrongThis = LocalWeakThis.Pin();
		if (!StrongThis.IsValid())
		{
			// We went away
			return;
		}
		if (StrongThis->bRequestCancelled)
		{
			// We were cancelled
			return;
		}
		if (!SessionPtr)
		{
			// Could not create session
			UE_LOG(LogConvaihttp, Warning, TEXT("Unable to create WinHttp Session, failing request"));
			StrongThis->OnWinHttpRequestComplete();
			return;
		}

		FCH_WinHttpConvaihttpRequestData& LocalRequestData = StrongThis->RequestData;

		// Create connection object
		TSharedPtr<FCH_WinHttpConnectionConvaihttp, ESPMode::ThreadSafe> LocalConnection = FCH_WinHttpConnectionConvaihttp::CreateConvaihttpConnection(*SessionPtr, LocalRequestData.Verb, LocalRequestData.Url, LocalRequestData.Headers, LocalRequestData.Payload);
		if (!LocalConnection.IsValid())
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Unable to create WinHttp Connection, failing request"));
			StrongThis->OnWinHttpRequestComplete();
			return;
		}

		// Bind listeners
		TSharedRef<FCH_WinHttpConvaihttpRequest, ESPMode::ThreadSafe> StrongThisRef = StrongThis.ToSharedRef();
		LocalConnection->SetDataTransferredHandler(FCH_WinHttpConnectionConvaihttpOnDataTransferred::CreateThreadSafeSP(StrongThisRef, &FCH_WinHttpConvaihttpRequest::HandleDataTransferred));
		LocalConnection->SetHeaderReceivedHandler(FCH_WinHttpConnectionConvaihttpOnHeaderReceived::CreateThreadSafeSP(StrongThisRef, &FCH_WinHttpConvaihttpRequest::HandleHeaderReceived));
		LocalConnection->SetRequestCompletedHandler(FCH_WinHttpConnectionConvaihttpOnRequestComplete::CreateThreadSafeSP(StrongThisRef, &FCH_WinHttpConvaihttpRequest::HandleRequestComplete));

		// Start request!
		StrongThisRef->RequestStartTimeSeconds = FPlatformTime::Seconds();
		if (!LocalConnection->StartRequest())
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Unable to start WinHttp Connection, failing request"));
			StrongThisRef->OnWinHttpRequestComplete();
			return;
		}

		// Save object
		StrongThisRef->Connection = MoveTemp(LocalConnection);
	}));

	// Store our request so it doesn't die if the requester doesn't store it (common use case)
	FConvaihttpModule::Get().GetConvaihttpManager().AddThreadedRequest(LocalStrongThis);
	return true;
}

void FCH_WinHttpConvaihttpRequest::CancelRequest()
{
	UE_LOG(LogConvaihttp, Log, TEXT("FCH_WinHttpConvaihttpRequest::CancelRequest() FCH_WinHttpConvaihttpRequest=[%p]"), this);

	if (EConvaihttpRequestStatus::IsFinished(State))
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to cancel a request that was already finished"));
		return;
	}
	if (bRequestCancelled)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Attempted to cancel a request that was already cancelled"));
		return;
	}

	// FinishRequest will cleanup connection
	bRequestCancelled = true;

	FConvaihttpManager& ConvaihttpManager = FConvaihttpModule::Get().GetConvaihttpManager();
	if (ConvaihttpManager.IsValidRequest(this))
	{
		ConvaihttpManager.CancelThreadedRequest(SharedThis(this));
	}
	else if (!IsInGameThread())
	{
		// Always finish on the game thread
		FConvaihttpModule::Get().GetConvaihttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FCH_WinHttpConvaihttpRequest>(AsShared())]()
		{
			StrongThis->FinishRequest();
		});
	}
	else
	{
		FinishRequest();
	}
}

EConvaihttpRequestStatus::Type FCH_WinHttpConvaihttpRequest::GetStatus() const
{
	return CompletionStatus;
}

const FConvaihttpResponsePtr FCH_WinHttpConvaihttpRequest::GetResponse() const
{
	return Response;
}

void FCH_WinHttpConvaihttpRequest::Tick(float DeltaSeconds)
{
	if (Connection.IsValid())
	{
		Connection->PumpMessages();
		// Connection is not guaranteed to be valid anymore here, be sure to check again if it gets used again below
	}
}

float FCH_WinHttpConvaihttpRequest::GetElapsedTime() const
{
	if (!RequestStartTimeSeconds.IsSet())
	{
		// Request hasn't started
		return 0.0f;
	}

	if (RequestFinishTimeSeconds.IsSet())
	{
		// Request finished
		return RequestFinishTimeSeconds.GetValue() - RequestStartTimeSeconds.GetValue();
	}

	// Request still in progress
	return FPlatformTime::Seconds() - RequestStartTimeSeconds.GetValue();
}

bool FCH_WinHttpConvaihttpRequest::StartThreadedRequest()
{
	// No-op, our request is already started
	return true;
}

bool FCH_WinHttpConvaihttpRequest::IsThreadedRequestComplete()
{
	if (bRequestCancelled)
	{
		return true;
	}
	return EConvaihttpRequestStatus::IsFinished(State);
}

void FCH_WinHttpConvaihttpRequest::TickThreadedRequest(float DeltaSeconds)
{
	TSharedPtr<FCH_WinHttpConnectionConvaihttp, ESPMode::ThreadSafe> LocalConnection = Connection;
	if (LocalConnection.IsValid())
	{
		LocalConnection->PumpStates();
		// Connection is not guaranteed to be valid anymore here, be sure to check again if it gets used again below
	}
}

void FCH_WinHttpConvaihttpRequest::OnWinHttpRequestComplete()
{
	if (RequestFinishTimeSeconds.IsSet())
	{
		// Already finished
		return;
	}
	RequestFinishTimeSeconds = FPlatformTime::Seconds();

	// Set our final state if it's not set yet
	if (!EConvaihttpRequestStatus::IsFinished(State))
	{
		State = EConvaihttpRequestStatus::Failed;
	}
}

void FCH_WinHttpConvaihttpRequest::FinishRequest()
{
	check(IsInGameThread());
	check(IsThreadedRequestComplete());

	// If we were cancelled, set our finished time
	if (bRequestCancelled && !RequestFinishTimeSeconds.IsSet())
	{
		RequestFinishTimeSeconds = FPlatformTime::Seconds();
	}

	// Shutdown our connection
	if (Connection.IsValid())
	{
		if (!Connection->IsComplete())
		{
			Connection->CancelRequest();
		}
		Connection.Reset();
	}

	CompletionStatus = State;

	TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
	OnProcessRequestComplete().ExecuteIfBound(KeepAlive, Response, Response.IsValid());
}

void FCH_WinHttpConvaihttpRequest::HandleDataTransferred(int32 BytesSent, int32 BytesReceived)
{
	check(IsInGameThread());

	if (BytesSent > 0 || BytesReceived > 0)
	{
		if (BytesReceived > 0)
		{
			UpdateResponseBody();
		}
		TotalBytesSent += BytesSent;
		TotalBytesReceived += BytesReceived;
		TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
		OnRequestProgress().ExecuteIfBound(AsShared(), TotalBytesSent, TotalBytesReceived);
	}
}

void FCH_WinHttpConvaihttpRequest::HandleHeaderReceived(const FString& HeaderKey, const FString& HeaderValue)
{
	check(IsInGameThread());

	if (Response.IsValid())
	{
		Response->AppendHeader(HeaderKey, HeaderValue);
	}
	else if (Connection.IsValid())
	{
		Response = MakeShared<FCH_WinHttpConvaihttpResponse, ESPMode::ThreadSafe>(RequestData.Url, Connection->GetResponseCode(), CopyTemp(Connection->GetHeadersReceived()), TArray64<uint8>());
	}

	TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
	OnHeaderReceived().ExecuteIfBound(AsShared(), HeaderKey, HeaderValue);
}

void FCH_WinHttpConvaihttpRequest::HandleRequestComplete(EConvaihttpRequestStatus::Type RequestCompletionStatus)
{
	check(IsInGameThread());
	check(EConvaihttpRequestStatus::IsFinished(RequestCompletionStatus));

	State = RequestCompletionStatus;

	if (RequestCompletionStatus == EConvaihttpRequestStatus::Succeeded)
	{
		UpdateResponseBody(true);
	}

	OnWinHttpRequestComplete();
}

void FCH_WinHttpConvaihttpRequest::UpdateResponseBody(bool bForceResponseExist)
{
	if (Connection.IsValid())
	{
		TArray64<uint8> NewChunk(MoveTemp(Connection->GetLastChunk()));
		if (NewChunk.Num() > 0 || bForceResponseExist)
		{
			if (Response.IsValid())
			{
				if (NewChunk.Num() > 0)
				{
					Response->AppendPayload(NewChunk);
				}
			}
			else
			{
				Response = MakeShared<FCH_WinHttpConvaihttpResponse, ESPMode::ThreadSafe>(RequestData.Url, Connection->GetResponseCode(), CopyTemp(Connection->GetHeadersReceived()), MoveTemp(NewChunk));
			}
		}
	}
}

#endif // WITH_WINHTTP
