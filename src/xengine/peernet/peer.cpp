// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peer.h"

#include <boost/bind.hpp>

#include "peernet.h"
#include "util.h"

using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CPeer

CPeer::CPeer(CPeerNet* pPeerNetIn, CIOClient* pClientIn, uint64 nNonceIn, bool fInBoundIn)
  : pPeerNet(pPeerNetIn), pClient(pClientIn), nNonce(nNonceIn), fInBound(fInBoundIn)
{
}

CPeer::~CPeer()
{
    if (pClient)
    {
        pClient->Close();
    }
}

uint64 CPeer::GetNonce()
{
    return nNonce;
}

bool CPeer::IsInBound()
{
    return fInBound;
}

bool CPeer::IsWriteable()
{
    return (indexStream == indexWrite || ssSend[indexStream].GetSize() == 0);
}

const tcp::endpoint CPeer::GetRemote()
{
    return (pClient->GetRemote());
}

const tcp::endpoint CPeer::GetLocal()
{
    return (pClient->GetLocal());
}

void CPeer::Activate()
{
    indexStream = 0;
    indexWrite = 0;
    ssSend[0].Clear();
    ssSend[1].Clear();

    nTimeActive = GetTime();
    nTimeRecv = 0;
    nTimeSend = 0;
}

CBufStream& CPeer::ReadStream()
{
    return ssRecv;
}

CBufStream& CPeer::WriteStream()
{
    return ssSend[indexStream];
}

void CPeer::Read(size_t nLength, CompltFunc fnComplt)
{
    ssRecv.Clear();
    pClient->Read(ssRecv, nLength,
                  boost::bind(&CPeer::HandleRead, this, _1, fnComplt));
}

void CPeer::Write()
{
    if (indexWrite == indexStream)
    {
        pClient->Write(ssSend[indexWrite],
                       boost::bind(&CPeer::HandleWriten, this, _1));
        indexStream = indexWrite ^ 1;
    }
}

void CPeer::HandleRead(size_t nTransferred, CompltFunc fnComplt)
{
    if (nTransferred != 0)
    {
        nTimeRecv = GetTime();
        if (!fnComplt())
        {
            pPeerNet->HandlePeerViolate(this);
        }
    }
    else
    {
        pPeerNet->HandlePeerError(this);
    }
}

void CPeer::HandleWriten(std::size_t nTransferred)
{
    if (nTransferred != 0)
    {
        nTimeSend = GetTime();

        indexWrite ^= 1;
        if (ssSend[indexWrite].GetSize() != 0)
        {
            Write();
        }
        pPeerNet->HandlePeerWriten(this);
    }
    else
    {
        pPeerNet->HandlePeerError(this);
    }
}

} // namespace xengine
