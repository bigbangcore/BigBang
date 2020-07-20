// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegateverify.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace delegate
{

//////////////////////////////
// CDelegateVerify

CDelegateVerify::CDelegateVerify(const map<CDestination, size_t>& mapWeight,
                                 const map<CDestination, vector<unsigned char>>& mapEnrollData)
{
    Enroll(mapWeight, mapEnrollData);
}

bool CDelegateVerify::VerifyProof(const vector<unsigned char>& vchProof, uint256& nAgreement,
                                  size_t& nWeight, map<CDestination, size_t>& mapBallot, bool fCheckRepeated)
{
    uint256 nAgreementParse;
    try
    {
        unsigned char nWeightParse;
        vector<CDelegateData> vPublish;
        CIDataStream is(vchProof);
        is >> nWeightParse >> nAgreementParse;
        if (nWeightParse == 0 && nAgreementParse == 0)
        {
            return true;
        }
        is >> vPublish;
        for (int i = 0; i < vPublish.size(); i++)
        {
            const CDelegateData& delegateData = vPublish[i];
            if (!witness.IsCollectCompleted()
                && (!VerifySignature(delegateData) || !witness.Collect(delegateData.nIdentFrom, delegateData.mapShare, fCheckRepeated)))
            {
                return false;
            }
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    GetAgreement(nAgreement, nWeight, mapBallot);

    return (nAgreement == nAgreementParse);
}

} // namespace delegate
} // namespace bigbang
