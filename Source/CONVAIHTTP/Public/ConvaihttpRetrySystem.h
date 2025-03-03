// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "Interfaces/IConvaihttpRequest.h"
#include "ConvaihttpRequestAdapter.h"

/**
 * Helpers of various types for the retry system
 */
namespace FConvaihttpRetrySystem
{
	typedef uint32 RetryLimitCountType;
	typedef double RetryTimeoutRelativeSecondsType;

	inline RetryLimitCountType             RetryLimitCount(uint32 Value)             { return Value; }
	inline RetryTimeoutRelativeSecondsType RetryTimeoutRelativeSeconds(double Value) { return Value; }

	template <typename  IntrinsicType>
	IntrinsicType TZero();

	template <> inline float                           TZero<float>()                           { return 0.0f; }
	template <> inline RetryLimitCountType             TZero<RetryLimitCountType>()             { return RetryLimitCount(0); }
	template <> inline RetryTimeoutRelativeSecondsType TZero<RetryTimeoutRelativeSecondsType>() { return RetryTimeoutRelativeSeconds(0.0); }

	typedef TOptional<float>                           FRandomFailureRateSetting;
	typedef TOptional<RetryLimitCountType>             FRetryLimitCountSetting;
	typedef TOptional<RetryTimeoutRelativeSecondsType> FRetryTimeoutRelativeSecondsSetting;
	typedef TSet<int32> FRetryResponseCodes;
	typedef TSet<FName> FRetryVerbs;

	struct FRetryDomains
	{
		FRetryDomains(TArray64<FString>&& InDomains) 
			: Domains(MoveTemp(InDomains))
			, ActiveIndex(0)
		{}

		/** The domains to use */
		const TArray64<FString> Domains;
		/**
		 * Index into Domains to attempt
		 * Domains are cycled through on some errors, and when we succeed on one domain, we remain on that domain until that domain results in an error
		 */
		TAtomic<int32> ActiveIndex;
	};
	typedef TSharedPtr<FRetryDomains, ESPMode::ThreadSafe> FRetryDomainsPtr;

	/**
	 * Read the number of seconds a CONVAIHTTP request is throttled for from the response
	 * @param Response the CONVAIHTTP response to read the value from
	 * @return If found, the number of seconds the request is rate limited for.  If not found, an unset TOptional
	 */
	TOptional<double> CONVAIHTTP_API ReadThrottledTimeFromResponseInSeconds(FConvaihttpResponsePtr Response);
};


namespace FConvaihttpRetrySystem
{
    /**
     * class FRequest is what the retry system accepts as inputs
     */
    class FRequest 
		: public FConvaihttpRequestAdapterBase
    {
    public:
        struct EStatus
        {
            enum Type
            {
                NotStarted = 0,
                Processing,
                ProcessingLockout,
                Cancelled,
                FailedRetry,
                FailedTimeout,
                Succeeded
            };
        };

    public:
		// IConvaihttpRequest interface
		CONVAIHTTP_API virtual bool ProcessRequest() override;
		CONVAIHTTP_API virtual void CancelRequest() override;
		
		// FRequest
		EStatus::Type GetRetryStatus() const { return Status; }

    protected:
		friend class FManager;

		CONVAIHTTP_API FRequest(
			class FManager& InManager,
			const TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>& ConvaihttpRequest,
			const FRetryLimitCountSetting& InRetryLimitCountOverride = FRetryLimitCountSetting(),
			const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride = FRetryTimeoutRelativeSecondsSetting(),
            const FRetryResponseCodes& InRetryResponseCodes = FRetryResponseCodes(),
            const FRetryVerbs& InRetryVerbs = FRetryVerbs(),
			const FRetryDomainsPtr& InRetryDomains = FRetryDomainsPtr()
			);

		void ConvaihttpOnRequestProgress(FConvaihttpRequestPtr InConvaihttpRequest, int64 BytesSent, int64 BytesRcv);

		/** Update our CONVAIHTTP request's URL's domain from our RetryDomains */
		void SetUrlFromRetryDomains();
		/** Move to the next retry domain from our RetryDomains */
		void MoveToNextRetryDomain();

		EStatus::Type                        Status;

        FRetryLimitCountSetting              RetryLimitCountOverride;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsOverride;
		FRetryResponseCodes					 RetryResponseCodes;
        FRetryVerbs                          RetryVerbs;
		FRetryDomainsPtr					 RetryDomains;
		/** The current index in RetryDomains we are attempting */
		int32								 RetryDomainsIndex = 0;
		/** The original URL before replacing anything from RetryDomains */
		FString								 OriginalUrl;

		FManager& RetryManager;
    };
}

