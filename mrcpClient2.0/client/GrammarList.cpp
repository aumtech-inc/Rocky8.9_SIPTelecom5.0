#include "GrammarList.hpp"

using namespace std;

extern "C"
{
	#include <stdio.h>
}

GrammarList::GrammarList()
{
	grammarData.clear();
} // END: GrammarList()

GrammarList::~GrammarList()
{
	grammarData.clear();

} // END: ~GrammarList()

int GrammarList::removeGrammarFromList(string & zGrammarName)
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;
	int								yRc = 0;

    iter = grammarData.begin();
	sz = grammarData.size();
	for (i = 0; i < sz; i++)
	{
		yGrammarData = *iter;
		if ( yGrammarData.grammarName == zGrammarName )
		{
			//Return 1 if it was a google grammar
			if (zGrammarName.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}
			else if (yGrammarData.data.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}

			grammarData.erase( iter );
			break;
		}
        iter++;
    }

	return (yRc);

} // END: removeGrammarFromList()

int GrammarList::activateGrammar(string & zGrammarName)
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;
	int								yRc = 0;

    iter = grammarData.begin();
	sz = grammarData.size();
	for (i = 0; i < sz; i++)
	{
		yGrammarData = *iter;
		if ( yGrammarData.grammarName == zGrammarName )
		{
			//Return 1 if it was a google grammar
			if (zGrammarName.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}
			else if (yGrammarData.data.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}

			grammarData.erase( iter );
			yGrammarData.isActivated		= true;
//			grammarData.insert(iter, yGrammarData2);		// doesn't work
			grammarData.push_back(yGrammarData);
			break;
		}
        iter++;
    }

	return (yRc);

} // END: activateGrammar()

int GrammarList::deactivateGrammar(string & zGrammarName)
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;
	int								yRc = 0;

    iter = grammarData.begin();
	sz = grammarData.size();
	for (i = 0; i < sz; i++)
	{
		yGrammarData = *iter;
		if ( yGrammarData.grammarName == zGrammarName )
		{
			//Return 1 if it was a google grammar
			if (zGrammarName.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}
			else if (yGrammarData.data.find("builtin:grammar/google") != std::string::npos)
			{
				yRc = 1;
			}

			grammarData.erase( iter );
			yGrammarData.isActivated = false;
//	fprintf(stderr, "[%s, %d] DJB: Grammar (%s) isActivated=%d.\n", __FILE__, __LINE__, 
 //         yGrammarData.grammarName.c_str(), yGrammarData.isActivated);
//			grammarData.insert(iter, yGrammarData2);		// doesn't work
			grammarData.push_back(yGrammarData);
			break;
		}
        iter++;
    }

	return (yRc);

} // END: deactivateGrammar()

void GrammarList::setLoadedOnBackend(string & zGrammarName,
									bool zLoadedOnBackend)
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;

    iter = grammarData.begin();
	sz = grammarData.size();
	for (i = 0; i < sz; i++)
	{
		yGrammarData = *iter;
		if ( yGrammarData.grammarName == zGrammarName )
		{
			grammarData.erase( iter );
			yGrammarData.isLoadedOnBackend = zLoadedOnBackend;
//			grammarData.insert(iter, yGrammarData2);		// doesn't work
			grammarData.push_back(yGrammarData);
			break;
		}
        iter++;
    }
} // END: setLoadedOnBackend()

void GrammarList::addGrammarToList(int zType,
			bool zIsActivated, bool zIsLoadedOnBackend,
			string & zGrammarName, string & zData)
{
	GrammarData		yGrammarData;

    yGrammarData.type               = zType;
    yGrammarData.isActivated        = zIsActivated;
    yGrammarData.isLoadedOnBackend  = zIsLoadedOnBackend;
    yGrammarData.grammarName        = zGrammarName;
    yGrammarData.data               = zData;

	grammarData.push_back(yGrammarData);

//	fprintf(stderr, "[%s, %d] DJB: Grammar (type=%d:%s:%s) isActivated=%d loaded in memory.\n", __FILE__, __LINE__, 
//         yGrammarData.type, yGrammarData.grammarName.c_str(),
//          yGrammarData.data.c_str(), yGrammarData.isActivated);
} // END: addGrammarToList()

bool GrammarList::isGrammarAlreadyLoaded(string & zGrammarName)
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;

    iter = grammarData.begin();
	sz = grammarData.size();

	for (i = 0; i < sz; i++)
	{
		yGrammarData = *iter;
		if ( yGrammarData.grammarName == zGrammarName )
		{
			return(true);
			break;
		}
        iter++;
    }
	return(false);
} // GrammarList::isGrammarAlreadyLoaded()

