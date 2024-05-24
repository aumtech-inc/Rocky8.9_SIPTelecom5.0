
* int processAppRequest (int zCall)

            if (gCall[zCall].msgToDM.opcode != DMOP_PLAYMEDIAAUDIO)
            {
                gCall[zCall].currentOpcode = gCall[zCall].msgToDM.opcode;
                gCall[zCall].msgToDM.opcode = DMOP_SPEAK;
            }

           gCall[zCall].currentOpcode = DMOP_PLAYMEDIAAUDIO;   // we should never get DMOP_SPEAK

* addToSpeakList / addToAppRequestList /  else if (lpMsgSpeak->list == 0)


* why change to DMOP_SPEAK to DMOP_PLAYMEDIA



