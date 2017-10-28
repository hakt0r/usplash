/*
                                                                  ██    ██ ██████  ██████  ██ ███    ██ ████████ ███████
                                                                  ██    ██ ██   ██ ██   ██ ██ ████   ██    ██    ██
                                                                  ██    ██ ██████  ██████  ██ ██ ██  ██    ██    █████
                                                                  ██    ██ ██      ██   ██ ██ ██  ██ ██    ██    ██
                                                                   ██████  ██      ██   ██ ██ ██   ████    ██    ██
*/

static int ftoa_fixed(char *buffer, double value);
static int uitoa(long long int value, char *buffer, int base);

char* uasvprintf(char const *fmt, va_list arg, int *len) {
  int pos = 0; // length, current output bytes
  char ch; // current char from format string
  char* out = malloc(1024); int l_out = 1024; // the output buffer
  char t_chr; char *t_str; double t_dbl; // temporary values
  int t_int;  long int t_li; long long int t_lli;
  int t_uint;
  *out = '\0';
  while ( ch = *fmt++) {
    if ( pos + 512 + 16 > l_out ){
      l_out += 1024; out = realloc(out,l_out); }
    if ( '%' == ch ) {
      switch (ch = *fmt++) {
        case '%': out[pos++] = '%'; break;
        case 'e': // %e: print an escape char
          out[pos++] = '\x1b'; break;
        case 'l': // long value
          switch (ch = *fmt++) {
            case 'i': // long int
              t_li = va_arg(arg, long int);
              pos += uitoa(t_li,out+pos,10); break;
            case 'x': // long int as hex
              t_li = va_arg(arg, long int);
              pos += uitoa(t_li,out+pos,16); break;
            case 'l': // long long value
              switch (ch = *fmt++) {
                case 'i': // long long int
                  t_lli = va_arg(arg, long long int);
                  pos += uitoa(t_lli,out+pos,10); break;
                case 'x': // long long int as hex
                  t_lli = va_arg(arg, long long int);
                  pos += uitoa(t_lli,out+pos,16); break; }
              break; }
          break;
        case 'c': // %c: print a character
          t_chr = va_arg(arg, int); out[pos++] = t_chr; break;
        case 's': // %s: print a string
          t_str  = va_arg(arg, char *);
          t_str  = t_str?t_str:"(null)";
          t_int  = strlen(t_str);
          l_out += t_int; out = realloc(out,l_out);
          strncpy(out+pos,t_str,t_int);
          pos += t_int; break;
        case 'i': // %i: print an uint
          t_uint = va_arg(arg, int); pos += uitoa(t_uint, out+pos, 10); break;
        case 'd': // %d: print an int
          t_int = va_arg(arg, int); pos += uitoa(t_int, out+pos, 10); break;
        case 'x': // %x: print an int in hex
          t_int = va_arg(arg, int);
          pos += uitoa(t_int, out+pos, 16);
          break;
        case 'f': // %f: print a float
          t_dbl = va_arg(arg, double); pos += ftoa_fixed(out+pos, t_dbl); break; }}
    else { out[pos++] = ch; }}
  out[pos++] = '\0';
  out = realloc(out,pos);
  *len = pos; return out; }

