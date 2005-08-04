//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


#include <algorithm>
#include "stlwatch.h"
#include "opp_utils.h"
#include "RoutingTable6.h"
#include "IPv6InterfaceData.h"
#include "InterfaceTableAccess.h"



Define_Module(RoutingTable6);


std::string IPv6Route::info() const
{
    std::stringstream out;
    out << destPrefix() << "/" << prefixLength() << " --> ";
    out << "if=" << interfaceID() << " " << nextHop(); // FIXME try printing interface name
    out << " " << routeSrcName(src());
    if (expiryTime()>0)
        out << " exp:" << simtimeToStr(expiryTime());
    return out.str();
}

std::string IPv6Route::detailedInfo() const
{
    return std::string();
}

const char *IPv6Route::routeSrcName(RouteSrc src)
{
    switch (src)
    {
        case ROUTERADV:       return "ROUTERADV";
        case OWNADVPREFIX:    return "OWNADVPREFIX";
        case STATIC:          return "STATIC";
        case ROUTINGPROTOCOL: return "ROUTINGPROTOCOL";
        default:              return "???";
    }
}

//----

std::ostream& operator<<(std::ostream& os, const IPv6Route& e)
{
    os << e.info();
    return os;
};

std::ostream& operator<<(std::ostream& os, const RoutingTable6::DestCacheEntry& e)
{
    os << "if=" << e.interfaceId << " " << e.nextHopAddr;  //FIXME try printing interface name
    return os;
};

void RoutingTable6::initialize(int stage)
{
    if (stage==1)
    {
        ift = InterfaceTableAccess().get();

        WATCH_PTRVECTOR(routeList);
        WATCH_MAP(destCache); // FIXME commented out for now
    //FIXME: we will use this isRouter flag for now. what if future implementations
    //have 2 interfaces where one interface is configured as a router and the other
    //as a host?
        isrouter = par("isRouter");
        WATCH(isrouter);

        // add IPv6InterfaceData to interfaces
        for (int i=0; i<ift->numInterfaces(); i++)
        {
            InterfaceEntry *ie = ift->interfaceAt(i);
            configureInterfaceForIPv6(ie);
        }

        // configure interfaces from XML config file
        cXMLElement *config = par("routingTableFile");
        for (cXMLElement *child=config->getFirstChild(); child; child = child->getNextSibling())
        {
            if (opp_strcmp(child->getTagName(),"interface")!=0)
                continue;
            const char *ifname = child->getAttribute("name");
            if (!ifname)
                error("<interface> without name attribute at %s", child->getSourceLocation());
            InterfaceEntry *ie = ift->interfaceByName(ifname);
            if (!ie)
                error("no interface named %s was registered, %s", ifname, child->getSourceLocation());
            configureInterfaceFromXML(ie, child);
        }

        // FIXME todo: read routing table as well
    }
    else if (stage==4)
    {
        // configurator adds routes only in stage==3
        updateDisplayString();
    }
}

void RoutingTable6::updateDisplayString()
{
    if (!ev.isGUI())
        return;

    char buf[80];
    sprintf(buf, "%d routes\n%d destcache entries", numRoutes(), destCache.size());
    displayString().setTagArg("t",0,buf);
}

void RoutingTable6::handleMessage(cMessage *msg)
{
    opp_error("This module doesn't process messages");
}

void RoutingTable6::configureInterfaceForIPv6(InterfaceEntry *ie)
{
    IPv6InterfaceData *d = new IPv6InterfaceData();
    ie->setIPv6Data(d);

    // for routers, turn on advertisements by default
    //FIXME: we will use this isRouter flag for now. what if future implementations
    //have 2 interfaces where one interface is configured as a router and the other
    //as a host?
    d->setAdvSendAdvertisements(isrouter);//Added by WEI
    //d->setSendRouterAdvertisements(isrouter);

    // metric: some hints: OSPF cost (2e9/bps value), MS KB article Q299540, ...
    //d->setMetric((int)ceil(2e9/ie->datarate())); // use OSPF cost as default
    //FIXME TBD fill in the rest
}

