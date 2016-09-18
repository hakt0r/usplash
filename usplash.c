static char* LICENSE[] = {
  "  usplash.c - a fancy dialog replacement and dynamic menu",
  "",
  "  Copyright (C) 2015  Sebastian Glaser ( anx@ulzq.de )",
  "",
  "  This program is free software; you can redistribute it and/or modify it under the terms of the",
  "    GNU General Public License",
  "  as published by the Free Software Foundation;",
  "  either version 3 of the License, or (at your option) any later version.",
  "",
  "  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;",
  "  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.",
  "  See the GNU General Public License for more details.",
  "  You should have received a copy of the GNU General Public License along with this program;",
  "  if not, write to the Free Software Foundation,",
  "    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA",0 };

static char* BG[] = {
  "                             ████████████████                             ",
  "                      ███████                ███████                      ",
  "                  ████                              ████                  ",
  "               ███        ▄██▄  ▄██▄  ▄██▄  ▄██▄        ███               ",
  "            ██████        █     █     █  █  █ ▄▀        ██████            ",
  "          █████████       █ ▀█  █▀▀   █ ▀█  █▀▄        █████████          ",
  "        █████████████     ▀██▀  ▀▄▄▄  █  █  █  █     ▓████████████        ",
  "       ███████████████                              ███████████████       ",
  "     ██████████████████▓                          ▓██████████████████     ",
  "    █████████████████████                        █████████████████████    ",
  "   ████████████████████████                    ████████████████████████   ",
  "  ███████████████████████████                ███████████████████████████  ",
  "  ██████████████████████████   ░▒▓██████▓▒░   ██████████████████████████  ",
  " █████████████████████████   ████████████████   █████████████████████████ ",
  " ████████████████████████  ████████████████████  ████████████████████████ ",
  "████████████████████████  ██████████████████████  ████████████████████████",
  "███████████████████████  ████████████████████████  ███████████████████████",
  "███████████████████████  ████████████████████████  ███████████████████████",
  "█                        ████████████████████████                        █",
  "█                         ██████████████████████                         █",
  " █                         ████████████████████                         █ ",
  " █                          ██████████████████                          █ ",
  "  █                          ████████████████                          █  ",
  "  █                              ▓▓████▓▓                              █  ",
  "   █                      ████▓▒          ▒▓█████                     █   ",
  "    █                    █████████████████████████                   █    ",
  "     ██                ▓███████████████████████████▓               ██     ",
  "       █              ███████████████████████████████             █       ",
  "        ██          ▓█████████████████████████████████▓         ██        ",
  "          ██       █████████████████████████████████████      ██          ",
  "            ███  ▓███████████████████████████████████████▓ ███            ",
  "               ████████████████████████████████████████████               ",
  "                  ██████████████████████████████████████                  ",
  "                      ██████████████████████████████                      ",
  "                             ████████████████                             ",0 };

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/mount.h>
#include <sys/vfs.h>

// Math: MIN, MAX
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
// bool
typedef void* bool;
#define true (void*) 0x01
#define false (void*) NULL
// callback masks
typedef void (*voidFn)(void);

void VIEW_render();
void VIEW_resize(unsigned int size);


static            int TTYFD = STDERR_FILENO;
static            int TOP  = 0;
static            int CUR  = 0;
static   unsigned int COLS = 80;
static   unsigned int ROWS = 24;
static          char* LOG_LINE;
static          char* DEFAULT          = NULL;
static           bool VERBOSE          = false;
static   unsigned int MENU_COLS        = 37;
static   unsigned int MENU_ROWS        = 10;
static          char* OPTIONS          = NULL;

static          char* TILE_HEADER      = "\x1b[1;41;33m GEAR0x1337";
static          char* TILE_FOOTER      = "\x1b[1;42;33m";
static          char* TILE_LEFT        = "\x1b[33m░▒▓█\x1b[0m";
static          char* TILE_RIGHT       = "\x1b[0;33m█▓▒░\x1b[0m";
static            int TILE_LEFT_BYTES  = 21;
static            int TILE_RIGHT_BYTES = 23;
static            int TILE_COLS        = 4;

static   unsigned int BG_ROWS          = 35;
static   unsigned int BG_COLS          = 74;
static   unsigned int BG_TRUNC_ODD     = 0;
static          char* PAD_BG           = NULL;
static          char* PAD_MENU         = NULL;
static            int PAD_BG_LENGTH    = 0;
static            int PAD_MENU_LENGTH  = 0;
static            int VIEWLEN          = 0;

static         voidFn RENDERER         = VIEW_render;
static struct termios orig_termios;
static         char** VIEW;
static          char* FILL_MENU;

void ulog(char* message);
#define ULOG(args...) ({ asprintf(&LOG_LINE,##args); ulog(LOG_LINE); })

// Unblock fd's
int unblock (int fd){ int f;
  f = fcntl(fd, F_GETFL, 0);
  if ( f == -1 ) return -1;
  f |= O_NONBLOCK;
  f = fcntl(fd, F_SETFL, f);
  if( f == -1 ) return -1;
  return 0; }

// _fatal("message") exit with message and status code 1
void _fatal(char *message){
  write(TTYFD,"ERROR ",6);
  write(TTYFD,message,strlen(message));
  write(TTYFD,"\n",1); exit(1); }

/*                                                          ##    ##  ###   ###  ########  #######
                                                            ##    ##  ## # # ##  ##    ##  ##    ##
                                                            ##    ##  ## ### ##  ########  ########
                                                            ##    ##  ##  #  ##  ##    ##  ##
                                                            ########  ##     ##  ##    ##  */

typedef uint32_t UHashT;
typedef struct URecord URecord;
typedef struct UPair   { UHashT hash; void* value; char* key; } UPair;
        struct URecord { UHashT hash; void* value; URecord* prev; URecord* next; };
typedef struct UMap    { URecord* first; URecord* last; uint64_t count; } UMap;

UHashT UFNV32a(char *str){
  // Placed into public domain by :chongo: <Landon C. Noll>
  // Released as is. Many thanks.
  unsigned char *s = (unsigned char *) str; UHashT hash = 0x811c9dc5;
  while (*s){
    hash ^= (UHashT)*s++;
    hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24); }
  return hash; }

UPair* new_UPair(char* key, void* value){
  UPair* i = malloc(sizeof(UPair)); i->key = key; i->value = value; return i; }

void free_UPair(UPair* p){
  if ( p ){
    if (p->key) free(p->key);
    if (p->value) free(p->value);
  }}

void UMap_insert(URecord* prev, URecord* item, URecord* next){
  next->prev = prev->next = item;
  item->prev = prev; item->next = next; }

void UMap_join(URecord* prev, URecord* next){
  next->prev = prev; prev->next = next; }

