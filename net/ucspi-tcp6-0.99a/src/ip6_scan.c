#include "scan.h"
#include "ip4.h"
#include "ip6.h"
#include "byte.h"
#include "stralloc.h"
#include "hexconversion.h"

/**
 * This function parses a valid IPv6-Address and returns it's 128 bit address
 *                              represented as 8 hex digits.
 * 				The string needs to be terminated with '\0'.
 * @param s is the pointer to the IPv6-Address.
 * @param ip6 the result of the parsing process.
 * @return 0 for failure, else it returns the number of the processed tokens.
 */

unsigned int ip6_scan(const char *s, char ip6[16]) {

  unsigned int i, j; 
  unsigned int blockcounter = 0;
  unsigned int lastlengthofxscan = 0;
  unsigned int len = 0; 
  unsigned long u;

  const char *beginningp = s; 
  char *endbufferp, *writerbufferp, *doublecolonp;
  unsigned int omittedzeroes = 0, doublecolonpindex;
  int saw_hexdigits;
  char ip6buffer[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; 
  unsigned char IP4mappedMask[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};

  stralloc addr = {0};
  stralloc_copyb(&addr,s,str_len(s));
  stralloc_0(&addr);

  if (i = ip4_scan((char *)s, ip6 + 12)) { 		/*Mapped IPv4 address. */
    for (len = 0; len < 12; ++len) 
      ip6[len] = IP4mappedMask[len];
    return i;
  }

  for (i = 0; i < 16; i++) 				 /*Set all elements of the array to zero. */
    ip6[i] = ip6buffer[i] = 0;
   
  writerbufferp = ip6buffer;       			/*writerp points to the next byte which will be written. */
  endbufferp = writerbufferp + 15; 			/*endp points to the end of the ip6buffer. */
  doublecolonp = 0;                			/*doublecolonp indicated the existence of '::' in the string*/

  if (*s == ':') { 		/*If a IPv6 address contains ':' at the beginning, * then the next token has also to be a ':'*/
    if (*++s != ':')
      return 0;
    else {
      len += 2;
      doublecolonp = writerbufferp;
      saw_hexdigits = 0;
      s++; 					/* s is always pointing to the next byte of the IPv6 source string. */
    }
  }

  for (;;) {
    i = scan_xlong((char*) s,&u);
    if (!i) {
      if (*s == ':') { 				/* IPv6 address expansion */
        if (!saw_hexdigits) return 0;
        if (*++s == ':') { 			/* Multiple occurrence of '::' detected!*/
         if (doublecolonp) return 0;
          if (blockcounter == 8) return 0;
          len += 2;
          doublecolonp = writerbufferp; 	/* Remember the position of the first '::'. */
          saw_hexdigits = 0;
          s++;
          continue;
        }
        if (*s == '\0') return 0;		/* But '::' is allowed at the end of the string. */
        len += 1;
        saw_hexdigits = 0;
        continue;
      } 
      else if (*s == '.') { 			/* Dot detected, there maybe an IPv4 address in the following tokens. */
        if ((writerbufferp - ip6buffer) > 14) return 0;	/*The entered address is too long!*/
        s -= lastlengthofxscan; 		/*Overwrite the last 2 Bytes, * because they are read as hex values*/
        i = ip4_scan(s,writerbufferp - 2);
        if (!i) return 0;
        s += i;
        if (*s != '\0') return 0;		/* No tokens are allowed after the IPv4 part. */
        len = len + i - lastlengthofxscan;
        writerbufferp += 2;
        break;
      } 
      else if (*s == '\0') 
        break;		 			/* Condition requires an '::' at the end! */
    } else {
      blockcounter++;				/* Remember the length of the last scanned hex value. */
      lastlengthofxscan = i; 			/* This is needed in order to delete the last hex value if a IPv4 trailer was found. */
    }

    if (*s != '\0') { 				/* Avoid buffer overflow and filter out * addresses which are too long. */
      if (u > 0xffff) return 0;
      if (writerbufferp > endbufferp) return 0;	/* Write the result byte for byte in the buffer. */

      *writerbufferp = (u >> 8) & 0xff;		/* Get the MSB. */
      writerbufferp += 1;
      *writerbufferp = u & 0xff;		/* Get the LSB. */
      writerbufferp += 1;

      saw_hexdigits = 1;
      len += i;
      s += i;
    } 
    else if (*s == '\0') 
      break; 	  	  		 	/* Canceling condition for the loop*/
  } 
       
  if ((writerbufferp < endbufferp) && (!doublecolonp)) return 0; /* The entered address is too short!*/

  omittedzeroes = endbufferp - writerbufferp + 1;  	/* Compute the omitted zeroes between '::'. */
  if (omittedzeroes) {
    doublecolonpindex = doublecolonp - ip6buffer;
    for (i = 0; i < doublecolonpindex; i++) 		/* Copy the bytes before '::' in the output. */
      ip6[i] = ip6buffer[i];
    for (i = 0, j = doublecolonpindex; i < omittedzeroes; i++, j++) 	/* Omitted zeroes will be written into the output. */
      ip6[j] = 0;
    for (; j < 16; j++) 				/* Copy the rest into the output. */
      ip6[j] = ip6buffer[doublecolonpindex++];
  }
  else {
    for (i=0; i<16; i++)
      ip6[i] = ip6buffer[i];
  }
      
  if (byte_chr(beginningp,len,':')  < len) 		/* IPv4 mapped IPv6 found, so check it for the valid format. */
    if (byte_chr(beginningp,len,'.')  < len) {
     for (i=0; i<10; i++)
       if (ip6[i] != 0) return 0;
        
    if ((unsigned char) ip6[10] != 0xFF) return 0;
    if ((unsigned char) ip6[11] != 0xFF) return 0;
  }
  return len;
}