static const char *getRequiredAttr(cXMLElement *elem, const char *attrName)
{
    const char *s = elem->getAttribute(attrName);
    if (!s)
        opp_error("element <%s> misses required attribute %s at %s",
                  elem->getTagName(), attrName, elem->getSourceLocation());
    return s;
}
static bool toBool(const char *s, bool defaultValue=false)
{
    if (!s)
        return defaultValue;
    return !strcmp(s,"on") || !strcmp(s,"true") || !strcmp(s,"yes");
}

void RoutingTable6::configureInterfaceFromXML(InterfaceEntry *ie, cXMLElement *cfg)
{
    IPv6InterfaceData *d = ie->ipv6();

    // parse basic config (attributes)
    d->setAdvSendAdvertisements(toBool(getRequiredAttr(cfg, "SendAdvertisements")));//Modified by WEI
    d->setMaxRtrAdvInterval(OPP_Global::atod(getRequiredAttr(cfg, "MaxRtrAdvInterval")));
    d->setMinRtrAdvInterval(OPP_Global::atod(getRequiredAttr(cfg, "MinRtrAdvInterval")));
    d->setAdvManagedFlag(toBool(getRequiredAttr(cfg, "AdvManagedFlag")));
    d->setAdvOtherConfigFlag(toBool(getRequiredAttr(cfg, "AdvOtherConfigFlag")));
    d->setAdvLinkMTU(OPP_Global::atoul(getRequiredAttr(cfg, "AdvLinkMTU")));
    d->setAdvReachableTime(OPP_Global::atoul(getRequiredAttr(cfg, "AdvReachableTime")));
    d->setAdvRetransTimer(OPP_Global::atoul(getRequiredAttr(cfg, "AdvRetransTimer")));
    d->setAdvCurHopLimit(OPP_Global::atoul(getRequiredAttr(cfg, "AdvCurHopLimit")));
    d->setAdvDefaultLifetime(OPP_Global::atoul(getRequiredAttr(cfg, "AdvDefaultLifetime")));
    ie->setMtu(OPP_Global::atoul(getRequiredAttr(cfg, "HostLinkMTU")));
    d->setCurHopLimit(OPP_Global::atoul(getRequiredAttr(cfg, "HostCurHopLimit")));
    d->setBaseReachableTime(OPP_Global::atoul(getRequiredAttr(cfg, "HostBaseReachableTime")));
    d->setRetransTimer(OPP_Global::atoul(getRequiredAttr(cfg, "HostRetransTimer")));
    d->setDupAddrDetectTransmits(OPP_Global::atoul(getRequiredAttr(cfg, "HostDupAddrDetectTransmits")));

    // parse prefixes (AdvPrefix elements; they should be inside an AdvPrefixList
    // element, but we don't check that)
    cXMLElementList prefixList = cfg->getElementsByTagName("AdvPrefix");
    for (unsigned int i=0; i<prefixList.size(); i++)
    {
        cXMLElement *node = prefixList[i];
        IPv6InterfaceData::AdvPrefix prefix;

        // FIXME todo implement: advValidLifetime, advPreferredLifetime can
        // store (absolute) expiry time (if >0) or lifetime (delta) (if <0);
        // 0 should be treated as infinity
        int pfxLen;
        if (!prefix.prefix.tryParseAddrWithPrefix(node->getNodeValue(),pfxLen))
            opp_error("element <%s> at %s: wrong IPv6Address/prefix syntax %s",
                      node->getTagName(), node->getSourceLocation(), node->getNodeValue());
        prefix.prefixLength = pfxLen;
        prefix.advValidLifetime = OPP_Global::atoul(getRequiredAttr(node, "AdvValidLifetime"));
        prefix.advOnLinkFlag = toBool(getRequiredAttr(node, "AdvOnLinkFlag"));
        prefix.advPreferredLifetime = OPP_Global::atoul(getRequiredAttr(node, "AdvPreferredLifetime"));
        prefix.advAutonomousFlag = toBool(getRequiredAttr(node, "AdvAutonomousFlag"));

        d->addAdvPrefix(prefix);
    }

    // parse addresses
    cXMLElementList addrList = cfg->getChildrenByTagName("inetAddr");
    for (unsigned int k=0; k<addrList.size(); k++)
    {
        cXMLElement *node = addrList[k];
        IPv6Address address(node->getNodeValue());
        d->assignAddress(address, true, 0, 0);  // set up as tentative, with infinite lifetimes
    }
}