URecord* UMap_lookupI(UMap* map, UHashT hash){ int i,l; URecord* rec = NULL;
  if ( !map ) _fatal("UMap_lookupI: empty map");
  if (( map->count == 0 ) || ( hash < map->first->hash ) || ( map->last->hash < hash  )) return NULL;
  for (i=0,l=map->count,rec=map->first; i<l; i++, rec=rec->next)if ( hash == rec->hash ) return rec;
  return NULL; }

void* UMap_getI(UMap* map, UHashT hash){
  if ( !map ) _fatal("UMap_getI: empty map");
  URecord* rec = UMap_lookupI(map,hash);
  return rec ? rec->value : NULL; }

URecord* UMap_setI(UMap* map, UHashT hash, void* value){ int i,l; URecord *rec,*r;
  if ( !map ) _fatal("UMap_setI: empty map");
  rec = UMap_lookupI(map, hash);
  if ( rec ){ free( rec->value ); rec->value = value; return rec; }
  rec = (URecord*) malloc(sizeof(URecord));
  rec->hash  = hash;
  rec->value = value;
       if ( 0 == map->count ){ rec->next = rec->prev = map->first = map->last = rec; }
  else if ( map->last->hash < hash ){  UMap_insert(map->last,rec,map->first); map->last  = rec; }
  else if ( map->first->hash > hash ){ UMap_insert(map->last,rec,map->first); map->first = rec; }
  else for ( i=0,l=map->count,r=map->first; i<l; i++,r=r->next )
       if ( hash < r->hash ){ UMap_insert(r->prev,rec,r); break; }
  map->count++; return rec; }

URecord* UMap_lookup(UMap* map, char* key, UHashT* gethash){ URecord* rec; UHashT hash = NULL;
  if ( !map ) _fatal("UMap_lookup: empty map");
  if ( !key ) return NULL;
  int i, l; UPair* pair;
  hash = UFNV32a(key); if ( gethash ) *gethash = hash;
  rec = UMap_lookupI(map,hash);
  if ( rec )
    for ( i=0, l=map->count; i<l; i++, rec = rec->next ){
      pair = (UPair*) rec->value;
      if ( ! pair )                      return NULL;
      if ( ! pair->key )                 return NULL;
      if ( 0 == strcmp(key,pair->key) )  return rec;
      if ( hash != rec->next->hash )     return NULL; }
  return NULL; }

void UMap_set(UMap* map, char* key, void* value){
  if ( !map ) _fatal("UMap_set: empty map");
  UHashT hash = NULL;
  URecord* rec = UMap_lookup( map, key, &hash );
  UPair* pair = rec ? rec->value : NULL;
  if ( ! pair ){
    pair = (UPair*) malloc(sizeof(UPair));
    pair->key = key; pair->hash = hash; }
  pair->value = value;
  if ( ! rec ) UMap_setI(map,hash,pair); }

void* UMap_get(UMap* map,char* key){
  if ( !map ) _fatal("UMap_get: empty map");
  URecord* rec = UMap_lookup(map,key,NULL);
  UPair*  pair = rec? (UPair*) rec->value :NULL;
  return pair ? pair->value : NULL; }


void UMap_delI(UMap* map, UHashT hash){
  if ( map->count == 0 ) return;
  URecord* rec = UMap_lookupI(map,hash);
  if ( !rec ) return;
  if ( map->count == 1 ){ map->first = map->last = NULL; }
  else {
    UMap_join(rec->prev, rec->next);
    if ( map->first == rec ) map->first = rec->next;
    if ( map->last  == rec ) map->last  = rec->prev; }
  if (rec->value) free(rec->value);
  free(rec); map->count--; }

void UMap_recfree(UMap* map, URecord* rec, void(*val_free)(void*) ){
  if (!rec||!map) return;
  UPair* pair = (UPair*) rec->value;
  if (   pair     ){     free(pair->key);
    if ( val_free ){ val_free(pair->value); }}
  UMap_delI(map,rec->hash); }

void UMap_free(UMap* map, char* key, void(*val_free)(void*) ){
  UHashT hash; URecord* rec = UMap_lookup(map,key,&hash);
  if (!rec) return; UMap_recfree(map,rec,val_free); }

void UMap_del(UMap* map, char* key){ UMap_free(map,key,free); }

void UMap_destroy(UMap* map,void(*val_free)(void*)){
  int i; URecord *rec,*next; for (
    i=0,rec=map->first->next=map->first->next;
    i<map->count,NULL!=next;
    i++,rec=next,next=next->next )
    UMap_recfree(map,rec,val_free);
  free(map); }

#define UMap_each(map,rec,nxt,i) \
  int i; URecord *rec,*nxt; if ( map->first ) for ( \
    i=0,rec=map->first,nxt=map->first->next; \
    i<map->count && NULL!=nxt; \
    i++,rec=nxt,nxt=nxt->next )

#define UMap_dump(map){ UPair* __pair; \
  UMap_each(map,__rec,__next,__dump) if ( __pair = __rec->value ) \
  printf("[%s] %llx\n",__pair->key,__pair->value); exit(1); }

