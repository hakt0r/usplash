
#define _GNU_SOURCE
#define _DEFALUT_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <execinfo.h>
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

void ulog(char*);
#define ULOG(args...) ({ asprintf(&LOG_LINE,##args); ulog(LOG_LINE); })
#define UDBG(args...) ({ if(VERBOSE||DEBUG){asprintf(&LOG_LINE,##args); ulog(LOG_LINE); }})
#define UVRB(args...) ({ if(VERBOSE){asprintf(&LOG_LINE,##args); ulog(LOG_LINE); }})
// #define UDBG(args...) ({})
// #define UVRB(args...) ({})

#include "ubase.h"
#include "ustr.h"
#include "umap.h"
#include "uio.h"
#include "utty.h"
#include "urpc.h"
#include "ukbd.h"

void VIEW_render();
void VIEW_resize(unsigned int size);

static   int CUR = 0;
static   int LAST_ROWS = 0;
static char* INPUT;
static  bool INPUT_RENDER = true;
static char* INPUT_VIEW;
static   int INPUT_MAX = 1024;
static   int INPUT_LEN = 0;

void cur_end(ioEvent* event){ CUR = INPUT_LEN; }
void cur_home(ioEvent* event){ CUR = 0; }
void cur_left(ioEvent* event){ CUR = CUR > 0 ? --CUR : 0; }
void cur_right(ioEvent* event){ CUR = CUR < INPUT_LEN ? ++CUR : INPUT_LEN; }

static          int   TOP                  = 0;
static unsigned int   MENU_COLS            = 40;
static unsigned int   MENU_ROWS            = 10;
static unsigned int   WIDGET_BYTES         = 0;
static unsigned int   WIDGET_COLS          = 0;
static         char  *WIDGET, *OPTIONS, *LOG_LINE, *DEFAULT;

static         char  *TILE_HEADER          = "\x1b[1;41;33m GEAR0x1337";
static         char  *TILE_FOOTER          = "\x1b[1;42;33m";
static         char  *TILE_LEFT            = "\x1b[33m░▒▓█\x1b[0m";
static         char  *TILE_RIGHT           = "\x1b[0;33m█▓▒░\x1b[0m";
static          int   TILE_LEFT_BYTES      = 21;
static          int   TILE_RIGHT_BYTES     = 23;
static          int   TILE_COLS            = 4;

static          int   PAD_MENU_LENGTH      = 0;
static          int   VIEWLEN              = 0;

static       voidFn   RENDERER             = VIEW_render;
static         bool   UC_DIRTY             = true;
static         bool   UC_REFRESH           = true;
static         bool   VIEW_SCROLL          = false;
static unsigned int   RENDER_BUFFER_LENGTH = 1024 * 1024 * 8;
static         char **VIEW, *FILL_MENU, *RENDER_BUFFER, **ROW_BUF;
static         uh32  *OUTLINE;
static         uh32  *CURLINE;

static               char *LOG[50], *LOG_FORMAT, *LOG_PAD;
static                int  LOGLEN[50];
static unsigned int const  LOG_MAX         = 50;
static unsigned int        LOG_CUR         = 0x00;
static          int        LOG_LENGTH      = -1;


/*
                                                                                                ██ ███    ██ ██ ████████
                                                                                                ██ ████   ██ ██    ██
                                                                                                ██ ██ ██  ██ ██    ██
                                                                                                ██ ██  ██ ██ ██    ██
                                                                                                ██ ██   ████ ██    ██
*/

void VIEW_init(void){
  OUTLINE = NULL;
  VIEW = ROW_BUF = NULL;
  WIDGET = LOG_LINE = DEFAULT = FILL_MENU = RENDER_BUFFER = NULL;
  for (size_t i = 0; i < LOG_MAX; i++) { LOG[i] = NULL; }
  RENDER_BUFFER = malloc(RENDER_BUFFER_LENGTH);
  ROW_BUF = malloc(ROWS*sizeof(char*));
  size_t s = ROWS*sizeof(uh32);
  OUTLINE = malloc(s); memset(OUTLINE,0,s);
  CURLINE = malloc(s); memset(CURLINE,0,s);
  LOG_FORMAT = malloc(COLS*3);
  LOG_PAD    = malloc(COLS+2);
  for (size_t i = 0; i < ROWS; i++) {
    ROW_BUF[i] = (char*) malloc(COLS*3);
    ROW_BUF[i][0] = '\0'; }
  VIEW = malloc(2*sizeof(char*));
  VIEW[0] = TILE_HEADER;
  VIEW[1] = TILE_FOOTER; }

/*
                                                                                               ██       ██████   ██████
                                                                                               ██      ██    ██ ██
                                                                                               ██      ██    ██ ██   ███
                                                                                               ██      ██    ██ ██    ██
                                                                                               ███████  ██████   ██████
*/

