/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Encapsulate a mutex for thread synchronization.
 */

#pragma once
#include <pthread.h>


class Mutex
{
public:
    /*******************************************************************************
    **
    ** Function:        Mutex
    **
    ** Description:     Initialize member variables.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    Mutex ();


    /*******************************************************************************
    **
    ** Function:        ~Mutex
    **
    ** Description:     Cleanup all resources.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    ~Mutex ();


    /*******************************************************************************
    **
    ** Function:        lock
    **
    ** Description:     Block the thread and try lock the mutex.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void lock ();


    /*******************************************************************************
    **
    ** Function:        unlock
    **
    ** Description:     Unlock a mutex to unblock a thread.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void unlock ();


    /*******************************************************************************
    **
    ** Function:        tryLock
    **
    ** Description:     Try to lock the mutex.
    **
    ** Returns:         True if the mutex is locked.
    **
    *******************************************************************************/
    bool tryLock ();


    /*******************************************************************************
    **
    ** Function:        nativeHandle
    **
    ** Description:     Get the handle of the mutex.
    **
    ** Returns:         Handle of the mutex.
    **
    *******************************************************************************/
    pthread_mutex_t* nativeHandle ();

    class Autolock {
        public:
            inline Autolock(Mutex& mutex) : mLock(mutex)  { mLock.lock(); }
            inline Autolock(Mutex* mutex) : mLock(*mutex) { mLock.lock(); }
            inline ~Autolock() { mLock.unlock(); }
        private:
            Mutex& mLock;
    };


private:
    pthread_mutex_t mMutex;
};

typedef Mutex::Autolock AutoMutex;
