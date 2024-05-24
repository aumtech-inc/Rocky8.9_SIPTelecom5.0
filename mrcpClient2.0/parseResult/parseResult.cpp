/*------------------------------------------------------------------------------
File:		parseResult.cpp
Author:		Aumtech, Inc.
Update: 06/02/2006 djb MR# 1257.  Added work-around for dtmf parsing result
Update: 06/29/2006 djb Added removal of GrammaId#RuleId to accomodate Loquendo.
Update: 06/30/2006 djb Added dtmf mode logic to accomodate Loquendo.
Update:	08/01/2006 djb Ported to mrcpV2.
------------------------------------------------------------------------------*/
/*Contact:	deep@aumtechinc.com*/

#include <util/PlatformUtils.hpp>
#include <util/XMLString.hpp>
#include <util/XMLUniDefs.hpp>
#include <framework/XMLFormatter.hpp>
#include <util/TranscodingException.hpp>
#include <dom/DOM_DOMException.hpp>
#include <parsers/DOMParser.hpp>
#include <dom/DOM.hpp>
#include "DOMTreeErrorReporter.hpp"
#include <string>
#include <stdlib.h>
#include <ctype.h>
#include <cstdio>
#include <framework/MemBufInputSource.hpp>
#include "mrcpCommon2.hpp"
#include "parseResult.hpp"

using namespace std;

typedef struct parserGlobals
{
	char	gReturnValue[128];
	char	gDtmfValue[128];
	int		gDtmfConfidence;
	char	gInterpretationGrammar[128];
	char	gLiteralTimings[2048];
	
	XmlResultInfo	*gResultPtr;
}ParserGlobals;

ParserGlobals yParserGlobals[300];

class DOMPrintFormatTarget : public XMLFormatTarget
{
public:
    DOMPrintFormatTarget()  {};
    ~DOMPrintFormatTarget() {};

    void writeChars(const   XMLByte* const  toWrite,
                    const   unsigned int    count,
                            XMLFormatter * const formatter)
    {
        cout.write((char *) toWrite, (int) count);
    };

private:
    DOMPrintFormatTarget(const DOMPrintFormatTarget& other);
    void operator=(const DOMPrintFormatTarget& rhs);
};

ostream& operator<<(ostream& target, const DOMString& toWrite);
ostream& operator<<(ostream& target, DOM_Node& toWrite);
XMLFormatter& operator<< (XMLFormatter& strm, const DOMString& s);

void cleanPStr(char *zStr, int zLen);
void insertBackSlashes(char *zStr, int zLen);
void removeBrackets(char *zStr);

int executeInputTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;
	char			tmpDtmf[256] = "null";
	int				tmpDtmfConfidence = -1;

	DOM_NamedNodeMap attributes ;
	int attrCount = -1;

	attributes = aNode.getAttributes();

	nodeNameChar = aNode.getFirstChild().getNodeValue().transcode();

	sprintf(yParserGlobals[zPort].gResultPtr->inputValue, "%s", nodeNameChar);
	cleanPStr(yParserGlobals[zPort].gResultPtr->inputValue, strlen(yParserGlobals[zPort].gResultPtr->inputValue));

	if(nodeNameChar != NULL)
		delete(nodeNameChar); 

	nodeNameChar = NULL;

	if(attributes != 0)
	{
		attrCount = attributes.getLength();

		for (int i = 0; i < attrCount; i++)
   		{
			DOM_Node  attribute = attributes.item(i);

			if(attribute.getNodeName().equals("mode"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();
				if ( strcmp(nodeNameChar, "dtmf") == 0 )
				{
					sprintf(yParserGlobals[zPort].gDtmfValue, "%s", yParserGlobals[zPort].gResultPtr->inputValue);
					sprintf(tmpDtmf, "%s", yParserGlobals[zPort].gResultPtr->inputValue);

					// djb - added this as we need to keep it for dmtf
					attribute = attributes.item(i + 1);
					if ( attribute != NULL )
					{
						if(attribute.getNodeName().equals("confidence"))
						{
							nodeNameChar = attribute.getNodeValue().transcode();
			
							if(nodeNameChar != NULL)
							{
								yParserGlobals[zPort].gResultPtr->confidence = 
									static_cast<int>(100 * atof(nodeNameChar)); 

								if ( strcmp(tmpDtmf, "null") != 0 )
								{
									tmpDtmfConfidence = yParserGlobals[zPort].gResultPtr->confidence;
								}
			
								if(nodeNameChar != NULL)
								{
									delete(nodeNameChar); 
								}
			
								nodeNameChar = NULL;
			
							}
						}
					}
				}
				if(nodeNameChar != NULL)
				{
					sprintf(yParserGlobals[zPort].gResultPtr->inputMode , "%s", nodeNameChar);

					if(nodeNameChar != NULL)
					{
						delete(nodeNameChar); 

					nodeNameChar = NULL;
					}

				}

			}
#if 0
            //YYQ: comment out following block to hide detailed confidence info
			else
			if(attribute.getNodeName().equals("confidence"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					yParserGlobals[zPort].gResultPtr->confidence = static_cast<int>(100 * atof(nodeNameChar)); 
					if ( strcmp(tmpDtmf, "null") != 0 )
					{
						tmpDtmfConfidence = yParserGlobals[zPort].gResultPtr->confidence;
					}

					if(nodeNameChar != NULL)
						delete(nodeNameChar); 

					nodeNameChar = NULL;

				}

			}
#endif
		}
	}

	if ( strcmp(tmpDtmf, "null") != 0 )
	{
		yParserGlobals[zPort].gDtmfConfidence	= tmpDtmfConfidence;
	}

	return (0);

}/*END: void executeInputTag*/

int executeTextTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;

	DOM_NamedNodeMap attributes ;
	int attrCount = -1;

	attributes = aNode.getAttributes();

	nodeNameChar = aNode.getFirstChild().getNodeValue().transcode();

	sprintf(yParserGlobals[zPort].gResultPtr->inputValue, "%s", nodeNameChar);

	if(nodeNameChar != NULL)
		delete(nodeNameChar); 

	nodeNameChar = NULL;

	if(attributes != 0)
	{
		attrCount = attributes.getLength();

		for (int i = 0; i < attrCount; i++)
   		{
			DOM_Node  attribute = attributes.item(i);

			if(attribute.getNodeName().equals("mode"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					sprintf(yParserGlobals[zPort].gResultPtr->inputMode , "%s", nodeNameChar);

					if(nodeNameChar != NULL)
						delete(nodeNameChar); 

					nodeNameChar = NULL;

				}

			}
			else
			if(attribute.getNodeName().equals("confidence"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					yParserGlobals[zPort].gResultPtr->confidence = static_cast<int>(100 * atof(nodeNameChar));

					if(nodeNameChar != NULL)
						delete(nodeNameChar); 

					nodeNameChar = NULL;

				}

			}
		}
	}

	return (0);

}/*END: void executeTextTag*/

int executeReturnValueTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;

	// yParserGlobals[zPort].gDtmfValue[0] = '\0';
	yParserGlobals[zPort].gReturnValue[0] = '\0';

	nodeNameChar = aNode.getNodeValue().transcode();

	if(aNode.hasChildNodes())
	{
		sprintf(yParserGlobals[zPort].gDtmfValue, 	"%s", nodeNameChar);
		sprintf(yParserGlobals[zPort].gReturnValue, 	"%s", nodeNameChar);

		if(nodeNameChar != NULL)
			delete(nodeNameChar);

		nodeNameChar = aNode.getFirstChild().getNodeValue().transcode();
	}

	if(nodeNameChar != NULL && nodeNameChar[0] != '\0')
	{
		sprintf(yParserGlobals[zPort].gDtmfValue, 	"%s", nodeNameChar);
		sprintf(yParserGlobals[zPort].gReturnValue, 	"%s", nodeNameChar);
	}

	if(nodeNameChar != NULL)
		delete(nodeNameChar);

	return (0);

}/*END: void executeReturnValueTag */

int executeRESULTTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;

	// gDtmfValue[0] = '\0';

	nodeNameChar = aNode.getNodeValue().transcode();

	if(aNode.hasChildNodes())
	{
		sprintf(yParserGlobals[zPort].gDtmfValue, "%s", nodeNameChar);

		if(nodeNameChar != NULL)
			delete(nodeNameChar);

		nodeNameChar = aNode.getFirstChild().getNodeValue().transcode();
	}

	if(nodeNameChar != NULL && nodeNameChar[0] != '\0')
	{
		sprintf(yParserGlobals[zPort].gDtmfValue, "%s", nodeNameChar);
	}

	if(nodeNameChar != NULL)
		delete(nodeNameChar);

	return (0);

}/*END: void executeResultTag*/

int executeResultTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;

	DOM_NamedNodeMap attributes ;
	int attrCount = -1;

	attributes = aNode.getAttributes();

	if(attributes != 0)
	{
		attrCount = attributes.getLength();

		for (int i = 0; i < attrCount; i++)
   		{
			DOM_Node  attribute = attributes.item(i);

			if(attribute.getNodeName().equals("grammar"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					sprintf(yParserGlobals[zPort].gResultPtr->resultGrammar , "%s", nodeNameChar);

					if(nodeNameChar != NULL)
						delete(nodeNameChar);

					nodeNameChar = NULL;
				}

			}
		}
	}

	return (0);

}/*END: void executeResultTag*/

int executeInterpretationTag(int zPort, DOM_Node aNode)
{
	char *nodeNameChar 		= NULL;
	const XMLCh *nodeName	= NULL;

	int attrCount = -1;

	DOM_NamedNodeMap attributes ;
	attributes = aNode.getAttributes();

	if(attributes != 0)
	{
		attrCount = attributes.getLength();

		for (int i = 0; i < attrCount; i++)
   		{
			DOM_Node  attribute = attributes.item(i);

			/* loquendo mrcp2.0 server returns float confidence[0,1],  
			 * Vxml requires int type confidence [0,100] 
			 */
			if (attribute.getNodeName().equals("confidence"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					yParserGlobals[zPort].gResultPtr->confidence = static_cast<int> (100 * atof(nodeNameChar));

					delete(nodeNameChar);
					nodeNameChar = NULL;
				}

			}
			else
			if (attribute.getNodeName().equals("conf"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					if ( strncmp(nodeNameChar, "0.", 2) == 0 )
					{
						yParserGlobals[zPort].gResultPtr->confidence = static_cast<int> (100*atof(&(nodeNameChar[2])));
					}
					else
					{
						yParserGlobals[zPort].gResultPtr->confidence = static_cast<int> (100 * atof(nodeNameChar));
					}
					delete(nodeNameChar);
					nodeNameChar = NULL;
				}

			}
			else
			if(attribute.getNodeName().equals("grammar"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					sprintf(yParserGlobals[zPort].gInterpretationGrammar, "%s", nodeNameChar);
					delete(nodeNameChar);
					nodeNameChar = NULL;
				}

			}
		}
	}

	return (0);

}/*END: void executeInterpretationTag(DOM_Node aNode)*/

