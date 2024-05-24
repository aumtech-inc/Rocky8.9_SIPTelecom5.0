#!/bin/bash 

indent -br -nut -l160 srtp.c 

gcc -ggdb -DMAIN -o test srtp.c -I ../../include/ -I/home/dev/isp2.2/SIPTelecom3.4/thirdParty/ortp-0.6.3.core.ipv6/src -L../lib -lsrtp

