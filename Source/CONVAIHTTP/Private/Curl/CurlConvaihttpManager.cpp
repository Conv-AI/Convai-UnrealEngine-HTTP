// Copyright Epic Games, Inc. All Rights Reserved.

#include "Curl/CurlConvaihttpManager.h"

#if WITH_CURL

#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/Paths.h"
#include "Misc/Fork.h"

#include "Curl/CurlConvaihttpThread.h"
#include "Curl/CurlConvaihttp.h"
#include "Misc/OutputDeviceRedirector.h"
#include "ConvaihttpModule.h"

#if WITH_SSL
#include "Modules/ModuleManager.h"
#include "Ssl.h"
#include <openssl/crypto.h>
#endif

#include "SocketSubsystem.h"
#include "IPAddress.h"

#include "Convaihttp.h"

#ifndef DISABLE_UNVERIFIED_CERTIFICATE_LOADING
#define DISABLE_UNVERIFIED_CERTIFICATE_LOADING 0
#endif

CURLM* FCurlConvaihttpManager::GMultiHandle = nullptr;
#if !WITH_CURL_XCURL
CURLSH* FCurlConvaihttpManager::GShareHandle = nullptr;
#endif

FCurlConvaihttpManager::FCurlRequestOptions FCurlConvaihttpManager::CurlRequestOptions;

// set functions that will init the memory
namespace LibCryptoMemHooks
{
	void* (*ChainedMalloc)(size_t Size, const char* File, int Line) = nullptr;
	void* (*ChainedRealloc)(void* Ptr, const size_t Size, const char* File, int Line) = nullptr;
	void (*ChainedFree)(void* Ptr, const char* File, int Line) = nullptr;
	bool bMemoryHooksSet = false;