int executeSwiLiteralTimings(int zPort, DOM_Node aNode)
{
	char	*rootNameChar 		= NULL;
	char	*nodeNameChar 		= NULL;
	char	*yNodeValue 		= NULL;
	char	*yNodeText = NULL;
	int		i;
	int		rc;

	rootNameChar = aNode.getNodeName().transcode();
	if(aNode.getNodeType() != DOM_Node::ELEMENT_NODE)
	{
		return(0);
	}
	if(rootNameChar == NULL)
	{
		return(0);
	}

	if ( strcmp(rootNameChar, "SWI_literalTimings") != 0 )
	{
		strcat(yParserGlobals[zPort].gLiteralTimings, "<");
		strcat(yParserGlobals[zPort].gLiteralTimings, rootNameChar);
	}

	// Output any attributes on this element
	DOM_NamedNodeMap atts = aNode.getAttributes ();

	// Process atts 
	for (unsigned int i = 0; atts != NULL, i < atts.getLength (); i++)
	{
		DOM_Node 	att = atts.item(i);
		char		*yNodeName;
		char		*yNodeValue;

		yNodeName = att.getNodeName().transcode();

		if ((yNodeValue = att.getNodeValue().transcode()) != NULL)
		{
			strcat(yParserGlobals[zPort].gLiteralTimings, " ");
			strcat(yParserGlobals[zPort].gLiteralTimings, yNodeName);
			strcat(yParserGlobals[zPort].gLiteralTimings, "=\"");
			strcat(yParserGlobals[zPort].gLiteralTimings, yNodeValue);
			strcat(yParserGlobals[zPort].gLiteralTimings, "\"");
		}
	}

	DOM_Node child;
	child = aNode.getFirstChild ();

	if (child != NULL)
	{
		// Close start and process kids
		if ( strcmp(rootNameChar, "SWI_literalTimings") != 0 )
		{
			strcat(yParserGlobals[zPort].gLiteralTimings, ">");
		}

		for (; child != NULL; child = child.getNextSibling ())
		{

			if (child.getNodeType () != DOM_Node::ELEMENT_NODE)
			{
				continue;
			}
	
			/*
			** Start the node
			*/ 
			strcat(yParserGlobals[zPort].gLiteralTimings, " <");
			nodeNameChar = child.getNodeName().transcode();
			strcat(yParserGlobals[zPort].gLiteralTimings, nodeNameChar);

			/*
			** Add the attributes of the node
			*/ 
			DOM_NamedNodeMap yAttribs = child.getAttributes ();

			for (unsigned int i = 0; 
				 yAttribs != NULL, i<yAttribs.getLength (); 
				 i++)
			{
				DOM_Node 	att = yAttribs.item (i);

				if(att == NULL)
				{
					continue;
				}

				if ((yNodeValue = att.getNodeValue().transcode()) != NULL)
				{
					strcat(yParserGlobals[zPort].gLiteralTimings, " ");
					nodeNameChar = att.getNodeName().transcode();
					strcat(yParserGlobals[zPort].gLiteralTimings, nodeNameChar);
					strcat(yParserGlobals[zPort].gLiteralTimings, "=\"");
					strcat(yParserGlobals[zPort].gLiteralTimings, yNodeValue);
					strcat(yParserGlobals[zPort].gLiteralTimings, "\"");
				}
			}

			/*
			** Close the start node
			*/ 
			strcat(yParserGlobals[zPort].gLiteralTimings, ">");

			/*
			** Add the text of the node
			*/
			DOM_Node	yTextChild = child.getFirstChild();

			for (; yTextChild != NULL;
						yTextChild = yTextChild.getNextSibling ())
			{
				if(yTextChild.getNodeType() != DOM_Node::TEXT_NODE)
				{
					continue;
				}

				yNodeText = yTextChild.getNodeValue().transcode();

				if(yNodeText)
				{
					cleanPStr(yNodeText, strlen(yNodeText));

					if(*yNodeText)
					{
						strcat(yParserGlobals[zPort].gLiteralTimings, " ");
						strcat(yParserGlobals[zPort].gLiteralTimings, yNodeText);
					}
				}
			}

			/*
			** Add the children 
			*/
			DOM_Node	yChild2 = child.getFirstChild();

			for (; yChild2 != NULL; yChild2 = yChild2.getNextSibling ())
			{
				rc = executeSwiLiteralTimings(zPort, yChild2);
			}

			/*
			** Close the end of the node
			*/ 
			strcat(yParserGlobals[zPort].gLiteralTimings, "</");
			nodeNameChar = child.getNodeName().transcode();
			strcat(yParserGlobals[zPort].gLiteralTimings, nodeNameChar);
			strcat(yParserGlobals[zPort].gLiteralTimings, ">");
		}

		/*
		** Add the text of the node
		*/
		DOM_Node	yTextChild = aNode.getFirstChild();

		for (; yTextChild != NULL; yTextChild = yTextChild.getNextSibling ())
		{
			if(yTextChild.getNodeType() != DOM_Node::TEXT_NODE)
			{
				continue;
			}
			yNodeText = yTextChild.getNodeValue().transcode();

			if(yNodeText)
			{
				cleanPStr(yNodeText, strlen(yNodeText));

				if(*yNodeText)
				{
					strcat(yParserGlobals[zPort].gLiteralTimings, " ");
					strcat(yParserGlobals[zPort].gLiteralTimings, yNodeText);
				}
			}
		}

		/*
		** Close the root tag
		*/ 
		if ( strcmp(rootNameChar, "SWI_literalTimings") != 0 )
		{
			strcat(yParserGlobals[zPort].gLiteralTimings, " </");
			strcat(yParserGlobals[zPort].gLiteralTimings, rootNameChar);
			strcat(yParserGlobals[zPort].gLiteralTimings, ">");
		}
	}
	else
	{
		strcat(yParserGlobals[zPort].gLiteralTimings, "/> ");
	}

	return(0);

} // executeSwiLiteralTimings

