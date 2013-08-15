/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 *  Tag-reading, tag-writing operations.
 */
#include "OverrideLog.h"
#include "NfcTag.h"
//#include "JavaClassConstants.h"
//#include <ScopedLocalRef.h>
//#include <ScopedPrimitiveArray.h>

extern "C"
{
    #include "rw_int.h"
}


/*******************************************************************************
**
** Function:        NfcTag
**
** Description:     Initialize member variables.
**
** Returns:         None
**
*******************************************************************************/
NfcTag::NfcTag ()
:   mActivationState (Idle),
    mProtocol(NFC_PROTOCOL_UNKNOWN),
    mNumTechList (0),
    mtT1tMaxMessageSize (0),
    mReadCompletedStatus (NFA_STATUS_OK),
    mLastKovioUidLen (0),
    mNdefDetectionTimedOut (false)
{
    memset (mTechList, 0, sizeof(mTechList));
    memset (mTechHandles, 0, sizeof(mTechHandles));
    memset (mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
    memset (mTechParams, 0, sizeof(mTechParams));
    memset(mLastKovioUid, 0, NFC_KOVIO_MAX_LEN);
}


/*******************************************************************************
**
** Function:        getInstance
**
** Description:     Get a reference to the singleton NfcTag object.
**
** Returns:         Reference to NfcTag object.
**
*******************************************************************************/
NfcTag& NfcTag::getInstance ()
{
    static NfcTag tag;
    return tag;
}


/*******************************************************************************
**
** Function:        initialize
**
** Description:     Reset member variables.
**
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::initialize ()
{
    mActivationState = Idle;
    mProtocol = NFC_PROTOCOL_UNKNOWN;
    mNumTechList = 0;
    mtT1tMaxMessageSize = 0;
    mReadCompletedStatus = NFA_STATUS_OK;
    resetTechnologies ();
}


/*******************************************************************************
**
** Function:        abort
**
** Description:     Unblock all operations.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::abort ()
{
    SyncEventGuard g (mReadCompleteEvent);
    mReadCompleteEvent.notifyOne ();
}


/*******************************************************************************
**
** Function:        getActivationState
**
** Description:     What is the current state: Idle, Sleep, or Activated.
**
** Returns:         Idle, Sleep, or Activated.
**
*******************************************************************************/
NfcTag::ActivationState NfcTag::getActivationState ()
{
    return mActivationState;
}


/*******************************************************************************
**
** Function:        setDeactivationState
**
** Description:     Set the current state: Idle or Sleep.
**                  deactivated: state of deactivation.
**
** Returns:         None.
**
*******************************************************************************/
void NfcTag::setDeactivationState (tNFA_DEACTIVATED& deactivated)
{
    static const char fn [] = "NfcTag::setDeactivationState";
    mActivationState = Idle;
    mNdefDetectionTimedOut = false;
    if (deactivated.type == NFA_DEACTIVATE_TYPE_SLEEP)
        mActivationState = Sleep;
    ALOGD ("%s: state=%u", fn, mActivationState);
}


/*******************************************************************************
**
** Function:        setActivationState
**
** Description:     Set the current state to Active.
**
** Returns:         None.
**
*******************************************************************************/
void NfcTag::setActivationState ()
{
    static const char fn [] = "NfcTag::setActivationState";
    mNdefDetectionTimedOut = false;
    mActivationState = Active;
    ALOGD ("%s: state=%u", fn, mActivationState);
}


/*******************************************************************************
**
** Function:        getProtocol
**
** Description:     Get the protocol of the current tag.
**
** Returns:         Protocol number.
**
*******************************************************************************/
tNFC_PROTOCOL NfcTag::getProtocol()
{
    return mProtocol;
}

/*******************************************************************************
**
** Function         TimeDiff
**
** Description      Computes time difference in milliseconds.
**
** Returns          Time difference in milliseconds
**
*******************************************************************************/
UINT32 TimeDiff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0)
    {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }

    return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

