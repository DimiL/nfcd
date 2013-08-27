/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Encapsulate a mutex for thread synchronization.
 */

#include "Mutex.h"
//#include "NfcJniUtil.h"
#include <errno.h>

/*******************************************************************************
**
** Function:        Mutex
**
** Description:     Initialize member variables.
**
** Returns:         None.
**
*******************************************************************************/
Mutex::Mutex ()
{
    memset (&mMutex, 0, sizeof(mMutex));
    int res = pthread_mutex_init (&mMutex, NULL);
    if (res != 0)
    {
        //LOGE ("Mutex::Mutex: fail init; error=0x%X", res);
    }
}


/*******************************************************************************
**
** Function:        ~Mutex
**
** Description:     Cleanup all resources.
**
** Returns:         None.
**
*******************************************************************************/
Mutex::~Mutex ()
{
    int res = pthread_mutex_destroy (&mMutex);
    if (res != 0)
    {
        //ALOGE ("Mutex::~Mutex: fail destroy; error=0x%X", res);
    }
}


/*******************************************************************************
**
** Function:        lock
**
** Description:     Block the thread and try lock the mutex.
**
** Returns:         None.
**
*******************************************************************************/
void Mutex::lock ()
{
    int res = pthread_mutex_lock (&mMutex);
    if (res != 0)
    {
        //ALOGE ("Mutex::lock: fail lock; error=0x%X", res);
    }
}


/*******************************************************************************
**
** Function:        unlock
**
** Description:     Unlock a mutex to unblock a thread.
**
** Returns:         None.
**
*******************************************************************************/
void Mutex::unlock ()
{
    int res = pthread_mutex_unlock (&mMutex);
    if (res != 0)
    {
        //ALOGE ("Mutex::unlock: fail unlock; error=0x%X", res);
    }
}


/*******************************************************************************
**
** Function:        tryLock
**
** Description:     Try to lock the mutex.
**
** Returns:         True if the mutex is locked.
**
*******************************************************************************/
bool Mutex::tryLock ()
{
    int res = pthread_mutex_trylock (&mMutex);
    if ((res != 0) && (res != EBUSY))
    {
        //ALOGE ("Mutex::tryLock: error=0x%X", res);
    }
    return res == 0;
}


/*******************************************************************************
**
** Function:        nativeHandle
**
** Description:     Get the handle of the mutex.
**
** Returns:         Handle of the mutex.
**
*******************************************************************************/
pthread_mutex_t* Mutex::nativeHandle ()
{
    return &mMutex;
}