InterfaceEntry *RoutingTable6::interfaceByAddress(const IPv6Address& addr)
{
    Enter_Method("interfaceByAddress(%s)=?", addr.str().c_str());

    if (addr.isUnspecified())
        return NULL;
    for (int i=0; i<ift->numInterfaces(); ++i)
    {
        InterfaceEntry *ie = ift->interfaceAt(i);
        if (ie->ipv6()->hasAddress(addr))
            return ie;
    }
    return NULL;
}

bool RoutingTable6::localDeliver(const IPv6Address& dest)
{
    Enter_Method("localDeliver(%s) y/n", dest.str().c_str());

    // first, check if we have an interface with this address
    for (int i=0; i<ift->numInterfaces(); i++)
    {
        InterfaceEntry *ie = ift->interfaceAt(i);
        if (ie->ipv6()->hasAddress(dest))
            return true;
    }

    // then check for special, preassigned multicast addresses
    // (these addresses occur more rarely than specific interface addresses,
    // that's why we check for them last)

    if (dest==IPv6Address::ALL_NODES_1 || dest==IPv6Address::ALL_NODES_2)
        return true;
    if (isRouter() && (dest==IPv6Address::ALL_ROUTERS_1 || dest==IPv6Address::ALL_ROUTERS_2 || dest==IPv6Address::ALL_ROUTERS_5))
        return true;

    // check for solicited-node multicast address
    if (dest.matches(IPv6Address::SOLICITED_NODE_PREFIX, 104))
    {
        for (int i=0; i<ift->numInterfaces(); i++)
        {
            InterfaceEntry *ie = ift->interfaceAt(i);
            if (ie->ipv6()->matchesSolicitedNodeMulticastAddress(dest))
                return true;
        }
    }
    return false;
}

const IPv6Address& RoutingTable6::lookupDestCache(const IPv6Address& dest, int& outInterfaceId)
{
    Enter_Method("lookupDestCache(%s)", dest.str().c_str());

    DestCache::iterator it = destCache.find(dest);
    if (it == destCache.end())
    {
        outInterfaceId = -1;
        return IPv6Address::UNSPECIFIED_ADDRESS;
    }
    outInterfaceId = it->second.interfaceId;
    return it->second.nextHopAddr;
}

const IPv6Route *RoutingTable6::doLongestPrefixMatch(const IPv6Address& dest)
{
    Enter_Method("doLongestPrefixMatch(%s)", dest.str().c_str());

    // we'll just stop at the first match, because the table is sorted
    // by prefix lengths and metric (see addRoute())
    for (RouteList::iterator it=routeList.begin(); it!=routeList.end(); it++)
        if (dest.matches((*it)->destPrefix(),(*it)->prefixLength()))
            return *it;

    // FIXME todo: if we selected an expired route, throw it out and select again!

    return NULL;
}

void RoutingTable6::updateDestCache(const IPv6Address& dest, const IPv6Address& nextHopAddr, int interfaceId)
{
    // FIXME this performs 2 lookups -- optimize to do only one
    destCache[dest].nextHopAddr = nextHopAddr;
    destCache[dest].interfaceId = interfaceId;

    updateDisplayString();
}

void RoutingTable6::purgeDestCache()
{
    destCache.clear();
    updateDisplayString();
}

void RoutingTable6::purgeDestCacheEntriesToNeighbour(const IPv6Address& nextHopAddr, int interfaceId)
{
    for (DestCache::iterator it=destCache.begin(); it!=destCache.end(); )
    {
        if (it->second.interfaceId==interfaceId && it->second.nextHopAddr==nextHopAddr)
        {
            // move the iterator past this element before removing it
            DestCache::iterator oldIt = it++;
            destCache.erase(oldIt);
        }
        else
        {
            it++;
        }
    }

    updateDisplayString();
}