UMap* UMap_create(void){
  UMap* map = (UMap*) malloc(sizeof(UMap));
  map->count = 0; map->first = map->last = 0; return map; }

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
  if(ix<=len) return ix;
  for (i=0,chars=0;i<ix;i++){
    if( chars == 1 + len ) return i;
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

/*                                                                      ########  ########  ##    ##
                                                                           ##        ##      ##  ##
                                                                           ##        ##       ####
                                                                           ##        ##        ##
                                                                           ##        ##        ##

     Based on an example by Wolf Holzmann
     see: http://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c */

int tty_reset(void){
  if (tcsetattr(TTYFD,TCSAFLUSH,&orig_termios) < 0) return -1;
  return 0; }
void tty_atexit(void){ tty_reset(); }

void tty_raw(int timeout){
  struct termios raw;
  raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag |= (OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG );
  raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = timeout;
  if (tcsetattr(TTYFD,TCSAFLUSH,&raw) < 0) _fatal("can't set raw mode"); }

void tty_init(){
  if ( !isatty(TTYFD)                            ) _fatal("not on a tty");
  if (  tcgetattr(TTYFD,&orig_termios) < 0       ) _fatal("can't get tty settings");
  if (  atexit(tty_atexit) != 0                  ) _fatal("atexit: can't register tty reset");
  tty_raw(0); tty_pollsize(); }

int tty_pollsize(void){
  fwrite("\e[24h\e[?25l\e[s\e[99999999C\e[9999999B\e[6n",39,1,stderr); }

int tty_readsize(char* buffer){
  int rows, cols = 0;
  sscanf(buffer,"[%i;%iR",&rows,&cols);
  struct winsize ws = {rows, cols, 0, 0};
  if (ioctl(TTYFD, TIOCSWINSZ, &ws) < 0) exit(1);
  ROWS = rows; COLS = cols;
  fwrite("\e[u\e[?25h",9,1,stderr); }

void tty_readsize_kernel(void){
  struct winsize w; ioctl(TTYFD, TIOCGWINSZ, &w); COLS = w.ws_col; ROWS = w.ws_row; }

/*                                                                                   ##     ########
                                                                                     ##     ##    ##
                                                                                     ##     ##    ##
                                                                                     ##     ##    ##
                                                                                     ##     ######*/



typedef struct ioBind {  char* name; void* value; } ioBind;
typedef struct ioEvent { char* type; char* name; char* message; } ioEvent;
typedef void (*EventHandler)(ioEvent* event);

static int IO_EPOLL = NULL;
static int IO_MAXEVENTS = 23;
static ioEvent UEVENT;
static UMap* IO_EVENTMAP;
static ioEvent USIG = {"SIG","SIGNAL",NULL};
static int IO_SIG[255];
static int IO_SIGMAP[] = { SIGWINCH, SIGQUIT, SIGUSR1, SIGUSR2, -1 };

void io_sig_handler( int signo ){ if ( signo < 0 || signo > 255 ) return;
  IO_SIG[signo] = 1; }

void io_init(){
  IO_EPOLL    = epoll_create(1);
  IO_EVENTMAP = UMap_create();
  USIG.message = malloc(1);
  int i,sig; for ( sig=IO_SIGMAP[i=0]; sig!=-1; sig=IO_SIGMAP[++i] ){
    if ( signal(sig,io_sig_handler) == SIG_ERR ) _fatal("can't catch SIG\n"); }}

void io_close(int fd){
  if ( -1 == epoll_ctl( IO_EPOLL, EPOLL_CTL_DEL, fd, NULL)); close(fd); }

void io_setflow(int fd, int mode, voidFn module){
  int i; URecord* rec;
  struct epoll_event *ev = malloc(sizeof(struct epoll_event));
  unblock(fd);
  ev->events = EPOLLIN; // & mode;
  ev->data.fd = fd;
  ev->data.ptr = module;
  if ( -1 == epoll_ctl(IO_EPOLL, EPOLL_CTL_ADD, fd, ev)) _fatal("epoll_ctl"); }

void io_bind(ioBind map[]){ int i;
  for ( i=0; map[i].name; i++ ) UMap_set( IO_EVENTMAP, map[i].name, map[i].value); }

void* io_emit(ioEvent* e){
  EventHandler     callback = UMap_get(IO_EVENTMAP,e->name);
  if ( !callback ) callback = UMap_get(IO_EVENTMAP,e->type);
  if ( VERBOSE )
    if ( "SIG" == e->type ) ULOG("\e[0m[\e[1;42mSIG\e[0m] '%s' 0x%x [%llx] \e[43m", e->name, e->message[0], callback);
    if ( "KEY" == e->type ) ULOG("\e[0m[\e[1;42mKEY\e[0m] '%s' 0x%02x%02x%02x%02x%02x%02x%02x%02x [%llx] \e[43m", e->name, e->message[0], e->message[1], e->message[2], e->message[3], e->message[4], e->message[5], e->message[6], e->message[7], callback);
    else                    ULOG("(%s) %s %s\n", e->type, e->name, e->message );
  if ( !callback ) return NULL;
  callback(e); return callback; }

void io_sigread(void){
  if ( 1 == IO_SIG[SIGWINCH] ) VIEW_resize(VIEWLEN);
  if ( 1 == IO_SIG[SIGQUIT] ){ tty_atexit(); exit(0); }
  int i,sig; for ( sig=IO_SIGMAP[i=0]; sig!=-1; sig=IO_SIGMAP[++i] ){
    if( IO_SIG[sig] ){ USIG.message[0] = sig; io_emit(&USIG); IO_SIG[i] = 0; }}}

void io_main(void){
  int ready, fd, j;
  struct epoll_event *ev, evlist[IO_MAXEVENTS];
  voidFn callback; URecord* rec;
  for(;;){
    RENDERER();
    ready = epoll_wait(IO_EPOLL, evlist, IO_MAXEVENTS, -1);
    if ( -1 == ready ) io_sigread(); // if ( errno == EINTR ) else _fatal("epoll_wait"); }
    for ( ev=&evlist[j=0]; j < ready; ev=&evlist[++j] ){
      callback = (voidFn) ev->data.ptr;
      callback(); }}}

/*                                                                      ##    ##  ######    ######
                                                                        ##  ##    ##    ##  ##    ##
                                                                        ####      ######    ##    ##
                                                                        ##  ##    ##    ##  ##    ##
                                                                        ##    ##  ######    #####*/

typedef struct ioKey  { uint32_t code; char* name;  } ioKey;

static ioKey IO_KEY_CODE[] = {
  { 0x01, "CTRL_A" },
  { 0x02, "CTRL_B" },
  { 0x03, "CTRL_C" },
  { 0x04, "CTRL_D" },
  { 0x06, "CTRL_F" },
  { 0x07, "CTRL_G" },
  { 0x08, "CTRL_H" }, { 0x09, "TAB" },    { 0x13, "CTRL_S" },
  { 0x16, "CTRL_V" }, { 0x0d, "ENTER" },  { 0x20, "SPACE" },  { 0x7f, "BACKSPACE" },
  { 0x1b5b317e, "UNKNOWN" }, { 0x1b5b327e, "INSERT" }, { 0x1b5b337e, "DELETE" },
  { 0x1b5b347e, "UNKNOWN" }, { 0x1b5b357e, "PGUP"   }, { 0x1b5b367e, "PGDN" },
  { 0x1b5b48,   "HOME"    }, { 0x1b5b46,   "END"    }, { 0x1b5b41,   "UP" },
  { 0x1b5b42,   "DOWN"    }, { 0x1b5b43,   "RIGHT"  }, { 0x1b5b44,   "LEFT" },
  { 0,0 }};

static        UMap* IO_KEYMAP;
static unsigned int UKBD_OFFSET = 0;
static         char UKBD_KEY[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static      ioEvent UKBD = {"KEY","UNKNOWN",NULL};

#define kbd_retry(){ \
  UKBD_OFFSET = 0; memset(UKBD_KEY,0,16); continue; }
#define kbd_emit_and_retry(){ \
  keyName = UMap_getI(IO_KEYMAP,keyCode); \
  UKBD.name = keyName? keyName : "KEY"; \
  io_emit(&UKBD); kbd_retry() }

