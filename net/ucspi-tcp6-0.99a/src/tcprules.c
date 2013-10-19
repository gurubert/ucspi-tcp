#include "strerr.h"
#include "stralloc.h"
#include "getln.h"
#include "buffer.h"
#include "exit.h"
#include "fmt.h"
#include "byte.h"
#include "cdb_make.h"
#include "ip4_bit.h"
#include "ip6.h"

#define FATAL "tcprules: fatal: "
#define SYNTAX "tcprules: syntax: "

unsigned long linenum = 0;
char *fntemp;
char *fn;

stralloc line = {0};
int match = 1;

stralloc address = {0};
stralloc ipstring = {0};
stralloc data = {0};
stralloc key = {0};

struct cdb_make c;

void nomem(void) {
    strerr_die2x(111,FATAL,"out of memory");
}

void usage(void) {
    strerr_die1x(100,"tcprules: usage: tcprules rules.cdb rules.tmp");
}

void die_bad(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,FATAL,"unable to parse this line: ",line.s);
}

void die_write(void) {
    strerr_die4sys(111,FATAL,"unable to write to ",fntemp,": ");
}

void die_length(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,SYNTAX,"invalid prefix length on line: ",line.s);
}

void die_ip(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,SYNTAX,"invalid address on line: ",line.s);
}
void die_ip4c(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,SYNTAX,"invalid IPv4 CIDR address on line: ",line.s);
}
void die_ip6c(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,SYNTAX,"invalid IPv6 CIDR address on line: ",line.s);
}

void die_ip6(void) {
    if (!stralloc_0(&line)) nomem();
    strerr_die3x(100,SYNTAX,"invalid IPv6 address: on line: ",line.s);
}

char strnum[FMT_ULONG];
stralloc sanum = {0};

void getnum(char *buf,int len,unsigned long *u) {
  if (!stralloc_copyb(&sanum,buf,len)) nomem();
  if (!stralloc_0(&sanum)) nomem();
  if (sanum.s[scan_ulong(sanum.s,u)]) die_bad();
}

void doaddressdata(void) {
  int i; 
  int ipv6 = 0;
  int cidr = 0;
  int info = 0;
  int left;
  int right;
  unsigned long bot;
  unsigned long top;
  unsigned long prefix;
  stralloc expandedip6address = {0};

  if (byte_chr(address.s,address.len,'=') < address.len) goto CDB;
  if (byte_chr(address.s,address.len,':') < address.len) ipv6 = 1;
  if (byte_chr(address.s,address.len,'/') < address.len) cidr = 1;
  if (byte_chr(address.s,address.len,'@') < address.len) info = 1;

  if (info) {
      if (ipv6 && !cidr) {
        i = byte_chr(address.s,address.len,'@');
        if (!stralloc_copyb(&key,address.s,i)) nomem();
        if (!stralloc_cats(&key,"@")) nomem();
        if (!stralloc_copyb(&ipstring,address.s+i+1,address.len-i-1)) nomem();
        if (!stralloc_0(&ipstring)) nomem();
        if (ip6_expandaddr(ipstring.s,&expandedip6address) == 1) {
          if (!stralloc_catb(&key,expandedip6address.s,expandedip6address.len)) nomem();
          if (cdb_make_add(&c,key.s,key.len,data.s,data.len) == -1) die_write();
          return;
        }
        else 
          die_ip6();
    }
    goto CDB;
  }
  
  if (!ipv6) {
    if (cidr) {
      i = byte_chr(address.s,address.len,'/');
      getnum(address.s + i + 1,address.len - i - 1,&prefix);
      if (prefix > 32 || prefix <= 0) die_length();
      switch (getaddressasbit(address.s,prefix,&ipstring)) {
        case -1: nomem();
        case  0: if (!stralloc_copys(&key,"_")) nomem();
                 if (!stralloc_cat(&key,&ipstring)) nomem();
                 if (cdb_make_add(&c,key.s,key.len,data.s,data.len) == -1) die_write();
                 break;
        case  2: die_ip4c();
      }
      return;
    }
    else if (i = byte_chr(address.s,address.len,'-') < address.len) {	
      left = byte_rchr(address.s,i,'.');
      if (left == i) left = 0; else ++left; 
            
      ++i;
      right = i + byte_chr(address.s + i,address.len - i,'.');
            
      getnum(address.s + left,i - 1 - left,&bot);
      getnum(address.s + i,right - i,&top);
      if (top > 255) top = 255;
      while (bot <= top) {
        if (!stralloc_copyb(&key,address.s,left)) nomem();
        if (!stralloc_catb(&key,strnum,fmt_ulong(strnum,bot))) nomem();
        if (!stralloc_catb(&key,address.s + right,address.len - right)) nomem();
        if (cdb_make_add(&c,key.s,key.len,data.s,data.len) == -1) die_write();
        ++bot;
      }
      return;
    }
  }
 
  if (ipv6) {
    if (cidr) {
      i = byte_chr(address.s,address.len,'/');
      getnum(address.s + i + 1,address.len - i - 1,&prefix);
      if (prefix > 128 ) die_length();
      switch (ip6tobitstring(address.s,&ipstring,prefix)) {
        case -1: nomem();
        case  1: if (!stralloc_copys(&key,"^")) nomem();
                 if (!stralloc_cat(&key,&ipstring)) nomem();
                 if (cdb_make_add(&c,key.s,key.len,data.s,data.len) == -1) die_write();
                 break;
        case  0: die_ip6c();
      }
      return;
    }
    else {
      if (!stralloc_copyb(&ipstring,address.s,address.len)) nomem();
      if (!stralloc_0(&ipstring)) nomem();
      switch (ip6_expandaddr(ipstring.s,&expandedip6address)) {
        case -1: nomem();
        case  0: die_ip6();
        case  1: if (cdb_make_add(&c,expandedip6address.s,expandedip6address.len,data.s,data.len) == -1)
                   die_write();
                 break;
      }
      return;
    }
  }

  CDB:
  if (cdb_make_add(&c,address.s,address.len,data.s,data.len) == -1) die_write();
  return;
}

