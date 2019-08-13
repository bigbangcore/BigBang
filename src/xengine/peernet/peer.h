// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_PEER_H
#define XENGINE_PEERNET_PEER_H

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <string>
#include <vector>

#include "netio/ioproc.h"
#include "stream/stream.h"

namespace xengine
{

class CPeerNet;

class CPeer
{
public:
    typedef boost::function<bool()> CompltFunc;

    CPeer(CPeerNet* pPeerNetIn, CIOClient* pClientIn, uint64 nNonceIn, bool fInBoundIn);
    virtual ~CPeer();
    uint64 GetNonce();
    bool IsInBound();
    bool IsWriteable();
    const boost::asio::ip::tcp::endpoint GetRemote();
    const boost::asio::ip::tcp::endpoint GetLocal();
    virtual void Activate();

protected:
    CBufStream& ReadStream();
    CBufStream& WriteStream();

    void Read(std::size_t nLength, CompltFunc fnComplt);
    void Write();

    void HandleRead(std::size_t nTransferred, CompltFunc fnComplt);
    void HandleWriten(std::size_t nTransferred);

public:
    int64 nTimeActive;
    int64 nTimeRecv;
    int64 nTimeSend;

protected:
    CPeerNet* pPeerNet;
    CIOClient* pClient;
    uint64 nNonce;
    bool fInBound;

    CBufStream ssRecv;
    CBufStream ssSend[2];
    int indexStream;
    int indexWrite;
};

} // namespace xengine

#endif //XENGINE_PEERNET_PEER_H
