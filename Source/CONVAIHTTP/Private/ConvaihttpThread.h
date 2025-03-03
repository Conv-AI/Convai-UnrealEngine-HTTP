// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "ConvaihttpPackage.h"
#include "Misc/SingleThreadRunnable.h"
#include "Containers/Queue.h"

class IConvaihttpThreadedRequest;

/**
 * Manages Convaihttp thread
 * Assumes any requests entering the system will remain valid (not deleted) until they exit the system
 */
class FConvaihttpThread
	: FRunnable, FSingleThreadRunnable
{
public:

	FConvaihttpThread();
	virtual ~FConvaihttpThread();

	/** 
	 * Start the CONVAIHTTP thread.
	 */
	void StartThread();

	/** 
	 * Stop the CONVAIHTTP thread.  Blocks until thread has stopped.
	 */
	void StopThread();

	/**
	 * Is the CONVAIHTTP thread started or stopped.
	 */
	bool IsStopped() const { return bIsStopped; }
	/** 
	 * Add a request to begin processing on CONVAIHTTP thread.
	 *
	 * @param Request the request to be processed on the CONVAIHTTP thread
	 */
	void AddRequest(IConvaihttpThreadedRequest* Request);

	/** 
	 * Mark a request as cancelled.    Called on non-CONVAIHTTP thread.
	 *
	 * @param Request the request to be processed on the CONVAIHTTP thread
	 */
	void CancelRequest(IConvaihttpThreadedRequest* Request);

	/** 
	 * Get completed requests.  Clears internal arrays.  Called on non-CONVAIHTTP thread.
	 *
	 * @param OutCompletedRequests array of requests that have been completed
	 */
	void GetCompletedRequests(TArray64<IConvaihttpThreadedRequest*>& OutCompletedRequests);

	//~ Begin FSingleThreadRunnable Interface
	// Cannot be overriden to ensure identical behavior with the threaded tick
	virtual void Tick() override final;
	//~ End FSingleThreadRunnable Interface

	/**
	 * When true the owner of the CONVAIHTTPThread needs to manually call Tick() since no automomous threads are
	 * executing the runnable object
	 */
	bool NeedsSingleThreadTick() const;

	/**
	 * Update configuration. Called when config has been updated and we need to apply any changes.
	 */
	virtual void UpdateConfigs();

protected:

	/**
	 * Tick on convaihttp thread
	 */
	virtual void ConvaihttpThreadTick(float DeltaSeconds);
	
	/** 
	 * Start processing a request on the convaihttp thread
	 */
	virtual bool StartThreadedRequest(IConvaihttpThreadedRequest* Request);

	/** 
	 * Complete a request on the convaihttp thread
	 */
	virtual void CompleteThreadedRequest(IConvaihttpThreadedRequest* Request);


protected:
	// Threading functions

	//~ Begin FRunnable Interface
	virtual bool Init() override;
	virtual uint32 Run() override final;
	virtual void Stop() override;
	virtual void Exit() override;
	//~ End FRunnable Interface

	/**
	*  FSingleThreadRunnable accessor for ticking this FRunnable when multi-threading is disabled.
	*  @return FSingleThreadRunnable Interface for this FRunnable object.
	*/
	virtual class FSingleThreadRunnable* GetSingleThreadInterface() override { return this; }

	void Process(TArray64<IConvaihttpThreadedRequest*>& RequestsToCancel, TArray64<IConvaihttpThreadedRequest*>& RequestsToComplete);

	/** signal request to stop and exit thread */
	FThreadSafeCounter ExitRequest;

	/** Time in seconds to use as frame time when actively processing requests. 0 means no frame time. */
	double ConvaihttpThreadActiveFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when actively processing requests. */
	double ConvaihttpThreadActiveMinimumSleepTimeInSeconds;
	/** Time in seconds to use as frame time when idle, waiting for requests. 0 means no frame time. */
	double ConvaihttpThreadIdleFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when idle, waiting for requests. */
	double ConvaihttpThreadIdleMinimumSleepTimeInSeconds;
	/** Last time the thread has been processed. Used in the non-game thread. */
	double LastTime;

protected:
	/** 
	 * Threaded requests that are waiting to be processed on the convaihttp thread.
	 * Added to on (any) non-CONVAIHTTP thread, processed then cleared on CONVAIHTTP thread.
	 */
	TQueue<IConvaihttpThreadedRequest*, EQueueMode::Mpsc> NewThreadedRequests;

	/**
	 * Threaded requests that are waiting to be cancelled on the convaihttp thread.
	 * Added to on (any) non-CONVAIHTTP thread, processed then cleared on CONVAIHTTP thread.
	 */
	TQueue<IConvaihttpThreadedRequest*, EQueueMode::Mpsc> CancelledThreadedRequests;

	/**
	 * Threaded requests that are ready to run, but waiting due to the running request limit (not in any of the other lists, except potentially CancelledThreadedRequests).
	 * Only accessed on the CONVAIHTTP thread.
	 */
	TArray64<IConvaihttpThreadedRequest*> RateLimitedThreadedRequests;

	/**
	 * Currently running threaded requests (not in any of the other lists, except potentially CancelledThreadedRequests).
	 * Only accessed on the CONVAIHTTP thread.
	 */
	TArray64<IConvaihttpThreadedRequest*> RunningThreadedRequests;

	/**
	 * Threaded requests that have completed and are waiting for the game thread to process.
	 * Added to on CONVAIHTTP thread, processed then cleared on game thread (Single producer, single consumer)
	 */
	TQueue<IConvaihttpThreadedRequest*, EQueueMode::Spsc> CompletedThreadedRequests;

	/** Pointer to Runnable Thread */
	FRunnableThread* Thread;

private:

	/** Are we holding a fake thread and we need to be ticked manually when Flushing */
	bool bIsSingleThread;

	/** Tells if the runnable thread is running or stopped */
	bool bIsStopped;

	/** Limit for threaded convaihttp requests running at the same time. If not specified through configuration values, there will be no limit */
	int32 RunningThreadedRequestLimit = INT_MAX;
};
