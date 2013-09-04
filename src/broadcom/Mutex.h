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
    Mutex ();

    ~Mutex ();

    void lock ();

    void unlock ();

    bool tryLock ();

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
