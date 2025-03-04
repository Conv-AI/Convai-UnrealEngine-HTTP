// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ConvaihttpRequestImpl.h"

/** 
  * Adapter class for IConvaihttpRequest abstract interface
  * does not fully expose the wrapped interface in the base. This allows client defined marshalling of the requests when end point permissions are at issue.
  */

class FConvaihttpRequestAdapterBase : public FConvaihttpRequestImpl
{
public:
    FConvaihttpRequestAdapterBase(const TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>& InConvaihttpRequest) 
		: ConvaihttpRequest(InConvaihttpRequest)
    {}

	// IConvaihttpRequest interface
    virtual FString                       GetURL() const override                                                  { return ConvaihttpRequest->GetURL(); }
	virtual FString                       GetURLParameter(const FString& ParameterName) const override             { return ConvaihttpRequest->GetURLParameter(ParameterName); }
	virtual FString                       GetHeader(const FString& HeaderName) const override                      { return ConvaihttpRequest->GetHeader(HeaderName); }
	virtual TArray64<FString>               GetAllHeaders() const override                                           { return ConvaihttpRequest->GetAllHeaders(); }
	virtual FString                       GetContentType() const override                                          { return ConvaihttpRequest->GetContentType(); }
	virtual uint64 GetContentLength() const override                                        { return ConvaihttpRequest->GetContentLength(); }
	virtual const TArray64<uint8>&          GetContent() const override                                              { return ConvaihttpRequest->GetContent(); }
	virtual FString                       GetVerb() const override                                                 { return ConvaihttpRequest->GetVerb(); }
	virtual void                          SetVerb(const FString& Verb) override                                    { ConvaihttpRequest->SetVerb(Verb); }
	virtual void                          SetURL(const FString& URL) override                                      { ConvaihttpRequest->SetURL(URL); }
	virtual void                          SetContent(const TArray64<uint8>& ContentPayload) override                 { ConvaihttpRequest->SetContent(ContentPayload); }
	virtual void                          SetContent(TArray64<uint8>&& ContentPayload) override                      { ConvaihttpRequest->SetContent(MoveTemp(ContentPayload)); }
	virtual void                          SetContentAsString(const FString& ContentString) override                { ConvaihttpRequest->SetContentAsString(ContentString); }
    virtual bool                          SetContentAsStreamedFile(const FString& Filename) override               { return ConvaihttpRequest->SetContentAsStreamedFile(Filename); }
	virtual bool                          SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override { return ConvaihttpRequest->SetContentFromStream(Stream); }
	virtual void                          SetHeader(const FString& HeaderName, const FString& HeaderValue) override { ConvaihttpRequest->SetHeader(HeaderName, HeaderValue); }
	virtual void                          AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override { ConvaihttpRequest->AppendToHeader(HeaderName, AdditionalHeaderValue); }
	virtual void                          SetTimeout(float InTimeoutSecs) override                                 { ConvaihttpRequest->SetTimeout(InTimeoutSecs); }
	virtual void                          ClearTimeout() override                                                  { ConvaihttpRequest->ClearTimeout(); }
	virtual TOptional<float>              GetTimeout() const override                                              { return ConvaihttpRequest->GetTimeout(); }
	virtual const FConvaihttpResponsePtr        GetResponse() const override                                             { return ConvaihttpRequest->GetResponse(); }
	virtual float                         GetElapsedTime() const override                                          { return ConvaihttpRequest->GetElapsedTime(); }
	virtual EConvaihttpRequestStatus::Type	  GetStatus() const override                                               { return ConvaihttpRequest->GetStatus(); }
	virtual void                          Tick(float DeltaSeconds) override                                        { ConvaihttpRequest->Tick(DeltaSeconds); }

protected:
    TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> ConvaihttpRequest;
};

