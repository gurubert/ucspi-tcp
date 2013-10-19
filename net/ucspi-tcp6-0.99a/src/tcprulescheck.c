#include "byte.h"
#include "buffer.h"
#include "strerr.h"
#include "env.h"
#include "rules.h"
#include "ip4_bit.h"
#include "stralloc.h"
#include "ip6.h"
#include "exit.h"

void found(char *data,unsigned int datalen)
{
  int i;
  unsigned int next0;
  stralloc ip6address = {0};
  stralloc expandedip6address = {0};
  stralloc rule_temp = {0};
  
  buffer_puts(buffer_1,"rule ");

  if (byte_chr(rules_name.s,rules_name.len,'^') < rules_name.len) {   	/* IPv6 CIDR */
    if (bitstringtoip6(&rules_name,&ip6address) == 1)
      stralloc_copys(&rules_name,ip6address.s);
    else
       strerr_die1x(100,"IPv6 address error!");
  }

  if (byte_chr(rules_name.s,rules_name.len,'_') < rules_name.len) {	/* IPv4 CIDR */
    if (getbitasaddress(&rules_name) == -1) 
      strerr_die1x(100,"IPv4 address error!");
  } 

  buffer_put(buffer_1,rules_name.s,rules_name.len);
  buffer_puts(buffer_1,":\n");
  while ((next0 = byte_chr(data,datalen,0)) < datalen) {
    switch(data[0]) {
      case 'D':
	buffer_puts(buffer_1,"deny connection\n");
	buffer_flush(buffer_1);
	_exit(0);
      case '+':
	buffer_puts(buffer_1,"set environment variable ");
	buffer_puts(buffer_1,data + 1);
	buffer_puts(buffer_1,"\n");
	break;
    }
    ++next0;
    data += next0; datalen -= next0;
  }
  buffer_puts(buffer_1,"allow connection\n");
  buffer_flush(buffer_1);
  _exit(0);
}

main(int argc,char **argv)
{
  char *fnrules;
  int fd;
  char *ip;
  char *info;
  char *host;

  fnrules = argv[1];
  if (!fnrules)
    strerr_die1x(100,"tcprulescheck: usage: tcprulescheck rules.cdb");

  ip = env_get("TCPREMOTEIP");
  if (!ip) ip = "::"; 
  info = env_get("TCPREMOTEINFO");
  host = env_get("TCPREMOTEHOST");

  fd = open_read(fnrules);
  if ((fd == -1) || (rules(found,fd,ip,host,info) == -1))
    strerr_die3sys(111,"tcprulescheck: fatal: unable to read ",fnrules,": ");

  buffer_putsflush(buffer_1,"default:\nallow connection\n");
  _exit(0);
}
