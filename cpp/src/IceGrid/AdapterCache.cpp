// **********************************************************************
//
// Copyright (c) 2003-2006 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/Random.h>
#include <Ice/LoggerUtil.h>
#include <Ice/Locator.h>
#include <IceGrid/AdapterCache.h>
#include <IceGrid/NodeSessionI.h>
#include <IceGrid/ServerCache.h>
#include <IceGrid/NodeCache.h>
#include <IceGrid/SessionI.h>

#include <functional>

using namespace std;
using namespace IceGrid;

namespace IceGrid
{

struct ReplicaLoadComp : binary_function<ServerAdapterEntryPtr&, ServerAdapterEntryPtr&, bool>
{
    bool operator()(const pair<float, ServerAdapterEntryPtr>& lhs, const pair<float, ServerAdapterEntryPtr>& rhs)
    {
	return lhs.first < rhs.first;
    }
};

struct ReplicaPriorityComp : binary_function<ServerAdapterEntryPtr&, ServerAdapterEntryPtr&, bool>
{
    bool operator()(const ServerAdapterEntryPtr& lhs, const ServerAdapterEntryPtr& rhs)
    {
	return lhs->getPriority() < rhs->getPriority();
    }
};

struct TransformToReplicaLoad : 
	public unary_function<const ServerAdapterEntryPtr&, pair<float, ServerAdapterEntryPtr> >
{
public:

    TransformToReplicaLoad(LoadSample loadSample) : _loadSample(loadSample) { }

    pair<float, ServerAdapterEntryPtr>
    operator()(const ServerAdapterEntryPtr& value)
    {
	return make_pair(value->getLeastLoadedNodeLoad(_loadSample), value);
    }

    LoadSample _loadSample;
};

struct TransformToReplica : public unary_function<const pair<string, ServerAdapterEntryPtr>&, ServerAdapterEntryPtr>
{
    ServerAdapterEntryPtr
    operator()(const pair<float, ServerAdapterEntryPtr>& value)
    {
	return value.second;
    }
};

}

ServerAdapterEntryPtr
AdapterCache::addServerAdapter(const AdapterDescriptor& desc, const ServerEntryPtr& server, const string& app)
{
    Lock sync(*this);
    assert(!getImpl(desc.id));

    istringstream is(desc.priority);
    int priority = 0;
    is >> priority;

    ServerAdapterEntryPtr entry = new ServerAdapterEntry(*this, desc.id, app, desc.replicaGroupId, priority, server);
    addImpl(desc.id, entry);

    if(!desc.replicaGroupId.empty())
    {
	ReplicaGroupEntryPtr repEntry = ReplicaGroupEntryPtr::dynamicCast(getImpl(desc.replicaGroupId));
	assert(repEntry);
	repEntry->addReplica(desc.id, entry);
    }

    return entry;
}

ReplicaGroupEntryPtr
AdapterCache::addReplicaGroup(const ReplicaGroupDescriptor& desc, const string& app)
{
    Lock sync(*this);
    assert(!getImpl(desc.id));
    ReplicaGroupEntryPtr entry = new ReplicaGroupEntry(*this, desc.id, app, desc.loadBalancing);
    addImpl(desc.id, entry);
    return entry;
}

AdapterEntryPtr
AdapterCache::get(const string& id) const
{
    Lock sync(*this);
    AdapterEntryPtr entry = getImpl(id);
    if(!entry)
    {
	throw AdapterNotExistException(id);
    }
    return entry;
}

ServerAdapterEntryPtr
AdapterCache::getServerAdapter(const string& id) const
{
    Lock sync(*this);
    ServerAdapterEntryPtr svrEntry = ServerAdapterEntryPtr::dynamicCast(getImpl(id));
    if(!svrEntry)
    {
	throw AdapterNotExistException(id);
    }
    return svrEntry;
}

ReplicaGroupEntryPtr
AdapterCache::getReplicaGroup(const string& id) const
{
    Lock sync(*this);
    ReplicaGroupEntryPtr repEntry = ReplicaGroupEntryPtr::dynamicCast(getImpl(id));
    if(!repEntry)
    {
	throw AdapterNotExistException(id);
    }
    return repEntry;
}

void
AdapterCache::removeServerAdapter(const string& id)
{
    Lock sync(*this);

    ServerAdapterEntryPtr entry = ServerAdapterEntryPtr::dynamicCast(getImpl(id));
    assert(entry);
    removeImpl(id);
    
    string replicaGroupId = entry->getReplicaGroupId();
    if(!replicaGroupId.empty())
    {
	ReplicaGroupEntryPtr repEntry = ReplicaGroupEntryPtr::dynamicCast(getImpl(replicaGroupId));
	assert(repEntry);
	repEntry->removeReplica(id);
    }
}

void
AdapterCache::removeReplicaGroup(const string& id)
{
    Lock sync(*this);
    removeImpl(id);
}

