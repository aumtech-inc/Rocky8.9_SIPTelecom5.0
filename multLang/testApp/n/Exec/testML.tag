#------------------------------------------------------------------------------
# File:		testML.tag
# Purpose:	Contains all the phrase definitions for the application. This
#			file is read by the taLoadTags() accessory routine to load
#			all phrase tags.
# Author:	Aumtech, Inc. (USA)
# Usage:	Each line contains a phrase tag definition entry. e.g.:
#
#		prompt		|prompt.32a	|This is a prompt
#		
#		In this case, the application would use the phrase tag "prompt"
#		to speak the phrase "prompt.32a". 
#
# Update:	02/17/2000      Aumtech, Inc.
#------------------------------------------------------------------------------
# Actual Phrase Tag	|Name of the phrase file|Actual Text or approximation.
#------------------------------------------------------------------------------
testFrench		|testMLPhrases/testFrench.32a	|To listen to French
testSpanish		|testMLPhrases/testSpanish.32a	|To listen to Spanish
testCurrency	|testMLPhrases/testCurrency.32a	|To listen to currency
testNumbers		|testMLPhrases/testNumbers.32a	|To listen to numbers
testTime 		|testMLPhrases/testTime.32a		|To listen to time
testDate 		|testMLPhrases/testDate.32a		|To listen to dates
toListenAgain	|testMLPhrases/toListenAgain.32a	|To listen again
toContinue		|testMLPhrases/toContinue.32a	|To continue
press1			|testMLPhrases/press1.32a		|Press 1
press2			|testMLPhrases/press2.32a		|Press 2
press3			|testMLPhrases/press3.32a		|Press 3
press4			|testMLPhrases/press4.32a		|Press 4
male			|testMLPhrases/male.32a			|Male
female			|testMLPhrases/female.32a		|Female

^langMenu = testFrench, press1, testSpanish, press2
^typeMenu = testCurrency, press1, testNumbers, press2, testTime, press3, testDate, press4
^continueMenu = toListenAgain, press1, toContinue, press2

#
# TEL_PromptAndCollect() entries.
#	The following entries control how the application presents menus,
#	prompting, and data collection.  Consult the Telecom API Guide for
#	complete details.
#	
#	Note:	Many of the entries have both AUTOSKIP and MANUAL prompts.  
#			For example, under the [enterAccount] header, the enterAccount
#			tag differs from enterAccountWPound in that the latter specifies
#			to the caller to terminate the input with a pound sign.  It is
#			important to ensure that the prompts are in sync with the minLen,
#			maxLen, and terminateOption.
#
[langMenu]
promptTag=langMenu
repromptTag=
invalidTag=
timeoutTag=
shortInputTag=
validKeys=12
hotkeyList=
party=FIRST_PARTY
firstDigitTimeout=3
interDigitTimeout=3
nTries=3
beep=YES
terminateKey=#
minLen=1
maxLen=1
terminateOption=AUTOSKIP
inputOption=NUMERIC
interruptOption=FIRST_PARTY_INTERRUPT

[typeMenu]
promptTag=typeMenu
repromptTag=
invalidTag=
timeoutTag=
shortInputTag=
validKeys=1234
hotkeyList=
party=FIRST_PARTY
firstDigitTimeout=3
interDigitTimeout=3
nTries=3
beep=YES
terminateKey=#
minLen=1
maxLen=1
terminateOption=AUTOSKIP
inputOption=NUMERIC
interruptOption=FIRST_PARTY_INTERRUPT

[continueMenu]
promptTag=continueMenu
repromptTag=
invalidTag=
timeoutTag=
shortInputTag=
validKeys=12
hotkeyList=
party=FIRST_PARTY
firstDigitTimeout=3
interDigitTimeout=3
nTries=3
beep=YES
terminateKey=#
minLen=1
maxLen=1
terminateOption=AUTOSKIP
inputOption=NUMERIC
interruptOption=FIRST_PARTY_INTERRUPT