char* usvprintf(char const *fmt, char* out, va_list arg) {
  int pos = 0; // length, current output bytes
  char ch; // current char from format string
  char t_chr; char *t_str; double t_dbl; // temporary values
  int t_int;  long int t_li; long long int t_lli;
  int t_uint;
  *out = '\0';
  while ( ch = *fmt++) {
    if ( '%' == ch ) {
      switch (ch = *fmt++) {
        case '%': out[pos++] = '%'; break;
        case 'e': // %e: print an escape char
          out[pos++] = '\x1b'; break;
        case 'l': // long value
          switch (ch = *fmt++) {
            case 'i': // long int
              t_li = va_arg(arg, long int);
              pos += uitoa(t_li,out+pos,10); break;
            case 'x': // long int as hex
              t_li = va_arg(arg, long int);
              pos += uitoa(t_li,out+pos,16); break;
            case 'l': // long long value
              switch (ch = *fmt++) {
                case 'i': // long long int
                  t_lli = va_arg(arg, long long int);
                  pos += uitoa(t_lli,out+pos,10); break;
                case 'x': // long long int as hex
                  t_lli = va_arg(arg, long long int);
                  pos += uitoa(t_lli,out+pos,16); break; }
              break; }
          break;
        case 'c': // %c: print a character
          t_chr = va_arg(arg, int); out[pos++] = t_chr; break;
        case 's': // %s: print a string
          t_str  = va_arg(arg, char *);
          t_str  = t_str?t_str:"(null)";
          t_int  = strlen(t_str);
          strncpy(out+pos,t_str,t_int);
          pos += t_int; break;
        case 'i': // %i: print an uint
          t_uint = va_arg(arg, int); pos += uitoa(t_uint, out+pos, 10); break;
        case 'd': // %d: print an int
          t_int = va_arg(arg, int); pos += uitoa(t_int, out+pos, 10); break;
        case 'x': // %x: print an int in hex
          t_int = va_arg(arg, int);
          pos += uitoa(t_int, out+pos, 16);
          break;
        case 'f': // %f: print a float
          t_dbl = va_arg(arg, double); pos += ftoa_fixed(out+pos, t_dbl); break; }}
    else { out[pos++] = ch; }}
  out[pos++] = '\0';
  return pos; }

int normalize(double *val) {
  int exponent = 0;
  double value = *val;
  while (value >= 1.0) {
    value /= 10.0;
    ++exponent; }
  while (value < 0.1) {
    value *= 10.0;
    --exponent; }
  *val = value;
  return exponent; }

static int ftoa_fixed(char *buffer, double value) {
  static const int width = 4;
  char *o = buffer;
  int exponent = 0;
  int places = 0;
  if (value == 0.0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return; }
  if (value < 0.0) {
    *buffer++ = '-';
    value = -value; }
  exponent = normalize(&value);
  while (exponent > 0) {
    int digit = value * 10;
    *buffer++ = digit + '0';
    value = value * 10 - digit;
    ++places;
    --exponent; }
  if (places == 0)
    *buffer++ = '0';
  *buffer++ = '.';
  while (exponent < 0 && places < width) {
    *buffer++ = '0';
    --exponent;
    ++places; }
  while (places < width) {
    int digit = value * 10.0;
    *buffer++ = digit + '0';
    value = value * 10.0 - digit;
    ++places; }
  *buffer = '\0';
  char* l = buffer - o;
  return (int) l; }

void ureverse(char s[], int j){ int i; char c;
 for (i = 0; i<j; i++, j--){
   c = s[i]; s[i] = s[j]; s[j] = c; }}

int uitoa(long long int n, char* s, int base){ int i = 0, r = 0; bool isNegative = false;
  if (n == 0){
    s[i++] = '0'; s[i] = '\0'; return 1; }
  if (n < 0 && base == 10){
    isNegative = true; n = -n;}
  while (n != 0){
    r = n % base;
    s[i++] = (r > 9)? (r-10) + 'a' : r + '0';
    n = n / base; }
  if (isNegative) s[i++] = '-';
  ureverse(s, i-1); s[i+1] = '\0'; return i; }

int asprintf(char** s, ...) {
  int len = 0; char *str, *fmt; va_list arg;
  va_start(arg, s);
  fmt = va_arg(arg,char*);
  *s = uasvprintf(fmt, arg, &len);
  va_end(arg);
  return len; }

int sprintf(char* s, ...) {
  int len = 0; char *fmt; va_list arg;
  va_start(arg, s);
  fmt = va_arg(arg,char*);
  len = usvprintf(fmt, s, arg);
  va_end(arg);
  return len; }

int unsprintf(char* s, int n, ...) {
  int len = 0; char *fmt; va_list arg;
  va_start(arg, n);
  fmt = va_arg(arg,char*);
  len = usvprintf(fmt, s, arg);
  va_end(arg);
  return len; }
