/*************************************************************************
 * Copyright (c) 2014 eProsima. All rights reserved.
 *
 * This copy of eProsima RTPS is licensed to you under the terms described in the
 * fastrtps_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/*
 * RTPSDomain.cpp
 *
 */

#include "fastrtps/rtps/RTPSDomain.h"

#include "fastrtps/rtps/participant/RTPSParticipant.h"
#include "fastrtps/rtps/participant/RTPSParticipantImpl.h"

#include "fastrtps/utils/RTPSLog.h"

#include "fastrtps/utils/IPFinder.h"
#include "fastrtps/utils/eClock.h"

#include "fastrtps/rtps/writer/RTPSWriter.h"
#include "fastrtps/rtps/reader/RTPSReader.h"

namespace eprosima {
namespace fastrtps{
namespace rtps {

static const char* const CLASS_NAME = "DomainRTPSParticipant";

bool RTPSDomain::instanceFlag = false;
RTPSDomain* RTPSDomain::single = nullptr;
uint32_t RTPSDomain::m_maxRTPSParticipantID = 0;
std::vector<RTPSDomain::t_p_RTPSParticipant> RTPSDomain::m_RTPSParticipants;
std::set<uint32_t> RTPSDomain::m_RTPSParticipantIDs;


RTPSDomain* RTPSDomain::getInstance()
{
	if(! instanceFlag)
	{
		single = new RTPSDomain();
		instanceFlag = true;
	}

	return single;
}

RTPSDomain::RTPSDomain()
{
	m_maxRTPSParticipantID = 0;//private constructor

	srand (static_cast <unsigned> (time(0)));
}

RTPSDomain::~RTPSDomain()
{
const char* const METHOD_NAME = "~RTPSDomain";
	logInfo(RTPS_PARTICIPANT,"DELETING ALL ENDPOINTS IN THIS DOMAIN",C_WHITE);

	for(auto it=m_RTPSParticipants.begin();
			it!=m_RTPSParticipants.end();++it)
	{
		delete(it->second);
	}

	logInfo(RTPS_PARTICIPANT,"RTPSParticipants deleted correctly ");

	RTPSDomain::instanceFlag = false;
	eClock::my_sleep(100);
	Log::removeLog();

}

void RTPSDomain::stopAll()
{
	delete(getInstance());
}

RTPSParticipant* RTPSDomain::createParticipant(RTPSParticipantAttributes& PParam,
														RTPSParticipantListener* listen)
{
	const char* const METHOD_NAME = "createRTPSParticipant";
	logInfo(RTPS_PARTICIPANT,"");

	if(PParam.builtin.leaseDuration < c_TimeInfinite && PParam.builtin.leaseDuration <= PParam.builtin.leaseDuration_announcementperiod)
	{
		logError(RTPS_PARTICIPANT,"RTPSParticipant Attributes: LeaseDuration should be >= leaseDuration announcement period");
		return nullptr;
	}
	uint32_t ID;
	if(PParam.participantID < 0)
	{
		ID = getNewId();
		while(m_RTPSParticipantIDs.insert(ID).second == false)
			ID = getNewId();
	}
	else
	{
		ID = PParam.participantID;
		if(m_RTPSParticipantIDs.insert(ID).second == false)
		{
			logError(RTPS_PARTICIPANT,"RTPSParticipant with the same ID already exists" << endl;)
			return nullptr;
		}
	}
	int pid;
#if defined(_WIN32)
	pid = (int)_getpid();
#else
	pid = (int)getpid();
#endif
	GuidPrefix_t guidP;
	LocatorList_t loc;
	IPFinder::getIPAddress(&loc);
	if(loc.size()>0)
	{
		guidP.value[0] = c_eProsimaVendorId[0];
		guidP.value[1] = c_eProsimaVendorId[1];
		guidP.value[2] = loc.begin()->address[14];
		guidP.value[3] = loc.begin()->address[15];
	}
	else
	{
		guidP.value[0] = c_eProsimaVendorId[0];
		guidP.value[1] = c_eProsimaVendorId[1];
		guidP.value[2] = 127;
		guidP.value[3] = 1;
	}
	guidP.value[4] = ((octet*)&pid)[0];
	guidP.value[5] = ((octet*)&pid)[1];
	guidP.value[6] = ((octet*)&pid)[2];
	guidP.value[7] = ((octet*)&pid)[3];
	guidP.value[8] = ((octet*)&ID)[0];
	guidP.value[9] = ((octet*)&ID)[1];
	guidP.value[10] = ((octet*)&ID)[2];
	guidP.value[11] = ((octet*)&ID)[3];

	RTPSParticipant* p = new RTPSParticipant(nullptr);
	RTPSParticipantImpl* pimpl = new RTPSParticipantImpl(PParam,guidP,p,listen);
	m_maxRTPSParticipantID = pimpl->getParticipantID();

	m_RTPSParticipants.push_back(t_p_RTPSParticipant(p,pimpl));
	return p;
}



bool RTPSDomain::removeRTPSParticipant(RTPSParticipant* p)
{
	const char* const METHOD_NAME = "removeRTPSParticipant";
	if(p!=nullptr)
	{
		for(auto it = m_RTPSParticipants.begin();it!= m_RTPSParticipants.end();++it)
		{
			if(it->second->getGuid().guidPrefix == p->getGuid().guidPrefix)
			{
				delete(it->first);
				delete(it->second);
				m_RTPSParticipants.erase(it);
				return true;
			}
		}
	}
	logError(RTPS_PARTICIPANT,"RTPSParticipant not valid or not recognized");
	return false;
}

RTPSWriter* RTPSDomain::createRTPSWriter(RTPSParticipant* p, WriterAttributes& watt, WriterHistory* hist, WriterListener* listen)
{
	for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
	{
		if(it->first->getGuid().guidPrefix == p->getGuid().guidPrefix)
		{
			RTPSWriter* writ;
			if(it->second->createWriter(&writ,watt,hist,listen))
				return writ;
			return nullptr;
		}
	}
	return nullptr;
}

bool RTPSDomain::removeRTPSWriter(RTPSWriter* writer)
{
	for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
	{
		if(it->first->getGuid().guidPrefix == writer->getGuid().guidPrefix)
		{
			return it->second->deleteUserEndpoint((Endpoint*)writer);
		}
	}
	return nullptr;
}

RTPSReader* RTPSDomain::createRTPSReader(RTPSParticipant* p, ReaderAttributes& ratt,
		ReaderHistory* rhist, ReaderListener* rlisten)
{
	for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
	{
		if(it->first->getGuid().guidPrefix == p->getGuid().guidPrefix)
		{
			RTPSReader* reader;
			if(it->second->createReader(&reader,ratt,rhist,rlisten))
				return reader;
			return nullptr;
		}
	}
	return nullptr;
}

bool RTPSDomain::removeRTPSReader(RTPSReader* reader)
{
	for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
	{
		if(it->first->getGuid().guidPrefix == reader->getGuid().guidPrefix)
		{
			return it->second->deleteUserEndpoint((Endpoint*)reader);
		}
	}
	return nullptr;
}


}
} /* namespace  */
} /* namespace eprosima */


