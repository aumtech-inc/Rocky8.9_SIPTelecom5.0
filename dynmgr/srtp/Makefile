CC=gcc -Wall -Werror  
CINCS= -I../../include -I../../thirdParty/ortp-0.6.3.core.ipv6/src/ -I../../thirdParty/ortp-0.6.3.core.ipv6/src/ 
CINCS+= -I../../include -I../../thirdParty/ortp-0.6.3.core.ipv6/src/ -I../../thirdParty/ortp-0.6.3.core.ipv6/ 
CLIBS= -L../../lib -L../../thirdParty/ortp-0.6.3.core.ipv6/src/.libs -lortp 

all:
	$(CC) $(CINCS) $(CLIBS) -DMAIN srtp.c -o test ../../lib/libsrtp.a 