/*******************************************************************************
**
** Function:        IsSameKovio
**
** Description:     Checks if tag activate is the same (UID) Kovio tag previously
**                  activated.  This is needed due to a problem with some Kovio
**                  tags re-activating multiple times.
**                  activationData: data from activation.
**
** Returns:         true if the activation is from the same tag previously
**                  activated, false otherwise
**
*******************************************************************************/
bool NfcTag::IsSameKovio(tNFA_ACTIVATED& activationData)
{
    static const char fn [] = "NfcTag::IsSameKovio";
    ALOGD ("%s: enter", fn);
    tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

    if (rfDetail.protocol != NFC_PROTOCOL_KOVIO)
        return false;

    memcpy (&(mTechParams[0]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
    if (mTechParams [0].mode != NFC_DISCOVERY_TYPE_POLL_KOVIO)
        return false;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    bool rVal = false;
    if (mTechParams[0].param.pk.uid_len == mLastKovioUidLen)
    {
        if (memcmp(mLastKovioUid, &mTechParams [0].param.pk.uid, mTechParams[0].param.pk.uid_len) == 0)
        {
            //same tag
            if (TimeDiff(mLastKovioTime, now) < 500)
            {
                // same tag within 500 ms, ignore activation
                rVal = true;
            }
        }
    }

    // save Kovio tag info
    if (!rVal)
    {
        if ((mLastKovioUidLen = mTechParams[0].param.pk.uid_len) > NFC_KOVIO_MAX_LEN)
            mLastKovioUidLen = NFC_KOVIO_MAX_LEN;
        memcpy(mLastKovioUid, mTechParams[0].param.pk.uid, mLastKovioUidLen);
    }
    mLastKovioTime = now;
    ALOGD ("%s: exit, is same Kovio=%d", fn, rVal);
    return rVal;
}

/*******************************************************************************
**
** Function:        discoverTechnologies
**
** Description:     Discover the technologies that NFC service needs by interpreting
**                  the data strucutures from the stack.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::discoverTechnologies (tNFA_ACTIVATED& activationData)
{
    static const char fn [] = "NfcTag::discoverTechnologies (activation)";
    ALOGD ("%s: enter", fn);
    tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

    mNumTechList = 0;
    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
    mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;

    //save the stack's data structure for interpretation later
    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

    switch (rfDetail.protocol)
    {
    case NFC_PROTOCOL_T1T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
        break;

    case NFC_PROTOCOL_T2T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
        // could be MifFare UL or Classic or Kovio
        {
            // need to look at first byte of uid to find manuf.
            tNFC_RF_TECH_PARAMS tech_params;
            memcpy (&tech_params, &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

            if ((tech_params.param.pa.nfcid1[0] == 0x04 && rfDetail.rf_tech_param.param.pa.sel_rsp == 0) ||
                rfDetail.rf_tech_param.param.pa.sel_rsp == 0x18 ||
                rfDetail.rf_tech_param.param.pa.sel_rsp == 0x08)
            {
                if (rfDetail.rf_tech_param.param.pa.sel_rsp == 0)
                {
                    mNumTechList++;
                    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
                    mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
                    //save the stack's data structure for interpretation later
                    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
                    mTechList [mNumTechList] = TARGET_TYPE_MIFARE_UL; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                }
            }
        }
        break;

    case NFC_PROTOCOL_T3T:
        mTechList [mNumTechList] = TARGET_TYPE_FELICA;
        break;

    case NFC_PROTOCOL_ISO_DEP: //type-4 tag uses technology ISO-DEP and technology A or B
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_4; //is TagTechnology.ISO_DEP by Java API
        if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
        }
        else if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
        }
        break;

    case NFC_PROTOCOL_15693: //is TagTechnology.NFC_V by Java API
        mTechList [mNumTechList] = TARGET_TYPE_ISO15693;
        break;

    case NFC_PROTOCOL_KOVIO:
        ALOGD ("%s: Kovio", fn);
        mTechList [mNumTechList] = TARGET_TYPE_KOVIO_BARCODE;
        break;

    default:
        ALOGE ("%s: unknown protocol ????", fn);
        mTechList [mNumTechList] = TARGET_TYPE_UNKNOWN;
        break;
    }

    mNumTechList++;
    for (int i=0; i < mNumTechList; i++)
    {
        ALOGD ("%s: index=%d; tech=%d; handle=%d; nfc type=%d", fn,
                i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
    }
    ALOGD ("%s: exit", fn);
}


/*******************************************************************************
**
** Function:        discoverTechnologies
**
** Description:     Discover the technologies that NFC service needs by interpreting
**                  the data strucutures from the stack.
**                  discoveryData: data from discovery events(s).
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::discoverTechnologies (tNFA_DISC_RESULT& discoveryData)
{
    static const char fn [] = "NfcTag::discoverTechnologies (discovery)";
    tNFC_RESULT_DEVT& discovery_ntf = discoveryData.discovery_ntf;

    ALOGD ("%s: enter: rf disc. id=%u; protocol=%u, mNumTechList=%u", fn, discovery_ntf.rf_disc_id, discovery_ntf.protocol, mNumTechList);
    if (mNumTechList >= MAX_NUM_TECHNOLOGY)
    {
        ALOGE ("%s: exceed max=%d", fn, MAX_NUM_TECHNOLOGY);
        goto TheEnd;
    }
    mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
    mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;

    //save the stack's data structure for interpretation later
    memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));

    switch (discovery_ntf.protocol)
    {
    case NFC_PROTOCOL_T1T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
        break;

    case NFC_PROTOCOL_T2T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
        //type-2 tags are identitical to Mifare Ultralight, so Ultralight is also discovered
        if (discovery_ntf.rf_tech_param.param.pa.sel_rsp == 0)
        {
            // mifare Ultralight
            mNumTechList++;
            mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_MIFARE_UL; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
        }

        //save the stack's data structure for interpretation later
        memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
        break;

    case NFC_PROTOCOL_T3T:
        mTechList [mNumTechList] = TARGET_TYPE_FELICA;
        break;

    case NFC_PROTOCOL_ISO_DEP: //type-4 tag uses technology ISO-DEP and technology A or B
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_4; //is TagTechnology.ISO_DEP by Java API
        if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
        }
        else if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
        }
        break;

    case NFC_PROTOCOL_15693: //is TagTechnology.NFC_V by Java API
        mTechList [mNumTechList] = TARGET_TYPE_ISO15693;
        break;

    default:
        ALOGE ("%s: unknown protocol ????", fn);
        mTechList [mNumTechList] = TARGET_TYPE_UNKNOWN;
        break;
    }

    mNumTechList++;
    if (discovery_ntf.more == FALSE)
    {
        for (int i=0; i < mNumTechList; i++)
        {
            ALOGD ("%s: index=%d; tech=%d; handle=%d; nfc type=%d", fn,
                    i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
        }
    }

TheEnd:
    ALOGD ("%s: exit", fn);
}


/*******************************************************************************
**
** Function:        createNativeNfcTag
**
** Description:     Create a brand new Java NativeNfcTag object;
**                  fill the objects's member variables with data;
**                  notify NFC service;
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::createNativeNfcTag (tNFA_ACTIVATED& activationData)
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        fillNativeNfcTagMembers1
**
** Description:     Fill NativeNfcTag's members: mProtocols, mTechList, mTechHandles, mTechLibNfcTypes.
**                  e: JVM environment.
**                  tag_cls: Java NativeNfcTag class.
**                  tag: Java NativeNfcTag object.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::fillNativeNfcTagMembers1 ()
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        fillNativeNfcTagMembers2
**
** Description:     Fill NativeNfcTag's members: mConnectedTechIndex or mConnectedTechnology.
**                  The original Google's implementation is in set_target_pollBytes(
**                  in com_android_nfc_NativeNfcTag.cpp;
**                  e: JVM environment.
**                  tag_cls: Java NativeNfcTag class.
**                  tag: Java NativeNfcTag object.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
//fill NativeNfcTag's members: mHandle, mConnectedTechnology
void NfcTag::fillNativeNfcTagMembers2 ()
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        fillNativeNfcTagMembers3
**
** Description:     Fill NativeNfcTag's members: mTechPollBytes.
**                  The original Google's implementation is in set_target_pollBytes(
**                  in com_android_nfc_NativeNfcTag.cpp;
**                  e: JVM environment.
**                  tag_cls: Java NativeNfcTag class.
**                  tag: Java NativeNfcTag object.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::fillNativeNfcTagMembers3 ()
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        fillNativeNfcTagMembers4
**
** Description:     Fill NativeNfcTag's members: mTechActBytes.
**                  The original Google's implementation is in set_target_activationBytes()
**                  in com_android_nfc_NativeNfcTag.cpp;
**                  e: JVM environment.
**                  tag_cls: Java NativeNfcTag class.
**                  tag: Java NativeNfcTag object.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::fillNativeNfcTagMembers4 ()
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        fillNativeNfcTagMembers5
**
** Description:     Fill NativeNfcTag's members: mUid.
**                  The original Google's implementation is in nfc_jni_Discovery_notification_callback()
**                  in com_android_nfc_NativeNfcManager.cpp;
**                  e: JVM environment.
**                  tag_cls: Java NativeNfcTag class.
**                  tag: Java NativeNfcTag object.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::fillNativeNfcTagMembers5 ()
{
    // Dimi : Remove JNI related code and need to be re-implement
}


/*******************************************************************************
**
** Function:        isP2pDiscovered
**
** Description:     Does the peer support P2P?
**
** Returns:         True if the peer supports P2P.
**
*******************************************************************************/
bool NfcTag::isP2pDiscovered ()
{
    static const char fn [] = "NfcTag::isP2pDiscovered";
    bool retval = false;

    for (int i = 0; i < mNumTechList; i++)
    {
        if (mTechLibNfcTypes[i] == NFA_PROTOCOL_NFC_DEP)
        {
            //if remote device supports P2P
            ALOGD ("%s: discovered P2P", fn);
            retval = true;
            break;
        }
    }
    ALOGD ("%s: return=%u", fn, retval);
    return retval;
}


/*******************************************************************************
**
** Function:        selectP2p
**
** Description:     Select the preferred P2P technology if there is a choice.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectP2p()
{
    static const char fn [] = "NfcTag::selectP2p";
    UINT8 rfDiscoveryId = 0;

    for (int i = 0; i < mNumTechList; i++)
    {
        //if remote device does not support P2P, just skip it
        if (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
            continue;

        //if remote device supports tech F;
        //tech F is preferred because it is faster than tech A
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE) )
        {
            rfDiscoveryId = mTechHandles[i];
            break; //no need to search further
        }
        else if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) )
        {
            //only choose tech A if tech F is unavailable
            if (rfDiscoveryId == 0)
                rfDiscoveryId = mTechHandles[i];
        }
    }

    if (rfDiscoveryId > 0)
    {
        ALOGD ("%s: select P2P; target rf discov id=0x%X", fn, rfDiscoveryId);
        tNFA_STATUS stat = NFA_Select (rfDiscoveryId, NFA_PROTOCOL_NFC_DEP, NFA_INTERFACE_NFC_DEP);
        if (stat != NFA_STATUS_OK)
            ALOGE ("%s: fail select P2P; error=0x%X", fn, stat);
    }
    else
        ALOGE ("%s: cannot find P2P", fn);
    resetTechnologies ();
}


/*******************************************************************************
**
** Function:        resetTechnologies
**
** Description:     Clear all data related to the technology, protocol of the tag.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::resetTechnologies ()
{
    static const char fn [] = "NfcTag::resetTechnologies";
    ALOGD ("%s", fn);
   	mNumTechList = 0;
    memset (mTechList, 0, sizeof(mTechList));
    memset (mTechHandles, 0, sizeof(mTechHandles));
    memset (mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
    memset (mTechParams, 0, sizeof(mTechParams));
}


/*******************************************************************************
**
** Function:        selectFirstTag
**
** Description:     When multiple tags are discovered, just select the first one to activate.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectFirstTag ()
{
    static const char fn [] = "NfcTag::selectFirstTag";
    ALOGD ("%s: nfa target h=0x%X; protocol=0x%X",
            fn, mTechHandles [0], mTechLibNfcTypes [0]);
	tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;

	if (mTechLibNfcTypes [0] == NFA_PROTOCOL_ISO_DEP)
	{
		rf_intf = NFA_INTERFACE_ISO_DEP;
	}
	else if (mTechLibNfcTypes [0] == NFA_PROTOCOL_NFC_DEP)
		rf_intf = NFA_INTERFACE_NFC_DEP;
	else
		rf_intf = NFA_INTERFACE_FRAME;

    tNFA_STATUS stat = NFA_Select (mTechHandles [0], mTechLibNfcTypes [0], rf_intf);
    if (stat != NFA_STATUS_OK)
        ALOGE ("%s: fail select; error=0x%X", fn, stat);
}


/*******************************************************************************
**
** Function:        getT1tMaxMessageSize
**
** Description:     Get the maximum size (octet) that a T1T can store.
**
** Returns:         Maximum size in octets.
**
*******************************************************************************/
int NfcTag::getT1tMaxMessageSize ()
{
    static const char fn [] = "NfcTag::getT1tMaxMessageSize";

    if (mProtocol != NFC_PROTOCOL_T1T)
    {
        ALOGE ("%s: wrong protocol %u", fn, mProtocol);
        return 0;
    }
    return mtT1tMaxMessageSize;
}


/*******************************************************************************
**
** Function:        calculateT1tMaxMessageSize
**
** Description:     Calculate type-1 tag's max message size based on header ROM bytes.
**                  activate: reference to activation data.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::calculateT1tMaxMessageSize (tNFA_ACTIVATED& activate)
{
    static const char fn [] = "NfcTag::calculateT1tMaxMessageSize";

    //make sure the tag is type-1
    if (activate.activate_ntf.protocol != NFC_PROTOCOL_T1T)
    {
        mtT1tMaxMessageSize = 0;
        return;
    }

    //examine the first byte of header ROM bytes
    switch (activate.params.t1t.hr[0])
    {
    case RW_T1T_IS_TOPAZ96:
        mtT1tMaxMessageSize = 90;
        break;
    case RW_T1T_IS_TOPAZ512:
        mtT1tMaxMessageSize = 462;
        break;
    default:
        ALOGE ("%s: unknown T1T HR0=%u", fn, activate.params.t1t.hr[0]);
        mtT1tMaxMessageSize = 0;
        break;
    }
}


/*******************************************************************************
**
** Function:        isMifareUltralight
**
** Description:     Whether the currently activated tag is Mifare Ultralight.
**
** Returns:         True if tag is Mifare Ultralight.
**
*******************************************************************************/
bool NfcTag::isMifareUltralight ()
{
    static const char fn [] = "NfcTag::isMifareUltralight";
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            //see NFC Digital Protocol, section 4.6.3 (SENS_RES); section 4.8.2 (SEL_RES).
            //see Mifare Type Identification Procedure, section 5.1 (ATQA), 5.2 (SAK).
            if ( (mTechParams[i].param.pa.sens_res[0] == 0x44) &&
                 (mTechParams[i].param.pa.sens_res[1] == 0) )
            {
                // SyncEventGuard g (mReadCompleteEvent);
                // mReadCompletedStatus = NFA_STATUS_BUSY;
                // ALOGD ("%s: read block 0x10", fn);
                // tNFA_STATUS stat = NFA_RwT2tRead (0x10);
                // if (stat == NFA_STATUS_OK)
                    // mReadCompleteEvent.wait ();
                //
                // //if read-completion status is failure, then the tag is
                // //definitely Mifare Ultralight;
                // //if read-completion status is OK, then the tag is
                // //definitely Mifare Ultralight C;
                // retval = (mReadCompletedStatus == NFA_STATUS_FAILED);
                retval = true;
            }
            break;
        }
    }
    ALOGD ("%s: return=%u", fn, retval);
    return retval;
}


