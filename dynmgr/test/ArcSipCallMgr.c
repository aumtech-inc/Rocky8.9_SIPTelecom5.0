#include <stdio.h>

typedef struct osip_message osip_message_t;			// from osipparser2/osip_message.h
struct osip_message {
  char *sip_version;   
};

int eXosip_call_build_prack(osip_message_t **prack)     // from eX_call.h
{
	printf("oink\n");
}

int main()
{

	int		yRc;
	osip_message_t *prack = NULL;

	yRc = eXosip_call_build_prack (&prack);


}
