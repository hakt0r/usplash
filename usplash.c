static const char* _usplash_license = "\
    usplash.c - a fancy dynamic menu\n\n\
\
    Copyright (C) 2015  Sebastian Glaser ( anx@ulzq.de )\n\n\
\
    This program is free software; you can redistribute it and/or modify it under the terms of the\n\
      GNU General Public License\n\
    as published by the Free Software Foundation;\n\
    either version 3 of the License, or (at your option) any later version.\n\n\
\
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;\n\
    without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
    See the GNU General Public License for more details.\n\
    You should have received a copy of the GNU General Public License along with this program;\n\
    if not, write to the Free Software Foundation,\n\
      Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA\n\n";

static const char* _usplash_info = "\
  usplash is a tiny dynamic menu application for the Linux command-line.\n\n\
\
  It was written specifically for gear0x's splash-screen menu\n\
    and is meant as a script-supported init process.\n\n\
\
  When you run usplash in menu mode (-m) the main-screen will pop up instantly,\n\
    now you can add menu items and scripts they should execute.\n\
  Opposed to dialog and other menu systems for the command-line, usplash can be changed at runtime;\n\
    meaning that background-processes can add and update entries in the menu,\n\
    or even trigger actions and select entries.\n\n";

static const char* _usplash_usage = "\
  usage$ usplash ACTION [OPTIONS]\n\n\
\
  ACTIONS: (*optional)\n\
    -m --menu                Menu/Daemon Mode\n\
       * -S \"PATH\"         Switch root to PATH\n\
       * -a \"SCRIPT\"       Run after switch\n\
       * -e \"SCRIPT\"       PRE-EXEC script\n\
       * -H \"VALUE\"        Menu Header\n\
       * -F \"VALUE\"        Menu Footer\n\
       * -L \"VALUE\"        Left Tile\n\
       * -R \"VALUE\"        Right Tile\n\
    -q --quit \"SCRIPT\"       Stop daemon and run (optional) SCRIPT\n\
    -i --item \"ID\"           Set/create item ID (requires -a -t)\n\
    -r --run \"ID\"            Run action from item ID (\n\
    -d --delete \"ID\"         Delete item ID\n\
    -h --help                This message.\n\n\
\
  OPTIONS:\n\
    -t --title \"TITLE\"       Set TITLE of item ID (-i)\n\
    -a --action \"SCRIPT\"     Set SCRIPT of item ID (-i)\n\n";

/*
                                                                         ██   ██ ███████  █████  ██████  ███████ ██████
                                                                         ██   ██ ██      ██   ██ ██   ██ ██      ██   ██
                                                                         ███████ █████   ███████ ██   ██ █████   ██████
                                                                         ██   ██ ██      ██   ██ ██   ██ ██      ██   ██
                                                                         ██   ██ ███████ ██   ██ ██████  ███████ ██   ██
*/

#define SOCKET_NAME "USPLASH_SOCK"
#include "ucurse.h"

struct UMenuItem;    typedef struct UMenuItem UMenuItem;
struct USubMenuItem; typedef struct USubMenuItem USubMenuItem;

   typedef struct UMenuItem {
        char* key;
        char* title;
        char* action;
        umap* list;
USubMenuItem* sel;
        urec* rec; } UMenuItem;


   UMenuItem* new_UMenuItem(char*, char*, char*, char*);
        void  free_UMenuItem(UMenuItem*);

      typedef struct USubMenuItem {
   UMenuItem* parent;
        char* key;
        char* index;
        char* title;
        char* action; } USubMenuItem;

USubMenuItem* new_USubMenuItem(UMenuItem*, char*, char*, char*, char*);
         void free_USubMenuItem(USubMenuItem*);

static      umap *MENU_ITEM          = NULL;
static      umap *MENU_ACTION        = NULL;
static      umap *MENU_WIDGET        = NULL;
static UMenuItem *MENU_SELECTED      = NULL;
static      char *MOPT               = NULL;
static      char *MOPT_KEY           = NULL;
static      char *MOPT_TITLE         = NULL;
static      char *MOPT_ACTION        = NULL;
static      char *MOPT_NEWROOT       = NULL;
static      bool  MOPT_SWROOT    = false;
static      bool  MOPT_DAEMON        = false;
static      char *MENU_VIEW_SELECTED = NULL;
static    voidFn *AFTER_CLOSE        = NULL;
static   ioEvent *END_EVENT          = NULL;