main(int argc,char **argv) {
  int lastcolonpos;
  char *x;
  int len;
  int fd;
  int i;
  char ch;

  fn = argv[1];
  if (!fn) usage();
  fntemp = argv[2];
  if (!fntemp) usage();

  fd = open_trunc(fntemp);
  if (fd == -1)
    strerr_die4sys(111,FATAL,"unable to create ",fntemp,": ");
  if (cdb_make_start(&c,fd) == -1) die_write();

  while (match) {
    if (getln(buffer_0,&line,&match,'\n') == -1)
      strerr_die2sys(111,FATAL,"unable to read input: ");

     x = line.s;
     len = line.len;

     if (!len) break;
     if (x[0] == '#') continue; /*Ignore comments.*/
     if (x[0] == '\n') continue/*Ignore empty lines*/;

     while (len) {
       ch = x[len - 1];
       if (ch != '\n') if (ch != ' ') if (ch != '\t') break;
       --len;
     }
     line.len = len; /* for die_bad() */

     lastcolonpos = byte_rchr(x,len,':');
     if (lastcolonpos == len) continue;

     if (!stralloc_copyb(&address,x,lastcolonpos)) nomem();
     if (!stralloc_copys(&data,"")) nomem();

     x += lastcolonpos + 1;
     len -= lastcolonpos + 1;

     if ((len >= 4) && byte_equal(x,4,"deny")) {
       if (!stralloc_catb(&data,"D",2)) nomem();
       x += 4;
       len -= 4;
     } else if ((len >= 5) && byte_equal(x,5,"allow")) {
       x += 5;
       len -= 5;
     } else
       die_bad();

     while (len)
       switch (*x) {
         case ',': i = byte_chr(x,len,'=');
                   if (i == len) die_bad();
                   if (!stralloc_catb(&data,"+",1)) nomem();
                   if (!stralloc_catb(&data,x + 1,i)) nomem();
                   x += i + 1;
                   len -= i + 1;
                   if (!len) die_bad();
                   ch = *x;
                   x += 1;
                   len -= 1;
                   i = byte_chr(x,len,ch);
                   if (i == len) die_bad();
                   if (!stralloc_catb(&data,x,i)) nomem();
                   if (!stralloc_0(&data)) nomem();
                   x += i + 1;
                   len -= i + 1;
                   break;
          default: die_bad();
       }

    doaddressdata();
  }

  if (cdb_make_finish(&c) == -1) die_write();
  if (fsync(fd) == -1) die_write();
  if (close(fd) == -1) die_write(); /* NFS stupidity */
  if (rename(fntemp,fn))
    strerr_die6sys(111,FATAL,"unable to move ",fntemp," to ",fn,": ");

  _exit(0);
}