/*******************************************************************************
**
** Function:        isT2tNackResponse
**
** Description:     Whether the response is a T2T NACK response.
**                  See NFC Digital Protocol Technical Specification (2010-11-17).
**                  Chapter 9 (Type 2 Tag Platform), section 9.6 (READ).
**                  response: buffer contains T2T response.
**                  responseLen: length of the response.
**
** Returns:         True if the response is NACK
**
*******************************************************************************/
bool NfcTag::isT2tNackResponse (const UINT8* response, UINT32 responseLen)
{
    static const char fn [] = "NfcTag::isT2tNackResponse";
    bool isNack = false;

    if (responseLen == 1)
    {
        if (response[0] == 0xA)
            isNack = false; //an ACK response, so definitely not a NACK
        else
            isNack = true; //assume every value is a NACK
    }
    ALOGD ("%s: return %u", fn, isNack);
    return isNack;
}


/*******************************************************************************
**
** Function:        isNdefDetectionTimedOut
**
** Description:     Whether NDEF-detection algorithm timed out.
**
** Returns:         True if NDEF-detection algorithm timed out.
**
*******************************************************************************/
bool NfcTag::isNdefDetectionTimedOut ()
{
    return mNdefDetectionTimedOut;
}


/*******************************************************************************
**
** Function:        connectionEventHandler
**
** Description:     Handle connection-related events.
**                  event: event code.
**                  data: pointer to event data.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::connectionEventHandler (UINT8 event, tNFA_CONN_EVT_DATA* data)
{
    static const char fn [] = "NfcTag::connectionEventHandler";

    switch (event)
    {
    case NFA_DISC_RESULT_EVT:
        {
            tNFA_DISC_RESULT& disc_result = data->disc_result;
            if (disc_result.status == NFA_STATUS_OK)
            {
                discoverTechnologies (disc_result);
            }
        }
        break;

    case NFA_ACTIVATED_EVT:
        // Only do tag detection if we are polling and it is not 'EE Direct RF' activation
        // (which may happen when we are activated as a tag).
        if (data->activated.activate_ntf.rf_tech_param.mode < NCI_DISCOVERY_TYPE_LISTEN_A
            && data->activated.activate_ntf.intf_param.type != NFC_INTERFACE_EE_DIRECT_RF)
        {
            tNFA_ACTIVATED& activated = data->activated;
            if (IsSameKovio(activated))
                break;
            mProtocol = activated.activate_ntf.protocol;
            calculateT1tMaxMessageSize (activated);
            discoverTechnologies (activated);
            createNativeNfcTag (activated);
        }
        break;

    case NFA_DEACTIVATED_EVT:
        mProtocol = NFC_PROTOCOL_UNKNOWN;
        resetTechnologies ();
        break;

    case NFA_READ_CPLT_EVT:
        {
            SyncEventGuard g (mReadCompleteEvent);
            mReadCompletedStatus = data->status;
            mReadCompleteEvent.notifyOne ();
        }
        break;

    case NFA_NDEF_DETECT_EVT:
        {
            tNFA_NDEF_DETECT& ndef_detect = data->ndef_detect;
            mNdefDetectionTimedOut = ndef_detect.status == NFA_STATUS_TIMEOUT;
            if (mNdefDetectionTimedOut)
                ALOGE ("%s: NDEF detection timed out", fn);
        }
    }
}
