/*
                                                                                                 ██   ██ ██████  ██████
                                                                                                 ██  ██  ██   ██ ██   ██
                                                                                                 █████   ██████  ██   ██
                                                                                                 ██  ██  ██   ██ ██   ██
                                                                                                 ██   ██ ██████  ██████
*/

typedef struct ioKey  { uint32_t code; char* name;  } ioKey;

static ioKey IO_KEY_CODE[] = {
  { 0x01, "CTRL_A" }, { 0x02, "CTRL_B" }, { 0x03, "CTRL_C" },
  { 0x04, "CTRL_D" }, { 0x06, "CTRL_F" }, { 0x07, "CTRL_G" },
  { 0x08, "CTRL_H" }, { 0x09, "TAB" },    { 0x11, "CTRL_Q" }, { 0x13, "CTRL_S" },
  { 0x16, "CTRL_V" }, { 0x0d, "ENTER" },  { 0x20, "SPACE" },  { 0x7f, "BACKSPACE" },
  { 0x1b5b317e, "UNKNOWN" }, { 0x1b5b327e, "INSERT" }, { 0x1b5b337e, "DELETE" },
  { 0x1b5b347e, "UNKNOWN" }, { 0x1b5b357e, "PGUP"   }, { 0x1b5b367e, "PGDN" },
  { 0x1b5b48,   "HOME"    }, { 0x1b5b46,   "END"    }, { 0x1b5b41,   "UP" },
  { 0x1b5b42,   "DOWN"    }, { 0x1b5b43,   "RIGHT"  }, { 0x1b5b44,   "LEFT" },
  { 0,0 }};

static        umap* IO_KEYMAP;
static unsigned int UKBD_OFFSET = 0;
static         char UKBD_KEY[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static      ioEvent UKBD = {0, NULL, NULL, NULL, false,  NULL, NULL,  NULL,  NULL,  NULL, "KEY", "UNKNOWN", "\x00", 1};

#define kbd_retry(){ \
  UKBD_OFFSET = 0; memset(UKBD_KEY,0,16); continue; }
#define kbd_emit_and_retry(){ \
  keyName = umap_getI(IO_KEYMAP,keyCode); \
  UKBD.name = keyName? keyName : "KEY"; \
  io_emit(&UKBD); kbd_retry() }

IOHANDLER(kbd_read){
  int bytes; char* keyName; int i; uint64_t keyCode = 0; char key;
  for (;;){
    bytes = read(TTYFD, UKBD_KEY + UKBD_OFFSET, 1);
    if  ( bytes ==  0 ) _fatal("kbd_closed");
    if (( bytes == -1 ) || ( bytes != 1 )) return 0;
    UKBD_OFFSET += bytes;
    key = UKBD_KEY[UKBD_OFFSET-1];
    keyCode = 0; for ( i=0; i<UKBD_OFFSET; i++ ) keyCode = ( keyCode << 8ull ) | UKBD_KEY[i];
    if ( 1 == UKBD_OFFSET ) if ( UKBD_KEY[0] != 0x1b ) kbd_emit_and_retry() else continue;
    if ( ! ( UKBD_KEY[0] == 0x1b && UKBD_KEY[1] == 0x5b ) ){
      UKBD.name = "ESCAPE"; io_emit(&UKBD);
      UKBD.name = "[";      io_emit(&UKBD); kbd_retry(); }
    keyName = umap_getI(IO_KEYMAP,keyCode);
    switch (key){
    case 0x52: tty_readsize(UKBD_KEY+1); kbd_retry();
    case 0x00: case 0x1b: case 0x7e: case 0x3f:  kbd_emit_and_retry();
    default: if( ( 0x40 < key && key < 0x5b ) || ( 0x60 < key && key < 0x7b ) ||
      ( 16  == UKBD_OFFSET ) ) kbd_emit_and_retry() }}}

static ioEvent* TTY_EVENT;
void kbd_init_evt(ioHandler write, ioHandler end){
  int i; char* key;
  UKBD.data = UKBD_KEY;
  IO_KEYMAP = umap_create();
  for(i=32;i<127;i++){
    asprintf(&key,"%c",i);
    umap_setI(IO_KEYMAP,i,key,(free_f)NULL); }
  for(i=0; IO_KEY_CODE[i].code!=0; i++)
    umap_setI(IO_KEYMAP,IO_KEY_CODE[i].code,IO_KEY_CODE[i].name,free);
  umap_delI(IO_KEYMAP,0x1b,free);
  io_register(TTYFD,0,NULL,kbd_read,write,end);
  TTY_EVENT = umap_getI(IO_EVENTMAP,TTYFD); }

void kbd_init(){ kbd_init_evt(NULL,NULL); }
