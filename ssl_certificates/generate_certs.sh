#!/bin/bash
#----------------------------------------------------------------------------
# Program:  generate_certs.sh
# Purpose:  Generate SSL certificates
# Author:   Aumtech, Inc.
# Update:   06/30/16 ddn	Created the file
#----------------------------------------------------------------------------
#
#eXtl_tls.c:#define CLIENT_KEYFILE "ckey.pem"
#eXtl_tls.c:#define CLIENT_CERTFILE "c.pem"
#eXtl_tls.c:#define SERVER_KEYFILE "skey.pem"
#eXtl_tls.c:#define SERVER_CERTFILE "s.pem"
#eXtl_tls.c:#define CA_LIST "cacert.pem"
#eXtl_tls.c:#define RANDOM  "random.pem"
#eXtl_tls.c:#define DHFILE "dh1024.pem"

#http://dev.mysql.com/doc/refman/5.7/en/creating-ssl-files-using-openssl.html

tmpDir="/tmp/$$"
mkdir $tmpDir

if [ $? -ne 0 ]
then
	echo
	echo "Unable to create temporary directory $tmpDir.  Correct and retry"
	exit
fi

cd $tmpDir

#1.
echo "ca-key.pem"
openssl genrsa 2048 > ca-key.pem

#2.
echo "ca.pem"
openssl req -new -x509 -nodes -days 3600 -key ca-key.pem -out ca.pem <<CA
US
New Jersey
East Brunswick
Aumtech, Inc.
SIP
Aumtech Product
license@aumtech.com
CA

#3. 
echo "server-req.pem"
openssl req -newkey rsa:2048 -days 3600 -nodes -keyout server-key.pem -out server-req.pem <<SERVER_REQ
US
New Jersey
East Brunswick
Aumtech, Inc.
SIP
Aumtech Product
license@aumtech.com
password
Aumtech, Inc.
SERVER_REQ


#4.
echo "server-key.pem"
openssl rsa -in server-key.pem -out server-key.pem

#5.
echo "server-cert.pem"
openssl x509 -req -in server-req.pem -days 3600 -CA ca.pem -CAkey ca-key.pem -set_serial 01 -out server-cert.pem

#6.
echo "client-req.pem"
openssl req -newkey rsa:2048 -days 3600 -nodes -keyout client-key.pem -out client-req.pem <<CLIENT_REQ
US
New Jersey
East Brunswick
Aumtech, Inc.
SIP
Aumtech Product
license@aumtech.com
password
Aumtech, Inc.
CLIENT_REQ


#7.
echo "client-key.pem"
openssl rsa -in client-key.pem -out client-key.pem

#8.
echo "client-cert.pem"
openssl x509 -req -in client-req.pem -days 3600 -CA ca.pem -CAkey ca-key.pem -set_serial 01 -out client-cert.pem

#9.  dh1024.pem:
echo "dh1024"
openssl dhparam -out dh1024.pem 1024

#10. rnad
cp -f ~/.rnd 		$ISPBASE/Telecom/Exec/random.pem
cp -f server-cert.pem  	$ISPBASE/Telecom/Exec/s.pem
cp -f server-key.pem  	$ISPBASE/Telecom/Exec/skey.pem
cp -f server-req.pem  	$ISPBASE/Telecom/Exec/server-req.pem
cp -f client-cert.pem  	$ISPBASE/Telecom/Exec/c.pem
cp -f client-key.pem  	$ISPBASE/Telecom/Exec/ckey.pem
cp -f client-req.pem  	$ISPBASE/Telecom/Exec/client-req.pem
cp -f ca.pem  		$ISPBASE/Telecom/Exec/ca.pem
cp -f ca.pem  		$ISPBASE/Telecom/Exec/cacert.pem
cp -f dh1024.pem  	$ISPBASE/Telecom/Exec/dh1024.pem

cd -
rm -rf $tmpDir

