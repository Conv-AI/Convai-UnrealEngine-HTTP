// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"
#include "GenericPlatform/ConvaihttpRequestImpl.h"
#include "Interfaces/IConvaihttpResponse.h"
#include "IConvaihttpThreadedRequest.h"

class FRequestPayload;
class FWinHttpConvaihttpResponse;
class FWinHttpConnectionConvaihttp;

using FStringKeyValueMap = TMap<FString, FString>;

class FWinHttpConvaihttpRequest
	: public IConvaihttpThreadedRequest
{
public:
	FWinHttpConvaihttpRequest();
	virtual ~FWinHttpConvaihttpRequest();

	//~ Begin IConvaihttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray64<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray64<uint8>& GetContent() const override;
	//~ End IConvaihttpBase Interface

	//~ Begin IConvaihttpRequest Interface
	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
	virtual void SetContent(const TArray64<uint8>& ContentPayload) override;
	virtual void SetContent(TArray64<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EConvaihttpRequestStatus::Type GetStatus() const override;
	virtual const FConvaihttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;
	//~ End IConvaihttpRequest Interface

	//~ Begin IConvaihttpRequestThreaded Interface
	/** Called on CONVAIHTTP thread */
	virtual bool StartThreadedRequest() override;
	/** Called on CONVAIHTTP thread */
	virtual bool IsThreadedRequestComplete() override;
	/** Called on CONVAIHTTP thread */
	virtual void TickThreadedRequest(float DeltaSeconds) override;

	/** Called on Game thread */
	virtual void FinishRequest() override;
	//~ End IConvaihttpRequestThreaded Interface

protected:
	void HandleDataTransferred(int32 BytesSent, int32 BytesReceived);
	void HandleHeaderReceived(const FString& HeaderKey, const FString& HeaderValue);
	void HandleRequestComplete(EConvaihttpRequestStatus::Type CompletionStatusUpdate);

	void UpdateResponseBody(bool bForceResponseExist = false);

	void OnWinHttpRequestComplete();
private:
	struct FWinHttpConvaihttpRequestData
	{
		/** */
		FString Url;

		/** */
		TMap<FString, FString> Headers;

		/** */
		FString ContentType;

		/** */
		FString Verb;

		/** Payload to use with the request. Typically for POST, PUT, or PATCH */
		TSharedPtr<FRequestPayload, ESPMode::ThreadSafe> Payload;
	} RequestData;

	/** */
	bool bRequestCancelled = false;

	/** The time this request was started, or unset if there is no request in progress */
	TOptional<double> RequestStartTimeSeconds;

	TOptional<double> RequestFinishTimeSeconds;

	/** Current status of request being processed */
	EConvaihttpRequestStatus::Type State = EConvaihttpRequestStatus::NotStarted;

	/** Status of request available via GetStatus */
	EConvaihttpRequestStatus::Type CompletionStatus = EConvaihttpRequestStatus::NotStarted;

	/** */
	TSharedPtr<FWinHttpConnectionConvaihttp, ESPMode::ThreadSafe> Connection;

	/** */
	TSharedPtr<FWinHttpConvaihttpResponse, ESPMode::ThreadSafe> Response;

	/** */
	int32 TotalBytesSent = 0;

	/** */
	int32 TotalBytesReceived = 0;
};

#endif // WITH_WINHTTP