int executeInstanceTag(int zPort, DOM_Node aNode)
{
	char	*nodeNameChar 		= NULL;
	int		i;

	nodeNameChar = aNode.getNodeName().transcode();
	if(nodeNameChar == NULL)
	{
		return(0);
	}
	if( strcmp(nodeNameChar,"instance") != 0 )
	{
		return(0);
	}

	char *NodeName;
	char *NodeValue;
	DOM_Node child;
	DOM_Node tempNode;

	int attrCount = -1;
	DOM_NamedNodeMap attributes ;
	attributes = aNode.getAttributes();
	if(attributes != 0)
	{
		attrCount = attributes.getLength();

		for (int i = 0; i < attrCount; i++)
   		{
			DOM_Node  attribute = attributes.item(i);
			if(attribute.getNodeName().equals("grammar"))
			{
				nodeNameChar = attribute.getNodeValue().transcode();

				if(nodeNameChar != NULL)
				{
					sprintf(yParserGlobals[zPort].gInterpretationGrammar, "%s", nodeNameChar);
					delete(nodeNameChar);
					nodeNameChar = NULL;
				}

			}
		}
	}

	i = 0;
	for(child = aNode.getFirstChild(); 
					child!=0; child = child.getNextSibling())
	{
		if ((NodeName = child.getNodeName().transcode()) == NULL)
		{
			continue;
		}
		if ((NodeValue = child.getNodeValue().transcode()) == NULL)
		{
			continue;
		}

		if ((tempNode = child.getFirstChild()) == NULL)
		{
			tempNode = child;
		} 
		
		if ((NodeValue = tempNode.getNodeValue().transcode()) == NULL)
		{
			continue;
		}

		cleanPStr(NodeName, strlen(NodeName));

		if ( strcmp(NodeName, "SWI_literalTimings") == 0)
		{
			executeSwiLiteralTimings(zPort, child);
			sprintf(yParserGlobals[zPort].gResultPtr->literalTimings, "%.*s",
					sizeof(yParserGlobals[zPort].gResultPtr->literalTimings), yParserGlobals[zPort].gLiteralTimings);
			cleanPStr(yParserGlobals[zPort].gResultPtr->literalTimings,
					strlen(yParserGlobals[zPort].gResultPtr->literalTimings));
			insertBackSlashes(yParserGlobals[zPort].gResultPtr->literalTimings,
					strlen(yParserGlobals[zPort].gResultPtr->literalTimings));
		}

		cleanPStr(NodeValue, strlen(NodeValue));
		if ( ( NodeName[0] == '\0' ) ||
		     ( NodeValue[0] == '\0' ) )
		{
			continue;
		}

		if ( strcmp(NodeName, "#text") == 0 )
		{
			sprintf(yParserGlobals[zPort].gResultPtr->instance, "%s", NodeValue);
			continue;
		}

		if ( strcmp(NodeName, "SWI_meaning") == 0 )
		{
			removeBrackets(NodeValue);
		}

		sprintf(yParserGlobals[zPort].gResultPtr->instanceChild[i].instanceName, "%s", NodeName);
		sprintf(yParserGlobals[zPort].gResultPtr->instanceChild[i].instanceValue, "%s",
											NodeValue);
		i++;
	}
	yParserGlobals[zPort].gResultPtr->numInstanceChildren = i;

	return (0);

}/*END: void executeInstanceTag*/


