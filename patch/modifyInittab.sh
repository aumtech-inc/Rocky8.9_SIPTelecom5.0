#!/bin/ksh
#
#
Does_inittab_entry_already_exist()
{
    if [ `grep -c "$1" $inittab` -gt 0 ]
    then
        echo "$1 already exists in $inittab:"
        echo "  $2"
        echo "$1 will not be added to $inittab."
        return 1
    fi
    return 0
}

#------------------------------------------------------------------------
# doInittab:
#   $1 - field1 of /etc/inittab
#   $2 - /etc/inittab line
#   $3 - either tts or asr or tones
#------------------------------------------------------------------------
doInittab()
{

    inittab="/etc/inittab"
    Does_inittab_entry_already_exist  "$1" "$2"
    if [ $? -eq 1 ]
    then
        return 0
    fi

    echo
	tones=0
    if [ "$3" = "tts" ]
    then
        echo -n "To enable Text-To-Speech, "
	elif [ "$3" = "asr" ]
	then
        echo -n "To enable MRCPv2 ASR Client, "
	else
		tones=1
    fi

	if [ $tones -eq 0 ]
	then
	    echo "the following line must be appended/modified "
	    echo "to the $inittab file :"
	    echo
	    echo "$2"
	
	    echo 
	    echo "You have the option of having it automatically appended now, or "
	    echo "you can manually append it to the $inittab file later."
	    echo -n "Automatically append the line to $inittab now [(y/n): def:y]: "
	    read ans
	    
	    if [ "$ans" = "n" -o "$ans" = "N" -o "$ans" = "no" -o "$ans" = "No" ]
	    then
	        return 0
	    fi
	fi
    
    inittabSave="$inittab.`date +%Y%m%d`"
	if [ $tones -eq 0 ]
	then
    	echo "Saving the current $inittab file to $inittabSave..."
	fi
    cp $inittab $inittabSave
    
    echo "$2" >>$inittab
}

tonesClientStr="tone:2345:respawn:su - arc -c \"cd $ISPBASE/Telecom/Exec; ArcSipTonesClientMgr 12\""

if [ ! -z "$tonesClientStr" ]
then
	doInittab  "^tone" "$tonesClientStr" "tones"
fi

