// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_HOLOLENS


#include "Containers/UnrealString.h"
#include "Convaihttp.h"
#include "HoloLens/AllowWindowsPlatformTypes.h"
#include <msxml6.h>
#include <wrl.h>
#include <Windows.h>

using namespace Microsoft::WRL;

// default buffer size and CONVAIHTTP request size
static const int MAX_CONVAIHTTP_BUFFER_SIZE = 8192;

//------------------------------------------------------------------------------
// Name: ConvaihttpCallback
// Desc: Implement the IXMLCONVAIHTTPRequest2Callback functions with
//       basic error reporting and an Event signaling when the request is
//       complete.
//------------------------------------------------------------------------------
class ConvaihttpCallback : public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLCONVAIHTTPRequest2Callback>
{
public:
	// Required functions
	STDMETHODIMP OnRedirect( IXMLCONVAIHTTPRequest2* XHR, const WCHAR* RedirectUrl );
	STDMETHODIMP OnHeadersAvailable( IXMLCONVAIHTTPRequest2* XHR, DWORD Status, const WCHAR *StatusString );
	STDMETHODIMP OnDataAvailable( IXMLCONVAIHTTPRequest2* XHR, ISequentialStream* ResponseStream );
	STDMETHODIMP OnResponseReceived( IXMLCONVAIHTTPRequest2* XHR, ISequentialStream* ResponseStream );
	STDMETHODIMP OnError( IXMLCONVAIHTTPRequest2* XHR, HRESULT InError );

	STDMETHODIMP RuntimeClassInitialize();
	friend HRESULT MakeAndInitialize<ConvaihttpCallback,ConvaihttpCallback>( ConvaihttpCallback ** );

	ConvaihttpCallback() : ConvaihttpStatus(0), LastResult(S_OK), CompleteSignal(nullptr), AllHeaders(), ContentData(), ContentRead(0) {}
	~ConvaihttpCallback();

	//. Accessors
	uint32			GetConvaihttpStatus()		{ return ConvaihttpStatus; }
	TArray64<uint8>&	GetContent()		{ return ContentData; }
	FString			GetAllHeaders()		{ return AllHeaders; }
	HRESULT			GetErrorCode()		{ return LastResult; }

	// Helper functions
	HRESULT			ReadDataFromStream( ISequentialStream *Stream );
	BOOL			IsFinished();
	BOOL			WaitForFinish();

private:
	uint32			ConvaihttpStatus;
	HRESULT			LastResult;
	HANDLE			CompleteSignal;
	FString			AllHeaders;
	TArray64<uint8>	ContentData;
	uint64			ContentRead;
};


// ----------------------------------------------------------------------------
// Name: RequestStream
// Desc: Encapsulates a request data stream. It inherits ISequentialStream,
// which the IXMLCONVAIHTTPRequest2 class uses to read from our buffer. It also 
// inherits IDispatch, which the IXMLCONVAIHTTPRequest2 interface on Durango requires 
// (unlike on Windows, where only ISequentialStream is necessary).
// ----------------------------------------------------------------------------
class RequestStream : public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>, ISequentialStream, IDispatch>
{
public:

	RequestStream();
	~RequestStream();

	// ISequentialStream
	STDMETHODIMP Open( LPCSTR Buffer, ULONG BufferSize );
	STDMETHODIMP Read( void *Data, ULONG ByteCount, ULONG *NumReadBytes );
	STDMETHODIMP Write( const void *Data,  ULONG ByteCount, ULONG *Written );

	//Helper
	STDMETHODIMP_(ULONGLONG) Size();

	//IUnknown
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP QueryInterface( REFIID InRIID, void **OutObj );

	//IDispatch
	STDMETHODIMP GetTypeInfoCount( uint32 FAR*  Info );
	STDMETHODIMP GetTypeInfo( uint32 Info, LCID InLCID, ITypeInfo FAR* FAR* TInfo );
	STDMETHODIMP GetIDsOfNames( REFIID InRIID, OLECHAR FAR* FAR* Names, uint32 NameCount, LCID InLCID, DISPID FAR* DispId );
	STDMETHODIMP Invoke(
			DISPID			DispIdMember, 
			REFIID			InRIID, 
			LCID			InLCID, 
			WORD			InFlags,
			DISPPARAMS FAR*	DispParams,
			VARIANT FAR*	VarResult,
			EXCEPINFO FAR*	ExcepInfo, 
			uint32 FAR*		ArgErr
		);

protected:
	LONG    RefCount;
	CHAR*   StreamBuffer;
	size_t  BuffSize;
	size_t  BuffIndex;
};


#include "HoloLens/HideWindowsPlatformTypes.h"

#endif
