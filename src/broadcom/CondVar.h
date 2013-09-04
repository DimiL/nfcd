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
    CondVar ();

    ~CondVar ();

    void wait (Mutex& mutex);

    bool wait (Mutex& mutex, long millisec);

    void notifyOne ();

private:
    pthread_cond_t mCondition;
};