/*
                                           ██    ██ ███    ███ ███████ ███    ██ ██    ██ ██ ████████ ███████ ███    ███
                                           ██    ██ ████  ████ ██      ████   ██ ██    ██ ██    ██    ██      ████  ████
                                           ██    ██ ██ ████ ██ █████   ██ ██  ██ ██    ██ ██    ██    █████   ██ ████ ██
                                           ██    ██ ██  ██  ██ ██      ██  ██ ██ ██    ██ ██    ██    ██      ██  ██  ██
                                            ██████  ██      ██ ███████ ██   ████  ██████  ██    ██    ███████ ██      ██
*/

UMenuItem* new_UMenuItem(char* key, char* index, char* title, char* action){
  UMenuItem*    i = malloc(sizeof(UMenuItem));
  USubMenuItem* s = NULL;
  i->key  = key;
  i->title = title;
  i->action = action;
  i->list = umap_create();
  i->sel = new_USubMenuItem(i,key,"default",title,action);
  i->rec = i->list->first;
  umap_set(MENU_ITEM,key,i,(free_f)free_UMenuItem);
  return i; }

void free_UMenuItem(UMenuItem* i){
  if ( i ){
    umap_destroy(i->list,(free_f)free_USubMenuItem);
    free(i->key); free(i); }}

USubMenuItem* new_USubMenuItem(UMenuItem* parent, char* key, char* index, char* title, char* action){
  USubMenuItem* i = malloc(sizeof(USubMenuItem));
  i->key = key;
  i->index = index;
  i->title = title;
  i->action = action;
  i->parent = parent;
  umap_set(parent->list,index,i,(free_f)free_USubMenuItem);
  return i; }

void free_USubMenuItem(USubMenuItem* i){
  if ( i ){
    if ( i->parent->key != i->key ) free(i->key);
    free(i); }}

/*
                                                                       ██████  ███████ ███    ██ ██████  ███████ ██████
                                                                       ██   ██ ██      ████   ██ ██   ██ ██      ██   ██
                                                                       ██████  █████   ██ ██  ██ ██   ██ █████   ██████
                                                                       ██   ██ ██      ██  ██ ██ ██   ██ ██      ██   ██
                                                                       ██   ██ ███████ ██   ████ ██████  ███████ ██   ██
*/

IOHANDLER(menu_resize){ VIEW_resize(VIEWLEN); }
void menu_render(){
  if ( !UC_DIRTY ) return; UC_DIRTY = false;
  int i; int count = MAX(1,MENU_ITEM->count);
  INPUT_LEN = MENU_ITEM->count - 1;
  if ( VIEWLEN != count ) VIEW_resize(count);
  MENU_SELECTED = NULL;
  if ( MENU_ITEM->count > 0 ){
    UMenuItem *item; urec *rec, **recs = umap_sort(MENU_ITEM);
    for (size_t i = 0; i < VIEWLEN; i++){
      rec = recs[i];
      if ( rec ) if ( item = rec->value ){
        VIEW[i] = item->title?item->title:"( null )";
        if ( i == CUR ){
          MENU_SELECTED = item;
          free(MENU_VIEW_SELECTED);
          asprintf(&MENU_VIEW_SELECTED,"\e[42;31m%s\e[42;31m",item->title);
          VIEW[i] = MENU_VIEW_SELECTED; }}}
    free(recs); }
  else VIEW[0] = DEFAULT;
  VIEW_render(); }

/*
                                                                                         ██   ██ ███████ ██      ██████
                                                                                         ██   ██ ██      ██      ██   ██
                                                                                         ███████ █████   ██      ██████
                                                                                         ██   ██ ██      ██      ██
                                                                                         ██   ██ ███████ ███████ ██
*/

IOHANDLER(usplash_usage){   write(1,"\n",1); write(1,_usplash_usage,   strlen(_usplash_usage)); }
IOHANDLER(usplash_license){ write(1,"\n",1); write(1,_usplash_license, strlen(_usplash_license)); }
IOHANDLER(usplash_info){    write(1,"\n",1); write(1,_usplash_info,    strlen(_usplash_info)); }
IOHANDLER(usplash_help){    write(1,"\n",1); write(1,_usplash_usage,   strlen(_usplash_usage));
                                             write(1,_usplash_info,    strlen(_usplash_info));
                                             write(1,_usplash_license, strlen(_usplash_license)); }

