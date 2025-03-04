// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "GenericPlatform/ConvaihttpRequestImpl.h"
#include "Interfaces/IConvaihttpResponse.h"
#include "ConvaihttpManager.h"
#include "PlatformConvaihttp.h"

class FConvaihttpManager;
class FString;
class IConvaihttpRequest;

/**
 * Apple implementation of an Convaihttp request
 */
class FAppleConvaihttpRequest : public FConvaihttpRequestImpl
{
public:
	// implementation friends
	friend class FAppleConvaihttpResponse;


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
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray64<uint8>& ContentPayload) override;
	virtual void SetContent(TArray64<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
    virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual void SetTimeout(float InTimeoutSecs) override;
	virtual void ClearTimeout() override;
	virtual TOptional<float> GetTimeout() const override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EConvaihttpRequestStatus::Type GetStatus() const override;
	virtual const FConvaihttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;
	//~ End IConvaihttpRequest Interface

	/**
	 * Constructor
	 */
	FAppleConvaihttpRequest();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FAppleConvaihttpRequest();


private:

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();

	/**
	 * Process state for a finished request that no longer needs to be ticked
	 * Calls the completion delegate
	 */
	void FinishedRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();


private:
	/** This is the NSMutableURLRequest, all our Apple functionality will deal with this. */
	NSMutableURLRequest* Request;

	/** This is the connection our request is sent along. */
	NSURLConnection* Connection;

	/** Flag whether the request payload source is a file */
	bool bIsPayloadFile;

	/** The request payload length in bytes. This must be tracked separately for a file stream */
	uint64 RequestPayloadByteLength;

	/** The response object which we will use to pair with this request */
	TSharedPtr<class FAppleConvaihttpResponse,ESPMode::ThreadSafe> Response;

	/** BYTE array payload to use with the request. Typically for a POST */
	mutable TArray64<uint8> RequestPayload;

	/** BYTE array for content which we now own */
	TArray64<uint8> ContentData;

	/** Current status of request being processed */
	EConvaihttpRequestStatus::Type CompletionStatus;

	/** Number of bytes sent to progress update */
	int32 ProgressBytesSent;

	/** Start of the request */
	double StartRequestTime;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;
};


/**
 * Apple Response Wrapper which will be used for it's delegates to receive responses.
 */
@interface FConvaihttpResponseAppleWrapper : NSObject
{
	/** Holds the payload as we receive it. */
	TArray64<uint8> Payload;
}
/** A handle for the response */
@property(retain) NSCONVAIHTTPURLResponse* Response;
/** Flag whether the response is ready */
@property BOOL bIsReady;
/** When the response is complete, indicates whether the response was received without error. */
@property BOOL bHadError;
/** When the response is complete, indicates whether the response failed with an error specific to connecting to the host. */
@property BOOL bIsHostConnectionFailure;
/** The total number of bytes written out during the request/response */
@property int32 BytesWritten;

/** Delegate called when we send data. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite;
/** Delegate called with we receive a response. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
/** Delegate called with we receive data. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
/** Delegate called with we complete with an error. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error;
/** Delegate called with we complete successfully. See Apple docs for when/how this should be used. */
-(void) connectionDidFinishLoading:(NSURLConnection *)connection;

#if WITH_SSL
/** Delegate called when the connection is about to validate an auth challenge. We only care about server trust. See Apple docs for when/how this should be used. */
-(void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge: (NSURLAuthenticationChallenge *)challenge;
#endif

- (TArray64<uint8>&)getPayload;
- (int32)getBytesWritten;
@end


/**
 * Apple implementation of an Convaihttp response
 */
class FAppleConvaihttpResponse : public IConvaihttpResponse
{
private:
	// This is the NSCONVAIHTTPURLResponse, all our functionality will deal with.
	FConvaihttpResponseAppleWrapper* ResponseWrapper;

	/** Request that owns this response */
	const FAppleConvaihttpRequest& Request;


public:
	// implementation friends
	friend class FAppleConvaihttpRequest;


	//~ Begin IConvaihttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray64<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray64<uint8>& GetContent() const override;
	//~ End IConvaihttpBase Interface

	//~ Begin IConvaihttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;
	//~ End IConvaihttpResponse Interface

	NSCONVAIHTTPURLResponse* GetResponseObj() const;

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady() const;
	
	/**
	 * Check whether a response had an error.
	 */
	bool HadError() const;

	/**
	 * Get the number of bytes received so far
	 */
	const uint64 GetNumBytesReceived() const;

	/**
	* Get the number of bytes sent so far
	*/
	const uint64 GetNumBytesWritten() const;

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FAppleConvaihttpResponse(const FAppleConvaihttpRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FAppleConvaihttpResponse();


private:

	/** BYTE array to fill in as the response is read via didReceiveData */
	mutable TArray64<uint8> Payload;
};