void kbd_read(void){ int bytes; char* keyName; int i; uint64_t keyCode = 0; char key;
  for (;;){
    bytes = read(TTYFD, UKBD_KEY + UKBD_OFFSET, 1);
    if  ( bytes ==  0 ) _fatal("kbd_closed");
    if (( bytes == -1 ) || ( bytes != 1 )) return;
    UKBD_OFFSET += bytes;
    key = UKBD_KEY[UKBD_OFFSET-1];
    keyCode = 0; for ( i=0; i<UKBD_OFFSET; i++ ) keyCode = ( keyCode << 8ull ) | UKBD_KEY[i];
    if ( 1 == UKBD_OFFSET ) if ( UKBD_KEY[0] != 0x1b ) kbd_emit_and_retry() else continue;
    if ( ! ( UKBD_KEY[0] == 0x1b && UKBD_KEY[1] == 0x5b ) ){
      UKBD.name = "ESCAPE"; io_emit(&UKBD);
      UKBD.name = "[";      io_emit(&UKBD); kbd_retry(); }
    keyName = UMap_getI(IO_KEYMAP,keyCode);
    switch (key){
    case 0x52: tty_readsize(UKBD_KEY+1); kbd_retry();
    case 0x00: case 0x1b: case 0x7e: case 0x3f:  kbd_emit_and_retry();
    default: if( ( 0x40 < key && key < 0x5b ) || ( 0x60 < key && key < 0x7b ) ||
      ( 16  == UKBD_OFFSET ) ) kbd_emit_and_retry() }}}

void kbd_init(void){ int i; char* key;
  UKBD.message = UKBD_KEY;
  IO_KEYMAP = UMap_create();
  for(i=32;i<127;i++){
    asprintf(&key,"%c",i);
    UMap_setI(IO_KEYMAP,i,key); }
  for(i=0; IO_KEY_CODE[i].code!=0; i++)
    UMap_setI(IO_KEYMAP,IO_KEY_CODE[i].code,IO_KEY_CODE[i].name);
  UMap_delI(IO_KEYMAP,0x1b);
  io_setflow(TTYFD,0,kbd_read); }

/*                                                                      ######    ######    ########
                                                                        ##    ##  ##    ##  ##
                                                                        ######    ######    ##
                                                                        ##    ##  ##        ##
                                                                        ##    ##  ##        ######*/

typedef struct RPC_Request {
  int fd;
  char* in;
  char* out;
  unsigned long int length;
  unsigned long int out_length;
  unsigned long int out_pos;
} RPC_Request;

static int UNIXFD;
static UMap* RPC_COMMAND;
static UMap* RPC_CLIENT;
static ioEvent URPC = {"RPC",NULL,NULL};

void* rpc_emit(char* name, char* message){
  URPC.name = name; URPC.message = message;
  return io_emit(&URPC); }

void rpc_read(void){
  int client, bytes; RPC_Request* rpc;

  TRY_ACCEPT:
  client = accept(UNIXFD, NULL, NULL);
  if ( client == -1 ) goto COMMENCE_READING;
  rpc = malloc(sizeof(RPC_Request));
  rpc->fd = client;
  rpc->length = 0;
  rpc->in = malloc(1024);
  UMap_setI(RPC_CLIENT,client,rpc);
  io_setflow(client,0,rpc_read);
  goto TRY_ACCEPT;

  COMMENCE_READING:
  if ( RPC_CLIENT->count == 0 ) return;
  UMap_each(RPC_CLIENT,rec,next,i){
    rpc = rec->value;
    client = rpc->fd;
    TRY_READ:
    bytes = read( client, rpc->in + rpc->length, 1024);
    if ( -1 == bytes ) continue;
    if (  0 == bytes ){
      io_close(client);
      rpc_emit(rpc->in, str_next(rpc->in,'\n'));
      free(rpc->in);
      UMap_delI(RPC_CLIENT,client); }
    else {
      if ( VERBOSE ) ULOG("unix(%i,%llx) read %i",client,rpc,bytes);
      rpc->length += bytes;
      rpc->in = realloc(rpc->in, rpc->length + 1024);
      goto TRY_READ; }}}

