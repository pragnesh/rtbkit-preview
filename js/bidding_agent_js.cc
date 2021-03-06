/* bidding_agent_js.cc                                              -*- C++ -*-
   Rémi Attab, 14 December 2011
   Copyright (c) 2011 Datacratic.  All rights reserved.

   Provides the JS bindings for the router proxy.
*/

#define BOOST_FUNCTION_MAX_ARGS 20

#include "bidding_agent_js.h"
#include "bid_request_js.h"
#include "soa/service/js/service_base_js.h"
#include "soa/service/js/opstats_js.h"

#include "v8.h"
#include "node.h"

#include "soa/js/js_call.h"
#include "soa/js/js_value.h"
#include "soa/js/js_utils.h"
#include "soa/js/js_wrapped.h"
#include "soa/js/js_registry.h"
#include "soa/sigslot/slot.h"


#include "jml/utils/smart_ptr_utils.h"
#include "soa/types/js/id_js.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using namespace std;
using namespace v8;
using namespace node;


namespace Datacratic {
namespace JS {


const char* BiddingAgentName = "BiddingAgent";

RegisterJsOps<RTBKIT::BiddingAgent::BidRequestCb> reg_bidRequestCb;
RegisterJsOps<RTBKIT::BiddingAgent::PingCb> reg_pingCb;
RegisterJsOps<RTBKIT::BiddingAgent::ErrorCb> reg_errorCb;
RegisterJsOps<RTBKIT::BiddingAgent::SimpleCb> reg_simpleCb;


/******************************************************************************/
/* RESULT CB OPS                                                              */
/******************************************************************************/

/** This class is used to override calling JS with a BidResultArgs structure.
    Instead of converting all of the arguments to a single JS value, it
    expands them and calls the function with the lot.
*/
struct ResultCbOps:
    public JS::JsOpsBase<ResultCbOps, RTBKIT::BiddingAgent::ResultCb>
{
    static v8::Handle<v8::Value>
    callBoost(const Function & fn,
              const JS::JSArgs & args)
    {
        throw ML::Exception("callBoost for resultCB");
    }

    struct Forwarder : public calltojsbase {

        Forwarder(v8::Handle<v8::Function> fn,
                  v8::Handle<v8::Object> This)
            : calltojsbase(fn, This)
        {
        }

        void operator () (const RTBKIT::BiddingAgent::BidResultArgs & args)
        {
            v8::HandleScope scope;
            JSValue result;
            {
                v8::TryCatch tc;
                v8::Handle<v8::Value> argv[10];
                argv[0] = JS::toJS(args.timestamp);
                argv[1] = JS::toJS(args.confidence);
                argv[2] = JS::toJS(args.auctionId);
                argv[3] = JS::toJS(args.spotNum);
                argv[4] = JS::toJS(args.secondPrice);
                if (args.request)
                    argv[5] = JS::toJS(args.request);
                else argv[5] = v8::Null();
                argv[6] = JS::toJS(args.ourBid);
                argv[7] = JS::toJS(args.accountInfo);
                argv[8] = JS::toJS(args.metadata);
                argv[9] = JS::toJS(args.augmentations);

                result = params->fn->Call(params->This, 10, argv);

                if (result.IsEmpty()) {
                    if(tc.HasCaught()) {
                        // Print JS error and stack trace
                        char msg[256];
                        tc.Message()->Get()->WriteAscii(msg, 0, 256);
                        cout << msg << endl;
                        char st_msg[2500];
                        tc.StackTrace()->ToString()->WriteAscii(st_msg, 0, 2500);
                        cout << st_msg << endl;

                        tc.ReThrow();
                        throw JSPassException();
                    }
                    throw ML::Exception("didn't return anything");
                }
            }
        }
    };

    static Function
    asBoost(const v8::Handle<v8::Function> & fn,
            const v8::Handle<v8::Object> * This)
    {
        v8::Handle<v8::Object> This2;
        if (!This)
            This2 = v8::Object::New();
        return Forwarder(fn, This ? *This : This2);
    }
};

RegisterJsOps<RTBKIT::BiddingAgent::ResultCb> reg_resultCb(ResultCbOps::op);


/******************************************************************************/
/* DELIVERY CB OPS                                                            */
/******************************************************************************/

struct DeliveryCbOps :
    public JS::JsOpsBase<DeliveryCbOps, RTBKIT::BiddingAgent::DeliveryCb>
{

    static v8::Handle<v8::Value>
    callBoost(const Function & fn,
              const JS::JSArgs & args)
    {
        throw ML::Exception("callBoost for resultCB");
    }

    struct Forwarder : public calltojsbase {

        Forwarder(v8::Handle<v8::Function> fn,
                  v8::Handle<v8::Object> This)
            : calltojsbase(fn, This)
        {
        }

        void operator () (const RTBKIT::BiddingAgent::DeliveryArgs & args)
        {
            v8::HandleScope scope;
            JSValue result;
            {
                v8::TryCatch tc;
                v8::Handle<v8::Value> argv[11];

                argv[0] = JS::toJS(args.timestamp);
                argv[1] = JS::toJS(args.auctionId);
                argv[2] = JS::toJS(args.spotId);
                argv[3] = JS::toJS(args.spotIndex);
                argv[4] = JS::toJS(args.bidRequest);
                argv[5] = JS::toJS(args.bid);
                argv[6] = JS::toJS(args.win);
                argv[7] = JS::toJS(args.impression);
                argv[8] = JS::toJS(args.click);
                argv[9] = JS::toJS(args.augmentations);
                argv[10] = JS::toJS(args.visits);

                result = params->fn->Call(params->This, 11, argv);

                if (result.IsEmpty()) {
                    if(tc.HasCaught()) {
                        // Print JS error and stack trace
                        char msg[256];
                        tc.Message()->Get()->WriteAscii(msg, 0, 256);
                        cout << msg << endl;
                        char st_msg[2500];
                        tc.StackTrace()->ToString()->WriteAscii(st_msg, 0, 2500);
                        cout << st_msg << endl;

                        tc.ReThrow();
                        throw JSPassException();
                    }
                    throw ML::Exception("didn't return anything");
                }
            }
        }
    };

    static Function
    asBoost(const v8::Handle<v8::Function> & fn,
            const v8::Handle<v8::Object> * This)
    {
        v8::Handle<v8::Object> This2;
        if (!This)
            This2 = v8::Object::New();
        return Forwarder(fn, This ? *This : This2);
    }
};

RegisterJsOps<RTBKIT::BiddingAgent::DeliveryCb> reg_deliveryCb(DeliveryCbOps::op);


/******************************************************************************/
/* ROUTER PROXY JS                                                            */
/******************************************************************************/

struct BiddingAgentJS :
    public JSWrapped2<
        RTBKIT::BiddingAgent,
        BiddingAgentJS,
        BiddingAgentName,
        rtbModule>
{

    BiddingAgentJS(
            v8::Handle<v8::Object> This,
            const std::shared_ptr<RTBKIT::BiddingAgent>& routerProxy =
                std::shared_ptr<RTBKIT::BiddingAgent>())
    {
        HandleScope scope;
        wrap(This, routerProxy);
    }

    static Handle<v8::Value> New(const Arguments& args)
    {
        try {
            string name = getArg(args, 0, "", "serviceName");
            auto proxies = getArg(
                    args, 1, std::make_shared<ServiceProxies>(), "proxies");

            auto routerProxy = ML::make_std_sp(new RTBKIT::BiddingAgent(proxies, name));
            new BiddingAgentJS(args.This(), routerProxy);

            return args.This();
        }
        HANDLE_JS_EXCEPTIONS;
    }

    static void Initialize () {
        Persistent<FunctionTemplate> t = Register(New);

        registerMemberFn(&RTBKIT::BiddingAgent::doBid, "doBid");
        registerMemberFn(&RTBKIT::BiddingAgent::doPong, "doPong");
        registerMemberFn(&RTBKIT::BiddingAgent::doConfig, "doConfig");
        registerMemberFn(&RTBKIT::BiddingAgent::start, "start");
        registerMemberFn(&RTBKIT::BiddingAgent::shutdown, "close");

        registerAsyncCallback(&RTBKIT::BiddingAgent::onBidRequest, "onBidRequest");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onPing, "onPing");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onWin, "onWin");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onLoss, "onLoss");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onNoBudget, "onNoBudget");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onTooLate, "onTooLate");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onInvalidBid, "onInvalidBid");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onDroppedBid, "onDroppedBid");

        registerAsyncCallback(&RTBKIT::BiddingAgent::onNeedConfig, "onNeedConfig");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onGotConfig, "onGotConfig");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onImpression, "onImpression");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onClick, "onClick");
        registerAsyncCallback(&RTBKIT::BiddingAgent::onVisit, "onVisit");

        registerAsyncCallback(&RTBKIT::BiddingAgent::onError, "onError");
    }
};

std::shared_ptr<RTBKIT::BiddingAgent>
from_js (const JSValue& value, std::shared_ptr<RTBKIT::BiddingAgent>*)
{
    return BiddingAgentJS::fromJS(value);
}

RTBKIT::BiddingAgent*
from_js (const JSValue& value, RTBKIT::BiddingAgent**)
{
    return BiddingAgentJS::fromJS(value).get();
}



} // namespace JS
} // namespace Datacratic