/*
                                                                                                ███████ ███████ ████████
                                                                                                ██      ██         ██
                                                                                                ███████ █████      ██
                                                                                                     ██ ██         ██
                                                                                                ███████ ███████    ██
*/

IOHANDLER(menu_set){
  UMenuItem* menuItm = NULL;
  USubMenuItem*  sub = NULL;
  int   len    = strlen(e->data);
  char* key    = malloc(len+1); memcpy( key, e->data, len ); key[len] = '\0';
  char* title  = str_next(key,  '\n');
  char* action = str_next(title,'\n');
  char* index  = str_next(key,':'); if ( index == key ) index = "default";
  char* oldkey = NULL;

  if ( !key ) return 1;

  if (!( menuItm = umap_get(MENU_ITEM,key) )) menuItm = new_UMenuItem(key,index,title,action);
  else {
    if ( sub = umap_get(menuItm->list,index) ){
      if (( sub->key == menuItm->key ) && ( sub->key != key )) { oldkey = sub->key; menuItm->key = key; }
      sub->key = key; sub->index = index; sub->title = title; sub->action = action; }
    else { sub = new_USubMenuItem(menuItm,key,index,title,action); }}

  if ( menuItm->sel == sub ){
    menuItm->title  = title;
    menuItm->action = action; }

  if ( MENU_ITEM->count == 1 ) { MENU_SELECTED = menuItm; }
  if ( oldkey ) free( oldkey );
  UC_DIRTY = true; return 0; }

/*
                                                                                                 ██████  ███████ ██
                                                                                                 ██   ██ ██      ██
                                                                                                 ██   ██ █████   ██
                                                                                                 ██   ██ ██      ██
                                                                                                 ██████  ███████ ███████
*/

IOHANDLER(menu_del){
  int i; USubMenuItem* sub; UMenuItem* item;
   int  len    = strlen(e->data);
  char* key    = e->data;
  char* title  = str_next(key,  '\n');
  char* action = str_next(title,'\n');
  char* index  = str_next(key,':');
  if ( !key ) return 1;
  if ( !(item = umap_get(MENU_ITEM,key)) ) return 1;
  if ( index != key && index != "default" ){
    if ( (sub = umap_get(item->list,index)) ){
      if ( MENU_SELECTED->sel == sub ) menu_left(NULL); }
    umap_del(item->list,index,(free_f)free_USubMenuItem); }
  else {
    umap_del(MENU_ITEM,key,(free_f)free_UMenuItem); }
  UC_DIRTY = true; return 0; }

/*
                                           ███    ██  █████  ██    ██ ██  ██████   █████  ████████ ██  ██████  ███    ██
                                           ████   ██ ██   ██ ██    ██ ██ ██       ██   ██    ██    ██ ██    ██ ████   ██
                                           ██ ██  ██ ███████ ██    ██ ██ ██   ███ ███████    ██    ██ ██    ██ ██ ██  ██
                                           ██  ██ ██ ██   ██  ██  ██  ██ ██    ██ ██   ██    ██    ██ ██    ██ ██  ██ ██
                                           ██   ████ ██   ██   ████   ██  ██████  ██   ██    ██    ██  ██████  ██   ████
*/

void menu_sub(bool left){
  UDBG("MENU_SUB %d",left);
  if ( !MENU_SELECTED      ) { UDBG("NO MENU_SELECTED");      return; }
  if ( !MENU_SELECTED->rec ) { UDBG("NO MENU_SELECTED->rec"); return; }
  urec* rec; rec = left ? MENU_SELECTED->rec->prev : MENU_SELECTED->rec->next;
  if ( !rec                ) { UDBG("CONCEPT ERROR no_rec");  return; }
  USubMenuItem* item = rec->value;
  if ( !item               ) { UDBG("CONCEPT ERROR item");    return; }
  MENU_SELECTED->rec = rec;
  MENU_SELECTED->sel = item;
  MENU_SELECTED->title = item->title;
  MENU_SELECTED->action = item->action;
  UC_DIRTY = true; }

IOHANDLER(menu_left){  menu_sub(true);  return 0; }
IOHANDLER(menu_right){ menu_sub(false); return 0; }

IOHANDLER(menu_end){   cur_end(e);      UC_DIRTY = true; return 0; }
IOHANDLER(menu_home){  cur_home(e);     UC_DIRTY = true; return 0; }
IOHANDLER(menu_up){    cur_left(e);     UC_DIRTY = true; return 0; }
IOHANDLER(menu_down){  cur_right(e);    UC_DIRTY = true; return 0; }