int traverseNode(int zPort, DOM_Node aNode, int level)
{

		char *nodeNameChar 		= NULL;

        DOM_NamedNodeMap attributes ;
		int attrCount = -1;
		int retCode = 0;

		while(aNode != NULL)
		{
			nodeNameChar = aNode.getNodeName().transcode();

			switch (aNode.getNodeType())
			{
				case DOM_Node::TEXT_NODE:
					//cout<< "TEXT NODE "<< " "<<nodeNameChar << " Val: "<< aNode.getNodeValue() << endl;
					break;

        		case DOM_Node::PROCESSING_INSTRUCTION_NODE :
					//cout<< "PROCESS INSTRUCTION"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::DOCUMENT_NODE :
					//cout<< "DOCUMENT NODE"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::ELEMENT_NODE :
					//cout<< "ELEMENT NODE"<< " "<<nodeNameChar << endl;

#if 0
					attributes = aNode.getAttributes();

					if(attributes != 0)
					{
            			attrCount = attributes.getLength();

            			for (int i = 0; i < attrCount; i++)
            			{
                			DOM_Node  attribute = attributes.item(i);
						}
					}
#endif

					if(strcmp(nodeNameChar, "result") == 0)
					{
						executeResultTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "RESULT") == 0)
					{
						executeRESULTTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "returnvalue") == 0)
					{
						executeReturnValueTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "interpretation") == 0)
					{
						executeInterpretationTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "text") == 0)
					{
						executeTextTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "instance") == 0)
					{
						executeInstanceTag(zPort, aNode);
					}
					else
					if(strcmp(nodeNameChar, "input") == 0)
					{
						executeInputTag(zPort, aNode);
					}
					else
					break;

        		case DOM_Node::ENTITY_REFERENCE_NODE:
					//cout<< "ENTITY REFERENCE"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::CDATA_SECTION_NODE:
					//cout<< "CDATA NODE"<< " "<<nodeNameChar << endl;
					break;
					
        		case DOM_Node::COMMENT_NODE:
					//cout<< "COMMENT NODE"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::DOCUMENT_TYPE_NODE:
					//cout<< "DOC TYPE NODE"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::ENTITY_NODE:
					//cout<< "ENTITY NODE"<< " "<<nodeNameChar << endl;
					break;

        		case DOM_Node::XML_DECL_NODE:
					//cout<< "XML DECL NODE"<< " "<<nodeNameChar << endl;
					break;

        		default:
					//cout<< "Unknown tag"<< " "<<nodeNameChar << endl;
					break;
			}

			if(nodeNameChar != NULL)
			{
				delete(nodeNameChar);
				nodeNameChar = NULL;
			}


			if(aNode.hasChildNodes())
			{
				retCode = traverseNode(zPort, aNode.getFirstChild(), level + 1);

				if(retCode !=0) break;
			}

			aNode = aNode.getNextSibling();
		}

		return (retCode);

}/*END: void traverseNode(DOM_Node aNode, int level)*/


