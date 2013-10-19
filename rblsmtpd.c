#include "byte.h"
#include "str.h"
#include "scan.h"
#include "fmt.h"
#include "env.h"
#include "exit.h"
#include "sig.h"
#include "buffer.h"
#include "readwrite.h"
#include "sgetopt.h"
#include "strerr.h"
#include "stralloc.h"
#include "commands.h"
#include "pathexec.h"
#include "dns.h"

#define FATAL "rblsmtpd: fatal: "

void nomem(void)
{
  strerr_die2x(111,FATAL,"out of memory");
}
void usage(void)
{
  strerr_die1x(100,"rblsmtpd: usage: rblsmtpd [ -b ] [ -R ] [ -t timeout ] [ -r base ] [ -a base ] smtpd [ arg ... ]");
}

int decision = 0; /* 0 undecided, 1 accept, 2 reject, 3 bounce */
static stralloc text; /* defined if decision is 2 or 3 */

char *ip_env;
static stralloc ip_reverse;

void ip_init(void)
{
  unsigned int i;
  unsigned int j;

  if (strcmp(env_get("PROTO"), "TCP6")) {
    ip_env = env_get("TCPREMOTEIP");
    if (!ip_env) ip_env = "";
  } else {
    ip_env = "";
    decision = 1; /* always accept IPv6 connections as there are no IPv6 DNSBLs (yet) */
  }

  if (!stralloc_copys(&ip_reverse,"")) nomem();

  i = str_len(ip_env);
  while (i) {
    for (j = i;j > 0;--j) if (ip_env[j - 1] == '.') break;
    if (!stralloc_catb(&ip_reverse,ip_env + j,i - j)) nomem();
    if (!stralloc_cats(&ip_reverse,".")) nomem();
    if (!j) break;
    i = j - 1;
  }
}

unsigned long timeout = 60;
int flagrblbounce = 0;
int flagfailclosed = 0;
int flagmustnotbounce = 0;

static stralloc tmp;

void rbl(char *base)
{
  if (decision) return;
  if (!stralloc_copy(&tmp,&ip_reverse)) nomem();
  if (!stralloc_cats(&tmp,base)) nomem();
  if (dns_txt(&text,&tmp) == -1) {
    flagmustnotbounce = 1;
    if (flagfailclosed) {
      if (!stralloc_copys(&text,"temporary RBL lookup error")) nomem();
      decision = 2;
    }
    return;
  }
  if (text.len)
    if (flagrblbounce)
      decision = 3;
    else
      decision = 2;
}

void antirbl(char *base)
{
  if (decision) return;
  if (!stralloc_copy(&tmp,&ip_reverse)) nomem();
  if (!stralloc_cats(&tmp,base)) nomem();
  if (dns_ip4(&text,&tmp) == -1) {
    flagmustnotbounce = 1;
    if (!flagfailclosed)
      decision = 1;
    return;
  }
  if (text.len)
    decision = 1;
}

char strnum[FMT_ULONG];
static stralloc message;

char inspace[64]; buffer in = BUFFER_INIT(read,0,inspace,sizeof inspace);
char outspace[1]; buffer out = BUFFER_INIT(write,1,outspace,sizeof outspace);

void reject() { buffer_putflush(&out,message.s,message.len); }
void accept() { buffer_putsflush(&out,"250 rblsmtpd.local\r\n"); }
void greet()  { buffer_putsflush(&out,"220 rblsmtpd.local\r\n"); }
void quit()   { buffer_putsflush(&out,"221 rblsmtpd.local\r\n"); _exit(0); }
void drop()   { _exit(0); }

struct commands smtpcommands[] = {
  { "quit", quit, 0 }
, { "helo", accept, 0 }
, { "ehlo", accept, 0 }
, { "mail", accept, 0 }
, { "rset", accept, 0 }
, { "noop", accept, 0 }
, { 0, reject, 0 }
} ;

void rblsmtpd(void)
{
  int i;
  char *x;

  x = env_get("RBLMESSAGE");
  
  if (flagmustnotbounce || (decision == 2)) {
    if (!stralloc_copys(&message,"451 ")) nomem();
  }
  else
    if (!stralloc_copys(&message,"553 ")) nomem();

  if (x)
    if (!stralloc_cats(&message, x))
      nomem();

  /* replace %IP% with content of $TCPREMOTEIP in RBLSMTPD string */
  for (i = 0;i <= ( text.len - 4 );++i)
    if (text.s[i] == '%' && text.s[i+1] == 'I' && text.s[i+2] == 'P' && text.s[i+3] == '%') {

      if (!stralloc_copyb(&tmp,text.s,i)) nomem();

      if (!stralloc_cats(&tmp,ip_env)) nomem();

      if (i < ( text.len - 4)) {
	if (!stralloc_catb(&tmp,text.s + i + 4, text.len - i - 4)) nomem();
      }

      if (!stralloc_copy(&text,&tmp)) nomem();

      i = i + 4;
    }
  
  if (text.len > 200) text.len = 200;
  if (!stralloc_cat(&message,&text)) nomem();
  for (i = 0;i < message.len;++i)
    if ((message.s[i] < 32) || (message.s[i] > 126))
      message.s[i] = '?';
  
  buffer_puts(buffer_2,"rblsmtpd: ");
  buffer_puts(buffer_2,ip_env);
  buffer_puts(buffer_2," pid ");
  buffer_put(buffer_2,strnum,fmt_ulong(strnum,getpid()));
  buffer_puts(buffer_2,": ");
  buffer_put(buffer_2,message.s,message.len);
  buffer_puts(buffer_2,"\n");
  buffer_flush(buffer_2);

  if (!stralloc_cats(&message,"\r\n")) nomem();

  if (!timeout)
    reject();
  else {
    sig_catch(sig_alarm,drop);
    alarm(timeout);
    greet();
    commands(&in,smtpcommands);
  }
  _exit(0);
}

main(int argc,char **argv,char **envp)
{
  int flagwantdefaultrbl = 1;
  char *x;
  int opt;

  ip_init();

  x = env_get("RBLSMTPD");
  if (x) {
    if (!*x)
      decision = 1;
    else if (*x == '-') {
      if (!stralloc_copys(&text,x + 1)) nomem();
      decision = 3;
    }
    else {
      if (!stralloc_copys(&text,x)) nomem();
      decision = 2;
    }
  }

  while ((opt = getopt(argc,argv,"bBcCt:r:a:")) != opteof)
    switch(opt) {
      case 'b': flagrblbounce = 1; break;
      case 'B': flagrblbounce = 0; break;
      case 'c': flagfailclosed = 1; break;
      case 'C': flagfailclosed = 0; break;
      case 't': scan_ulong(optarg,&timeout); break;
      case 'r': rbl(optarg); flagwantdefaultrbl = 0; break;
      case 'a': antirbl(optarg); break;
      default: usage();
    }

  argv += optind;
  if (!*argv) usage();

  if (flagwantdefaultrbl) rbl("rbl.maps.vix.com");
  if (decision >= 2) rblsmtpd();

  pathexec_run(*argv,argv,envp);
  strerr_die4sys(111,FATAL,"unable to run ",*argv,": ");
}