/*
                                                                                              ██████  ██    ██ ███    ██
                                                                                              ██   ██ ██    ██ ████   ██
                                                                                              ██████  ██    ██ ██ ██  ██
                                                                                              ██   ██ ██    ██ ██  ██ ██
                                                                                              ██   ██  ██████  ██   ████
*/

IOHANDLER(menu_run){
  UMenuItem* item;
  USubMenuItem* sub;
  char *key   = e->data;
  char *index = str_next(key,':'); if ( index == key ) index = "default";
  if ( !key ) return 1;
  ULOG("RPC RUN %s",key);
  if ( !(item = umap_get(MENU_ITEM,key)) )    return 1;
  ULOG("RPC RUN %s:%s",key,index);
  if ( !(sub  = umap_get(item->list,index)) ) return 1;
  ULOG("RPC RUN %s:%s %s",sub->key,sub->index,sub->action);
  return io_shell(sub->action)?0:1; }

IOHANDLER(menu_run_selected){ char* action;
  if ( MENU_SELECTED ) if ( action = MENU_SELECTED->action ) {
    io_shell(action);
    return 0; }
  return 1; }

/*
                                                                                            ██████  ██    ██ ██ ████████
                                                                                           ██    ██ ██    ██ ██    ██
                                                                                           ██    ██ ██    ██ ██    ██
                                                                                           ██ ▄▄ ██ ██    ██ ██    ██
                                                                                            ██████   ██████  ██    ██
                                                                                               ▀▀
*/

static char* END_DATA = NULL;
IOHANDLER(menu_quit){
  if (END_DATA == NULL) {
    PRE_IO = (void*)&menu_quit;
    END_DATA = NULL;
    if ( e->data ) END_DATA = strdup(e->data);
    return 0; }
  // for (size_t i = 1; i < 7; i++) io_close_fd(i);
  // umap_destroy(MENU_ITEM,(free_f)free_UMenuItem);
  ULOG("QUIT %s",END_DATA);
  if ( !END_DATA ) return 1;
  char *args[] = {"/bin/sh","-c",END_DATA,NULL};
  tty_atexit();
  execvp("/bin/sh",args); }

/*
                                        ███████ ██     ██ ██ ████████  ██████ ██   ██ ██████   ██████   ██████  ████████
                                        ██      ██     ██ ██    ██    ██      ██   ██ ██   ██ ██    ██ ██    ██    ██
                                        ███████ ██  █  ██ ██    ██    ██      ███████ ██████  ██    ██ ██    ██    ██
                                             ██ ██ ███ ██ ██    ██    ██      ██   ██ ██   ██ ██    ██ ██    ██    ██
                                        ███████  ███ ███  ██    ██     ██████ ██   ██ ██   ██  ██████   ██████     ██
*/

/* adapted from the busybox sources
    [switchroot]   Copyright 2005 Rob Landley      <rob@landley.net>
  licensed under GPLv2; thus compatible with usplashes license */

IOHANDLER(rescue_shell);
IOHANDLER(switchroot){
  char *newroot, *action;
  struct stat st; struct statfs stfs; dev_t rootdev;
  if ( MOPT_SWROOT ){
    MOPT_SWROOT = false; newroot = MOPT_NEWROOT; action = MOPT_ACTION; }
  else {                 newroot = e->data;      action = str_next(newroot,'\n'); }
  if ( action == newroot ) action = NULL;
  if ( getpid() != 1 ){ ULOG("switchroot(%s) we must be PID 1; script=%s",newroot,action); rescue_shell(NULL); return -1; }
  chdir(newroot);  stat("/", &st); rootdev = st.st_dev; stat(".", &st);
  if ( st.st_dev == rootdev || getpid() != 1 ){ ULOG("switchroot(%s) must be a mountpoint; script=%s",newroot,action); rescue_shell(NULL); return -1; }
  if (mount(".", "/", NULL, MS_MOVE, NULL)) {
    ULOG("error moving root"); return -1; }
  chroot("."); chdir("/"); ULOG("switchroot(%s) complete script=%s",newroot,action); sync();
  if (action) io_shell(action);
  return 0; }