int parseXMLResultString(int zPort, const char *zData, int zSize, 
				XmlResultInfo* lpResult, char*zErrMsg)
{

    int retval = 0;
	static int platformInitDone = 0;
	char		*pData;
	int			pSize;
	int			i;
	char		*p;

	yParserGlobals[zPort].gLiteralTimings[0]		= '\0';
	yParserGlobals[zPort].gReturnValue[0] 		= '\0';
	yParserGlobals[zPort].gDtmfValue[0] 			= '\0';
	yParserGlobals[zPort].gDtmfConfidence			= -1;

	yParserGlobals[zPort].gResultPtr = lpResult;

//	memset((XmlResultInfo *) lpResult, '\0', sizeof(XmlResultInfo));

	if(zErrMsg == NULL)
	{
		return (-1);
	}
	
	i=0;
	pSize = zSize;
	pData = (char *)zData;
	while ( isspace (zData[i]) )
	{
		i++;
		pData = (char *)&(zData[i]);
		pSize--;
	}

	if(platformInitDone == 0)
	{
    	try
    	{
        	XMLPlatformUtils::Initialize();
    	}
    	catch(const XMLException& toCatch)
    	{
			sprintf(zErrMsg, "%s", "Error initializing XML platform");
        	return(-1);
    	}

		platformInitDone = 1;
	}

	MemBufInputSource *lpMemBuf = 
		new  MemBufInputSource((const XMLByte*)pData, pSize, (char*)NULL, false);

    DOMParser *parser = new DOMParser;

	parser->setExitOnFirstFatalError(false);

    DOMTreeErrorReporter *errReporter = new DOMTreeErrorReporter(zErrMsg);

    parser->setErrorHandler(errReporter);

    bool errorsOccured = false;

    try
    {
        parser->parse(*lpMemBuf);

        int errorCount = parser->getErrorCount();

        if (errorCount > 0)
		{
            errorsOccured = true;
			retval = -1;
		}
    }

    catch (const XMLException& e)
    {
		DOMString x = e.getMessage();

		char * errMsg = x.transcode();

        //cerr << "An error occured during parsing\n   Message: " << DOMString(e.getMessage()) << endl;

		sprintf(zErrMsg, "%s", errMsg);

		delete(errMsg);
		delete(lpMemBuf);
        errorsOccured = true;
		retval = -1;
    }

    catch (const DOM_DOMException& e)
    {
		DOMString x = e.msg;

		char * errMsg = x.transcode();

		sprintf(zErrMsg, "%s", errMsg);

		delete(errMsg);
		delete(lpMemBuf);
		retval = -1;
        errorsOccured = true;

       	//cerr << "A DOM error occured during parsing\n   DOMException code: " << e.code << endl;
    }

    catch (...)
    {
        cerr << "An error occured during parsing\n " << endl;

		sprintf(zErrMsg, "Unknown syntax error");

        errorsOccured = true;

		retval = -1;
    }

    if (!errorsOccured && !errReporter->getSawErrors())
    {
        DOM_Node doc = parser->getDocument();
		if(doc == NULL)
		{
			delete(lpMemBuf);
    		delete errReporter;
			delete parser;
			return (-1);
		}

        DOM_Node aNode = doc.getFirstChild();
		if(aNode == NULL)
		{
			delete(lpMemBuf);
    		delete errReporter;
			delete parser;
			return (-1);
		}

		int rc = traverseNode(zPort, aNode, 1);

		if(rc == 0)
		{
			if(yParserGlobals[zPort].gResultPtr->resultGrammar[0] == '\0')
			{
				sprintf(yParserGlobals[zPort].gResultPtr->resultGrammar, 	"%s",
								yParserGlobals[zPort].gInterpretationGrammar);
			}

			if(yParserGlobals[zPort].gReturnValue[0] != '\0')
			{
				sprintf(lpResult->inputValue,  "%s", yParserGlobals[zPort].gReturnValue);
			}

			if( (strcmp(yParserGlobals[zPort].gResultPtr->inputMode, "dtmf") == 0) ||
			    (strcmp(yParserGlobals[zPort].gResultPtr->inputMode, "DTMF") == 0) )
			{
				if(yParserGlobals[zPort].gDtmfValue[0])
				{
					sprintf(lpResult->inputValue,  "%s", yParserGlobals[zPort].gDtmfValue);
					if ( yParserGlobals[zPort].gDtmfConfidence > -1 )
					{
						lpResult->confidence = yParserGlobals[zPort].gDtmfConfidence;
					}
				}
			}

            if(strstr(lpResult->inputValue, "returnvalue=\'") != NULL)
            {
                /* It means Telisma results need to be 
				   filtered so that VXML wont throw JS error*/

                char tmpResult[256];

                char * tmpString = strstr(lpResult->inputValue, "returnvalue=\'");

                sprintf(tmpResult, "%s", "\0");

                sprintf(tmpResult, "%s", tmpString + strlen("returnvalue=\'") );

                if(tmpResult && *tmpResult && tmpResult[strlen(tmpResult) - 1] == '\'')
                {
                    tmpResult[strlen(tmpResult) -1] = '\0';

                }

                sprintf(lpResult->inputValue,  "%s", tmpResult);

            }

		}
		else
		{
			lpResult->confidence 	= 0;
//			lpResult->retCode 		= -1;

//			sprintf(lpResult->instance, 			"%s", "\0");
			sprintf(lpResult->resultGrammar, 			"%s", "\0");
//			sprintf(lpResult->interpretationGrammar, 	"%s", "\0");
			sprintf(lpResult->inputValue, 				"%s", "\0");
		}

		parser->resetDocument();
	}
	else
	{
		lpResult->confidence 	= 0;

//		sprintf(lpResult->instance, 			"%s", "\0");
		sprintf(lpResult->resultGrammar, 			"%s", "\0");
//		sprintf(lpResult->interpretationGrammar, 	"%s", "\0");
		sprintf(lpResult->inputValue, 				"%s", "\0");
	}

    if ( (p = strchr((const char *)lpResult->resultGrammar, '#')) !=
                                    (char *)NULL)
    {
        *p = '\0';
    }

    delete errReporter;

    delete parser;

    //XMLPlatformUtils::Terminate();

	if ( ( lpResult->instance[0] == '\0' ) &&
	     ( lpResult->inputValue[0] != '\0' ) )
	{
		sprintf(lpResult->instance, "%s", lpResult->inputValue);
	}

	if ( ( lpResult->inputValue[0] == '\0' ) &&
	     ( lpResult->instance[0] != '\0' ) )
	{
		sprintf(lpResult->inputValue, "%s", lpResult->instance);
	}
//	if ( lpResult->swiMeaning[0] == '\0' )
//	{
//		sprintf(lpResult->swiMeaning, "%s", lpResult->instance);
//	}

	cleanPStr(lpResult->inputValue, strlen(lpResult->inputValue));
//	removeBrackets(lpResult->swiMeaning);

    return retval;
}

