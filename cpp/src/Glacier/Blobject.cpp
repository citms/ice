// **********************************************************************
//
// Copyright (c) 2001
// MutableRealms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

#include <Ice/RoutingTable.h>
#include <Glacier/Blobject.h>

using namespace std;
using namespace Ice;
using namespace Glacier;

Glacier::Blobject::Blobject(const CommunicatorPtr& communicator) :
    _communicator(communicator),
    _logger(_communicator->getLogger())
{
}

Glacier::Blobject::~Blobject()
{
    assert(!_communicator);
    assert(!_missiveQueue);
}

void
Glacier::Blobject::destroy()
{
    //
    // No mutex protection necessary, destroy is only called after all
    // object adapters have shut down.
    //
    _communicator = 0;
    _logger = 0;

    {
	IceUtil::Mutex::Lock lock(_missiveQueueMutex);
	if (_missiveQueue)
	{
	    _missiveQueue->destroy();
	    _missiveQueueControl.join();
	    _missiveQueue = 0;
	}
    }
}


MissiveQueuePtr
Glacier::Blobject::modifyProxy(ObjectPrx& proxy, const Current& current)
{
    if (!current.facet.empty())
    {
	proxy = proxy->ice_newFacet(current.facet);
    }

    MissiveQueuePtr missiveQueue;
    Context::const_iterator p = current.context.find("_fwd");
    if (p != current.context.end())
    {
	for (unsigned int i = 0; i < p->second.length(); ++i)
	{
	    char option = p->second[i];
	    switch (option)
	    {
		case 't':
		{
		    proxy = proxy->ice_twoway();
		    missiveQueue = 0;
		    break;
		}
		
		case 'o':
		{
		    proxy = proxy->ice_oneway();
		    missiveQueue = 0;
		    break;
		}
		
		case 'd':
		{
		    proxy = proxy->ice_datagram();
		    missiveQueue = 0;
		    break;
		}
		
		case 'O':
		{
		    proxy = proxy->ice_batchOneway();
		    missiveQueue = getMissiveQueue();
		    break;
		}
		
		case 'D':
		{
		    proxy = proxy->ice_batchDatagram();
		    missiveQueue = getMissiveQueue();
		    break;
		}
		
		case 's':
		{
		    proxy = proxy->ice_secure(true);
		    break;
		}
		
		default:
		{
		    Warning out(_logger);
		    out << "unknown forward option `" << option << "'";
		    break;
		}
	    }
	}
    }

    return missiveQueue;
}

MissiveQueuePtr
Glacier::Blobject::getMissiveQueue()
{
    //
    // Lazy missive queue initialization.
    //
    IceUtil::Mutex::Lock lock(_missiveQueueMutex);
    if (!_missiveQueue)
    {
	_missiveQueue = new MissiveQueue;
	_missiveQueueControl = _missiveQueue->start();
    }
    return _missiveQueue;
}
