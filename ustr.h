/*                                                          ##    ##  ########  ########     ######
                                                            ##    ##     ##     ##          ##    ##
                                                            ##    ##     ##     ######       ######
                                                            ##    ##     ##     ##          ##    ##
                                                             ######      ##     ##           ######

    FIXME Tools for handling utf8_ansi-escaped Strings
    TODO: map <physically> wide characters to support chinese, etc.
           right now: don't use or break

  W - Wide 1100..11F9 3000..303F 3041..3094 3099..309E 30A1..30FE 3131..318E 3190..319F 3200..321C
           3220..3243 3260..32B0 32C0..3376 337B..33DD 33E0..33FE 4E00..9FA5 AC00..D7A3 E000..E757
           F900..FA2D
  F - FullWidth FE30..FE44 FE49..FE52 FE54..FE6B FF01..FF5E FFE0..FFE6 */

#include <string.h>

char* utf8ansi_strip(char* str){
  int p=0, c=0, i=0, chars=0, op=0;
  size_t len = strlen(str);
  char  *out = (char*) malloc(len);
  for (i=0,chars=0;i<len;i++){
    c = (unsigned char) str[i];
    if (c == 0x1b && str[i+1] == '['){
      for(p=i;p<=len;p++){ c = str[p];
        if((0x40<c&&c<0x5B)||(0x60<c&&c<0x7B)) break; }
      if(p<=len){ i = p; }}
    else{ chars++; out[op++] = c; }}
  out[op] = '\0';
  return out; }

unsigned int utf8ansi_cols(char* str){
  int p=0,c=0,i=0,ix=strlen(str),chars=0;
  for (i=0,chars=0;i<ix;i++){
    c = (unsigned char) str[i];
    if (c == 0x1b && str[i+1] == '['){
      for(p=i;p<=ix;p++){ c = str[p];
        if((0x40<c&&c<0x5B)||(0x60<c&&c<0x7B)) break; }
      if(p<=ix){ i = p; }}
    else{ chars++;
      if      (c>=0x0&&c<=127) i+=0;
      else if ((c&0xe0)==0xc0) i+=1;
      else if ((c&0xf0)==0xe0) i+=2;
      else if ((c&0xf8)==0xf0) i+=3;
      else if ((c&0xfc)==0xf8) i+=4;
      else if ((c&0xfe)==0xfc) i+=5;
      else return ix; }} // invalid utf8
  return chars; }

unsigned int utf8ansi_trunc(char* str, unsigned int len){
  int p=0,c=0,i=0,ix=strlen(str),chars=0;
  // if(ix<=len) return ix;
  for (i=0,chars=0;i<ix;i++){
    c = (unsigned char) str[i];
    if (chars == len) return i;
    if (chars >  len) return i - 1;
    if (c == 0xa) return i;
    if (c == 0xd) return i;
    if (c == 0x1b && str[i+1] == '['){
      for(p=i;p<=ix;p++){ c = str[p];
        if((0x40<c&&c<0x5B)||(0x60<c&&c<0x7B)) break; }
      if(p<=ix){ i = p; }}
    else{ chars++;
      if      (c>=0x0&&c<=127) i+=0;
      else if ((c&0xe0)==0xc0) i+=1;
      else if ((c&0xf0)==0xe0) i+=2;
      else if ((c&0xf8)==0xf0) i+=3;
      else if ((c&0xfc)==0xf8) i+=4;
      else if ((c&0xfe)==0xfc) i+=5;
      else return ix; }} // invalid utf8
  return ix; }           // string too short

char* str_next(char* str, char tok){ int i;
  for( i=0 ;; i++ ) if (str[i] == 0x00) return str;
    else if (str[i] == tok){ str[i] = 0x00; return str + i + 1; }}

char* str_snext(char* str, char tok){ int i = 0;
  if (str[0] == tok) for(i=1 ;; i++)
         if (str[i] == 0)   return str;
    else if (str[i] == tok) str[i] = 0x00;
    else if (str[i] != tok) break;
  for(;; i++ )
         if (str[i] == 0x00) return str;
    else if (str[i] == tok){ str[i] = 0x00; return str + i + 1; }}
