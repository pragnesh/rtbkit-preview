#include "rtbkit/core/router/router.h"
#include "rtbkit/plugins/bidding_agent/bidding_agent.h"
#include <map>
#include <functional>
#include <iostream>
#include <sstream>

using namespace Datacratic;
using namespace RTBKIT;

int main() {
    auto proxies = std::make_shared<ServiceProxies>();

    BiddingAgent agent(proxies);
    AgentConfig config;
    config.campaign = "testCampaign";
    config.strategy = "testStrategy";
    config.account = { "testCampaign", "testStrategy" };
    config.maxInFlight = 20000;
    config.minTimeAvailableMs = 0;
    config.creatives.push_back(Creative::sampleLB);
    config.creatives.push_back(Creative::sampleWS);
    config.creatives.push_back(Creative::sampleBB);

    bool bidding = false;

    agent.onError = [&] (double timestamp, const std::string & error, const std::vector<std::string> & message) {
        std::cout << "agent got error: " << error << " from message: " << message << std::endl;
    };

    agent.onGotConfig = [&] (double) {
        std::cout << "agent got config" << std::endl;
    };

    agent.onBidRequest = [&] (double timestamp, const Id & id, std::shared_ptr<BidRequest> br, const Json::Value & spots, double timeLeftMs,  const Json::Value & aug) {
        std::cout << "agent got bid request " << id << std::endl;
        Json::Value response;
        Json::Value metadata;
        if(bidding) {
            agent.doBid(id, response, metadata);
        }
    };

    auto onResult = [&] (const BiddingAgent::BidResultArgs & args) {
        std::cout << "agent got result " << args.result << std::endl;
    };

    agent.onWin = agent.onLoss = agent.onNoBudget = agent.onTooLate = agent.onInvalidBid = agent.onDroppedBid = onResult;

    agent.onTooLate = [&] (const BiddingAgent::BidResultArgs & args) {
        std::cout << "agent got too late" << std::endl;
    };

    agent.onNeedConfig = [&](double ts) {
        return config.toJson();
    };

    std::map<std::string, std::function<void(std::istream &)>> commands;

    commands.insert(std::make_pair("help", [&](std::istream & args) {
        std::cout << "here are the possible commands:" << std::endl;
        for(auto i : commands) {
            std::cout << "- " << i.first << std::endl;
        }
    }));

    commands.insert(std::make_pair("zookeeper", [&](std::istream & args) {
        std::string host; args >> host; if(host.empty()) host = "localhost:2181";
        std::string path; args >> path; if(path.empty()) path = "CWD";
        std::cout << "using url=" << host << " prefix=" << path << std::endl;
        proxies->useZookeeper(host, path);
    }));

    commands.insert(std::make_pair("carbon", [&](std::istream & args) {
        std::string host; args >> host;
        std::string path; args >> path; if(path.empty()) path = "CWD";
        std::cout << "using url=" << host << " prefix=" << path << std::endl;
        proxies->logToCarbon(host, path);
    }));

    commands.insert(std::make_pair("dump", [&](std::istream & args) {
        proxies->config->dump(std::cout);
        std::cout << std::endl;
    }));

    commands.insert(std::make_pair("start", [&](std::istream & args) {
        std::string name; args >> name; if(name.empty()) name = "test";
        agent.start("", name);
        std::cout << "agent started name=" << name << std::endl;
    }));

    commands.insert(std::make_pair("doconfig", [&](std::istream & args) {
        auto value = config.toJson();
        std::cout << "setting config" << std::endl << "value=" << value << std::endl;
        agent.doConfig(value);
    }));

    commands.insert(std::make_pair("bid", [&](std::istream & args) {
        bidding ^= 1;
        std::cout << "now bidding=" << bidding << std::endl;
    }));

    for(;;) {
        std::cout << "$>" << std::flush;

        std::string line;
        getline(std::cin, line);

        if(line == "quit") {
            break;
        }

        std::istringstream args(line);
        std::string command;
        args >> command;

        auto i = commands.find(command);
        if(commands.end() == i) {
            std::cout << "unknown command '" << command << "'" << std::endl;
        }
        else {
            i->second(args);
        }
    }
}