/*
                                                                              ████████  ██████   ██████  ██      ███████
                                                                                 ██    ██    ██ ██    ██ ██      ██
                                                                                 ██    ██    ██ ██    ██ ██      ███████
                                                                                 ██    ██    ██ ██    ██ ██           ██
                                                                                 ██     ██████   ██████  ███████ ███████
*/

IOHANDLER(menu_widget){
  char *key, *value; size_t len = strlen(e->data);
  key = e->data; value = str_next(key,'\n'); if ( key == value ) value = NULL;
  if ( value ){ // Set/Update
    value = strdup(value);
    umap_set(MENU_WIDGET,key,value,NULL); }
  else {
    umap_del(MENU_WIDGET,key,NULL); }
  if ( MENU_WIDGET->count != 0 ){
    int pos = 0; int len = 0, bytes = 0; char *str = NULL; urec *itm = NULL, **itms = umap_sort(MENU_WIDGET);
    for (size_t i = 0; i < MENU_WIDGET->count; i++) {
      itm = itms[i]; str = itm->value; len += utf8ansi_cols(str); bytes += strlen(str); }
    bytes += TILE_LEFT_BYTES + 3;
    len   += TILE_COLS + 2;
    if ( WIDGET ) { WIDGET = realloc(WIDGET,bytes); WIDGET[bytes-1] = '\0'; }
    else          { WIDGET =  malloc(bytes);        WIDGET[bytes-1] = '\0'; }
    memcpy(WIDGET,TILE_LEFT,TILE_LEFT_BYTES); pos += TILE_LEFT_BYTES;
    for (size_t i = 0; i < MENU_WIDGET->count; i++) {
      itm = itms[i]; str = itm->value;
      int l = strlen(str);
      memcpy(WIDGET+pos,str,l); pos += l; }
    WIDGET[pos++] = '\0';
    free(itms);
    WIDGET_BYTES = pos + 1; WIDGET_COLS = utf8ansi_cols(WIDGET); }
  UC_DIRTY = true; }

IOHANDLER(menu_log){
  char *line = e->data, *next = str_next(e->data,'\n');
  if ( line == next ) ULOG("%s",line);
  else {
    while ( line != next ){
      ULOG("%s",line);
      next = str_next(line = next,'\n'); }
    ULOG("%s",line); }
  return 0; }

IOHANDLER(toggle_debug)   { VERBOSE = DEBUG = DEBUG? false:true; }
IOHANDLER(toggle_verbose) { DEBUG = false; VERBOSE = VERBOSE? false:true; }
IOHANDLER(rescue_shell)   { char *args[] = {"sh",NULL}; execvp("/bin/sh",args); }

/*
                                                                                                ██ ███    ██ ██ ████████
                                                                                                ██ ████   ██ ██    ██
                                                                                                ██ ██ ██  ██ ██    ██
                                                                                                ██ ██  ██ ██ ██    ██
                                                                                                ██ ██   ████ ██    ██
*/

void menu_init(void){
  char* message; PRE_IO = RENDERER = menu_render; DEFAULT = "usplash ready";
  if ( MOPT && ( !MOPT_SWROOT || !MOPT_DAEMON ) ){
         if ( MOPT_ACTION && MOPT_TITLE ) asprintf(&message,"%s\n%s\n%s\n%s",MOPT,MOPT_KEY,MOPT_TITLE,MOPT_ACTION);
    else if ( MOPT_ACTION               ) asprintf(&message,"%s\n%s\n%s",    MOPT,MOPT_KEY,MOPT_ACTION);
    else if ( MOPT_TITLE                ) asprintf(&message,"%s\n%s\n%s",    MOPT,MOPT_KEY,MOPT_TITLE);
    else if ( MOPT_KEY                  ) asprintf(&message,"%s\n%s",        MOPT,MOPT_KEY);
    else message = MOPT;
    rpc_send(message,strlen(message)); exit(0); }
  UC_DIRTY = true; INPUT_LEN = 1; CUR = TOP = 0; VIEW[0] = DEFAULT; VIEW_SCROLL =  true;
  MENU_ITEM   = umap_create();
  MENU_ACTION = umap_create();
  MENU_WIDGET = umap_create();
  MENU_VIEW_SELECTED = (char*)  malloc( 1024 * sizeof(char));
  ioBind map[] = {
    {"CTRL_Q",rescue_shell},{"CTRL_V",toggle_verbose},{"CTRL_D",toggle_debug},
    {"SIGWINCH",menu_resize},{"SIGUSR1",rescue_shell},
    {"log",menu_log},{"widget",menu_widget},{"switchroot",switchroot},
    {"UP",menu_up},{"PGUP",menu_home},{"HOME",menu_home},{"DOWN",menu_down},{"PGDN",menu_end},{"END",menu_end},
    {"LEFT",menu_left},{"RIGHT",menu_right},{"ENTER",menu_run_selected},
    {"set",menu_set},{"del",menu_del},{"end",menu_quit},{"run",menu_run},{0,0}};
  io_bind(map);
  rpc_init(); }


