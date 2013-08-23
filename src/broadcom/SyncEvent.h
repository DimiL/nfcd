/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Synchronize two or more threads using a condition variable and a mutex.
 */
#pragma once
#include "CondVar.h"
#include "Mutex.h"


class SyncEvent
{
public:
    /*******************************************************************************
    **
    ** Function:        ~SyncEvent
    **
    ** Description:     Cleanup all resources.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    ~SyncEvent ()
    {
    }


    /*******************************************************************************
    **
    ** Function:        start
    **
    ** Description:     Start a synchronization operation.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void start ()
    {
        mMutex.lock ();
    }


    /*******************************************************************************
    **
    ** Function:        wait
    **
    ** Description:     Block the thread and wait for the event to occur.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void wait ()
    {
        mCondVar.wait (mMutex);
    }


    /*******************************************************************************
    **
    ** Function:        wait
    **
    ** Description:     Block the thread and wait for the event to occur.
    **                  millisec: Timeout in milliseconds.
    **
    ** Returns:         True if wait is successful; false if timeout occurs.
    **
    *******************************************************************************/
    bool wait (long millisec)
    {
        bool retVal = mCondVar.wait (mMutex, millisec);
        return retVal;
    }


    /*******************************************************************************
    **
    ** Function:        notifyOne
    **
    ** Description:     Notify a blocked thread that the event has occured. Unblocks it.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void notifyOne ()
    {
        mCondVar.notifyOne ();
    }


    /*******************************************************************************
    **
    ** Function:        end
    **
    ** Description:     End a synchronization operation.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void end ()
    {
        mMutex.unlock ();
    }

private:
    CondVar mCondVar;
    Mutex mMutex;
};


/*****************************************************************************/
/*****************************************************************************/


/*****************************************************************************
**
**  Name:           SyncEventGuard
**
**  Description:    Automatically start and end a synchronization event.
**
*****************************************************************************/
class SyncEventGuard
{
public:
    /*******************************************************************************
    **
    ** Function:        SyncEventGuard
    **
    ** Description:     Start a synchronization operation.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    SyncEventGuard (SyncEvent& event)
    :   mEvent (event)
    {
        event.start (); //automatically start operation
    };


    /*******************************************************************************
    **
    ** Function:        ~SyncEventGuard
    **
    ** Description:     End a synchronization operation.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    ~SyncEventGuard ()
    {
        mEvent.end (); //automatically end operation
    };

private:
    SyncEvent& mEvent;
};