void ulog(char* message){
  //io_append_file_sync("./ucurse.log",message); free(message);
  if( NULL != LOG[LOG_CUR] ) free(LOG[LOG_CUR]);
  LOG[LOG_CUR] = message;
  LOGLEN[LOG_CUR] = strlen(message);
  LOG_CUR = ++LOG_CUR % LOG_MAX;
  LOG_LENGTH = MIN(++LOG_LENGTH,LOG_MAX);
  UC_DIRTY = true; }

/*
                                                                              ██████  ███████ ███████ ██ ███████ ███████
                                                                              ██   ██ ██      ██      ██    ███  ██
                                                                              ██████  █████   ███████ ██   ███   █████
                                                                              ██   ██ ██           ██ ██  ███    ██
                                                                              ██   ██ ███████ ███████ ██ ███████ ███████
*/

void VIEW_resize(unsigned int size){
  int c = COLS; int r = ROWS; size = MAX(1,size); LAST_ROWS = ROWS;
  tty_readsize_kernel();
  // if ( size == VIEWLEN && c == COLS && r == ROWS ) return;
  LOG_FORMAT = realloc(LOG_FORMAT,COLS*3);
  LOG_PAD    = realloc(LOG_PAD,   COLS+2); memset(LOG_PAD,' ',COLS);
  PAD_MENU_LENGTH = ( COLS - MENU_COLS ) / 2;
  FILL_MENU = realloc( FILL_MENU, MENU_COLS ); memset(FILL_MENU,' ',MENU_COLS);
  VIEW = realloc( VIEW, ( VIEWLEN = size ) * sizeof(char*) );
  // realloc rows
  if ( ROWS != LAST_ROWS ){
    if ( ROWS < LAST_ROWS ){ for (size_t i = ROWS + 1; i < LAST_ROWS; i++) free(ROW_BUF[i]); }
    ROW_BUF = realloc(ROW_BUF,ROWS*sizeof(char*));
    size_t o = sizeof(uh32);
    size_t s =      (ROWS) * o;
    size_t l = (LAST_ROWS) * o;
    OUTLINE = realloc(OUTLINE,s);
    CURLINE = realloc(CURLINE,s);
    if ( ROWS > LAST_ROWS ){
      for (size_t i = LAST_ROWS; i < ROWS; i++) CURLINE[i] = 0;
      for (size_t i = LAST_ROWS; i < ROWS; i++) OUTLINE[i] = 0;
      for (size_t i = LAST_ROWS; i < ROWS; i++){
        ROW_BUF[i] = malloc(COLS*3*sizeof(char));
        ROW_BUF[i][0] = '\0'; }}}
  UC_DIRTY = UC_REFRESH = true; }

/*
                                                                       ██████  ███████ ███    ██ ██████  ███████ ██████
                                                                       ██   ██ ██      ████   ██ ██   ██ ██      ██   ██
                                                                       ██████  █████   ██ ██  ██ ██   ██ █████   ██████
                                                                       ██   ██ ██      ██  ██ ██ ██   ██ ██      ██   ██
                                                                       ██   ██ ███████ ██   ████ ██████  ███████ ██   ██
*/

#define _RENDER_(str,len){ memcpy(R+pos,str,len); pos+=len; }
#define _FORMAT_(args...){ pos += sprintf(R+pos,##args); }
static int is_writing  = 0;
static int   to_write  = 0;
static int    written  = 0;
static int is_updated  = 0;

IOHANDLER(render_swap);

void VIEW_UPDATE_SCROLL(){ if ( VIEW_SCROLL ){
  int visibile_rows = MIN(MENU_ROWS,VIEWLEN);
  TOP = CUR > visibile_rows-2 ? CUR - visibile_rows +1 : 0; }}

