static char* LICENSE[] = {
  "  udg.c - a fancy dialog replacement",
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

#include "ucurse.h"

/*
                                                                         ██    ██ ███████ ███████     ███    ██  ██████
                                                                          ██  ██  ██      ██          ████   ██ ██    ██
                                                                           ████   █████   ███████     ██ ██  ██ ██    ██
                                                                            ██    ██           ██     ██  ██ ██ ██    ██
                                                                            ██    ███████ ███████     ██   ████  ██████
*/

static char* YESNO_YES   = "    no                    \e[30;42m▓▒░ yes ░▒▓\e[0m";
static char* YESNO_NO    = "\e[30;41m▓▒░ no ░▒▓\e[0m                    yes    ";
static bool  YESNO_VALUE = false;

void yesno_yes (ioEvent* event){
  VIEW[0] = YESNO_YES; VIEW_render(); exit(0); }

void yesno_no (ioEvent* event){
  VIEW[0] = YESNO_NO;  VIEW_render(); exit(1); }

void yesno_finish (ioEvent* event){
  exit( YESNO_VALUE? 0:1 ); }

static unsigned long int count = 0;
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

/*
                                                                    ██████  ██████   ██████  ███    ███ ██████  ████████
                                                                    ██   ██ ██   ██ ██    ██ ████  ████ ██   ██    ██
                                                                    ██████  ██████  ██    ██ ██ ████ ██ ██████     ██
                                                                    ██      ██   ██ ██    ██ ██  ██  ██ ██         ██
                                                                    ██      ██   ██  ██████  ██      ██ ██         ██
*/

void prompt_backspace(ioEvent* event){
  if (CUR > 0){
  memmove(INPUT+CUR-1, INPUT+CUR, INPUT_LEN-CUR );
  INPUT_LEN--; CUR--; }}

void prompt_delete(ioEvent* event){
  if ( INPUT_LEN > 0 && ( CUR == 0 || CUR < INPUT_LEN ) ){
  memmove(INPUT+CUR, INPUT+CUR+1, INPUT_MAX-CUR-1);
  INPUT_LEN--; }}

void prompt_insert(ioEvent* event){
  if ( event->data[1] != 0x00 ) return;
  memmove(INPUT+CUR+1, INPUT+CUR, INPUT_MAX-CUR);
  INPUT[CUR] = event->data[0];
  CUR++; INPUT_LEN++; }

void prompt_finish(ioEvent* event){
  write(STDOUT_FILENO,INPUT,INPUT_LEN); exit(0); }

void prompt_render(void){ char cursor = ' ';
  if ( CUR < 0 )           CUR = 0;
  if ( CUR > INPUT_LEN+1 ) CUR = INPUT_LEN+1;
  if ( CUR < INPUT_LEN ) cursor = INPUT[CUR];
  memset(INPUT+INPUT_LEN,0x00,INPUT_MAX-INPUT_LEN-1);
  memset(INPUT_VIEW,     0x00,INPUT_MAX);
  INPUT[CUR] = 0x00;
  sprintf(INPUT_VIEW,"%s\e[42m%c\e[0m%s",INPUT,cursor,INPUT+CUR+1);
  INPUT[CUR] = cursor;
  VIEW[0] = INPUT_RENDER? INPUT_VIEW : " * * * * ";
  VIEW_render(); }

void prompt_init(void){
  RENDERER   = prompt_render;
  INPUT      = (char*) malloc(INPUT_MAX * sizeof(char*));
  INPUT_VIEW = (char*) malloc(INPUT_MAX * sizeof(char*) + 64); // account for cursor, colors, etc.
  if ( DEFAULT ){
    INPUT_LEN = CUR = strlen(DEFAULT);
    snprintf(INPUT,INPUT_LEN,"%s",DEFAULT); }
  else { INPUT[0] = 0x00; TOP = CUR = INPUT_LEN = 0; }
  VIEW_resize(1);
  VIEW[0] = INPUT_RENDER? INPUT_VIEW : " * * * * ";
  ioBind map[] = {
    {"UP",cur_home},{"PGUP",cur_home},{"HOME",cur_home},
    {"DOWN",cur_end},{"PGDN",cur_end},{"END", cur_end},
    {"LEFT",cur_left},{"RIGHT",cur_right},
    {"BACKSPACE", prompt_backspace }, {"DELETE", prompt_delete },
    {"ENTER", prompt_finish }, {"KEY", prompt_insert },
    NULL };
  io_bind(map); }

void passwd_init(void){
  INPUT_RENDER = false; prompt_init(); }

/*
                                                                        ███████ ███████ ██      ███████  ██████ ████████
                                                                        ██      ██      ██      ██      ██         ██
                                                                        ███████ █████   ██      █████   ██         ██
                                                                             ██ ██      ██      ██      ██         ██
                                                                        ███████ ███████ ███████ ███████  ██████    ██
*/

typedef struct UListItem { char* key; char* on; char* off; bool value; } UListItem;
static char* VIEW_SELECTED = NULL;
static void* ITEM_SELECTED = NULL;
static umap* ITEM_MAP      = NULL;

UListItem* new_UListItem(char* key,char* on,char* off,bool value){
  UListItem* i = malloc(sizeof(UListItem));
  i->key = key; i->on = on; i->off = off; i->value = value; return i; }

void select_render(void){ UListItem* item = NULL;
  VIEW_UPDATE_SCROLL();
  umap_each(ITEM_MAP,rec,next,i) if ( item = rec->value )
    VIEW[i] = item->value? item->on:item->off;
  else break;
  ITEM_SELECTED = umap_getI(ITEM_MAP,CUR);
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
  ITEM_MAP = umap_create();
  ioBind map[] = {{"UP",cur_left},{"PGUP",cur_home},{"HOME",cur_home},{"DOWN",cur_right},
    {"PGDN",cur_end},{"END",cur_end},{"ENTER",select_finish},{0,0}};
  line = str_next(line,'\n'); umap_setI(ITEM_MAP,count++,new_UListItem(last,last,last,false),NULL);
  for(;line!=last;last=line,line=str_next(line,'\n'))
    umap_setI(ITEM_MAP,count++,new_UListItem(line,line,line,false),NULL);
  CUR = MIN(count,MAX(0,DEFAULT?atoi(DEFAULT):0));
  INPUT_LEN = count - 1; io_bind(map); VIEW_resize(count); }

/*
                                                                        ██████ ██   ██  ██████   ██████  ███████ ███████
                                                                       ██      ██   ██ ██    ██ ██    ██ ██      ██
                                                                       ██      ███████ ██    ██ ██    ██ ███████ █████
                                                                       ██      ██   ██ ██    ██ ██    ██      ██ ██
                                                                        ██████ ██   ██  ██████   ██████  ███████ ███████
*/

void choose_toggle(ioEvent* event){ UListItem* item = NULL; char* key; int i,len,l;
  if ( item = ITEM_SELECTED ){ key = item->key; len = strlen(key);
    if ( item->value ){ item->value = false;
      umap_each(ITEM_MAP,rec,next,i) if ( item = rec->value )
        if ( 0 == strncmp(item->key,key,len) && strlen(item->key) > len ) item->value = false; }
    else { item->value = true;
      umap_each(ITEM_MAP,rec,next,i) if ( item = rec->value ) if ( strlen(item->key) < len )
        for(l=len-1;l>0;l--) if ( 0 == strncmp(item->key,key,l) ) item->value = true; }}}

void choose_finish(ioEvent* event){ UListItem* item = NULL; char *s="", *sp=" ";
  umap_each(ITEM_MAP,rec,next,i) if ( item = rec->value ) if( item->value ){
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
  ITEM_MAP = umap_create();
  tmp = OPTIONS;
  while ( (tmp[0] == '\n') || (tmp[0] == '\r') || (tmp[0] == ' ') || (tmp[0] == '\t')  ) tmp++;
  READ_LINE:
  key   = tmp; tmp = str_snext(key,' ' ); if ( tmp == key )   goto READ_DONE;
  val   = tmp; tmp = str_snext(tmp,' ' ); if ( tmp == val )   goto READ_DONE;
  title = tmp; tmp = str_next(tmp,'\n');  if ( tmp == title ) if ( strlen(tmp) < 1 ) goto READ_DONE; else title = tmp;
  if ( strlen(key) == 2 ) asprintf(&title," %s", title);
  if ( strlen(key) == 3 ) asprintf(&title,"  %s",title);
  asprintf(&on, " [\e[1;32m*\e[0m] %s ",title);
  asprintf(&off," [\e[1;32m \e[0m] %s ",title);
  umap_setI(ITEM_MAP,count++,new_UListItem(key,on,off,atoi(val)==1?true:false),NULL); goto READ_LINE;
  READ_DONE:
  INPUT_LEN = count - 1; io_bind(map); VIEW_resize(count); }

/*
                                                                                         ███    ███  █████  ██ ███    ██
                                                                                         ████  ████ ██   ██ ██ ████   ██
                                                                                         ██ ████ ██ ███████ ██ ██ ██  ██
                                                                                         ██  ██  ██ ██   ██ ██ ██  ██ ██
                                                                                         ██      ██ ██   ██ ██ ██   ████
*/

static char* PREEXEC = NULL;
static struct option long_options[] =
  {{"choose",  required_argument, 0, 'c'},
   {"select",  required_argument, 0, 's'},
   {"yesno",   no_argument,       0, 'y'},
   {"prompt",  no_argument,       0, 'p'},
   {"passwd",  no_argument,       0, 'P'},
   {"help",    no_argument,       0, 'h'},
   {0,0,0,0}};

void udg_help() {
  const char* h = "\
udg [OPTIONS] ACTION [ARGUMENTS]\n\
\n\
ACTIONS:\n\
  -y --yesno               Simple binary choice\n\
  -p --prompt              Answer a prompt\n\
  -P --passwd              Prompt for a password (dots)\n\
  -c --choose \"ITEM ITEM\"  Choose one of multiple options\n\
  -s --select \"ITEM ITEM\"  Select multiple options.\n\
  -h --help                This message.\n\
\n\
OPTIONS:\n\
  -H \"VALUE\"               Header\n\
  -F \"VALUE\"               Footer\n\
  -L \"VALUE\"               Left\n\
  -R \"VALUE\"               Right\n\
";
  write(1,h,strlen(h)); }

umap* CUSTOM_EVENTS = NULL;
IOHANDLER(udg_custom_event_close);
IOHANDLER(udg_custom_event){
  char* a = umap_get(CUSTOM_EVENTS,e->name); if ( !a ) return 1;
  ULOG("custom event: name=%s ation=%s",e->name,a);
  io_exec(a,udg_custom_event_close,0); }
IOHANDLER(udg_custom_event_close){
  ULOG("custom event close: pid=%i status=%i",IO_CHILD_PID,IO_CHILD_STATUS);
  if ( IO_CHILD_STATUS > 0 ){ tty_atexit(); exit(0); }}

int main(int argc, char *argv[]){
  int c, option_index;
  voidFn MODULE = NULL;
  char *key_code = NULL, *action = NULL;
  CUSTOM_EVENTS = umap_create();
  while(1){
    option_index = 0;
    c = getopt_long (argc, argv, "a:c:DeF:hH:L:o:pPR:s:v:Vy", long_options, &option_index);
    #define _update_tile() TILE_COLS = MIN(utf8ansi_cols(TILE_LEFT),utf8ansi_cols(TILE_RIGHT))
    if (c == -1) break;
    switch (c){
      case 'o': key_code = strdup(optarg); break;
      case 'a': if ( key_code ){
        umap_set(CUSTOM_EVENTS,key_code,strdup(optarg),free); key_code = NULL; }; break;
      case 'h': udg_help(); break;
      case 'V': VERBOSE         = true;           break;
      case 'D': DEBUG = VERBOSE = true;           break;
      case 'v': DEFAULT         = strdup(optarg); break;
      case 'e': PREEXEC         = strdup(optarg); break;
      case 'H': TILE_HEADER     = strdup(optarg); break;
      case 'F': TILE_FOOTER     = strdup(optarg); break;
      case 'L': TILE_LEFT       = strdup(optarg); TILE_LEFT_BYTES  = strlen(optarg); _update_tile(); break;
      case 'R': TILE_RIGHT      = strdup(optarg); TILE_RIGHT_BYTES = strlen(optarg); _update_tile(); break;
      case 'c': MODULE = choose_init; OPTIONS = strdup(optarg); break;
      case 's': MODULE = select_init; OPTIONS = strdup(optarg); break;
      case 'p': MODULE = prompt_init; break;
      case 'P': MODULE = passwd_init; break;
      case 'y': MODULE = yesno_init;  break;
      case '?': break; default: abort(); }}
  if ( !MODULE ) _fatal("no MODULE selected");
  io_init(); tty_init(); kbd_init();
  VIEW_init(); MODULE(); PRE_IO = RENDERER;
  if ( PREEXEC ) system( PREEXEC );
  UDBG("\e[33mIO\e[0m \x1b[33mMAIN\x1b[0m\n");
  if ( CUSTOM_EVENTS->count != 0 ){
    urec* r = CUSTOM_EVENTS->last;
    for (size_t i = 0; i < CUSTOM_EVENTS->count; i++){
      if (( r = r->next )){
        ULOG("bind_script debug=%i evt='%s' action='%s'",DEBUG,r->key,r->value);
        io_bind_event(r->key,udg_custom_event); }}}
  io_main(); }