	/** This malloc will init the memory, keeping valgrind happy */
	void* MallocWithInit(size_t Size, const char* File, int Line)
	{
		void* Result = FMemory::Malloc(Size);
		if (LIKELY(Result))
		{
			FMemory::Memzero(Result, Size);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void* ReallocWithInit(void* Ptr, const size_t Size, const char* File, int Line)
	{
		size_t CurrentUsableSize = FMemory::GetAllocSize(Ptr);
		void* Result = FMemory::Realloc(Ptr, Size);
		if (LIKELY(Result) && CurrentUsableSize < Size)
		{
			FMemory::Memzero(reinterpret_cast<uint8 *>(Result) + CurrentUsableSize, Size - CurrentUsableSize);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void Free(void* Ptr, const char* File, int Line)
	{
		return FMemory::Free(Ptr);
	}

	void SetMemoryHooks()
	{
		// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL
		CRYPTO_get_mem_functions(&ChainedMalloc, &ChainedRealloc, &ChainedFree);
		CRYPTO_set_mem_functions(MallocWithInit, ReallocWithInit, Free);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL

		bMemoryHooksSet = true;
	}

	void UnsetMemoryHooks()
	{
		// remove our overrides
		if (LibCryptoMemHooks::bMemoryHooksSet)
		{
			// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL
			CRYPTO_set_mem_functions(LibCryptoMemHooks::ChainedMalloc, LibCryptoMemHooks::ChainedRealloc, LibCryptoMemHooks::ChainedFree);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING && WITH_SSL

			bMemoryHooksSet = false;
			ChainedMalloc = nullptr;
			ChainedRealloc = nullptr;
			ChainedFree = nullptr;
		}
	}
}

bool FCurlConvaihttpManager::IsInit()
{
	return GMultiHandle != nullptr;
}

void FCurlConvaihttpManager::InitCurl()
{
	if (IsInit())
	{
		UE_LOG(LogInit, Warning, TEXT("Already initialized multi handle"));
		return;
	}

	int32 CurlInitFlags = CURL_GLOBAL_ALL;
#if WITH_SSL
	// Make sure SSL is loaded so that we can use the shared cert pool, and to globally initialize OpenSSL if possible
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	if (SslModule.GetSslManager().InitializeSsl())
	{
		// Do not need Curl to initialize its own SSL
		CurlInitFlags = CurlInitFlags & ~(CURL_GLOBAL_SSL);
	}
#endif // #if WITH_SSL

	// Override libcrypt functions to initialize memory since OpenSSL triggers multiple valgrind warnings due to this.
	// Do this before libcurl/libopenssl/libcrypto has been inited.
	LibCryptoMemHooks::SetMemoryHooks();

	CURLcode InitResult = curl_global_init_mem(CurlInitFlags, CurlMalloc, CurlFree, CurlRealloc, CurlStrdup, CurlCalloc);
	if (InitResult == 0)
	{
		curl_version_info_data * VersionInfo = curl_version_info(CURLVERSION_NOW);
		if (VersionInfo)
		{
			UE_LOG(LogInit, Log, TEXT("Using libcurl %s"), ANSI_TO_TCHAR(VersionInfo->version));
			UE_LOG(LogInit, Log, TEXT(" - built for %s"), ANSI_TO_TCHAR(VersionInfo->host));

			if (VersionInfo->features & CURL_VERSION_SSL)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports SSL with %s"), ANSI_TO_TCHAR(VersionInfo->ssl_version));
			}
			else
			{
				// No SSL
				UE_LOG(LogInit, Log, TEXT(" - NO SSL SUPPORT!"));
			}

			if (VersionInfo->features & CURL_VERSION_LIBZ)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports CONVAIHTTP deflate (compression) using libz %s"), ANSI_TO_TCHAR(VersionInfo->libz_version));
			}

			UE_LOG(LogInit, Log, TEXT(" - other features:"));

#define PrintCurlFeature(Feature)	\
			if (VersionInfo->features & Feature) \
			{ \
			UE_LOG(LogInit, Log, TEXT("     %s"), TEXT(#Feature));	\
			}

			PrintCurlFeature(CURL_VERSION_SSL);
			PrintCurlFeature(CURL_VERSION_LIBZ);

			PrintCurlFeature(CURL_VERSION_DEBUG);
			PrintCurlFeature(CURL_VERSION_IPV6);
			PrintCurlFeature(CURL_VERSION_ASYNCHDNS);
			PrintCurlFeature(CURL_VERSION_LARGEFILE);
			PrintCurlFeature(CURL_VERSION_IDN);
			PrintCurlFeature(CURL_VERSION_CONV);
			PrintCurlFeature(CURL_VERSION_TLSAUTH_SRP);
#undef PrintCurlFeature
		}

		GMultiHandle = curl_multi_init();
		if (NULL == GMultiHandle)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize create libcurl multi handle! CONVAIHTTP transfers will not function properly."));
		}

		int32 MaxTotalConnections = 0;
		if (GConfig->GetInt(TEXT("CONVAIHTTP.Curl"), TEXT("MaxTotalConnections"), MaxTotalConnections, GEngineIni) && MaxTotalConnections > 0)
		{
			const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(MaxTotalConnections));
			if (SetOptResult != CURLM_OK)
			{
				UE_LOG(LogInit, Warning, TEXT("Failed to set libcurl max total connections options (%d), error %d ('%s')"),
					MaxTotalConnections, static_cast<int32>(SetOptResult), StringCast<TCHAR>(curl_multi_strerror(SetOptResult)).Get());
			}
		}

#if !WITH_CURL_XCURL
		GShareHandle = curl_share_init();
		if (NULL != GShareHandle)
		{
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}
		else
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl share handle!"));
		}
#endif
	}
	else
	{
		UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl (result=%d), CONVAIHTTP transfers will not function properly."), (int32)InitResult);
	}

	// Init curl request options
	if (FParse::Param(FCommandLine::Get(), TEXT("noreuseconn")))
	{
		CurlRequestOptions.bDontReuseConnections = true;
	}

#if WITH_SSL
	// Set default verify peer value based on availability of certificates
	CurlRequestOptions.bVerifyPeer = SslModule.GetCertificateManager().HasCertificatesAvailable();
#endif

	bool bVerifyPeer = true;
#if DISABLE_UNVERIFIED_CERTIFICATE_LOADING
	CurlRequestOptions.bVerifyPeer = bVerifyPeer;
#else
	if (GConfig->GetBool(TEXT("/Script/Engine.NetworkSettings"), TEXT("n.VerifyPeer"), bVerifyPeer, GEngineIni))
	{
		CurlRequestOptions.bVerifyPeer = bVerifyPeer;
	}
#endif

	bool bAcceptCompressedContent = true;
	if (GConfig->GetBool(TEXT("CONVAIHTTP"), TEXT("AcceptCompressedContent"), bAcceptCompressedContent, GEngineIni))
	{
		CurlRequestOptions.bAcceptCompressedContent = bAcceptCompressedContent;
	}

	int32 ConfigBufferSize = 0;
	if (GConfig->GetInt(TEXT("CONVAIHTTP.Curl"), TEXT("BufferSize"), ConfigBufferSize, GEngineIni) && ConfigBufferSize > 0)
	{
		CurlRequestOptions.BufferSize = ConfigBufferSize;
	}

	GConfig->GetBool(TEXT("CONVAIHTTP.Curl"), TEXT("bAllowSeekFunction"), CurlRequestOptions.bAllowSeekFunction, GEngineIni);

	CurlRequestOptions.MaxHostConnections = FConvaihttpModule::Get().GetConvaihttpMaxConnectionsPerServer();
	if (CurlRequestOptions.MaxHostConnections > 0)
	{
		const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, static_cast<long>(CurlRequestOptions.MaxHostConnections));
		if (SetOptResult != CURLM_OK)
		{
			FUTF8ToTCHAR Converter(curl_multi_strerror(SetOptResult));
			UE_LOG(LogInit, Warning, TEXT("Failed to set max host connections options (%d), error %d ('%s')"),
				CurlRequestOptions.MaxHostConnections, (int32)SetOptResult, Converter.Get());
			CurlRequestOptions.MaxHostConnections = 0;
		}
	}
	else
	{
		CurlRequestOptions.MaxHostConnections = 0;
	}

