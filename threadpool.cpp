#include <thread>
#include <chrono>
#include <mutex>
#include <stdio.h>
#include "threadpool.h"

namespace Thread
{
	void poolWorker(void *aParam);

	void Pool::init(int aThreadCount)
	{
		if (aThreadCount > 0)
		{
			mMaxTask = 0;
			mWorkMutex = new std::mutex();
			mRunning = 1;
			mTasksRunning = 0;
			mThreadCount = aThreadCount;
			mThread = new std::thread*[aThreadCount];
			int i;
			for (i = 0; i < mThreadCount; i++)
			{
				mThread[i] = new std::thread(poolWorker, this);
			}
		}
	}

	Pool::Pool()
	{
		for (int i = 0; i < MAX_THREADPOOL_TASKS; i++)
			mTaskArray[i] = 0;
		mTasksRunning = 0;
		mRunning = 0;
		mThreadCount = 0;
		mThread = 0;
		mWorkMutex = 0;
		mRobin = 0;
		mMaxTask = 0;
	}

	Pool::~Pool()
	{
		mRunning = 0;
		int i;
		for (i = 0; i < mThreadCount; i++)
		{
			mThread[i]->join();
			delete mThread[i];
		}
		delete[] mThread;
		if (mWorkMutex)
			delete mWorkMutex;
	}

	void Pool::addWork(PoolTask *aTask)
	{
		if (mThreadCount == 0)
		{
			aTask->work();
		}
		else
		{
			if (mWorkMutex) mWorkMutex->lock();
			if (mMaxTask == MAX_THREADPOOL_TASKS)
			{
				// If we're at max tasks, do the task on calling thread 
				// (we're in trouble anyway, might as well slow down adding more work)
				if (mWorkMutex) mWorkMutex->unlock();
				aTask->work();
			}
			else
			{
				mTaskArray[mMaxTask] = aTask;
				mMaxTask++;
				if (mWorkMutex) mWorkMutex->unlock();
			}
		}
	}

	PoolTask * Pool::getWork()
	{
		PoolTask *t = 0;
		if (mWorkMutex) mWorkMutex->lock();
		if (mMaxTask > 0)
		{
			int r = mRobin % mMaxTask;
			mRobin++;
			t = mTaskArray[r];
			mTaskArray[r] = mTaskArray[mMaxTask - 1];
			mMaxTask--;
			mTasksRunning++;
		}
		if (mWorkMutex) mWorkMutex->unlock();
		return t;
	}

	void Pool::doneWork()
	{
		if (mWorkMutex) mWorkMutex->lock();
		mTasksRunning--;
		if (mWorkMutex) mWorkMutex->unlock();
	}

	void Pool::waitUntilDone(int aSleep, bool aPrintout)
	{
		while (mMaxTask + mTasksRunning) 
		{ 
			std::this_thread::sleep_for(std::chrono::milliseconds(aSleep));
			if (aPrintout)
			{
				printf("\r%d tasks left     ", mMaxTask + mTasksRunning);
			}
		}
		printf("\r                          \r");
	}

	void poolWorker(void *aParam)
	{
		Pool *myPool = (Pool*)aParam;
		while (myPool->mRunning)
		{
			PoolTask *t = myPool->getWork();
			if (!t)
			{
				std::this_thread::yield();
			}
			else
			{
				t->work();				
				if (t->mDeleteTask)
					delete t;
				myPool->doneWork();
			}
		}
	}
}