int rpc_send(char* buf){
  struct sockaddr_un addr; int bytes=0,len,tries=0;
  char *socket_path = getenv("USPLASH_SOCK"); if (!socket_path) socket_path = "./socket";
  if ((UNIXFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) _fatal("[socket] error");
  unblock(UNIXFD); memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  while ( connect(UNIXFD, (struct sockaddr*) &addr, sizeof(addr)) == -1)
    if ( 3 == tries++ ) { printf("sock: %s\n",socket_path); _fatal("connect error"); }
    else sleep(0.05);
  len = strlen(buf); int written = 0;
  while ( written < len && 0 < ( bytes = write(UNIXFD,buf+written,len) ))
    if ( bytes > 0 ) written += bytes; else _fatal("write error");
  close(UNIXFD);
  return 0; }

void rpc_init(void){
  struct sockaddr_un addr;
  char *socket_path = getenv("USPLASH_SOCK"); if (!socket_path) socket_path = "./socket";
  if ((UNIXFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)             _fatal("[socket] error");
  memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  unlink(socket_path);
  if (bind(UNIXFD, (struct sockaddr*)&addr, sizeof(addr)) == -1)    _fatal("[socket] bind error");
  if (listen(UNIXFD, 128) == -1)                                    _fatal("[socket] listen error");
  RPC_CLIENT  = UMap_create();
  RPC_COMMAND = UMap_create();
  io_setflow(UNIXFD,0,rpc_read); }

/*                                                                      ##        ########  ########
                                                                        ##        ##    ##  ##
                                                                        ##        ##    ##  ##  ####
                                                                        ##        ##    ##  ##    ##
                                                                        ########  ########  ######*/

static              char* LOG[0x0a];
static unsigned int const LOG_MAX = 0x0a;
static unsigned int       LOG_CUR = 0x00;
static unsigned int       LOG_LENGTH = 0x00;

void ulog(char* message){
  LOG[LOG_CUR] = message;
  LOG_CUR = ++LOG_CUR % LOG_MAX;
  if( NULL != LOG[LOG_CUR] ) free(LOG[LOG_CUR]);
  LOG_LENGTH = MIN(++LOG_LENGTH,LOG_MAX); }

/*                                       #######   ########  ####   ##  #######   ########  #######
                                         ##    ##  ##        ## ##  ##  ##    ##  ##        ##    ##
                                         ######    ######    ## ##  ##  ##    ##  ######    ######
                                         ##    ##  ##        ##  ## ##  ##    ##  ##        ##    ##
                                         ##    ##  ########  ##   ####  #######   ########  ##    */

static bool VIEW_SCROLL = false;
static char *OUTPUT_BUFFER, *RENDER_BUFFER;

void VIEW_init(void){
  OUTPUT_BUFFER = malloc(1024 * 1024);
  RENDER_BUFFER = malloc(1024 * 1024);
  VIEW     = (char**) malloc(2*sizeof(char*));
  PAD_BG   = (char*)  malloc(2*sizeof(char));
  PAD_MENU = (char*)  malloc(2*sizeof(char));
  VIEW[1] = TILE_HEADER;
  VIEW[2] = TILE_FOOTER; }

void VIEW_resize(unsigned int size){ int c = COLS; int r = ROWS;
  tty_readsize_kernel();
  size = MIN( size+2, MENU_ROWS );
  if ( size == VIEWLEN && c == COLS && r == ROWS ) return;
  VIEW = (char**) realloc( VIEW, ( VIEWLEN = size ) * sizeof(char*) );
  // fill up to bg
  PAD_BG_LENGTH = ( COLS - BG_COLS ) / 2 + 1;
  PAD_BG = (char*) realloc( PAD_BG, PAD_BG_LENGTH );
  memset(PAD_BG,' ',PAD_BG_LENGTH);
  // fill up to menu
  BG_TRUNC_ODD = 1 - ( ( COLS - MENU_COLS ) / 2 ) % 2;
  PAD_MENU_LENGTH =    ( COLS - MENU_COLS ) / 2 + 1;
  PAD_MENU = (char*) realloc( PAD_MENU, PAD_MENU_LENGTH );
  memset(PAD_MENU,' ',PAD_MENU_LENGTH);
  // spaces to fill menu-enties
  FILL_MENU = (char*) realloc( FILL_MENU, MENU_COLS );
  memset(FILL_MENU,' ',MENU_COLS); }

void VIEW_render(){
  int isbg = 0x00, len, beg, end; char *row, *buffer;
  unsigned int i=0, j=0, pos=0;
  unsigned int bgTop   = MAX(0, ( ROWS    - BG_ROWS    ) / 2);
  unsigned int bgEnd   = MAX(0, ( bgTop   + BG_ROWS    + 1 ));
  unsigned int menuTop = MAX(0, ( ROWS    - VIEWLEN )  / 2  );
  unsigned int menuEnd = MAX(0, ( menuTop + VIEWLEN    + 1 ));
  unsigned int log_lines_rendered = 0; char* log_line;
  #define _RENDER_(str,len){ \
    memcpy(RENDER_BUFFER+pos,str,len); pos+=len; }
  if ( VIEW_SCROLL ){
    if ( CUR - TOP < 0 ){ TOP = MIN(MENU_ROWS,CUR); }
    if ( CUR - TOP > VIEWLEN - 3 ){ TOP = CUR - MENU_ROWS + 3; }}
  if ( bgTop < 0 )   _fatal("bgTop");
  if ( bgEnd < 0 )   _fatal("bgEnd");
  if ( menuTop < 0 ) _fatal("menuTop");
  if ( menuEnd < 0 ) _fatal("menuEnd");
  if ( CUR < 0 )     _fatal("CUR");
  // if ( TOP < 0 )     _fatal("TOP");
  for(i=0;i<ROWS;i++){
    sprintf(RENDER_BUFFER+pos,"\e[%i;1H\e[2K\e[43;30m",i);
    pos+= strlen(RENDER_BUFFER+pos);
    isbg = bgTop < i && i < bgEnd;
    if ( menuTop < i && i < menuEnd ){
      if ( isbg ){
        row = BG[i-bgTop-1];
        _RENDER_(PAD_BG,PAD_BG_LENGTH);
        _RENDER_(row,utf8ansi_trunc(row, PAD_MENU_LENGTH - PAD_BG_LENGTH - TILE_COLS )); }
      else _RENDER_(PAD_MENU,PAD_MENU_LENGTH-3);
      _RENDER_("\e[0m",4);
      _RENDER_(TILE_LEFT,TILE_LEFT_BYTES);
      //
      if      ( i-1 == menuTop ) row = TILE_HEADER;
      else if ( 1+i == menuEnd ) row = TILE_FOOTER;
      else row = VIEW[i-menuTop-2+TOP];
      if ( !row ) row = "( UNDEFINED )";
      len = utf8ansi_cols(row);
      if ( len > MENU_COLS){
        _RENDER_(row,utf8ansi_trunc(row,MENU_COLS-1)); }
      else _RENDER_(row,strlen(row));
      if ( len < MENU_COLS)
        _RENDER_(FILL_MENU,MENU_COLS-len);
      //
      _RENDER_(TILE_RIGHT,TILE_RIGHT_BYTES);
      _RENDER_("\e[43;30m",8);
      if ( isbg ){
        row = BG[i-bgTop-1];
        len = BG_COLS - ( PAD_MENU_LENGTH - PAD_BG_LENGTH - TILE_COLS );
        beg = utf8ansi_trunc(row,len);
        end = strlen(row) - beg;
        _RENDER_(row+beg,end); }}
    else if ( isbg ){
      _RENDER_(PAD_BG, PAD_BG_LENGTH);
      _RENDER_(BG[i-bgTop-1], strlen(BG[i-bgTop-1])); }
    else if ((menuTop > i) && (log_lines_rendered < LOG_LENGTH)){
      log_line = LOG[( LOG_CUR - 1 - log_lines_rendered++ ) % LOG_MAX ];
      if ( !log_line ) continue;
      _RENDER_(log_line,strlen(log_line)); RENDER_BUFFER[pos++] = '\n'; }}
  write(TTYFD,RENDER_BUFFER,pos);
  buffer = RENDER_BUFFER; RENDER_BUFFER = OUTPUT_BUFFER; OUTPUT_BUFFER = buffer; }

/*                                              ##    ##  ########  ########     ####   ##  ########
                                                ##    ##  ##        ##           ## ##  ##  ##    ##
                                                ########  ######    ########     ## ##  ##  ##    ##
                                                   ##     ##              ##     ##  ## ##  ##    ##
                                                   ##     ########  ########     ##   ####  ######*/

static char* YESNO_YES   = "    no                    \e[30;42m▓▒░ yes ░▒▓\e[0m";
static char* YESNO_NO    = "\e[30;41m▓▒░ no ░▒▓\e[0m                    yes    ";
static bool  YESNO_VALUE = false;

void yesno_yes (ioEvent* event){
  VIEW[1] = YESNO_YES; VIEW_render(); exit(0); }

void yesno_no (ioEvent* event){
  VIEW[1] = YESNO_NO;  VIEW_render(); exit(1); }

void yesno_finish (ioEvent* event){
  exit( YESNO_VALUE? 0:1 ); }

void yesno_swap (ioEvent* event){
  YESNO_VALUE = YESNO_VALUE? false:true;
  VIEW[0] = YESNO_VALUE? YESNO_YES : YESNO_NO;
  VIEW_render(); }

void yesno_init(void){
  VIEW_resize(1);
  YESNO_VALUE = DEFAULT? (atoi(DEFAULT)==1?true:false):false;
  VIEW[0] = YESNO_VALUE? YESNO_YES:YESNO_NO;
  ioBind map[] = {{"y",yesno_yes},{"n",yesno_no},{"ENTER",yesno_finish},{"KEY",yesno_swap},{0,0}};
  io_bind(map); VIEW_render(); }

/*                                       #######   #######   ########  ###   ###  #######   ########
                                         ##    ##  ##    ##  ##    ##  ## # # ##  ##    ##     ##
                                         ########  ######    ##    ##  ## ### ##  ########     ##
                                         ##        ##    ##  ##    ##  ##  #  ##  ##           ##
                                         ##        ##    ##  ########  ##     ##  ##           ## */

static char* INPUT;
static bool  INPUT_RENDER = true;
static char* INPUT_VIEW;
static int   INPUT_MAX = 1024;
static int   INPUT_LEN = 0;

void cur_end(ioEvent* event){ CUR = INPUT_LEN; }
void cur_home(ioEvent* event){ CUR = 0; }
void cur_left(ioEvent* event){ CUR = CUR > 0 ? --CUR : 0; }
void cur_right(ioEvent* event){ CUR = CUR < INPUT_LEN - 1 ? ++CUR : INPUT_LEN - 1; }

void prompt_backspace(ioEvent* event){
  if (CUR > 0){
  memmove(INPUT+CUR-1, INPUT+CUR, INPUT_LEN-CUR );
  INPUT_LEN--; CUR--; }}

void prompt_delete(ioEvent* event){
  if ( INPUT_LEN > 0 && ( CUR == 0 || CUR < INPUT_LEN ) ){
  memmove(INPUT+CUR, INPUT+CUR+1, INPUT_MAX-CUR-1);
  INPUT_LEN--; }}

void prompt_insert(ioEvent* event){
  if ( event->message[1] != 0x00 ) return;
  memmove(INPUT+CUR+1, INPUT+CUR, INPUT_MAX-CUR);
  INPUT[CUR] = event->message[0];
  CUR++; INPUT_LEN++; }

void prompt_finish(ioEvent* event){
  write(STDOUT_FILENO,INPUT,INPUT_LEN);
  write(STDOUT_FILENO,"\n",1); exit(0); }

void prompt_render(void){ char cursor = ' ';
  if ( CUR < 0 )         CUR    = 0;
  if ( CUR > INPUT_LEN ) CUR    = INPUT_LEN;
  if ( CUR < INPUT_LEN ) cursor = INPUT[CUR];
  memset(INPUT+INPUT_LEN,0x00,INPUT_MAX-INPUT_LEN-1);
  memset(INPUT_VIEW,0x00,INPUT_MAX);
  INPUT[CUR] = 0x00;
  sprintf(INPUT_VIEW,"%s\e[42m%c\e[0m%s",INPUT,cursor,INPUT+CUR+1);
  INPUT[CUR] = cursor;
  VIEW_render(); }

void prompt_init(void){
  RENDERER   = prompt_render;
  INPUT      = (char*) malloc(INPUT_MAX * sizeof(char*));
  INPUT_VIEW = (char*) malloc(INPUT_MAX * sizeof(char*) + 64); // account for cursor, colors, etc.
  if ( DEFAULT ){
    INPUT_LEN = CUR = strlen(DEFAULT);
    snprintf(INPUT,INPUT_LEN,"%s",DEFAULT); }
  else { INPUT[0] = 0x00; CUR = INPUT_LEN = 0; }
  VIEW_resize(1);
  VIEW[0] = INPUT_RENDER? INPUT_VIEW : " * * * * ";
  ioBind map[] = {
    {"UP",cur_home},{"PGUP",cur_home},{"HOME",cur_home},
    {"DOWN",cur_end},{"PGDN",cur_end},{"END", cur_end},
    {"LEFT",cur_left},{"RIGHT",cur_right},
    {"BACKSPACE", prompt_backspace }, {"DELETE", prompt_delete },
    {"ENTER", prompt_finish }, {"KEY", prompt_insert }};
  io_bind(map); }

void passwd_init(void){
  INPUT_RENDER = false; prompt_init(); }

/*                                        ########  ########  ##        ########  ########  ########
                                          ##        ##        ##        ##        ##           ##
                                          ########  ######    ##        ######    ##           ##
                                                ##  ##        ##        ##        ##           ##
                                          ########  ########  ########  ########  ########     */

typedef struct UListItem { char* key; char* on; char* off; bool value; } UListItem;
static char* VIEW_SELECTED = NULL;
static void* ITEM_SELECTED = NULL;
static UMap* ITEM_MAP      = NULL;

UListItem* new_UListItem(char* key,char* on,char* off,bool value){
  UListItem* i = malloc(sizeof(UListItem));
  i->key = key; i->on = on; i->off = off; i->value = value; return i; }

void select_render(void){ UListItem* item = NULL;
  UMap_each(ITEM_MAP,rec,next,i) if ( item = rec->value )
    VIEW[i] = item->value? item->on:item->off;
  else break;
  ITEM_SELECTED = UMap_getI(ITEM_MAP,CUR);
  if ( !ITEM_SELECTED ) ITEM_SELECTED = ITEM_MAP->first->value;
  if ( ITEM_SELECTED ){
    free(VIEW_SELECTED); asprintf(&VIEW_SELECTED,"\e[1;42m%s",VIEW[CUR]);
    VIEW[CUR] = VIEW_SELECTED; }
  VIEW_render(); }

void select_finish(ioEvent* event){ UListItem* item = ITEM_SELECTED;
  if ( !item ) exit(1); printf("%s\n",item->key); exit(0); }

void select_init(void){ int count = 0; char *line, *last;
  if( !OPTIONS ) _fatal("Select module: Please provide a list of options");
  VIEW_SCROLL = true; RENDERER = select_render; line = last = OPTIONS;
  ITEM_MAP = UMap_create();
  ioBind map[] = {{"UP",cur_left},{"PGUP",cur_home},{"HOME",cur_home},{"DOWN",cur_right},
    {"PGDN",cur_end},{"END",cur_end},{"ENTER",select_finish},{0,0}};
  line = str_next(line,'\n'); UMap_setI(ITEM_MAP,count++,new_UListItem(last,last,last,false));
  for(;line!=last;last=line,line=str_next(line,'\n'))
    UMap_setI(ITEM_MAP,count++,new_UListItem(line,line,line,false));
  INPUT_LEN = count; CUR = MIN(count,MAX(0,DEFAULT?atoi(DEFAULT):0));
  io_bind(map); VIEW_resize(count); }

/*                                        ########  ##    ##  ########  ########  ########  ########
                                          ##        ##    ##  ##    ##  ##    ##  ##        ##
                                          ##        ########  ##    ##  ##    ##  ########  ######
                                          ##        ##    ##  ##    ##  ##    ##        ##  ##
                                          ########  ##    ##  ########  ########  ########  ######*/

void choose_toggle(ioEvent* event){ UListItem* item = NULL; char* key; int i,len,l;
  if ( item = ITEM_SELECTED ){ key = item->key; len = strlen(key);
    if ( item->value ){ item->value = false;
      UMap_each(ITEM_MAP,rec,next,i) if ( item = rec->value )
        if ( 0 == strncmp(item->key,key,len) && strlen(item->key) > len ) item->value = false; }
    else { item->value = true;
      UMap_each(ITEM_MAP,rec,next,i) if ( item = rec->value ) if ( strlen(item->key) < len )
        for(l=len-1;l>0;l--) if ( 0 == strncmp(item->key,key,l) ) item->value = true; }}}

void choose_finish(ioEvent* event){ UListItem* item = NULL; char *s="", *sp=" ";
  UMap_each(ITEM_MAP,rec,next,i) if ( item = rec->value ) if( item->value ){
    printf("%s%s",s,item->key); s = sp; }
  exit(0); }

void choose_init(void){
  ioBind map[] = {
    {"UP",cur_left},{"PGUP",cur_home},{"HOME",cur_home},{"DOWN",cur_right},{"PGDN",cur_end},{"END",cur_end},
    {"SPACE",choose_toggle},{"ENTER", choose_finish },{0,0}};
  if( NULL == OPTIONS ) _fatal("Please provide a list of options");
  CUR = 0; TOP = 0; RENDERER = select_render; VIEW_SCROLL = true;
  char chr,*tmp=OPTIONS,*key,*val,*title,*on,*off;
  int count, byte, len;
  count = byte = 0; len = strlen(OPTIONS);
  if ( len < 1 ) _fatal("Options list too short");
  ITEM_MAP = UMap_create();
  tmp = OPTIONS;
  READ_LINE:
  key   = tmp; tmp = str_snext(key,' ' ); if ( tmp == key )   goto READ_DONE;
  val   = tmp; tmp = str_snext(tmp,' ' ); if ( tmp == val )   goto READ_DONE;
  title = tmp; tmp = str_next(tmp,'\n');  if ( tmp == title ) if ( strlen(tmp) < 1 ) goto READ_DONE; else title = tmp;
  asprintf(&on, "[\e[1;32m*\e[0m] %s",title);
  asprintf(&off,"[\e[1;32m \e[0m] %s",title);
  UMap_setI(ITEM_MAP,count++,new_UListItem(key,on,off,atoi(val)==1?true:false)); goto READ_LINE;
  READ_DONE:
  io_bind(map); VIEW_resize(INPUT_LEN = count); }

/*                                                          ###   ###  ########  ####   ##  ##    ##
                                                            ## # # ##  ##        ## ##  ##  ##    ##
                                                            ## ### ##  ######    ## ##  ##  ##    ##
                                                            ##  #  ##  ##        ##  ## ##  ##    ##
                                                            ##     ##  ########  ##   ####  ######*/


typedef struct UMenuItem {
  char* key;
  char* title;
  char* action;
  UMap* list;
  UPair* sel;
  URecord* rec; } UMenuItem;

UMenuItem* new_UMenuItem(char* key, char* index, char* title, char* action){
  UMenuItem* i = malloc(sizeof(UMenuItem));
  i->key = key; i->title = title; i->action = action;
  i->list = UMap_create();
  UMap_set(i->list,"default",i->sel = new_UPair(title,action));
  i->rec = i->list->first;
  return i; }

void free_UMenuItem(UMenuItem* i){ if ( i ){
  free(i->key); free(i->title); free(i->action); free(i->list); }}

static UMap* MENU_ITEM;
static UMap* MENU_ACTION;
static UMenuItem* MENU_SELECTED = NULL;
static char* MOPT        = NULL;
static char* MOPT_KEY    = NULL;
static char* MOPT_TITLE  = NULL;
static char* MOPT_ACTION = NULL;

void menu_render(){
  int i; int count = MAX(1,MENU_ITEM->count);
  URecord* rec; UPair* pair; UMenuItem* item;
  if ( VIEWLEN != count + 2 ) VIEW_resize(count);
  INPUT_LEN = count; MENU_SELECTED = NULL;
  if ( MENU_ITEM->count > 0 ){
    for ( i=TOP,rec=MENU_ITEM->first; i<INPUT_LEN; i++,rec=rec->next )
      if ( rec ) if ( pair = rec->value ) if ( item = pair->value ){
        VIEW[i] = item->title?item->title:"( null )";
        if ( i == CUR ){
          MENU_SELECTED = item;
          free(VIEW_SELECTED); asprintf(&VIEW_SELECTED,"\e[42;31m%s",item->title);
          VIEW[CUR] = VIEW_SELECTED; }}}
  else  VIEW[0] = DEFAULT;
  VIEW_render(); }

void menu_sub(bool left){ URecord* rec; UPair* pair; UPair* item;
  if ( !MENU_SELECTED ) return; if ( !MENU_SELECTED->rec ) return;
  rec = left ? MENU_SELECTED->rec->prev : MENU_SELECTED->rec->next; if ( !rec ) _fatal("no_rec");
  pair = rec->value;  if ( !pair ) _fatal("CONCEPT ERROR pair");
  item = pair->value; if ( !item ) _fatal("CONCEPT ERROR item");
  MENU_SELECTED->rec = rec;         MENU_SELECTED->sel = item;
  MENU_SELECTED->title = item->key; MENU_SELECTED->action = item->value; }

void menu_left(ioEvent* e){ menu_sub(true); }
void menu_right(ioEvent* e){ menu_sub(false); }

void menu_exec(ioEvent* e){ char* action;
  if ( MENU_SELECTED ) if ( action = MENU_SELECTED->action ) system(action); }

void menu_end(ioEvent* e){
  if ( !e->message ) exit(0);
  char *args[] = {"sh","-c",e->message,NULL};
  tty_atexit(); execvp("/bin/sh",args); }

void menu_set(ioEvent* e){
   int  len    = strlen(e->message);
  char* key    = malloc(len); memcpy( key, e->message, len );
  char* title  = str_next(key,  '\n');
  char* action = str_next(title,'\n');
  char* index  = str_next(key,':'); if ( index == key ) index = "default";
  UMenuItem* menuItm; UPair* pair; UPair* item;
  if ( !key ) return;
  if ( !( menuItm = UMap_get(MENU_ITEM,key)))
    menuItm = new_UMenuItem(key,index,title,action);
  else {
    if ( item = UMap_get(menuItm->list,index) ){
      item->key = title;
      item->value = action;
      if ( menuItm->sel == item ){
        menuItm->title = title;
        menuItm->action = action; }}
    else { UMap_set(menuItm->list,index,new_UPair(title,action)); }}
  UMap_set(MENU_ITEM,key,menuItm); if ( MENU_ITEM->count == 1 ) MENU_SELECTED = menuItm; }

void menu_del(ioEvent* e){
  int i; UPair* pair; URecord* rec; UMenuItem* item;
   int  len    = strlen(e->message);
  char* key    = malloc(len); memcpy( key, e->message, len );
  char* title  = str_next(key,  '\n');
  char* action = str_next(title,'\n');
  char* index  = str_next(key,':');
  if ( !key ) return;
  if ( !(item = UMap_get(MENU_ITEM,key)) ) return;
  if ( index != key && index != "default" ){
    if ( (pair = UMap_get(item->list,index)) ){
      if ( MENU_SELECTED->sel == pair ) menu_left(NULL); }
    UMap_free(item->list,index,(void (*)(void *))free_UPair); }
  else {
    UMap_destroy(item->list,(void (*)(void *))free_UPair);
    UMap_free(MENU_ITEM,key,(void (*)(void *))free_UMenuItem); }}

/* adapted from the busybox sources
    [switchroot]   Copyright 2005 Rob Landley      <rob@landley.net>
  licensed under GPLv2; thus compatible with usplashes license */

void switchroot(ioEvent* e) {
  char *newroot, *postscript; struct stat st; struct statfs stfs; dev_t rootdev;
  postscript = str_next(newroot = e->message,'\n');
  // Change to new root directory and verify it's a different fs
  chdir(newroot);  stat("/", &st); rootdev = st.st_dev; stat(".", &st);
  if ( st.st_dev == rootdev || getpid() != 1 ){
    ULOG("new_root(%s) must be a mountpoint; we must be PID 1",newroot); return; }
  if (mount(".", "/", NULL, MS_MOVE, NULL)) { // For example, fails when newroot is not a mountpoint
    ULOG("error moving root"); return; }
  chroot("."); chdir("/");                  // The chdir is needed to recalculate "." and ".." links
  ULOG("chroot_complete: %s",postscript);
  system(postscript); }

void rescue_shell(ioEvent* e) {
  char *args[] = {"sh",NULL};
  execvp("/bin/sh",args); }

void menu_init(void){
  char* message; RENDERER = menu_render; DEFAULT = "usplash ready";
  if ( MOPT ){
        if ( MOPT_ACTION ) asprintf(&message,"%s\n%s\n%s\n%s",MOPT,MOPT_KEY,MOPT_TITLE,MOPT_ACTION);
   else if ( MOPT_TITLE  ) asprintf(&message,"%s\n%s\n%s",    MOPT,MOPT_KEY,MOPT_TITLE);
   else if ( MOPT_KEY    ) asprintf(&message,"%s\n%s",        MOPT,MOPT_KEY);
   else                   message = MOPT;
   rpc_send(message); exit(0); }
  INPUT_LEN = 1; CUR = TOP = 0;VIEW[0] = DEFAULT;
  MENU_ITEM     = UMap_create();
  MENU_ACTION   = UMap_create();
  VIEW_SELECTED = (char*)  malloc( 1024 * sizeof(char));
  ioBind map[] = {
    {"UP",cur_left},{"PGUP",cur_home},{"HOME",cur_home},{"DOWN",cur_right},{"PGDN",cur_end},{"END",cur_end},
    {"LEFT",menu_left},{"RIGHT",menu_right},{"ENTER",menu_exec},
    {"set",menu_set},{"del",menu_del},{"end",menu_end},{"switch_root",switchroot},{0,0}};
  io_bind(map);
  rpc_init(); }

/*                                                          ###   ###  ########     ##     ####   ##
                                                            ## # # ##  ##    ##     ##     ## ##  ##
                                                            ## ### ##  ########     ##     ## ##  ##
                                                            ##  #  ##  ##    ##     ##     ##  ## ##
                                                            ##     ##  ##    ##     ##     ##   ##*/


static char* PREEXEC = NULL;
static struct option long_options[] =
  {{"choose",  required_argument, 0, 'c'},
   {"select",  required_argument, 0, 's'},
   {"yesno",   no_argument,       0, 'y'},
   {"prompt",  no_argument,       0, 'p'},
   {"passwd",  no_argument,       0, 'P'},
   {"menu",    no_argument,       0, 'm'},
    {"item",   required_argument, 0, 'i'},
   {0,0,0,0}};

int main(int argc, char *argv[]){ voidFn module = NULL; int c, option_index;
  while(1){
    option_index = 0;
    c = getopt_long (argc, argv, "ypPrmVe:L:R:H:F:v:c:s:d:i:t:a:q:S:", long_options, &option_index);
    #define _update_tile() TILE_COLS = MIN(utf8ansi_cols(TILE_LEFT),utf8ansi_cols(TILE_RIGHT))
    if (c == -1) break;
    switch (c){
      case 'V': VERBOSE = true; break;
      case 'v': DEFAULT      = strdup(optarg); break;
      case 'e': PREEXEC      = strdup(optarg); break;
      case 'H': TILE_HEADER  = strdup(optarg); break;
      case 'F': TILE_FOOTER  = strdup(optarg); break;
      case 'L': TILE_LEFT    = strdup(optarg); TILE_LEFT_BYTES  = strlen(optarg); _update_tile(); break;
      case 'R': TILE_RIGHT   = strdup(optarg); TILE_RIGHT_BYTES = strlen(optarg); _update_tile(); break;
      case 'c': module = choose_init; OPTIONS = strdup(optarg); break;
      case 's': module = select_init; OPTIONS = strdup(optarg); break;
      case 'p': module = prompt_init; break;
      case 'P': module = passwd_init; break;
      case 'y': module = yesno_init;  break;
      case 'm': module = menu_init;   break;
      case 'q': MOPT_KEY    = optarg; MOPT = "end"; module = menu_init; break;
      case 'S': MOPT_KEY    = optarg; MOPT = "switch_root"; module = menu_init; break;
      case 'i': MOPT_KEY    = optarg; MOPT = "set"; module = menu_init; break;
      case 'd': MOPT_KEY    = optarg; MOPT = "del"; module = menu_init; break;
      case 't': MOPT_TITLE  = optarg; break;
      case 'a': MOPT_ACTION = optarg; break;
      case '?': break; default: abort(); }}
  if ( !module ) _fatal("no module selected");
  VIEW_init(); io_init(); module(); tty_init(); kbd_init(); RENDERER();
  if ( PREEXEC ) system( PREEXEC );
  io_main(); }
