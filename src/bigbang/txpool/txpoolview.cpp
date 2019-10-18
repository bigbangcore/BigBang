// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpoolview.h"

using namespace std;

namespace bigbang
{

//////////////////////////////
// CTxPoolView
void CTxPoolView::InvalidateSpent(const CTxOutPoint& out, vector<uint256>& vInvolvedTx)
{
    vector<CTxOutPoint> vOutPoint;
    vOutPoint.push_back(out);
    for (std::size_t i = 0; i < vOutPoint.size(); i++)
    {
        uint256 txidNextTx;
        if (GetSpent(vOutPoint[i], txidNextTx))
        {
            CPooledTx* pNextTx = nullptr;
            if ((pNextTx = Get(txidNextTx)) != nullptr)
            {
                for (const CTxIn& txin : pNextTx->vInput)
                {
                    SetUnspent(txin.prevout);
                }
                CTxOutPoint out0(txidNextTx, 0);
                if (IsSpent(out0))
                {
                    vOutPoint.push_back(out0);
                }
                else
                {
                    mapSpent.erase(out0);
                }
                CTxOutPoint out1(txidNextTx, 1);
                if (IsSpent(out1))
                {
                    vOutPoint.push_back(out1);
                }
                else
                {
                    mapSpent.erase(out1);
                }
                setTxLinkIndex.erase(txidNextTx);
                vInvolvedTx.push_back(txidNextTx);
            }
        }
    }
}

void CTxPoolView::ArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize) const
{
    size_t nTotalSize = 0;
    nTotalTxFee = 0;

    const CPooledTxLinkSetBySequenceNumber& idxTxLinkSeq = setTxLinkIndex.get<1>();
    CPooledTxLinkSetBySequenceNumber::const_iterator it = idxTxLinkSeq.begin();
    for (; it != idxTxLinkSeq.end(); ++it)
    {
        if ((*it).ptx && (*it).ptx->GetTxTime() <= nBlockTime)
        {
            if (nTotalSize + (*it).ptx->nSerializeSize > nMaxSize)
            {
                break;
            }
            vtx.push_back(*static_cast<CTransaction*>((*it).ptx));
            nTotalSize += (*it).ptx->nSerializeSize;
            nTotalTxFee += (*it).ptx->nTxFee;
        }
    }
}

} // namespace bigbang
