// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DBP_UTILS_H
#define BIGBANG_DBP_UTILS_H

#include <arpa/inet.h>
#include <cstring>
#include <random>
#include <vector>

#include "dbp.pb.h"
#include "lws.pb.h"
#include "sn.pb.h"

namespace bigbang
{
class CDbpUtils
{
public:
    static uint32_t ParseLenFromMsgHeader(const char *header, int size)
    {
        uint32_t lenNetWorkOrder = 0;
        std::memcpy(&lenNetWorkOrder, header, 4);
        return ntohl(lenNetWorkOrder);
    }

    static void WriteLenToMsgHeader(uint32_t len, char *header, int size)
    {
        uint32_t lenNetWorkOrder = htonl(len);
        std::memcpy(header, &lenNetWorkOrder, 4);
    }

    static uint64 CurrentUTC()
    {
        boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
        boost::posix_time::time_duration time_from_epoch =
            boost::posix_time::second_clock::universal_time() - epoch;
        return time_from_epoch.total_seconds();
    }

    static std::string RandomString()
    {
        static auto &chrs = "0123456789"
                            "abcdefghijklmnopqrstuvwxyz"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        thread_local static std::mt19937 rg{std::random_device{}()};
        thread_local static std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

        std::string s;

        int length = 20;

        s.reserve(length);

        while (length--)
            s += chrs[pick(rg)];

        return s;
    }

