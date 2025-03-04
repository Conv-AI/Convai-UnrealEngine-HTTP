// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_HOLOLENS

#include "ConvaihttpIXMLSupport.h"
#include "Interfaces/IConvaihttpResponse.h"
#include "GenericPlatform/ConvaihttpRequestImpl.h"
#include "GenericPlatform/ConvaihttpRequestPayload.h"

// Default user agent string
static const WCHAR USER_AGENT[] = L"UECONVAIHTTPIXML\r\n";


/**
 * IXML implementation of an Convaihttp request
 */
class FConvaihttpRequestIXML : public FConvaihttpRequestImpl
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

	/**
	 * Constructor
	 */
	FConvaihttpRequestIXML();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FConvaihttpRequestIXML();

private:

	uint32		CreateRequest();
	uint32		ApplyHeaders();
	uint32		SendRequest();
	void		FinishedRequest();
	void		CleanupRequest();

private:
	TMap<FString, FString>				Headers;
	TUniquePtr<FRequestPayload>			Payload;
	FString								URL;
	FString								Verb;

	EConvaihttpRequestStatus::Type			RequestStatus;
	float								ElapsedTime;

	ComPtr<IXMLCONVAIHTTPRequest2>			XHR;
	ComPtr<IXMLCONVAIHTTPRequest2Callback>	XHRCallback;
	ComPtr<ConvaihttpCallback>				ConvaihttpCB;
	ComPtr<RequestStream>				SendStream;

	TSharedPtr<class FConvaihttpResponseIXML, ESPMode::ThreadSafe> Response;

	friend class FConvaihttpResponseIXML;
};

/**
 * IXML implementation of an Convaihttp response
 */
class FConvaihttpResponseIXML : public IConvaihttpResponse
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

	// IConvaihttpResponse

	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;

	// FConvaihttpResponseIXML

	FConvaihttpResponseIXML(FConvaihttpRequestIXML& InRequest, ComPtr<ConvaihttpCallback> InConvaihttpCB);
	virtual ~FConvaihttpResponseIXML();

	bool	Succeeded();

private:

	FConvaihttpRequestIXML&				Request;
	FString							RequestURL;
	ComPtr<ConvaihttpCallback>			ConvaihttpCB;

};

#endif
