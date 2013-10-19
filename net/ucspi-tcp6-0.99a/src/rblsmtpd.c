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
#include "ip6.h"

#define FATAL "rblsmtpd: fatal: "

void nomem(void)
{
  strerr_die2x(111,FATAL,"out of memory");
}
void usage(void)
{
  strerr_die1x(100,"rblsmtpd: usage: rblsmtpd [ -B ] [ -b ] [ -C ] [ -c ] [ -i ] [ -t timeout ] [ -r base ] [ -a base ] [-W] [-w delay] smtpd [ arg ... ]");
}

char *tcp_proto;
char *ip_env;
static stralloc ip_reverse;
int flagip6 = 0;

static inline char tohex(char c) { return c>=10?c-10+'a':c+'0'; }

void ip_init(void)
{
  unsigned int i;
  unsigned int j;
  char hexval;
  unsigned char remoteip[16];

  tcp_proto = env_get("PROTO");
  if (!tcp_proto) tcp_proto = "";
  ip_env = env_get("TCPREMOTEIP");
  if (!ip_env) ip_env = "";
  if (str_diff(tcp_proto,"TCP6") == 0) {
    if (byte_equal(ip_env,7,V4MAPPREFIX)) 
      ip_env = ip_env + 7;
     else
      flagip6 = 1;
  }
 
  if (!stralloc_copys(&ip_reverse,"")) nomem();

  if (flagip6) { 
    if ((ip6_scan(ip_env,remoteip)) == 0) return;

    for (j = 16; j > 0; j--) {
      hexval = tohex(remoteip[j - 1] & 15);
      if (!stralloc_catb(&ip_reverse,&hexval,1)) nomem();
      if (!stralloc_cats(&ip_reverse,".")) nomem();

      hexval = tohex(remoteip[j - 1] >> 4);
      if (!stralloc_catb(&ip_reverse,&hexval,1)) nomem();
      if (!stralloc_cats(&ip_reverse,".")) nomem(); 
    }
  } else {
    i = str_len(ip_env);
    while (i) {
      for (j = i;j > 0;--j) if (ip_env[j - 1] == '.') break;
      if (!stralloc_catb(&ip_reverse,ip_env + j,i - j)) nomem();
      if (!stralloc_cats(&ip_reverse,".")) nomem();
      if (!j) break;
      i = j - 1;
    }
  }
}

unsigned long timeout = 60;
int flagrblbounce = 0;
int flagfailclosed = 0;
int flagmustnotbounce = 0;
int flagrblinfo = 0;

int decision = 0; /* 0 undecided, 1 accept, 2 direct refuse, 3 direct bounce, 4 rbl refuse, 5 rbl bounce */
static stralloc text; /* defined if decision is > 2 */

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
      decision = 4;
    }
    return;
  }
  if (text.len)
    if (flagrblbounce)
      decision = 5;
    else
      decision = 4;
}

void antirbl(char *base)
{
  if (decision) return;
  int flagip;

  if (!stralloc_copy(&tmp,&ip_reverse)) nomem();
  if (!stralloc_cats(&tmp,base)) nomem();
   
  if (flagip6) 
    flagip = dns_ip6(&text,&tmp);
  else
    flagip = dns_ip4(&text,&tmp);
 
  if (flagip == -1) {
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
static stralloc info;

char inspace[64]; buffer in = BUFFER_INIT(read,0,inspace,sizeof inspace);
char outspace[1]; buffer out = BUFFER_INIT(write,1,outspace,sizeof outspace);

void wait(unsigned long delay)
{
  unsigned long u;
  char *x;
 
  if (!delay) {
    x = env_get("GREETDELAY");
    if (x) { 
      scan_ulong(x,&u); 
      delay = u; 
    }
  }

  if (delay) {
    if (!stralloc_copys(&info,"greetdelay: ")) nomem();

    buffer_puts(buffer_2,"rblsmtpd: ");
    buffer_puts(buffer_2,ip_env);
    buffer_puts(buffer_2," pid ");
    buffer_put(buffer_2,strnum,fmt_ulong(strnum,getpid()));
    buffer_puts(buffer_2,": ");
    buffer_put(buffer_2,info.s,info.len);
    buffer_put(buffer_2,strnum,fmt_ulong(strnum,delay));
    buffer_puts(buffer_2,"\n");
    buffer_flush(buffer_2);

    if (!stralloc_cats(&info,"\r\n")) nomem();

    sleep(delay);
  }
}

void rblinfo(void)
{
  int i; 
 
  if (!stralloc_copys(&message,"[RBL info] ")) nomem();

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

  if (!stralloc_copy(&info,&message)) nomem();
  if (!stralloc_0(&info)) nomem();
  if (!pathexec_env("RBLSMTPD",info.s)) nomem();
}

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
  char *rblmessage;

  rblmessage = env_get("RBLMESSAGE");

  if (flagmustnotbounce || (decision == 2) || (decision == 4)) {
    if (!stralloc_copys(&message,"451 ")) nomem();
  }
  else
    if (!stralloc_copys(&message,"553 ")) nomem();

  if (rblmessage)
    if (!stralloc_cats(&message, rblmessage))
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

int main(int argc,char **argv,char **envp)
{
  char *x;
  int opt;
  unsigned long greetdelay = 0;

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

  while ((opt = getopt(argc,argv,"bBcCit:r:a:w:W")) != opteof)
    switch(opt) {
      case 'b': flagrblbounce = 1; break;
      case 'B': flagrblbounce = 0; break;
      case 'c': flagfailclosed = 1; break;
      case 'C': flagfailclosed = 0; break;
      case 'i': flagrblinfo = 1; break;
      case 't': scan_ulong(optarg,&timeout); break;
      case 'r': rbl(optarg); break;
      case 'a': antirbl(optarg); break;
      case 'W': if (!decision) { wait(greetdelay); } break;
      case 'w': if (!decision) { scan_ulong(optarg,&greetdelay); wait(greetdelay); } break;
      default: usage();
    }

  argv += optind;
  if (!*argv) usage();
 
  if (flagrblinfo && decision >= 4)
    rblinfo(); 
  else if (decision >= 2)
    rblsmtpd();
  

  pathexec(argv);
  strerr_die4sys(111,FATAL,"unable to run ",*argv,": ");

  return 0;
}
