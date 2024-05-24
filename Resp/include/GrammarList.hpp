#ifndef GRAMMARLIST_HPP
#define GRAMMARLIST_HPP

#include <iostream>
#include <string>
#include <list>

using namespace std;

const int	GRAMMARLIST_START_FROM_FIRST	= 0;
const int	GRAMMARLIST_NEXT				= 1;

struct GrammarData
{
    int           type;
    bool          isActivated;
    bool          isLoadedOnBackend;
    string        grammarName;
    string        data;
}; 

class GrammarList
{
public:
	GrammarList();
	~GrammarList();

	void addGrammarToList(int zType, bool zIsActivated, bool zIsLoadedOnBackend,
				string & zGrammarName, string & zData);
	void printGrammarList();
	void removeGrammarFromList(string & zGrammarName);
	void activateGrammar(string & zGrammarName);
	void deactivateGrammar(string & zGrammarName);
    void deactivateAllGrammars();
	void setLoadedOnBackend(string & zGrammarName, bool zLoadedOnBackend);
	void removeAll();
	bool isGrammarAlreadyLoaded(string & zGrammarName);
	bool isEmpty();
	int  getNext(int zLocator, int *zGrammarType,
            string & zGrammarName, string & zGrammarData);
    int  getNextActiveGrammar(int zLocator, int *zGrammarType,
            string & zGrammarName, string & zGrammarData);
	
private:
	list <GrammarData>	grammarData;
    list <GrammarData>::iterator grammarIter;
}; 
#endif
