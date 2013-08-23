/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Encapsulate a condition variable for thread synchronization.
 */

#pragma once
#include <pthread.h>
#include "Mutex.h"


class CondVar
{
public:
    /*******************************************************************************
    **
    ** Function:        CondVar
    **
    ** Description:     Initialize member variables.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    CondVar ();


    /*******************************************************************************
    **
    ** Function:        ~CondVar
    **
    ** Description:     Cleanup all resources.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    ~CondVar ();


    /*******************************************************************************
    **
    ** Function:        wait
    **
    ** Description:     Block the caller and wait for a condition.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void wait (Mutex& mutex);


    /*******************************************************************************
    **
    ** Function:        wait
    **
    ** Description:     Block the caller and wait for a condition.
    **                  millisec: Timeout in milliseconds.
    **
    ** Returns:         True if wait is successful; false if timeout occurs.
    **
    *******************************************************************************/
    bool wait (Mutex& mutex, long millisec);


    /*******************************************************************************
    **
    ** Function:        notifyOne
    **
    ** Description:     Unblock the waiting thread.
    **
    ** Returns:         None.
    **
    *******************************************************************************/
    void notifyOne ();

private:
    pthread_cond_t mCondition;
};