namespace FConvaihttpRetrySystem
{
    class FManager
    {
    public:
        // FManager
		CONVAIHTTP_API FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault);

		/**
		 * Create a new convaihttp request with retries
		 */
		CONVAIHTTP_API TSharedRef<class FConvaihttpRetrySystem::FRequest, ESPMode::ThreadSafe> CreateRequest(
			const FRetryLimitCountSetting& InRetryLimitCountOverride = FRetryLimitCountSetting(),
			const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride = FRetryTimeoutRelativeSecondsSetting(),
			const FRetryResponseCodes& InRetryResponseCodes = FRetryResponseCodes(),
			const FRetryVerbs& InRetryVerbs = FRetryVerbs(),
			const FRetryDomainsPtr& InRetryDomains = FRetryDomainsPtr()
			);

		CONVAIHTTP_API virtual ~FManager() = default;

        /**
         * Updates the entries in the list of retry requests. Optional parameters are for future connection health assessment
         *
         * @param FileCount       optional parameter that will be filled with the total files updated
         * @param FailingCount    optional parameter that will be filled with the total files that have are in a retrying state
         * @param FailedCount     optional parameter that will be filled with the total files that have failed
         * @param CompletedCount  optional parameter that will be filled with the total files that have completed
         *
         * @return                true if there are no failures or retries
         */
        CONVAIHTTP_API bool Update(uint32* FileCount = NULL, uint32* FailingCount = NULL, uint32* FailedCount = NULL, uint32* CompletedCount = NULL);
		CONVAIHTTP_API void SetRandomFailureRate(float Value) { RandomFailureRate = FRandomFailureRateSetting(Value); }
		CONVAIHTTP_API void SetDefaultRetryLimit(uint32 Value) { RetryLimitCountDefault = FRetryLimitCountSetting(Value); }
		
		// @return Block the current process until all requests are flushed, or timeout has elapsed
		CONVAIHTTP_API void BlockUntilFlushed(float TimeoutSec);

    protected:
		friend class FRequest;

        struct FConvaihttpRetryRequestEntry
        {
            FConvaihttpRetryRequestEntry(TSharedRef<FRequest, ESPMode::ThreadSafe>& InRequest);

            bool                    bShouldCancel;
            uint32                  CurrentRetryCount;
            double                  RequestStartTimeAbsoluteSeconds;
            double                  LockoutEndTimeAbsoluteSeconds;

            TSharedRef<FRequest, ESPMode::ThreadSafe>	Request;
        };

		bool ProcessRequest(TSharedRef<FRequest, ESPMode::ThreadSafe>& ConvaihttpRequest);
		void CancelRequest(TSharedRef<FRequest, ESPMode::ThreadSafe>& ConvaihttpRequest);

        // @return true if there is a no formal response to the request
        // @TODO return true if a variety of 5xx errors are the result of a formal response
        bool ShouldRetry(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry);

        // @return true if retry chances have not been exhausted
        bool CanRetry(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry);

        // @return true if the retry request has timed out
        bool HasTimedOut(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry, const double NowAbsoluteSeconds);

        // @return number of seconds to lockout for
        float GetLockoutPeriodSeconds(const FConvaihttpRetryRequestEntry& ConvaihttpRetryRequestEntry);

        // Default configuration for the retry system
        FRandomFailureRateSetting            RandomFailureRate;
        FRetryLimitCountSetting              RetryLimitCountDefault;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsDefault;

        TArray64<FConvaihttpRetryRequestEntry>        RequestList;
    };
}