    static std::vector<std::string> Split(const std::string& str, char delim)
    {
        std::vector<std::string> ret;
        std::istringstream ss(str);
        std::string s;    
        while (std::getline(ss, s, ';')) 
        {
            ret.push_back(s);
        }
        return ret;
    }
/*
    static void DbpToLwsTransaction(const CBbDbpTransaction* dbptx, lws::Transaction* tx)
    {
        tx->set_nversion(dbptx->nVersion);
        tx->set_ntype(dbptx->nType);
        tx->set_nlockuntil(dbptx->nLockUntil);

        std::string hashAnchor(dbptx->hashAnchor.begin(), dbptx->hashAnchor.end());
        tx->set_hashanchor(hashAnchor);

        for (const auto& input : dbptx->vInput)
        {
            std::string inputHash(input.hash.begin(), input.hash.end());
            auto add = tx->add_vinput();
            add->set_hash(inputHash);
            add->set_n(input.n);
        }

        lws::Transaction::CDestination* dest = new lws::Transaction::CDestination();
        dest->set_prefix(dbptx->cDestination.prefix);
        dest->set_size(dbptx->cDestination.size);

        std::string destData(dbptx->cDestination.data.begin(), dbptx->cDestination.data.end());
        dest->set_data(destData);
        tx->set_allocated_cdestination(dest);

        tx->set_namount(dbptx->nAmount);
        tx->set_ntxfee(dbptx->nTxFee);

        std::string mintVchData(dbptx->vchData.begin(), dbptx->vchData.end());
        tx->set_vchdata(mintVchData);

        std::string mintVchSig(dbptx->vchSig.begin(), dbptx->vchSig.end());
        tx->set_vchsig(mintVchSig);

        std::string hash(dbptx->hash.begin(), dbptx->hash.end());
        tx->set_hash(hash);
        tx->set_nchange(dbptx->nChange);
    }

    static void LwsToDbpTransaction(const lws::Transaction* tx, CBbDbpTransaction* dbptx)
    {
        dbptx->nVersion = tx->nversion();
        dbptx->nType = tx->ntype();
        dbptx->nLockUntil = tx->nlockuntil();

        std::vector<unsigned char> hashAnchor(tx->hashanchor().begin(), tx->hashanchor().end());
        dbptx->hashAnchor = hashAnchor;

        for(int i = 0; i < tx->vinput_size(); ++i)
        {
            auto input = tx->vinput(i);
            CBbDbpTxIn txin;
            txin.hash = std::vector<unsigned char>(input.hash().begin(), input.hash().end());
            txin.n = input.n();

            dbptx->vInput.push_back(txin);
        }

        dbptx->cDestination.prefix = tx->cdestination().prefix();
        dbptx->cDestination.size = tx->cdestination().size();
        auto& data = tx->cdestination().data();
        dbptx->cDestination.data = std::vector<unsigned char>(data.begin(),data.end());

        dbptx->nAmount = tx->namount();
        dbptx->nTxFee = tx->ntxfee();

        dbptx->vchData = std::vector<unsigned char>(tx->vchdata().begin(), tx->vchdata().end());
        dbptx->vchSig = std::vector<unsigned char>(tx->vchsig().begin(), tx->vchsig().end());

        dbptx->hash = std::vector<unsigned char>(tx->hash().begin(), tx->hash().end());

        dbptx->nChange = tx->nchange();

    }

    static void DbpToLwsBlock(const CBbDbpBlock* pBlock, lws::Block& block)
    {
        block.set_nversion(pBlock->nVersion);
        block.set_ntype(pBlock->nType);
        block.set_ntimestamp(pBlock->nTimeStamp);

        std::string hashPrev(pBlock->hashPrev.begin(), pBlock->hashPrev.end());
        block.set_hashprev(hashPrev);

        std::string hashMerkle(pBlock->hashMerkle.begin(), pBlock->hashMerkle.end());
        block.set_hashmerkle(hashMerkle);

        std::string vchproof(pBlock->vchProof.begin(), pBlock->vchProof.end());
        block.set_vchproof(vchproof);

        std::string vchSig(pBlock->vchSig.begin(), pBlock->vchSig.end());
        block.set_vchsig(vchSig);

        //txMint
        lws::Transaction* txMint = new lws::Transaction();
        DbpToLwsTransaction(&(pBlock->txMint), txMint);
        block.set_allocated_txmint(txMint);

        //repeated vtx
        for (const auto& tx : pBlock->vtx)
        {
            DbpToLwsTransaction(&tx, block.add_vtx());
        }

        block.set_nheight(pBlock->nHeight);
        std::string hash(pBlock->hash.begin(), pBlock->hash.end());
        block.set_hash(hash);
    }



    static void LwsToDbpBlock(const lws::Block *pBlock, CBbDbpBlock& block)
    {
        block.nVersion = pBlock->nversion();
        block.nType = pBlock->ntype();
        block.nTimeStamp = pBlock->ntimestamp();

        block.hashPrev = std::vector<unsigned char>(pBlock->hashprev().begin(),pBlock->hashprev().end());
        block.hashMerkle = std::vector<unsigned char>(pBlock->hashmerkle().begin(),pBlock->hashmerkle().end());
        block.vchProof = std::vector<unsigned char>(pBlock->vchproof().begin(), pBlock->vchproof().end());
        block.vchSig = std::vector<unsigned char>(pBlock->vchsig().begin(), pBlock->vchsig().end());

        //txMint
        LwsToDbpTransaction(&(pBlock->txmint()), &(block.txMint));

        //repeated vtx
        for(int i = 0; i < pBlock->vtx_size(); ++i)
        {
            auto vtx = pBlock->vtx(i);
            CBbDbpTransaction tx;
            LwsToDbpTransaction(&vtx,&tx);
            block.vtx.push_back(tx);
        }

        block.nHeight = pBlock->nheight();
        block.hash = std::vector<unsigned char>(pBlock->hash().begin(), pBlock->hash().end());

    }

    static void DbpToSnTransaction(const CBbDbpTransaction* dbptx, sn::Transaction* tx)
    {
        tx->set_nversion(dbptx->nVersion);
        tx->set_ntype(dbptx->nType);
        tx->set_nlockuntil(dbptx->nLockUntil);

        std::string hashAnchor(dbptx->hashAnchor.begin(), dbptx->hashAnchor.end());
        tx->set_hashanchor(hashAnchor);

        for (const auto& input : dbptx->vInput)
        {
            std::string inputHash(input.hash.begin(), input.hash.end());
            auto add = tx->add_vinput();
            add->set_hash(inputHash);
            add->set_n(input.n);
        }

        sn::Transaction::CDestination* dest = new sn::Transaction::CDestination();
        dest->set_prefix(dbptx->cDestination.prefix);
        dest->set_size(dbptx->cDestination.size);

        std::string destData(dbptx->cDestination.data.begin(), dbptx->cDestination.data.end());
        dest->set_data(destData);
        tx->set_allocated_cdestination(dest);

        tx->set_namount(dbptx->nAmount);
        tx->set_ntxfee(dbptx->nTxFee);

        std::string mintVchData(dbptx->vchData.begin(), dbptx->vchData.end());
        tx->set_vchdata(mintVchData);

        std::string mintVchSig(dbptx->vchSig.begin(), dbptx->vchSig.end());
        tx->set_vchsig(mintVchSig);

        std::string hash(dbptx->hash.begin(), dbptx->hash.end());
        tx->set_hash(hash);
    }

    static void DbpToSnBlock(const CBbDbpBlock* pBlock, sn::Block& block)
    {
        block.set_nversion(pBlock->nVersion);
        block.set_ntype(pBlock->nType);
        block.set_ntimestamp(pBlock->nTimeStamp);

        std::string hashPrev(pBlock->hashPrev.begin(), pBlock->hashPrev.end());
        block.set_hashprev(hashPrev);

        std::string hashMerkle(pBlock->hashMerkle.begin(), pBlock->hashMerkle.end());
        block.set_hashmerkle(hashMerkle);

        std::string vchproof(pBlock->vchProof.begin(), pBlock->vchProof.end());
        block.set_vchproof(vchproof);

        std::string vchSig(pBlock->vchSig.begin(), pBlock->vchSig.end());
        block.set_vchsig(vchSig);

        //txMint
        sn::Transaction* txMint = new sn::Transaction();
        DbpToSnTransaction(&(pBlock->txMint), txMint);
        block.set_allocated_txmint(txMint);

        //repeated vtx
        for (const auto& tx : pBlock->vtx)
        {
            DbpToSnTransaction(&tx, block.add_vtx());
        }

        block.set_nheight(pBlock->nHeight);
        std::string hash(pBlock->hash.begin(), pBlock->hash.end());
        block.set_hash(hash);
    }

    static void SnToDbpTransaction(const sn::Transaction* tx, CBbDbpTransaction* dbptx)
    {
        dbptx->nVersion = tx->nversion();
        dbptx->nType = tx->ntype();
        dbptx->nLockUntil = tx->nlockuntil();

        std::vector<unsigned char> hashAnchor(tx->hashanchor().begin(), tx->hashanchor().end());
        dbptx->hashAnchor = hashAnchor;

        for(int i = 0; i < tx->vinput_size(); ++i)
        {
            auto input = tx->vinput(i);
            CBbDbpTxIn txin;
            txin.hash = std::vector<unsigned char>(input.hash().begin(), input.hash().end());
            txin.n = input.n();

            dbptx->vInput.push_back(txin);
        }

        dbptx->cDestination.prefix = tx->cdestination().prefix();
        dbptx->cDestination.size = tx->cdestination().size();
        auto& data = tx->cdestination().data();
        dbptx->cDestination.data = std::vector<unsigned char>(data.begin(),data.end());

        dbptx->nAmount = tx->namount();
        dbptx->nTxFee = tx->ntxfee();

        dbptx->vchData = std::vector<unsigned char>(tx->vchdata().begin(), tx->vchdata().end());
        dbptx->vchSig = std::vector<unsigned char>(tx->vchsig().begin(), tx->vchsig().end());

        dbptx->hash = std::vector<unsigned char>(tx->hash().begin(), tx->hash().end());
    }

    static void SnToDbpBlock(const sn::Block *pBlock, CBbDbpBlock& block)
    {
        block.nVersion = pBlock->nversion();
        block.nType = pBlock->ntype();
        block.nTimeStamp = pBlock->ntimestamp();

        block.hashPrev = std::vector<unsigned char>(pBlock->hashprev().begin(),pBlock->hashprev().end());
        block.hashMerkle = std::vector<unsigned char>(pBlock->hashmerkle().begin(),pBlock->hashmerkle().end());
        block.vchProof = std::vector<unsigned char>(pBlock->vchproof().begin(), pBlock->vchproof().end());
        block.vchSig = std::vector<unsigned char>(pBlock->vchsig().begin(), pBlock->vchsig().end());

        //txMint
        SnToDbpTransaction(&(pBlock->txmint()), &(block.txMint));

        //repeated vtx
        for(int i = 0; i < pBlock->vtx_size(); ++i)
        {
            auto vtx = pBlock->vtx(i);
            CBbDbpTransaction tx;
            SnToDbpTransaction(&vtx,&tx);
            block.vtx.push_back(tx);
        }

        block.nHeight = pBlock->nheight();
        block.hash = std::vector<unsigned char>(pBlock->hash().begin(), pBlock->hash().end());
    }*/

};
} // namespace bigbang
#endif // BIGBANG_DBP_UTILS_H
