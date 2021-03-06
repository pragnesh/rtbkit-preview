/* bidding_agent.h                                                   -*- C++ -*-
   Rémi Attab, 14 December 2011
   Copyright (c) 2011 Datacratic.  All rights reserved.

   Simple remote interface to the router.
*/


#ifndef __rtb__bidding_agent_h__
#define __rtb__bidding_agent_h__


#include "rtbkit/common/auction.h"
#include "soa/service/zmq.hpp"
#include "soa/service/carbon_connector.h"
#include "soa/jsoncpp/json.h"
#include "soa/types/id.h"
#include "soa/service/service_base.h"
#include "jml/arch/spinlock.h"
#include "soa/service/zmq_endpoint.h"
#include "soa/service/typed_message_channel.h"

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include <string>
#include <vector>
#include <thread>
#include <map>


namespace RTBKIT {


/*****************************************************************************/
/* ROUTER PROXY                                                              */
/*****************************************************************************/

/** Proxy class that a bidding agent uses to communicate with the rest of the
    system:

    * Routers
    * Post auction services
    * Configuration service

*/

struct BiddingAgent : public ServiceBase, public MessageLoop {

    BiddingAgent(std::shared_ptr<ServiceProxies> proxies,
                const std::string & name = "bidding_agent");
    BiddingAgent(ServiceBase & parent,
                const std::string & name = "bidding_agent");

    ~BiddingAgent();

    BiddingAgent(const BiddingAgent& other) = delete;
    BiddingAgent& operator=(const BiddingAgent& other) = delete;

    /** If set to true then an exception will thrown if a callback is not
        registered.
    */
    void strictMode(bool strict) { requiresAllCB = strict; }

    void start(const std::string& clientSocketURI, const std::string& name);
    void shutdown();

    void doBid(Id id, Json::Value response, Json::Value meta);
    void doPong(const std::string & fromRouter, Date sent, Date received,
                const std::vector<std::string> & payload);
    void doConfig(Json::Value config);

    /** The odd double typedefs is to simplify the JS wrappers. */

    typedef void (SimpleCb) (double timestamp);
    typedef boost::function<SimpleCb> SimpleCbFn;

    typedef void (BidRequestCb)
        (double timestamp,
         Id id,
         std::shared_ptr<BidRequest> bidRequest,
         Json::Value spots,
         double timeLeftMs,
         Json::Value augmentations);
    typedef boost::function<BidRequestCb> BidRequestCbFn;

    typedef void (PingCb) (const std::string & fromRouter,
                           Date timestamp,
                           const std::vector<std::string> & args);
    typedef boost::function<PingCb> PingCbFn;

    typedef void (ErrorCb) (double timestamp,
                            std::string description,
                            std::vector<std::string> originalError);
    typedef boost::function<ErrorCb> ErrorCbFn;

    // Delivery message
    struct DeliveryArgs {
        double timestamp;
        Id auctionId;
        Id spotId;
        int spotIndex;
        std::shared_ptr<BidRequest> bidRequest;
        Json::Value bid;
        Json::Value win;
        Json::Value impression;
        Json::Value click;
        Json::Value augmentations;
        Json::Value visits;
    };

    typedef void (DeliveryCb) (const DeliveryArgs & args);
    typedef boost::function<DeliveryCb> DeliveryCbFn;

    struct BidResultArgs {
        std::string result;
        double timestamp;
        std::string confidence;
        Id auctionId;
        int spotNum;
        int secondPrice;
        std::shared_ptr<BidRequest> request;
        Json::Value ourBid;
        Json::Value accountInfo;
        Json::Value metadata;
        Json::Value augmentations;
        UserIds uids;
    };

    typedef void (ResultCb) (const BidResultArgs & args);
    typedef boost::function<ResultCb> ResultCbFn;

    BidRequestCbFn onBidRequest;
    ResultCbFn onWin;
    ResultCbFn onLoss;
    ResultCbFn onNoBudget;
    ResultCbFn onTooLate;
    ResultCbFn onInvalidBid;
    ResultCbFn onDroppedBid;

    PingCbFn onPing;

    DeliveryCbFn onImpression, onClick, onVisit;

    SimpleCbFn onGotConfig;
    SimpleCbFn onNeedConfig;
    ErrorCbFn onError;

private:
    std::string agentName;

    /** Format of a message to a router. */
    struct RouterMessage {
        RouterMessage(const std::string & toRouter = "",
                      const std::string & type = "",
                      const std::vector<std::string> & payload
                          = std::vector<std::string>())
            : toRouter(toRouter),
              type(type),
              payload(payload)
        {
        }

        std::string toRouter;
        std::string type;
        std::vector<std::string> payload;
    };

    ZmqMultipleNamedClientBusProxy toRouters;
    ZmqMultipleNamedClientBusProxy toPostAuctionServices;
    ZmqNamedClientBusProxy toConfigurationAgent;
    TypedMessageSink<RouterMessage> toRouterChannel;

    struct RequestStatus {
        Date timestamp;
        std::string fromRouter;
    };
    
    std::map<Id, RequestStatus> requests;
    std::mutex requestsLock;

    bool requiresAllCB;

    void checkMessageSize(const std::vector<std::string>& msg, int expectedSize);

    // void doHeartbeat();

    void handleRouterMessage(const std::string & fromRouter,
                             const std::vector<std::string>& msg);
    void handleError(const std::vector<std::string>& msg, ErrorCbFn& callback);
    void handleBidRequest(const std::string & fromRouter,
            const std::vector<std::string>& msg, BidRequestCbFn& callback);
    void handleWin(
            const std::vector<std::string>& msg, ResultCbFn& callback);
    void handleResult(
            const std::vector<std::string>& msg, ResultCbFn& callback);
    void handleSimple(
            const std::vector<std::string>& msg, SimpleCbFn& callback);
    void handleDelivery(
            const std::vector<std::string>& msg, DeliveryCbFn& callback);
    void handlePing(const std::string & fromRouter,
            const std::vector<std::string>& msg, PingCbFn& callback);
};



} // namespace RTBKIT

#endif // __rtb__bidding_agent_h__


