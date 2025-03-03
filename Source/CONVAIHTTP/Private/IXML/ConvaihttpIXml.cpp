// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpIXML.h"
#include "ConvaihttpManager.h"
#include "HAL/FileManager.h"

#if PLATFORM_HOLOLENS

#define CHECK_SUCCESS(a)  { bool success = SUCCEEDED( (a) ); check( success ); }

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FConvaihttpRequestIXML::FConvaihttpRequestIXML()
	: RequestStatus( EConvaihttpRequestStatus::NotStarted )
	, XHR( nullptr )
	, XHRCallback( nullptr )
	, ElapsedTime(0)
	, ConvaihttpCB(nullptr)
{
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FConvaihttpRequestIXML::~FConvaihttpRequestIXML()
{
	// ComPtr<> smart pointers should handle releasing the COM objects.
}
//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FConvaihttpRequestIXML::GetURL() const
{
	return URL;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FConvaihttpRequestIXML::GetURLParameter(const FString& ParameterName) const
{
	check(false);
	return TEXT("Not yet implemented");
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FConvaihttpRequestIXML::GetHeader(const FString& HeaderName) const
{
	const FString* Header = Headers.Find(HeaderName);
	return Header != NULL ? *Header : TEXT("");
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
TArray64<FString> FConvaihttpRequestIXML::GetAllHeaders() const
{
	TArray64<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FConvaihttpRequestIXML::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
int32 FConvaihttpRequestIXML::GetContentLength() const
{
	return Payload->GetContentLength();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
const TArray64<uint8>& FConvaihttpRequestIXML::GetContent() const
{
	return Payload->GetContent();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FConvaihttpRequestIXML::GetVerb() const
{
	return Verb;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetVerb(const FString& InVerb)
{
	Verb = InVerb;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetURL(const FString& InURL)
{
	URL = InURL;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetContent(const TArray64<uint8>& ContentPayload)
{
	Payload = MakeUnique<FRequestPayloadInMemory>(ContentPayload);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetContent(TArray64<uint8>&& ContentPayload)
{
	Payload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(ContentPayload));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetContentAsString(const FString& ContentString)
{
	if ( ContentString.Len() )
	{
		int32 Utf8Length = FTCHARToUTF8_Convert::ConvertedLength(*ContentString, ContentString.Len());
		TArray64<uint8> Buffer;
		Buffer.SetNumUninitialized(Utf8Length);
		FTCHARToUTF8_Convert::Convert((ANSICHAR*)Buffer.GetData(), Buffer.Num(), *ContentString, ContentString.Len());
		Payload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(Buffer));
	}
}

bool FConvaihttpRequestIXML::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogConvaihttp, Verbose, TEXT("FCurlConvaihttpRequest::SetContentAsStreamedFile() - %s"), *Filename);

	if (RequestStatus == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("FCurlConvaihttpRequest::SetContentAsStreamedFile() - attempted to set content on a request that is inflight"));
		return false;
	}

	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);
	if (File)
	{
		Payload = MakeUnique<FRequestPayloadInFileStream>(MakeShareable(File));
		return true;
	}
	else
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("FCurlConvaihttpRequest::SetContentAsStreamedFile Failed to open %s for reading"), *Filename);
		Payload.Reset();
		return false;
	}
}

bool FConvaihttpRequestIXML::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogConvaihttp, Verbose, TEXT("FCurlConvaihttpRequest::SetContentFromStream() - %s"), *Stream->GetArchiveName());

	if (RequestStatus == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("FCurlConvaihttpRequest::SetContentFromStream() - attempted to set content on a request that is inflight"));
		return false;
	}

	Payload = MakeUnique<FRequestPayloadInFileStream>(Stream);
	return true;
}


//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	Headers.Add(HeaderName, HeaderValue);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
bool FConvaihttpRequestIXML::ProcessRequest()
{
	uint32 Result = 0;

	// Are we already processing?
	if (RequestStatus == EConvaihttpRequestStatus::Processing)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	// Nothing to do without a URL
	else if (URL.IsEmpty())
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	else
	{
		Result = CreateRequest();

		if (SUCCEEDED(Result))
		{
			Result = ApplyHeaders();

			if (SUCCEEDED(Result))
			{
				RequestStatus = EConvaihttpRequestStatus::Processing;
				Response = MakeShareable( new FConvaihttpResponseIXML(*this, ConvaihttpCB) );

				// Try to start the connection and send the Convaihttp request
				Result = SendRequest();

				if (SUCCEEDED(Result))
				{
					// Add to global list while being processed so that the ref counted request does not get deleted
					FConvaihttpModule::Get().GetConvaihttpManager().AddRequest(SharedThis(this));
				}
				else
				{
					// No response since connection failed
					Response = NULL;

					// Cleanup and call delegate
					if (!IsInGameThread())
					{
						FConvaihttpModule::Get().GetConvaihttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FConvaihttpRequestIXML>(AsShared())]()
						{
							StrongThis->FinishedRequest();
						});
					}
					else
					{
						FinishedRequest();
					}
				}
			}
		}
		else
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("CreateRequest failed with error code %d URL=%s"), Result, *URL);
		}

	}

	return SUCCEEDED(Result);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FConvaihttpRequestIXML::CreateRequest()
{
	// Create the IXmlConvaihttpRequest2 object.
	CHECK_SUCCESS( ::CoCreateInstance( __uuidof(FreeThreadedXMLCONVAIHTTP60),
		nullptr,
		CLSCTX_SERVER,
		__uuidof(IXMLCONVAIHTTPRequest2),
		&XHR ) );

	// Create the IXmlConvaihttpRequest2Callback object and initialize it.
	CHECK_SUCCESS( Microsoft::WRL::Details::MakeAndInitialize<ConvaihttpCallback>( &ConvaihttpCB ) );
	CHECK_SUCCESS( ConvaihttpCB.As( &XHRCallback ) );

	// Open a connection for an CONVAIHTTP GET request.
	// NOTE: This is where the IXMLCONVAIHTTPRequest2 object gets given a
	// pointer to the IXMLCONVAIHTTPRequest2Callback object.
	return XHR->Open( 
		*Verb,				// CONVAIHTTP method
		*URL,					// URL string as wchar*
		XHRCallback.Get(),	// callback object from a ComPtr<>
		NULL,					// username
		NULL,					// password
		NULL,					// proxy username
		NULL );					// proxy password
};

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FConvaihttpRequestIXML::ApplyHeaders()
{
	uint32 hr = S_OK;

	for ( auto It = Headers.CreateConstIterator(); It; ++It )
	{
		FString HeaderName  = It.Key();
		FString HeaderValue = It.Value();

		hr = XHR->SetRequestHeader( *HeaderName, *HeaderValue );

		if( FAILED( hr ) )
		{
			break;
		}
	}

	return hr;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FConvaihttpRequestIXML::SendRequest()
{
	uint32 hr = E_FAIL;

	if( Payload )
	{
		uint32 SizeInBytes = Payload->GetContentLength();

		// Create and open a new runtime class
		SendStream = Make<RequestStream>();
		LPCSTR PayloadChars = (LPCSTR)Payload->GetContent().GetData();
		SendStream->Open( PayloadChars, (ULONG) SizeInBytes );

		hr = XHR->Send(
			SendStream.Get(),        // body message as an ISequentialStream*
			SendStream->Size() );    // count of bytes in the stream.
	}
	else
	{
		hr = XHR->Send( NULL, 0 );
	}

	// The CONVAIHTTP Request runs asynchronously from here...
	return hr;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::CancelRequest()
{
	check ( XHR );

	XHR->Abort();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
EConvaihttpRequestStatus::Type FConvaihttpRequestIXML::GetStatus() const
{
	return RequestStatus;
}

const FConvaihttpResponsePtr FConvaihttpRequestIXML::GetResponse() const
{
	return Response;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::Tick(float DeltaSeconds)
{
	// IXML requests may need the app message pump operational
	// in order to progress.  If the core engine loop is not
	// running then we'll just pump messages ourselves.
	if (!GIsRunning)
	{
		FPlatformMisc::PumpMessages(true);
	}

	// keep track of elapsed seconds
	ElapsedTime += DeltaSeconds;
	const float ConvaihttpTimeout = GetTimeoutOrDefault();
	if (ConvaihttpTimeout > 0 &&
		ElapsedTime >= ConvaihttpTimeout)
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("Timeout processing Convaihttp request. %p"),
			this);

		// finish it off since it is timeout
		FinishedRequest();
	}

	// No longer waiting for a response and done processing it
	if (RequestStatus == EConvaihttpRequestStatus::Processing &&
		Response.IsValid() &&
		ConvaihttpCB &&
		ConvaihttpCB->IsFinished() )
	{
		FinishedRequest();
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::FinishedRequest()
{
	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	FConvaihttpModule::Get().GetConvaihttpManager().RemoveRequest(SharedThis(this));

	if (Response.IsValid() &&
		Response->Succeeded() )
	{
		// Mark last request attempt as completed successfully
		RequestStatus = EConvaihttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		// Mark last request attempt as completed but failed
		RequestStatus = EConvaihttpRequestStatus::Failed;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,false);
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FConvaihttpRequestIXML::CleanupRequest()
{
}

float FConvaihttpRequestIXML::GetElapsedTime() const
{
	return ElapsedTime;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	
// FConvaihttpResponseIXML
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FConvaihttpResponseIXML::FConvaihttpResponseIXML(FConvaihttpRequestIXML& InRequest, ComPtr<ConvaihttpCallback> InConvaihttpCB)
	: Request( InRequest )
	, ConvaihttpCB( InConvaihttpCB )
{
	check( ConvaihttpCB );
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FConvaihttpResponseIXML::~FConvaihttpResponseIXML()
{
	// ComPtr<> smart pointers should handle releasing the COM objects.
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

bool FConvaihttpResponseIXML::Succeeded()
{
	check ( ConvaihttpCB );

	if ( ConvaihttpCB->IsFinished() )
	{
		if (   ( ConvaihttpCB->GetConvaihttpStatus() >= 200 )
			&& ( ConvaihttpCB->GetConvaihttpStatus() <  300 ) )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FConvaihttpResponseIXML::GetURL() const
{
	return Request.GetURL();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FConvaihttpResponseIXML::GetURLParameter(const FString& ParameterName) const
{
	return Request.GetURLParameter( ParameterName );
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FConvaihttpResponseIXML::GetHeader(const FString& HeaderName) const
{
	FString SingleHeader;
	PWSTR SingleHeaderPtr;

	if( SUCCEEDED( Request.XHR->GetResponseHeader( *HeaderName, &SingleHeaderPtr )))
	{
		SingleHeader = SingleHeaderPtr;
	}

	return SingleHeader;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

TArray64<FString> FConvaihttpResponseIXML::GetAllHeaders() const
{
	TArray64<FString> AllHeaders;
	PWSTR AllHeadersPtr;

	if( SUCCEEDED( Request.XHR->GetAllResponseHeaders( &AllHeadersPtr )))
	{
		FString AllHeadersString = FString( AllHeadersPtr );
		while( AllHeadersString.Contains(TEXT("\r\n")) )
		{
			FString Header, RestOfHeaders;
			AllHeadersString.Split(TEXT("\r\n"), &Header, &RestOfHeaders);

			if( !Header.IsEmpty() &&  Header.Contains(TEXT(":")) )
			{
				AllHeaders.Add( Header );
			}
			AllHeadersString = RestOfHeaders;
		}
	}

	return AllHeaders;
}

//-----------------------------------------------------------------------------
//	
//----------------------------------------------------------->-----------------

FString FConvaihttpResponseIXML::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

int32 FConvaihttpResponseIXML::GetContentLength() const
{
	check ( ConvaihttpCB );
	
	return ConvaihttpCB->GetContent().Num();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

const TArray64<uint8>& FConvaihttpResponseIXML::GetContent() const
{
	check ( ConvaihttpCB );
	
	return ConvaihttpCB->GetContent();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

int32 FConvaihttpResponseIXML::GetResponseCode() const
{
	check ( ConvaihttpCB );

	return ConvaihttpCB->GetConvaihttpStatus();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FConvaihttpResponseIXML::GetContentAsString() const
{
	TArray64<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

//-----------------------------------------------------------------------------
//	End of file

#endif // PLATFORM_HOLOLENS