ostream& operator<<(ostream& target, DOM_Node& toWrite)
{
    // Get the name and value out for convenience
    DOMString   nodeName = toWrite.getNodeName();
    DOMString   nodeValue = toWrite.getNodeValue();
    unsigned long lent = nodeValue.length();

    return target;
}

ostream& operator<< (ostream& target, const DOMString& s)
{
    char *p = s.transcode();
    target << p;
    delete [] p;
    return target;
}


XMLFormatter& operator<< (XMLFormatter& strm, const DOMString& s)
{
    unsigned int lent = s.length();

	if (lent <= 0)
		return strm;

    XMLCh*  buf = new XMLCh[lent + 1];
    XMLString::copyNString(buf, s.rawBuffer(), lent);
    buf[lent] = 0;
    strm << buf;
    delete [] buf;
    return strm;
}

/*------------------------------------------------------------------------------
insertBackSlashes()
------------------------------------------------------------------------------*/
void insertBackSlashes(char *zStr, int zLen)
{
	int			i, j;
	int			spaceFound;
	char		*tmpP;
	char		*tmpP2;
	char		*p;

	if ( (zStr[0] == '\0' ) || (zLen == 0) )
	{
		return;
	}

	if(strstr(zStr, "\"") == NULL)
	{
		return;
	}

	if ((tmpP = (char *) calloc(zLen + 256, sizeof(char))) == NULL)
	{
		return;
	}
	tmpP2 = tmpP;

	j = 0;
	p = zStr;
	while ( *p != '\0' )
	{
		if (*p != '"')
		{
			*tmpP2 = *p;
			tmpP2++;
			p++;
			continue;
		}
		*tmpP2 = '\\';
		tmpP2++;
		*tmpP2 = '"';
		tmpP2++;
		p++;
	}
	sprintf(zStr, "%s", tmpP);
	free(tmpP);

	return;
} /* END: insertBackSlashes() */

/*------------------------------------------------------------------------------
cleanPStr()
------------------------------------------------------------------------------*/
void cleanPStr(char *zStr, int zLen)
{
	int			i;
	int			spaceFound;
	char		*p;

	if ( (zStr[0] == '\0' ) || (zLen == 0) )
	{
		return;
	}

	for (i = zLen - 1; i >= 0; i--)
	{
		if ( isspace(zStr[i]) )
		{
			zStr[i] = '\0';
		}
		else
		{
			break;
		}
	}

	p = zStr;
	spaceFound = 0;
	while ( (*p != '\0') && ( isspace(*p) ) )
	{
		p++;
		spaceFound = 1;
	}

	if (spaceFound)
	{
		sprintf(zStr, "%s", p);
	}
	return;
} /* END: cleanPStr() */

/*------------------------------------------------------------------------------
removeBrackets()
------------------------------------------------------------------------------*/
void removeBrackets(char *zStr)
{
	char		tmpString[256];
	char		*ptr;

	memset((char *)tmpString, '\0', sizeof(tmpString));
	ptr = zStr;

	while ( (isspace(*ptr)) || (*ptr == '{') )
	{
		ptr++;
	}

	sprintf(tmpString, "%s", ptr);
	ptr = &tmpString[(int)strlen(tmpString)-1];
	while ( (isspace(*ptr)) || (*ptr == '}') )
	{
		*ptr = '\0';
		ptr--;
	}
	sprintf(zStr, "%s", tmpString);
	
} // END: removeBrackets()
