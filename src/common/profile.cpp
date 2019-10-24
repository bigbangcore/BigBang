// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "profile.h"

#include <compacttv.h>
#include <stream/datastream.h>
#include <util.h>

#include "logger.h"

using namespace std;
using namespace xengine;

//////////////////////////////
// CProfile

bool CProfile::Save(std::vector<unsigned char>& vchProfile)
{
    try
    {
        CCompactTagValue encoder;
        encoder.Push(PROFILE_VERSION, nVersion);
        encoder.Push(PROFILE_NAME, strName);
        encoder.Push(PROFILE_SYMBOL, strSymbol);
        encoder.Push(PROFILE_FLAG, nFlag);
        encoder.Push(PROFILE_AMOUNT, nAmount);
        encoder.Push(PROFILE_MINTREWARD, nMintReward);
        encoder.Push(PROFILE_MINTXFEE, nMinTxFee);
        encoder.Push(PROFILE_HALVECYCLE, nHalveCycle);

        vector<unsigned char> vchDestOwner;
        CODataStream os(vchDestOwner);
        os << destOwner.prefix << destOwner.data;
        encoder.Push(PROFILE_OWNER, vchDestOwner);

        if (hashParent != 0)
        {
            encoder.Push(PROFILE_PARENT, hashParent);
            encoder.Push(PROFILE_JOINTHEIGHT, nJointHeight);
        }

        encoder.Encode(vchProfile);
    }
    catch (const exception& e)
    {
        LOG_ERROR("CProfile", "Save error: %s", e.what());
        return false;
    }
    return true;
}

bool CProfile::Load(const vector<unsigned char>& vchProfile)
{
    SetNull();
    CIDataStream is(vchProfile);
    try
    {
        CCompactTagValue decoder;
        decoder.Decode(vchProfile);
        if (!decoder.Get(PROFILE_VERSION, nVersion))
        {
            return false;
        }
        if (nVersion != 1)
        {
            return false;
        }
        if (!decoder.Get(PROFILE_NAME, strName))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_SYMBOL, strSymbol))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_FLAG, nFlag))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_AMOUNT, nAmount))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_MINTREWARD, nMintReward))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_MINTXFEE, nMinTxFee))
        {
            return false;
        }
        if (!decoder.Get(PROFILE_HALVECYCLE, nHalveCycle))
        {
            return false;
        }

        if (!decoder.Get(PROFILE_PARENT, hashParent))
        {
            hashParent = 0;
        }

        if (!decoder.Get(PROFILE_JOINTHEIGHT, nJointHeight))
        {
            if (hashParent != 0)
            {
                return false;
            }
            nJointHeight = -1;
        }

        vector<unsigned char> vchDestOwner;
        if (decoder.Get(PROFILE_OWNER, vchDestOwner))
        {
            CIDataStream is(vchDestOwner);
            is >> destOwner.prefix >> destOwner.data;
        }
    }
    catch (const exception& e)
    {
        LOG_ERROR("CProfile", "Load error: %s", e.what());
        return false;
    }
    return true;
}