void VIEW_render(){
  if ( is_writing ) { is_updated = 1; }
  VIEW_UPDATE_SCROLL();

  int log_overflow = 0, log_pos, islog = 0x0, isview = 0x0, len, beg, end; char *row;
  unsigned int i=0, j=0, pos=0, visibile_rows = MIN(MENU_ROWS,VIEWLEN);
  // TOP/MID/BTM maybe
  // unsigned int menuTop = MAX(0,  ( ROWS - visibile_rows ) / 2 );
  // unsigned int menuEnd = MAX(0, menuTop + visibile_rows ) + 1;
  unsigned int menuEnd = MAX(0,ROWS - 1);
  unsigned int menuTop = MAX(0,ROWS - visibile_rows - 2);
  unsigned int log_lines_rendered = 0; char* log_line;

  for( i = 0; i < ROWS; i++ ){
    bool format_log = false; char *R = ROW_BUF[i]; pos = 0;
    log_pos  = ( LOG_CUR - 1 + LOG_MAX - log_lines_rendered ) % LOG_MAX;
    log_line = LOG[log_pos];
    islog    = log_lines_rendered < MIN( LOG_LENGTH + 2, MIN( LOG_MAX, ROWS ));
    isview   = menuTop <= i && i <= menuEnd;
    _FORMAT_("\e[%i;1H\e[2K\e[0;31m",i+1);
    if ( islog && log_line ) if ( (log_line[0] == '%') && (log_line[1] == 'c') ){
      int len = utf8ansi_cols(log_line) - 2;
      int off = MAX(0, (COLS-len)/2 );
      if ( len < COLS ){
        format_log = true;
        sprintf(LOG_FORMAT,"%.*s%s",off,LOG_PAD,log_line+2);
        log_line = LOG_FORMAT; }}
    if ( isview ){
      if ( islog && log_line ){
        _RENDER_( log_line, utf8ansi_trunc(log_line, PAD_MENU_LENGTH )); }
      // RENDER LEFT TILE
      _FORMAT_("\e[%iG\e[0K",PAD_MENU_LENGTH-3);
      _RENDER_("\e[0m",4);
      _RENDER_(TILE_LEFT,TILE_LEFT_BYTES);
      // RENDER MENU ITEM
      if      ( i == menuTop ) row = TILE_HEADER;
      else if ( i == menuEnd ) row = TILE_FOOTER;
      else { row = VIEW[i-menuTop-1+TOP]; }

      if ( !row ) row = "( UNDEFINED )";
      len = utf8ansi_cols(row);
      if ( len > MENU_COLS) { _RENDER_(row,utf8ansi_trunc(row,MENU_COLS)); }
      else                  { _RENDER_(row,strlen(row)); }
      if ( len < MENU_COLS) { _RENDER_(FILL_MENU,MENU_COLS-len); }
      // RENDER RIGHT TILE
      _RENDER_(TILE_RIGHT,TILE_RIGHT_BYTES);
      _RENDER_("\e[0;31m",7);
      if ( islog && log_line ) {
        int crop = utf8ansi_trunc ( log_line, PAD_MENU_LENGTH + TILE_COLS + MENU_COLS );
        int len  = utf8ansi_cols  ( log_line + crop );
        //_FORMAT_("(i=%i s=%i c=%i c=%i l=%i d=%i o=%i:%ib)",format_log,PAD_MENU_LENGTH + TILE_COLS + MENU_COLS,COLS,crop,len,COLS-crop,utf8ansi_cols(log_line),strlen(log_line));
        _RENDER_( log_line + crop, utf8ansi_trunc(log_line + crop, MAX(0,COLS-crop) )); }}
    else if ( islog && log_line ){
      _RENDER_(log_line,utf8ansi_trunc(log_line,COLS-1)); }
    if ( islog ) log_lines_rendered++;
    // RENDER WIDGETS
    if ( i == 0 ) if ( WIDGET ){
      int d = COLS - WIDGET_COLS;
      _FORMAT_("\e[K\e[%iG%s",d+1,WIDGET); }
    // TERMINATE AND HASH LINE
    R[pos] = 0; CURLINE[i] = UFNV32a(R); }
  // create renderbuffer
  size_t o = 0, l = 0;
  if ( UC_REFRESH ) {
    for (size_t i = 0; i < ROWS; i++){
      l = strlen(ROW_BUF[i]);
      memcpy(RENDER_BUFFER+o,ROW_BUF[i],l);
      o += l; }
    RENDER_BUFFER[o] = '\0';
    UC_REFRESH = false; }
  else {
    for (size_t i = 0; i < ROWS; i++) if ( CURLINE[i] != OUTLINE[i] ){
      l = strlen(ROW_BUF[i]);
      memcpy(RENDER_BUFFER+o,ROW_BUF[i],l);
      o += l; }
    RENDER_BUFFER[o] = '\0';
    UC_REFRESH = false; }
  to_write = o;
  written = write(TTYFD,RENDER_BUFFER,to_write);
  if ( written < 0 ) written = 0;
  if ( to_write != written ){
    is_writing = 1;
    io_wantwrite(TTY_EVENT,render_swap); }}

IOHANDLER(render_swap){
  size_t w = write(TTYFD,RENDER_BUFFER+written,to_write-written);
  if ( w <= 0 ) return;
  written = written + w;
  if ( written == to_write ){
    memcpy(CURLINE,OUTLINE,ROWS*sizeof(uh32));
    is_writing = 0;
    if ( is_updated ){
      is_updated = 0;
      RENDERER(); }
    else io_donewriting(e); }}