void RoutingTable6::addOrUpdateOnLinkPrefix(const IPv6Address& destPrefix, int prefixLength,
                                            int interfaceId, simtime_t expiryTime)
{
    // see if prefix exists in table
    IPv6Route *route = NULL;
    for (RouteList::iterator it=routeList.begin(); it!=routeList.end(); it++)
    {
        if ((*it)->src()==IPv6Route::ROUTERADV && (*it)->destPrefix()==destPrefix && (*it)->prefixLength()==prefixLength)
        {
            route = *it;
            break;
        }
    }

    if (route==NULL)
    {
        // create new route object
        IPv6Route *route = new IPv6Route(destPrefix, prefixLength, IPv6Route::ROUTERADV);
        route->setInterfaceID(interfaceId);
        route->setExpiryTime(expiryTime);
        route->setMetric(0);

        // then add it
        addRoute(route);
    }
    else
    {
        // update existing one
        route->setInterfaceID(interfaceId);
        route->setExpiryTime(expiryTime);
    }

    updateDisplayString();
}

void RoutingTable6::addOrUpdateOwnAdvPrefix(const IPv6Address& destPrefix, int prefixLength,
                                            int interfaceId, simtime_t expiryTime)
{
    // FIXME this is very similar to the one above -- refactor!!

    // see if prefix exists in table
    IPv6Route *route = NULL;
    for (RouteList::iterator it=routeList.begin(); it!=routeList.end(); it++)
    {
        if ((*it)->src()==IPv6Route::OWNADVPREFIX && (*it)->destPrefix()==destPrefix && (*it)->prefixLength()==prefixLength)
        {
            route = *it;
            break;
        }
    }

    if (route==NULL)
    {
        // create new route object
        IPv6Route *route = new IPv6Route(destPrefix, prefixLength, IPv6Route::OWNADVPREFIX);
        route->setInterfaceID(interfaceId);
        route->setExpiryTime(expiryTime);
        route->setMetric(0);

        // then add it
        addRoute(route);
    }
    else
    {
        // update existing one
        route->setInterfaceID(interfaceId);
        route->setExpiryTime(expiryTime);
    }

    updateDisplayString();
}

void RoutingTable6::removeOnLinkPrefix(const IPv6Address& destPrefix, int prefixLength)
{
    // scan the routing table for this prefix and remove it
    for (RouteList::iterator it=routeList.begin(); it!=routeList.end(); it++)
    {
        if ((*it)->src()==IPv6Route::ROUTERADV && (*it)->destPrefix()==destPrefix && (*it)->prefixLength()==prefixLength)
        {
            routeList.erase(it);
            return; // there can be only one such route, addOrUpdateOnLinkPrefix() guarantees that
        }
    }

    updateDisplayString();
}

void RoutingTable6::addStaticRoute(const IPv6Address& destPrefix, int prefixLength,
                    unsigned int interfaceId, const IPv6Address& nextHop,
                    int metric)
{
    // create route object
    IPv6Route *route = new IPv6Route(destPrefix, prefixLength, IPv6Route::STATIC);
    route->setInterfaceID(interfaceId);
    route->setNextHop(nextHop);
    if (metric==0)
    {
        metric = 10; // TBD should be filled from interface metric
    }
    route->setMetric(metric);

    // then add it
    addRoute(route);
}

void RoutingTable6::addRoutingProtocolRoute(IPv6Route *route)
{
    ASSERT(route->src()==IPv6Route::ROUTINGPROTOCOL);
    addRoute(route);
}

bool RoutingTable6::routeLessThan(const IPv6Route *a, const IPv6Route *b)
{
    // helper for sort() in addRoute(). We want routes with longer
    // prefixes to be at front, so we compare them as "less".
    // For metric, a smaller value is better (we report that as "less").
    if (a->prefixLength()!=b->prefixLength())
        return a->prefixLength() > b->prefixLength();
    return a->metric() < b->metric();
}

void RoutingTable6::addRoute(IPv6Route *route)
{
    routeList.push_back(route);

    // we keep entries sorted by prefix length in routeList, so that we can
    // stop at the first match when doing the longest prefix matching
    std::sort(routeList.begin(), routeList.end(), routeLessThan);

    updateDisplayString();
}

void RoutingTable6::removeRoute(IPv6Route *route)
{
    RouteList::iterator it = std::find(routeList.begin(), routeList.end(), route);
    ASSERT(it!=routeList.end());
    routeList.erase(it);

    updateDisplayString();
}

int RoutingTable6::numRoutes() const
{
    return routeList.size();
}

IPv6Route *RoutingTable6::route(int i)
{
    ASSERT(i>=0 && i<routeList.size());
    return routeList[i];
}