/*
                                                                                         ███    ███  █████  ██ ███    ██
                                                                                         ████  ████ ██   ██ ██ ████   ██
                                                                                         ██ ████ ██ ███████ ██ ██ ██  ██
                                                                                         ██  ██  ██ ██   ██ ██ ██  ██ ██
                                                                                         ██      ██ ██   ██ ██ ██   ████
*/

static char* PREEXEC = NULL;
static struct option long_options[] = {
   {"menu",       no_argument,       0, 'm'},
   {"item",       required_argument, 0, 'i'},
   {"title",      required_argument, 0, 't'},
   {"action",     required_argument, 0, 'a'},
   {"run",        optional_argument, 0, 'r'},
   {"delete",     required_argument, 0, 'd'},
   {"quit",       optional_argument, 0, 'q'},
   {"switchroot", required_argument, 0, 'S'},
   {"widget",     required_argument, 0, 'w'},
   {"log",        required_argument, 0, 'l'},
   {0,0,0,0}};

#define _update_tile() \
  TILE_COLS = MIN(utf8ansi_cols(TILE_LEFT),utf8ansi_cols(TILE_RIGHT))

int main(int argc, char *argv[]){
  int c, idx;
  while(1){
    idx = 0;
    c = getopt_long (argc, argv, ":a:Dd:e:F:GhH:Ii:kl:L:mq::r::R:S:t:UVw:", long_options, &idx);
    if (c == -1) break;
    switch (c){
      case 'h': usplash_help(NULL);                                                              exit(0);
      case 'I': usplash_info(NULL);                                                              exit(0);
      case 'G': usplash_license(NULL);                                                           exit(0);
      case 'U': usplash_usage(NULL);                                                             exit(0);
      case 'V': VERBOSE = true;                                                                    break;
      case 'D': VERBOSE = DEBUG = true;                                                            break;
      case 'v': DEFAULT       = strdup(optarg);                                                    break;
      case 'e': PREEXEC       = strdup(optarg);                                                    break;
      case 'H': TILE_HEADER   = strdup(optarg);                                                    break;
      case 'F': TILE_FOOTER   = strdup(optarg);                                                    break;
      case 'L': TILE_LEFT     = strdup(optarg); TILE_LEFT_BYTES  = strlen(optarg); _update_tile(); break;
      case 'R': TILE_RIGHT    = strdup(optarg); TILE_RIGHT_BYTES = strlen(optarg); _update_tile(); break;
      case 't': MOPT_TITLE  = optarg;                                                              break;
      case 'a': MOPT_ACTION = optarg;                                                              break;
      case 'm': MOPT_DAEMON = true;                                                                break;
      case 'k': MOPT = "dmesg";                                                                    break;
      case 'w': MOPT = "widget";        MOPT_KEY = optarg;                                         break;
      case 'l': MOPT = "log";           MOPT_KEY = optarg;                                         break;
      case 'q': MOPT = "end";           MOPT_KEY = optarg;                                         break;
      case 'i': MOPT = "set";           MOPT_KEY = optarg;                                         break;
      case 'd': MOPT = "del";           MOPT_KEY = optarg;                                         break;
      case 'r': MOPT = "run";           MOPT_KEY = optarg;                                         break;
      case 'S': MOPT = "switchroot";    MOPT_KEY = MOPT_NEWROOT = optarg; MOPT_SWROOT = true;      break;
      case '?': break; default: abort(); }}
  if ( !MOPT_DAEMON ) if ( !MOPT ) {
    usplash_help(NULL); exit(1); }
  VIEW_init();
  io_init();
  menu_init();
  tty_init();
  kbd_init();
  if ( PREEXEC ) io_shell( PREEXEC );
  if ( MOPT_SWROOT && MOPT_DAEMON ) switchroot(NULL);
  io_main(); }
