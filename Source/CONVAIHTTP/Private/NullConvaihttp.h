// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ConvaihttpRequestImpl.h"
#include "Interfaces/IConvaihttpResponse.h"

/**
 * Null (mock) implementation of an CONVAIHTTP request
 */
class FNullConvaihttpRequest : public FConvaihttpRequestImpl
{
public:

	// IConvaihttpBase
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray64<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray64<uint8>& GetContent() const override;
	// IConvaihttpRequest 
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

	FNullConvaihttpRequest()
		: CompletionStatus(EConvaihttpRequestStatus::NotStarted)
		, ElapsedTime(0)
	{}
	virtual ~FNullConvaihttpRequest() {}

private:
	void FinishedRequest();

	FString Url;
	FString Verb;
	TArray64<uint8> Payload;
	EConvaihttpRequestStatus::Type CompletionStatus;
	TMap<FString, FString> Headers;
	float ElapsedTime;
};

/**
 * Null (mock) implementation of an CONVAIHTTP request
 */
class FNullConvaihttpResponse : public IConvaihttpResponse
{
	// IConvaihttpBase 
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray64<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray64<uint8>& GetContent() const override;
	//~ Begin IConvaihttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;

	FNullConvaihttpResponse() {}
	virtual ~FNullConvaihttpResponse() {}

private:
	TArray64<uint8> Payload;
};
