// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_TEMPLATE_H
#define COMMON_TEMPLATE_TEMPLATE_H

#include <boost/shared_ptr.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "destination.h"
#include "stream/datastream.h"
#include "templateid.h"
#include "util.h"
#include "xengine.h"

class CSpendableTemplate
{
};

class CSendToRecordedTemplate
{
public:
    static void RecordDest(const CDestination& sendToDelegate, const CDestination& sendToOwner,
                           const std::vector<uint8>& vchPreSigIn, std::vector<uint8>& vchSigOut)
    {
        vchSigOut.clear();
        xengine::CODataStream ods(vchSigOut, CDestination::DESTINATION_SIZE * 2 + vchPreSigIn.size());
        ods << sendToDelegate << sendToOwner;
        ods.Push(&vchPreSigIn[0], vchPreSigIn.size());
    }

    static bool ParseDest(const std::vector<uint8>& vchSigIn,
                          CDestination& sendToDelegateOut, CDestination& sendToOwnerOut,
                          std::vector<uint8>& vchSubSigOut)
    {
        xengine::CIDataStream is(vchSigIn);
        try
        {
            is >> sendToDelegateOut >> sendToOwnerOut;
            vchSubSigOut.assign(vchSigIn.begin() + (CDestination::DESTINATION_SIZE * 2), vchSigIn.end());
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
        return true;
    }

    virtual bool GetDelegateOwnerDestination(CDestination& destDelegateOut, CDestination& destOwnerOut) const = 0;
};

class CLockedCoinTemplate
{
public:
    virtual int64 LockedCoin(const CDestination& destTo, const int32 nForkHeight) const = 0;
};

enum TemplateType
{
    TEMPLATE_MIN,
    TEMPLATE_WEIGHTED,
    TEMPLATE_MULTISIG,
    TEMPLATE_FORK,
    TEMPLATE_PROOF,
    TEMPLATE_DELEGATE,
    TEMPLATE_EXCHANGE,
    TEMPLATE_VOTE,
    TEMPLATE_PAYMENT,
    TEMPLATE_MAX
};

class CTemplate;
typedef boost::shared_ptr<CTemplate> CTemplatePtr;

class CTransaction;
class CTxOut;
namespace bigbang
{
namespace rpc
{
class CTemplateRequest;
class CTemplateResponse;
} // namespace rpc
} // namespace bigbang

/**
 * template basic class
 */
class CTemplate
{
public:
    // Construct by CTemplate pointer.
    static const CTemplatePtr CreateTemplatePtr(CTemplate* ptr);

    // Construct by template destination and template data.
    static const CTemplatePtr CreateTemplatePtr(const CDestination& destIn, const std::vector<uint8>& vchDataIn);

    // Construct by template id and template data.
    static const CTemplatePtr CreateTemplatePtr(const CTemplateId& nIdIn, const std::vector<uint8>& vchDataIn);

    // Construct by template type and template data.
    static const CTemplatePtr CreateTemplatePtr(uint16 nTypeIn, const std::vector<uint8>& vchDataIn);

    // Construct by json object.
    static const CTemplatePtr CreateTemplatePtr(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);

    // Construct by exported template data.
    static const CTemplatePtr Import(const std::vector<uint8>& vchTemplateIn);

    // Return template type is between TEMPLATE_MIN and TEMPLATE_MAX or not.
    static bool IsValidType(const uint16 nTypeIn);

    // Return template type name.
    static std::string GetTypeName(uint16 nTypeIn);

    // Verify transaction signature.
    static bool VerifyTxSignature(const CTemplateId& nIdIn, const uint16 nType, const uint256& hash, const uint256& hashAnchor,
                                  const CDestination& destTo, const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted);

    // Return dest is spendable or not.
    static bool IsTxSpendable(const CDestination& dest);

    // Return dest is destIn recorded or not.
    static bool IsDestInRecorded(const CDestination& dest);

    // Return delegate address.
    static bool ParseDelegateDest(const CDestination& destIn, const CDestination& sendTo, const std::vector<uint8>& vchSigIn, CDestination& destInDelegateOut, CDestination& sendToDelegateOut);

    // Return dest limits coin on transaction or not.
    static bool IsLockedCoin(const CDestination& dest);

public:
    // Deconstructor
    virtual ~CTemplate(){};

    // Return template type.
    const uint16& GetTemplateType() const;

    // Return template id.
    const CTemplateId& GetTemplateId() const;

    // Return template data.
    const std::vector<uint8>& GetTemplateData() const;

    // Return template name.
    std::string GetName() const;

    // Export template type and template data.
    std::vector<uint8> Export() const;

    // Build transaction signature by concrete template.
    bool BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo, const int32 nForkHeight,
                          const std::vector<uint8>& vchPreSig, std::vector<uint8>& vchSig, bool& fCompleted) const;

    // Build transaction signature by concrete template.
    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;

    // Convert params of template to json object
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const = 0;

protected:
    // Constructor
    CTemplate() = default;
    CTemplate(const uint16 nTypeIn);

    // Verify vchSig is begin with template data or not.
    bool VerifyTemplateData(const std::vector<uint8>& vchSig) const;

    // Build template id by vchData.
    void BuildTemplateId();

protected:
    virtual CTemplate* clone() const = 0;

    // Validate concrete params of template.
    virtual bool ValidateParam() const = 0;

    // Convert vchData to concrete params of template.
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn) = 0;

    // Convert json object to concrete params of template.
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance) = 0;

    // Convert concrete params of template to vchData.
    virtual void BuildTemplateData() = 0;

    // Build transaction signature by concrete template.
    virtual bool BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                  const std::vector<uint8>& vchPreSig, std::vector<uint8>& vchSig) const;

    // Verify transaction signature by concrete template.
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nHeight, bool& fCompleted) const = 0;

protected:
    uint16 nType;
    CTemplateId nId;
    std::vector<uint8> vchData;
};

#endif // COMMON_TEMPLATE_TEMPLATE_H