AdapterEntryPtr
AdapterCache::addImpl(const string& id, const AdapterEntryPtr& entry)
{
    if(_traceLevels && _traceLevels->adapter > 0)
    {
	Ice::Trace out(_traceLevels->logger, _traceLevels->adapterCat);
	out << "added adapter `" << id << "'";	
    }    
    return Cache<string, AdapterEntry>::addImpl(id, entry);
}

void
AdapterCache::removeImpl(const string& id)
{
    if(_traceLevels && _traceLevels->adapter > 0)
    {
	Ice::Trace out(_traceLevels->logger, _traceLevels->adapterCat);
	out << "removed adapter `" << id << "'";	
    }    
    Cache<string, AdapterEntry>::removeImpl(id);
}

AdapterEntry::AdapterEntry(AdapterCache& cache, const string& id, const string& application) :
    _cache(cache),
    _id(id),
    _application(application)
{
}

bool
AdapterEntry::canRemove()
{
    return true;
}

string
AdapterEntry::getId() const
{
    return _id;
}

string
AdapterEntry::getApplication() const
{
    return _application;
}

ServerAdapterEntry::ServerAdapterEntry(AdapterCache& cache,
				       const string& id,
				       const string& application,
				       const string& replicaGroupId, 
				       int priority,
				       const ServerEntryPtr& server) : 
    AdapterEntry(cache, id, application),
    _replicaGroupId(replicaGroupId),
    _priority(priority),
    _server(server)
{
}

vector<pair<string, AdapterPrx> >
ServerAdapterEntry::getProxies(int& nReplicas, bool& replicaGroup)
{
    vector<pair<string, AdapterPrx> > adapters;
    nReplicas = 1;
    replicaGroup = false;
    // 
    // COMPILEFIX: We need to use a temporary here to work around a
    // compiler bug with xlC on AIX which causes a segfault if
    // getProxy raises an exception.
    //
    AdapterPrx adpt = getProxy("", true); 
    adapters.push_back(make_pair(_id, adpt));
    return adapters;
}

float
ServerAdapterEntry::getLeastLoadedNodeLoad(LoadSample loadSample) const
{
    try
    {
	return _server->getLoad(loadSample);
    }
    catch(const ServerNotExistException&)
    {
	// This might happen if the application is updated concurrently.
    }
    catch(const NodeNotExistException&)
    {
	// This might happen if the application is updated concurrently.
    }
    catch(const NodeUnreachableException&)
    {
    }
    catch(const Ice::Exception& ex)
    {
	Ice::Error error(_cache.getTraceLevels()->logger);
	error << "unexpected exception while getting node load:\n" << ex;
    }
    return 999.9f;
}

AdapterInfoSeq
ServerAdapterEntry::getAdapterInfo() const
{
    AdapterInfo info;
    info.id = _id;
    info.replicaGroupId = _replicaGroupId;
    try
    {
	info.proxy = getProxy("", true)->getDirectProxy();
    }
    catch(const Ice::Exception&)
    {
    }
    AdapterInfoSeq infos;
    infos.push_back(info);
    return infos;
}

AdapterPrx
ServerAdapterEntry::getProxy(const string& replicaGroupId, bool upToDate) const
{
    if(replicaGroupId.empty())
    {
	return _server->getAdapter(_id, upToDate);
    }
    else
    {
	if(_replicaGroupId != replicaGroupId)
	{
	    throw Ice::InvalidReplicaGroupIdException();
	}
	return _server->getAdapter(_id, upToDate);
    }
}

int
ServerAdapterEntry::getPriority() const
{
    return _priority;
}

ReplicaGroupEntry::ReplicaGroupEntry(AdapterCache& cache,
				     const string& id,
				     const string& application,
				     const LoadBalancingPolicyPtr& policy) : 
    AdapterEntry(cache, id, application),
    _lastReplica(0)
{
    update(policy);
}

void
ReplicaGroupEntry::addReplica(const string& replicaId, const ServerAdapterEntryPtr& adapter)
{
    Lock sync(*this);
    _replicas.push_back(adapter);
}

void
ReplicaGroupEntry::removeReplica(const string& replicaId)
{
    Lock sync(*this);
    for(vector<ServerAdapterEntryPtr>::iterator p = _replicas.begin(); p != _replicas.end(); ++p)
    {
	if(replicaId == (*p)->getId())
	{
	    _replicas.erase(p);		
	    // Make sure _lastReplica is still within the bounds.
	    _lastReplica = _replicas.empty() ? 0 : _lastReplica % static_cast<int>(_replicas.size());
	    break;
	}
    }
}

void
ReplicaGroupEntry::update(const LoadBalancingPolicyPtr& policy)
{
    Lock sync(*this);
    assert(policy);

    _loadBalancing = policy;

    istringstream is(_loadBalancing->nReplicas);
    int nReplicas = 0;
    is >> nReplicas;
    _loadBalancingNReplicas = nReplicas < 0 ? 1 : nReplicas;
    AdaptiveLoadBalancingPolicyPtr alb = AdaptiveLoadBalancingPolicyPtr::dynamicCast(_loadBalancing);
    if(alb)
    {
	if(alb->loadSample == "1")
	{
	    _loadSample = LoadSample1;
	}
	else if(alb->loadSample == "5")
	{
	    _loadSample = LoadSample5;
	}
	else if(alb->loadSample == "15")
	{
	    _loadSample = LoadSample15;
	}
	else
	{
	    _loadSample = LoadSample1;
	}
    }
}