bool GrammarList::isEmpty()
{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
	int								sz;
	int								i;

	if ( grammarData.empty() )
	{
		return(true);
	}
	else
	{
		return(false);
	}
} // GrammarList::isEmpty()

void GrammarList::removeAll()
{
	if ( grammarData.empty() )
	{
		return;
	}
	grammarData.clear();
} // GrammarList::removeAll()

/*------------------------------------------------------------------------------
int GrammarList::getNext()
	Return values:
		0 : no more data in list
		1 : data found and zGrammarType, zGrammarName, and zGrammarData
			are populated.
------------------------------------------------------------------------------*/
int GrammarList::getNext(int zLocator, int *zGrammarType,
			string & zGrammarName, string & zGrammarData)
{
    GrammarData							yGrammarData;
	int									sz;
	int									i;

	*zGrammarType	= -1;
	zGrammarName	= "";
	zGrammarData	= "";
	if ( grammarData.empty() )
	{
		return(0);
	}
	if ( zLocator == GRAMMARLIST_START_FROM_FIRST )
	{
    	grammarIter = grammarData.begin();
	}

    if ( grammarIter == grammarData.end() )
	{
		return(0);
	}

	yGrammarData		= *grammarIter;
	*zGrammarType 		= yGrammarData.type;
	zGrammarName 		= yGrammarData.grammarName;
	zGrammarData 		= yGrammarData.data;
	grammarIter++;

	return(1);

} // GrammarList::getNext()

/*------------------------------------------------------------------------------
int GrammarList::getNextActiveGrammar()
	Return values:
		0 : no more data in list
		1 : data found and zGrammarType, zGrammarName, and zGrammarData
			are populated.
------------------------------------------------------------------------------*/
int GrammarList::getNextActiveGrammar(int zLocator, int *zGrammarType,
			string & zGrammarName, string & zGrammarData)
{
    GrammarData							yGrammarData;
	int									sz;
	int									i;

	*zGrammarType	= -1;
	zGrammarName	= "";
	zGrammarData	= "";
	if ( grammarData.empty() )
	{
		return(0);
	}
	if ( zLocator == GRAMMARLIST_START_FROM_FIRST )
	{
    	grammarIter = grammarData.begin();
	}

    if ( grammarIter == grammarData.end() )
	{
		return(0);
	}

	yGrammarData		= *grammarIter;

//	fprintf(stderr, "[%s, %d] DJB: yGrammarData.grammarName=%s, yGrammarData.isActivated=%d\n",
//				__FILE__, __LINE__, yGrammarData.grammarName, yGrammarData.isActivated);

	if(yGrammarData.isActivated == false)
	{
		grammarIter++;
		return getNextActiveGrammar(GRAMMARLIST_NEXT, zGrammarType,
            					zGrammarName, zGrammarData);
	}
	*zGrammarType 		= yGrammarData.type;
	zGrammarName 		= yGrammarData.grammarName;
	zGrammarData 		= yGrammarData.data;
	grammarIter++;

	return(1);

} // GrammarList::getNextActiveGrammar()

void GrammarList::printGrammarList()

{
    list <GrammarData>::iterator    iter;
    GrammarData                     yGrammarData;
    int                             yCounter;

    iter = grammarData.begin();

    yCounter = 1;
    while ( iter != grammarData.end() )
    {
		yGrammarData = *iter;

//		fprintf(stderr, "[%s, %d] %d: type=%d; name=(%s); data=(%s); "
//				"isActivated=%d; isLoadedOnBackend=%d.\n",
//			__FILE__, __LINE__, 
//           yCounter, yGrammarData.type,
//          yGrammarData.grammarName.c_str(), yGrammarData.data.c_str(),
//			yGrammarData.isActivated, yGrammarData.isLoadedOnBackend);
        iter++;
        yCounter++;
    }

} // END: printGrammarList()

void GrammarList::deactivateAllGrammars()
{
    list <GrammarData>::iterator    iter;
    int                             sz;
    int                             i;
    GrammarData                     yGrammarData;

    iter = grammarData.begin();
    sz = grammarData.size();

    for (i = 0; i < sz; i++)
    {
        iter->isActivated = false;
//		fprintf(stderr, "DJB [%s, %d] isActivated=%d; \n",
//			__FILE__, __LINE__, iter->isActivated);
        //yGrammarData.isActivated = false;
        iter++;
    }
    printGrammarList();
}
