/*------------------------------------------------------------------------------
Program: readerThread.hpp
Author:	Aumtech, Inc.
Update:	07/26/2006  yyq Created the file.
------------------------------------------------------------------------------*/

#ifndef READERTHREAD_HPP
#define READERTHREAD_HPP

#include "mrcp2_HeaderList.hpp"


class ReaderThread{
public:
	ReaderThread(){
		pthread_t threadId;
		readerThreadId = threadId;
	};
	~ReaderThread(){};
	void start();
	pthread_t getThreadId() const;
private:
	pthread_t	readerThreadId;
	string		client2Fifo;
	int			client2FifoFd;
};


#endif
