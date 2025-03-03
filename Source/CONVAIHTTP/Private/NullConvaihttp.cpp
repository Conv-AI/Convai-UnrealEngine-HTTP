// Copyright Epic Games, Inc. All Rights Reserved.

#include "NullConvaihttp.h"
#include "ConvaihttpManager.h"
#include "ConvaihttpModule.h"
#include "Convaihttp.h"

// FNullConvaihttpRequest

FString FNullConvaihttpRequest::GetURL() const
{
	return Url;
}

FString FNullConvaihttpRequest::GetURLParameter(const FString& ParameterName) const
{
	return FString();
}

FString FNullConvaihttpRequest::GetHeader(const FString& HeaderName) const
{
	const FString* Header = Headers.Find(HeaderName);
	if (Header != NULL)
	{
		return *Header;
	}
	return FString();
}

TArray64<FString> FNullConvaihttpRequest::GetAllHeaders() const
{
	TArray64<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FNullConvaihttpRequest::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int64 FNullConvaihttpRequest::GetContentLength() const
{
	return Payload.Num();
}

const TArray64<uint8>& FNullConvaihttpRequest::GetContent() const
{
	return Payload;
}

FString FNullConvaihttpRequest::GetVerb() const
{
	return Verb;
}

void FNullConvaihttpRequest::SetVerb(const FString& InVerb)
{
	Verb = InVerb;
}

void FNullConvaihttpRequest::SetURL(const FString& InURL)
{
	Url = InURL;
}

void FNullConvaihttpRequest::SetContent(const TArray64<uint8>& ContentPayload)
{
	Payload = ContentPayload;
}

void FNullConvaihttpRequest::SetContent(TArray64<uint8>&& ContentPayload)
{
	Payload = MoveTemp(ContentPayload);
}

void FNullConvaihttpRequest::SetContentAsString(const FString& ContentString)
{
	int32 Utf8Length = FTCHARToUTF8_Convert::ConvertedLength(*ContentString, ContentString.Len());
	Payload.SetNumUninitialized(Utf8Length);
	FTCHARToUTF8_Convert::Convert((UTF8CHAR*)Payload.GetData(), Payload.Num(), *ContentString, ContentString.Len());
}

bool FNullConvaihttpRequest::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogConvaihttp, Warning, TEXT("FNullConvaihttpRequest::SetContentAsStreamedFile is not implemented"));
	return false;
}

bool FNullConvaihttpRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	// TODO: Not implemented.
	UE_LOG(LogConvaihttp, Warning, TEXT("FNullConvaihttpRequest::SetContentFromStream is not implemented"));
	return false;
}

void FNullConvaihttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	Headers.Add(HeaderName, HeaderValue);
}

void FNullConvaihttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (HeaderName.Len() > 0 && AdditionalHeaderValue.Len() > 0)
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != NULL && PreviousValue->Len() > 0)
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

bool FNullConvaihttpRequest::ProcessRequest()
{
	ElapsedTime = 0;
	CompletionStatus = EConvaihttpRequestStatus::Processing;

	UE_LOG(LogConvaihttp, Log, TEXT("Start request. %p %s url=%s"), this, *GetVerb(), *GetURL());

	FConvaihttpModule::Get().GetConvaihttpManager().AddRequest(SharedThis(this));
	return true;
}

void FNullConvaihttpRequest::CancelRequest()
{
	if (!IsInGameThread())
	{
		FConvaihttpModule::Get().GetConvaihttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FNullConvaihttpRequest>(AsShared())]()
		{
			StrongThis->FinishedRequest();
		});
	}
	else
	{
		FinishedRequest();
	}
}

EConvaihttpRequestStatus::Type FNullConvaihttpRequest::GetStatus() const
{
	return CompletionStatus;
}

const FConvaihttpResponsePtr FNullConvaihttpRequest::GetResponse() const
{
	return FConvaihttpResponsePtr(nullptr);
}

void FNullConvaihttpRequest::Tick(float DeltaSeconds)
{
	if (CompletionStatus == EConvaihttpRequestStatus::Processing)
	{
		ElapsedTime += DeltaSeconds;
		const float ConvaihttpTimeout = GetTimeoutOrDefault();
		if (ConvaihttpTimeout > 0 && ElapsedTime >= ConvaihttpTimeout)
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Timeout processing Convaihttp request. %p"),
				this);

			FinishedRequest();
		}
	}
}

float FNullConvaihttpRequest::GetElapsedTime() const
{
	return ElapsedTime;
}

void FNullConvaihttpRequest::FinishedRequest()
{
	CompletionStatus = EConvaihttpRequestStatus::Failed;
	TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> Request = SharedThis(this);
	FConvaihttpModule::Get().GetConvaihttpManager().RemoveRequest(Request);

	UE_LOG(LogConvaihttp, Log, TEXT("Finished request %p. no response %s url=%s elapsed=%.3f"),
		this, *GetVerb(), *GetURL(), ElapsedTime);

	OnProcessRequestComplete().ExecuteIfBound(Request, NULL, false);
}

// FNullConvaihttpResponse

FString FNullConvaihttpResponse::GetURL() const
{
	return FString();
}

FString FNullConvaihttpResponse::GetURLParameter(const FString& ParameterName) const
{
	return FString();
}

FString FNullConvaihttpResponse::GetHeader(const FString& HeaderName) const
{
	return FString();
}

TArray64<FString> FNullConvaihttpResponse::GetAllHeaders() const
{
	return TArray64<FString>();
}

FString FNullConvaihttpResponse::GetContentType() const
{
	return FString();
}

int64 FNullConvaihttpResponse::GetContentLength() const
{
	return 0;
}

const TArray64<uint8>& FNullConvaihttpResponse::GetContent() const
{
	return Payload;
}

int32 FNullConvaihttpResponse::GetResponseCode() const
{
	return 0;
}

FString FNullConvaihttpResponse::GetContentAsString() const
{
	return FString();
}


