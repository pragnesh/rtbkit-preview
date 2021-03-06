/* master_banker.h                                                 -*- C++ -*-
   Jeremy Barnes, 8 November 2012
   Copyright (c) 2012 Datacratic Inc.  All rights reserved.

   Master banker class to deal with all budgeting.
*/

#ifndef __banker__master_banker_h__
#define __banker__master_banker_h__

#include "banker.h"
#include "soa/service/named_endpoint.h"
#include "soa/service/message_loop.h"
#include "soa/service/redis.h"
#include "soa/service/rest_service_endpoint.h"
#include "soa/service/rest_request_router.h"
#include "jml/utils/vector_utils.h"
#include "jml/utils/positioned_types.h"
#include <type_traits>
#include "rtbkit/core/monitor/monitor_provider.h"

#include "null_banker.h"  // debug

namespace RTBKIT {

class Accounts;

inline AccountKey restDecode(const std::string & str, AccountKey *)
{
    return AccountKey(ML::split(str, ':'));
}

inline std::string restEncode(const AccountKey & val)
{
    return val.toString();
}


/*****************************************************************************/
/* REAL BANKER                                                               */
/*****************************************************************************/

/** For want of a better name... */

/** 
    Router accounts have a float that they try to maintain; this is topped
    up either by recycling from post auction loops or increasing the
    budgets.

    Router
       - Commitment Account (committments made and retired)
         - Budget (read-only)
         - Recycled In (read-only)
         - Commitments Made
         - Commitments Retired
         - Available = Budget + Recycled In - Recycled Out + Commitments Retired - Commitments Made
         - No authorization if Available < 0
       - Authorize
         - Commitments Made += Authorized Amount
       - Cancel
         - Commitments Retired += Authorized Amount
       - Detach
         - nothing

    Post Auction Loop
       - Spend Tracking Account
         - Budget = 0
         - Commitments Retired
         - Spent (and Line Items)
         - Commitments Made = 0
         - Not Spent = Commitments Retired - Spent
         - Recyclable = Commitments Retired - Spent - Commitments Made
                      = Not Spent - Commitments Made
       - Commit
         - Spent += Paid Amount
         - Commitments Retired += Authorized Amount
       - Cancel = Commit with Paid Amount = 0
       - Force = Commit with Authorized Amount = 0
       

    Budget Controller
       - Budget Account
           - Budget
           - Recycled Out

       - SetBudget (top level)
           - Budget = New Budget
       - AddBudget (top level)
           - Budget += Increment
       - SetBudget (lower level)
           - Increment = New Budget - Budget
           - Parent: Authorize Increment

       - Sum (child budgets) = Commitments Made
       - Commitments Made - Commitments Retired <= Budget 

    Banker
       - Read-only access to the whole lot
         - Budget
         - Recycled
         - Commitments Made
         - Commitments Retired
         - Spent (and Line Items) (totalled over sub-campaigns)
         - In Flight = Commitments Made - Commitments Retired
         - Condition: Budget + Recycled - In Flight = Spent (?)


    Principles:
       - Every transaction must add the amount into two columns
---

   Start
   PNT: Bud = $100, CM = $10, CR = $0, RO = $0, Avl = $89
   RTR: Bud = $10, RI = $0, CM = $0, CR = $0, Sp = $0, Avl = $10
   PAL: CR = $0, Sp = $0

   Authorize $2
   PNT: Bud = $100, CM = $10, CR = $0, RO = $0, Avl = $89
   RTR: Bud = $10, RI = $0, CM = $2, CR = $0, Sp = $0, Avl = $8
   PAL: CR = $0, Sp = $0

   Win $1
   PNT: Bud = $100, CM = $10, CR = $0, RO = $0, Avl = $89
   RTR: Bud = $10, RI = $0, CM = $2, CR = $0, Sp = $0, Avl = $8
   PAL: CR = $2, Sp = $1
   
   SetAvail $10
   $1
   PNT: Bud = $100, CM = $11, CR = $0, RO = $1, Avl = $89
   RTR: Bud = $11, RI = $1, CM = $2, CR = $0, Sp = $0, Avl = $10
   PAL: CR = $2, Sp = $1
   
   

   Recycled = $


*/


/*****************************************************************************/
/* BANKER PERSISTENCE                                                        */
/*****************************************************************************/

struct BankerPersistence {
    enum PersistenceCallbackStatus {
        SUCCESS,             /* info = "" */
        BACKEND_ERROR,       /* info = error string */
        DATA_INCONSISTENCY   /* info = json array of account keys */
    };

