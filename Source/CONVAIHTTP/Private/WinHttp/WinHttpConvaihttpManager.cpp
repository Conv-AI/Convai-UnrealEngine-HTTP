// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpConvaihttpManager.h"
#include "WinHttp/Support/WinHttpSession.h"
#include "WinHttp/Support/WinHttpTypes.h"
#include "Convaihttp.h"
#include "Misc/CoreDelegates.h"
#include "Stats/Stats.h"

namespace
{
	FCH_WinHttpConvaihttpManager* GWinHttpManager = nullptr;

	DWORD GetPlatformProtocolFlags()
	{
		// Enable "all" protocols (but not SSL2, it is insecure).
		// For legacy reasons, "all" isn't actually all protocols, so we explicitly enable some below based on windows versions
		DWORD ProtocolFlags = WINHTTP_FLAG_SECURE_PROTOCOL_ALL & ~WINHTTP_FLAG_SECURE_PROTOCOL_SSL2;
#if PLATFORM_WINDOWS
		const bool bIsWindows7OrGreater = FPlatformMisc::VerifyWindowsVersion(6, 1);
		if (bIsWindows7OrGreater)
		{
			ProtocolFlags |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;
		}
		const bool bIsWindows8Point1OrGreater = FPlatformMisc::VerifyWindowsVersion(6, 3);
		if (bIsWindows8Point1OrGreater)
		{
			ProtocolFlags |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		}
#endif // PLATFORM_WINDOWS
		return ProtocolFlags;
	}
}

FCH_WinHttpConvaihttpManager* FCH_WinHttpConvaihttpManager::GetManager()
{
	return GWinHttpManager;
}

FCH_WinHttpConvaihttpManager::FCH_WinHttpConvaihttpManager()
{
	if (GWinHttpManager == nullptr)
	{
		GWinHttpManager = this;
	}

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda([]()
	{
		if (FCH_WinHttpConvaihttpManager* const Manager = FCH_WinHttpConvaihttpManager::GetManager())
		{
			Manager->HandleApplicationSuspending();
		}
	});
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda([]()
	{
		if (FCH_WinHttpConvaihttpManager* const Manager = FCH_WinHttpConvaihttpManager::GetManager())
		{
			Manager->HandleApplicationResuming();
		}
	});
}

FCH_WinHttpConvaihttpManager::~FCH_WinHttpConvaihttpManager()
{
	if (GWinHttpManager == this)
	{
		GWinHttpManager = nullptr;
	}
}

void FCH_WinHttpConvaihttpManager::OnBeforeFork()
{
	// FConvaihttpManager's OnBeforeFork will flush all active requests, so it will be safe to reset our active sessions
	FConvaihttpManager::OnBeforeFork();
	ActiveSessions.Reset();
}

void FCH_WinHttpConvaihttpManager::HandleApplicationSuspending()
{
	SCOPED_ENTER_BACKGROUND_EVENT(FCH_WinHttpConvaihttpManager_HandleApplicationSuspending);

	Flush(EConvaihttpFlushReason::Background);
	ActiveSessions.Reset();
}

void FCH_WinHttpConvaihttpManager::HandleApplicationResuming()
{
	// No-op
}

void FCH_WinHttpConvaihttpManager::QuerySessionForUrl(const FString& /*UnusedUrl*/, FCH_WinHttpQuerySessionComplete&& Delegate)
{
	// Pretend to be async here so applications properly react on platforms where this is actually async
	AddGameThreadTask([LambdaDelegate = MoveTemp(Delegate)]()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCH_WinHttpConvaihttpManager_QuerySessionForUrlLambda);

		FCH_WinHttpSession* SessionPtr = nullptr;
		if (FCH_WinHttpConvaihttpManager* ConvaihttpManager = GetManager())
		{
			const uint32 DefaultProtocolFlags = GetPlatformProtocolFlags();
			SessionPtr = ConvaihttpManager->FindOrCreateSession(DefaultProtocolFlags);
		}

		LambdaDelegate.ExecuteIfBound(SessionPtr);
	});
}

bool FCH_WinHttpConvaihttpManager::ValidateRequestCertificates(IWinHttpConnection& Connection)
{
	// WinHttp already does regular validation of certificates, this is for additional validation
	// NOTE: this is usually not called on the game thread! Everything that happens here must be thread safe!

	// TODO: Add support for client cert pinning here when we want to support that on Windows

	// True means this connection did not fail validation
	return true;
}

void FCH_WinHttpConvaihttpManager::ReleaseRequestResources(IWinHttpConnection& Connection)
{
	// No-op
}

FCH_WinHttpSession* FCH_WinHttpConvaihttpManager::FindOrCreateSession(const uint32 SecurityProtocols)
{
	check(IsInGameThread());

	TUniquePtr<FCH_WinHttpSession>* SessionPtrPtr = ActiveSessions.Find(SecurityProtocols);

	FCH_WinHttpSession* SessionPtr = SessionPtrPtr ? SessionPtrPtr->Get() : nullptr;
	if (!SessionPtr)
	{
		SessionPtr = ActiveSessions.Emplace(SecurityProtocols, MakeUnique<FCH_WinHttpSession>(SecurityProtocols, bPlatformForcesSecureConnections)).Get();
	}

	return SessionPtr;
}

#endif //WITH_WINHTTP