vector<pair<string, AdapterPrx> >
ReplicaGroupEntry::getProxies(int& nReplicas, bool& replicaGroup)
{
    vector<ServerAdapterEntryPtr> replicas;
    bool adaptive = false;
    LoadSample loadSample = LoadSample1;
    {
	Lock sync(*this);
	replicaGroup = true;
	
	if(_replicas.empty())
	{
	    return vector<pair<string, AdapterPrx> >();
	}

	nReplicas = _loadBalancingNReplicas > 0 ? _loadBalancingNReplicas : static_cast<int>(_replicas.size());
	replicas.reserve(_replicas.size());
	if(RoundRobinLoadBalancingPolicyPtr::dynamicCast(_loadBalancing))
	{
	    for(unsigned int i = 0; i < _replicas.size(); ++i)
	    {
		replicas.push_back(_replicas[(_lastReplica + i) % _replicas.size()]);
	    }
	    _lastReplica = (_lastReplica + 1) % static_cast<int>(_replicas.size());
	}
	else if(AdaptiveLoadBalancingPolicyPtr::dynamicCast(_loadBalancing))
	{
	    replicas = _replicas;
	    RandomNumberGenerator rng;
	    random_shuffle(replicas.begin(), replicas.end(), rng);
	    loadSample = _loadSample;
	    adaptive = true;
	}
	else if(OrderedLoadBalancingPolicyPtr::dynamicCast(_loadBalancing))
	{
	    replicas = _replicas;
	    sort(replicas.begin(), replicas.end(), ReplicaPriorityComp());
	}
	else if(RandomLoadBalancingPolicyPtr::dynamicCast(_loadBalancing))
	{
	    replicas = _replicas;
	    RandomNumberGenerator rng;
	    random_shuffle(replicas.begin(), replicas.end(), rng);
	}
    }

    if(adaptive)
    {
	//
	// This must be done outside the synchronization block since
	// the trasnform() might call and lock each server adapter
	// entry. We also can't sort directly as the load of each
	// server adapter is not stable so we first take a snapshot of
	// each adapter and sort the snapshot.
	//
	vector<pair<float, ServerAdapterEntryPtr> > rl;
	transform(replicas.begin(), replicas.end(), back_inserter(rl), TransformToReplicaLoad(loadSample));
	sort(rl.begin(), rl.end(), ReplicaLoadComp());
	replicas.clear();
	transform(rl.begin(), rl.end(), back_inserter(replicas), TransformToReplica());
    }

    //
    // Retrieve the proxy of each adapter from the server. The adapter
    // might not exist anymore at this time or the node might not be
    // reachable.
    //
    vector<pair<string, AdapterPrx> > adapters;
    for(vector<ServerAdapterEntryPtr>::const_iterator p = replicas.begin(); p != replicas.end(); ++p)
    {
	try
	{
	    // 
	    // COMPILEFIX: We need to use a temporary here to work around a
	    // compiler bug with xlC on AIX which causes a segfault if
	    // getProxy raises an exception.
	    //
	    AdapterPrx adpt = (*p)->getProxy(_id, true);
	    adapters.push_back(make_pair((*p)->getId(), adpt));
	}
	catch(const AdapterNotExistException&)
	{
	}
	catch(const Ice::InvalidReplicaGroupIdException&)
	{
	}
	catch(const NodeUnreachableException&)
	{
	}
    }

    return adapters;
}

float
ReplicaGroupEntry::getLeastLoadedNodeLoad(LoadSample loadSample) const
{
    vector<ServerAdapterEntryPtr> replicas;
    {
	Lock sync(*this);
	replicas = _replicas;
    }

    if(replicas.empty())
    {
	return 999.9f;
    }
    else if(replicas.size() == 1)
    {
	return replicas.back()->getLeastLoadedNodeLoad(loadSample);
    }
    else
    {
	RandomNumberGenerator rng;
	random_shuffle(replicas.begin(), replicas.end(), rng);
	vector<pair<float, ServerAdapterEntryPtr> > rl;
	transform(replicas.begin(), replicas.end(), back_inserter(rl), TransformToReplicaLoad(loadSample));
	return min_element(rl.begin(), rl.end(), ReplicaLoadComp())->first;
    }
}

AdapterInfoSeq
ReplicaGroupEntry::getAdapterInfo() const
{
    vector<ServerAdapterEntryPtr> replicas;
    {
	Lock sync(*this);
	replicas = _replicas;
    }

    AdapterInfoSeq infos;
    for(vector<ServerAdapterEntryPtr>::const_iterator p = replicas.begin(); p != replicas.end(); ++p)
    {
	AdapterInfoSeq infs = (*p)->getAdapterInfo();
	assert(infs.size() == 1);
	infos.push_back(infs[0]);
    }
    return infos;
}