	TCHAR Home[256] = TEXT("");
	if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOMECONVAIHTTP="), Home, UE_ARRAY_COUNT(Home)))
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem && SocketSubsystem->GetAddressFromString(Home).IsValid())
		{
			CurlRequestOptions.LocalHostAddr = FString(Home);
		}
	}

	// print for visibility
	CurlRequestOptions.Log();
}

void FCurlConvaihttpManager::FCurlRequestOptions::Log()
{
	UE_LOG(LogInit, Log, TEXT(" CurlRequestOptions (configurable via config and command line):"));
		UE_LOG(LogInit, Log, TEXT(" - bVerifyPeer = %s  - Libcurl will %sverify peer certificate"),
		bVerifyPeer ? TEXT("true") : TEXT("false"),
		bVerifyPeer ? TEXT("") : TEXT("NOT ")
		);

	const FString& ProxyAddress = FConvaihttpModule::Get().GetProxyAddress();
	const bool bUseConvaihttpProxy = !ProxyAddress.IsEmpty();
	UE_LOG(LogInit, Log, TEXT(" - bUseConvaihttpProxy = %s  - Libcurl will %suse CONVAIHTTP proxy"),
		bUseConvaihttpProxy ? TEXT("true") : TEXT("false"),
		bUseConvaihttpProxy ? TEXT("") : TEXT("NOT ")
		);	
	if (bUseConvaihttpProxy)
	{
		UE_LOG(LogInit, Log, TEXT(" - ConvaihttpProxyAddress = '%s'"), *ProxyAddress);
	}

	UE_LOG(LogInit, Log, TEXT(" - bDontReuseConnections = %s  - Libcurl will %sreuse connections"),
		bDontReuseConnections ? TEXT("true") : TEXT("false"),
		bDontReuseConnections ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - MaxHostConnections = %d  - Libcurl will %slimit the number of connections to a host"),
		MaxHostConnections,
		(MaxHostConnections == 0) ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - LocalHostAddr = %s"), LocalHostAddr.IsEmpty() ? TEXT("Default") : *LocalHostAddr);

	UE_LOG(LogInit, Log, TEXT(" - BufferSize = %d"), CurlRequestOptions.BufferSize);
}


void FCurlConvaihttpManager::ShutdownCurl()
{
#if !WITH_CURL_XCURL
	if (GShareHandle != nullptr)
	{
		CURLSHcode ShareCleanupCode = curl_share_cleanup(GShareHandle);
		ensureMsgf(ShareCleanupCode == CURLSHE_OK, TEXT("CurlShareCleanup failed. ReturnValue=[%d]"), static_cast<int32>(ShareCleanupCode));
		GShareHandle = nullptr;
	}
#endif

	if (GMultiHandle != nullptr)
	{
		CURLMcode MutliCleanupCode = curl_multi_cleanup(GMultiHandle);
		ensureMsgf(MutliCleanupCode == CURLM_OK, TEXT("CurlMultiCleanup failed. ReturnValue=[%d]"), static_cast<int32>(MutliCleanupCode));
		GMultiHandle = nullptr;
	}

	curl_global_cleanup();

	LibCryptoMemHooks::UnsetMemoryHooks();

#if WITH_SSL
	// Shutdown OpenSSL
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	SslModule.GetSslManager().ShutdownSsl();
#endif // #if WITH_SSL
}

void FCurlConvaihttpManager::OnBeforeFork()
{
	FConvaihttpManager::OnBeforeFork();

	Thread->StopThread();
	ShutdownCurl();
}

void FCurlConvaihttpManager::OnAfterFork()
{
	InitCurl();

	if (FForkProcessHelper::IsForkedChildProcess() == false || FForkProcessHelper::SupportsMultithreadingPostFork() == false)
	{
		// Since this will create a fake thread its safe to create it immediately here
		Thread->StartThread();
	}

	FConvaihttpManager::OnAfterFork();
}

void FCurlConvaihttpManager::OnEndFramePostFork()
{
	if (FForkProcessHelper::SupportsMultithreadingPostFork())
	{
		// We forked and the frame is done, time to start the autonomous thread
		check(FForkProcessHelper::IsForkedMultithreadInstance());
		Thread->StartThread();
	}

	FConvaihttpManager::OnEndFramePostFork();
}

void FCurlConvaihttpManager::UpdateConfigs()
{
	// Update configs - update settings that are safe to update after initialize 
	FConvaihttpManager::UpdateConfigs();

	{
		bool bAcceptCompressedContent = true;
		if (GConfig->GetBool(TEXT("CONVAIHTTP"), TEXT("AcceptCompressedContent"), bAcceptCompressedContent, GEngineIni))
		{
			if (CurlRequestOptions.bAcceptCompressedContent != bAcceptCompressedContent)
			{
				UE_LOG(LogConvaihttp, Log, TEXT("AcceptCompressedContent changed from %s to %s"), *LexToString(CurlRequestOptions.bAcceptCompressedContent), *LexToString(bAcceptCompressedContent));
				CurlRequestOptions.bAcceptCompressedContent = bAcceptCompressedContent;
			}
		}
	}

	{
		int32 ConfigBufferSize = 0;
		if (GConfig->GetInt(TEXT("CONVAIHTTP.Curl"), TEXT("BufferSize"), ConfigBufferSize, GEngineIni) && ConfigBufferSize > 0)
		{
			if (CurlRequestOptions.BufferSize != ConfigBufferSize)
			{
				UE_LOG(LogConvaihttp, Log, TEXT("BufferSize changed from %d to %d"), CurlRequestOptions.BufferSize, ConfigBufferSize);
				CurlRequestOptions.BufferSize = ConfigBufferSize;
			}
		}
	}

	{
		bool bConfigAllowSeekFunction = false;
		if (GConfig->GetBool(TEXT("CONVAIHTTP.Curl"), TEXT("bAllowSeekFunction"), bConfigAllowSeekFunction, GEngineIni))
		{
			if (CurlRequestOptions.bAllowSeekFunction != bConfigAllowSeekFunction)
			{
				UE_LOG(LogConvaihttp, Log, TEXT("bAllowSeekFunction changed from %s to %s"), *LexToString(CurlRequestOptions.bAllowSeekFunction), *LexToString(bConfigAllowSeekFunction));
				CurlRequestOptions.bAllowSeekFunction = bConfigAllowSeekFunction;
			}
		}
	}
}

FConvaihttpThread* FCurlConvaihttpManager::CreateConvaihttpThread()
{
	return new FCurlConvaihttpThread();
}

bool FCurlConvaihttpManager::SupportsDynamicProxy() const
{
	return true;
}
#endif //WITH_CURL