    virtual ~BankerPersistence()
    {
    }

    /* callback types */
    typedef std::function<void (std::shared_ptr<Accounts>,
                                PersistenceCallbackStatus,
                                const std::string & info)>
        OnLoadedCallback;
    typedef std::function<void (PersistenceCallbackStatus,
                                const std::string & info)>
        OnSavedCallback;

    /* backend methods */
    virtual void loadAll(const std::string & topLevelKey,
                         OnLoadedCallback onLoaded) = 0;
    virtual void saveAll(const Accounts & toSave,
                         OnSavedCallback onDone) = 0;
};


/*****************************************************************************/
/* NO BANKER PERSISTENCE                                                     */
/*****************************************************************************/

struct NoBankerPersistence : public BankerPersistence {
    NoBankerPersistence()
    {
    }

    virtual ~NoBankerPersistence()
    {
    }

    virtual void
    loadAll(const std::string & topLevelKey, OnLoadedCallback onLoaded)
    {
        onLoaded(std::make_shared<Accounts>(), SUCCESS, "");
    }

    virtual void
    saveAll(const Accounts & toSave, OnSavedCallback onDone)
    {
        onDone(SUCCESS, "");
    }
};

/*****************************************************************************/
/* REDIS BANKER PERSISTENCE                                                  */
/*****************************************************************************/

struct RedisBankerPersistence : public BankerPersistence {
    RedisBankerPersistence(const Redis::Address & redis);
    RedisBankerPersistence(std::shared_ptr<Redis::AsyncConnection> redis);

    struct Itl;
    std::shared_ptr<Itl> itl;

    void loadAll(const std::string & topLevelKey, OnLoadedCallback onLoaded);
    void saveAll(const Accounts & toSave, OnSavedCallback onDone);
};

/*****************************************************************************/
/* OLD REDIS BANKER PERSISTENCE                                              */
/*****************************************************************************/

struct OldRedisBankerPersistence : public BankerPersistence {

    OldRedisBankerPersistence();

    struct Itl;
    std::shared_ptr<Itl> itl;

    void loadAll(const std::string & topLevelKey, OnLoadedCallback onLoaded);
    void saveAll(const Accounts & toSave, OnSavedCallback onDone);
};


/*****************************************************************************/
/* MASTER BANKER                                                             */
/*****************************************************************************/

/** Master banker class.  This provides a REST interface to an underlying
    banker implementation.
*/

struct MasterBanker
    : public ServiceBase,
      public RestServiceEndpoint,
      public MonitorProvider
{

    MasterBanker(std::shared_ptr<ServiceProxies> proxies,
                 const std::string & serviceName = "masterBanker");
    ~MasterBanker();

    std::shared_ptr<BankerPersistence> storage_;

    void init(const std::shared_ptr<BankerPersistence> & storage);
    void start();
    std::pair<std::string, std::string> bindTcp();

    void shutdown();

    /** Bind the HTTP REST endpoint to the given address on a fixed port,
        for services that must be discoverable via DNS.

        The address will still be published into Zookeeper.

        example: "*:4444", "localhost:8888"
    */
    void bindFixedHttpAddress(const std::string & uri);

    RestRequestRouter router;
    Accounts accounts;
    Date lastSavedState;
    BankerPersistence::PersistenceCallbackStatus lastSaveStatus;

    typedef ML::Spinlock Lock;
    typedef std::unique_lock<Lock> Guard;
    mutable Lock saveLock;
    int saving;

    Json::Value createAccount(const AccountKey & key, AccountType type);

    /** Save the entire state asynchronously.  Will return straight away. */
    void saveState();

    /** Load the entire state sychronously.  Will return once the state has
        been loaded.
    */
    void loadStateSync();

    void onStateLoaded(std::shared_ptr<Accounts> newAccounts,
                       BankerPersistence::PersistenceCallbackStatus status,
                       const std::string & info);
    void onStateSaved(BankerPersistence::PersistenceCallbackStatus status,
                      const std::string & info);

    /* Reponds to Monitor requests */
    MonitorProviderEndpoint monitorProviderEndpoint;

    /* MonitorProvider interface */
    Date lastWin;
    Date lastImpression;

    Json::Value getMonitorIndicators();
};

} // namespace RTBKIT

#endif /* __banker__master_banker_h__ */
