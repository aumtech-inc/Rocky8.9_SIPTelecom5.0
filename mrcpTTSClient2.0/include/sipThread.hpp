/*------------------------------------------------------------------------------
Program: sipThread.hpp
Purpose: Common includes, defines, globals, and prototypes.
Author:	Aumtech, Inc.
Update:	07/26/2006  yyq Created the file.
------------------------------------------------------------------------------*/

#ifndef SIPTHREADS_HPP
#define SIPTHREADS_HPP

#include "mrcp2_HeaderList.hpp"

class SipThread
{
	public:
		static SipThread* getInstance();
		void start();
		pthread_t getThreadId() const;

	protected:
		SipThread(){};

	private:
		SipThread(const SipThread&);
		SipThread& operator=(const SipThread&);
		static pthread_t threadId;
};

#endif
