/*
                                                                                                 ██████  ██████   ██████
                                                                                                 ██   ██ ██   ██ ██
                                                                                                 ██████  ██████  ██
                                                                                                 ██   ██ ██      ██
                                                                                                 ██   ██ ██       ██████
*/

#ifndef SOCKET_NAME
#define SOCKET_NAME "TASKD_SOCK"
#endif

typedef struct ioRPC {
  int fd;
  char* in;
  char* out;
  ioEvent* evt;
  unsigned long int length;
  unsigned long int out_length;
  unsigned long int out_pos;
} ioRPC;

static int UNIXFD;
static umap* RPC_CLIENT;
static ioEvent URPC = {0, NULL, NULL, NULL, false,  NULL, NULL,  NULL,  NULL,  NULL, "RPC", NULL, "\x00", 1};
static bool RPC_DEFER = false;

#define RPC(name) ioRPC* name = (ioRPC*) e->ptr;

IOHANDLER(rpc_autostart){
  FILE *output; char self[128]; // ULOG("starting daemon\n");
  if (!( readlink("/proc/self/exe", self, 128) )) _fatal("readlink error");
  if (!( output = popen(self,"r")              )) _fatal("popen error"); }

int rpc_close(ioRPC* rpc){
  UDBG("\e[34mRPC\e[0m CLOSE fd=%i\n",rpc->fd);
  io_close_fd(rpc->fd); }

IOHANDLER(rpc_free){ RPC(r);
  UDBG("\e[34mRPC\e[0m FREE fd=%i\n",e->fd);
  if (r->in)  free(r->in);
  if (r->out) free(r->out);
  umap_delI(RPC_CLIENT,r->fd,free);
  io_unregister(e); }

IOHANDLER(rpc_wait_close){
  e->closed = true;
  UDBG("\e[34mRPC\e[0m \e[32mDONE\e[0m fd=%i\n",e->fd); }

IOHANDLER(rpc_error_close){
  e->closed = true;
  UDBG("\e[34mRPC\e[0m \e[31mERROR\e[0m fd=%i err=%i %s\n",e->fd,errno,strerror(errno)); }

IOHANDLER(rpc_reply_end, char* data, int len){
  RPC_DEFER = true;
  int l; l = ( len == 0 ) ? strlen(data) : len;
  UDBG("\e[34mRPC\e[0m \e[35mREPLY\e[0m fd=%i l=%i len=%i '%s'\n",e->fd,l,len,data);
  io_change(e,0,rpc_wait_close,rpc_wait_close,rpc_wait_close);
  write(e->fd, data, l);
  write(e->fd, "\0", 1);
  return -1; }

IOHANDLER(rpc_reply, char* data, int len){
  RPC_DEFER = true;
  int l; l = ( len == 0 ) ? strlen(data) : len;
  write(e->fd, data, l);
  UDBG("\e[34mRPC\e[0m REPLY fd=%i l=%i len=%i '%s'\n",e->fd,l,len,data);
  return -1; }

IOHANDLER(rpc_reply_done){
  io_change(e,0,rpc_wait_close,rpc_wait_close,rpc_wait_close);
  return rpc_reply(e, "\0", 1); }

IOHANDLER(rpc_reply_error){
  io_change(e,0,rpc_wait_close,rpc_wait_close,rpc_wait_close);
  return rpc_reply(e, "\1", 1); }

IOHANDLER(rpc_read){ RPC(rpc);
  bool success = false;
  int  fd = e->fd, bytes;
  if ( RPC_CLIENT->count == 0 ) return 0;
  TRY_READ:
  bytes = recv( fd, rpc->in + rpc->length, 1024, 0);
  if ( -1 == bytes ) {
    if ( errno == 11 ) goto TRY_EMIT;
    UDBG("\e[34mRPC\e[0m READ %i %s\n",errno,strerror(errno));
    rpc_close(rpc); }
  else if ( 0 == bytes ) {
    if ( !success ){
      UDBG("\e[34mRPC\e[0m READ \e[31mERROR\e[0m fd=%i err=-1 Empty read\n",fd);
      rpc_error_close(e); return 0; }
    goto TRY_EMIT; }
  else { success = true;
    UDBG("\e[34mRPC\e[0m READ fd=%i rpc=%llx bytes=%i\n",fd,rpc,bytes);
    rpc->length += bytes;
    rpc->in = (char*) realloc(rpc->in, rpc->length + 1025);
    goto TRY_READ; }
  TRY_EMIT:
  UDBG("\e[34mRPC\e[0m EMIT fd=%i rpc=%llx bytes=%i\n",fd,rpc,bytes);
  RPC_DEFER = false;
  if ( rpc->length == 0 ) { rpc_close(rpc); return 0; };
  rpc->in = (char*) realloc(rpc->in, rpc->length + 2);
  rpc->in[rpc->length+1] = '\0';
  e->name = rpc->in;
  e->data = str_next(rpc->in,'\n');
  e->type = "RPC";
  e->length = strlen(rpc->in);
  int result = io_emit(e);
  rpc->length = 0;
       if ( result == 0 ) { rpc_reply_done(e);  return -1; }
  else if ( result  > 0 ) { rpc_reply_error(e); return -1; }
  if ( ! RPC_DEFER ) { rpc_close(rpc); return 1; }
  return -1; }

IOHANDLER(rpc_accept){
  int fd, bytes; ioRPC* rpc;
  struct sockaddr_un local, remote;
  TRY_ACCEPT:
  fd = accept(UNIXFD,0,0);
  if ( fd == -1 ) {
    if ( 11 != errno ) UDBG("\e[34mRPC\e[0m ACCEPT \e[31mERROR\e[0m err=%i %s",errno,strerror(errno));
    UDBG("\e[34mRPC\e[0m ACCEPT DONE\n");
    return 1; }
  UDBG("\e[34mRPC\e[0m ACCEPT fd=%i\n",fd);
  rpc = (ioRPC*) malloc(sizeof(ioRPC));
  rpc->fd = fd;
  rpc->length = 0;
  rpc->in = (char*) malloc(1024); rpc->out = NULL;
  rpc->evt = io_register(fd,0,rpc,rpc_read,NULL,rpc_error_close);
  rpc->evt->free = rpc_free;
  umap_setI(RPC_CLIENT,fd,rpc,(free_f)NULL);
  goto TRY_ACCEPT; }

int rpc_send(char* buf, int len){
  size_t bytes=0;
  int fd, written = 0;
  char *socket_path = getenv(SOCKET_NAME); if (!socket_path) socket_path = "./socket";
  if( -1 == ( fd = io_connect_unix_stream(socket_path))){ UDBG("ERROR RPC CONNECT\n"); exit(1); }
  TRY_WRITE:
  bytes = write(fd,buf+written,len);
  if ( 0 < bytes ){
    written += bytes;
    if ( written < len ) goto TRY_WRITE; }
  else if ( -1 == bytes ){
    ULOG("WRITE ERROR: %i %s\n",errno,strerror(errno)); exit(1); }
  write(fd,"\0",1);
  char rbuf[1024]; bytes = 0;
  TRY_READ:
  bytes = recv(fd,rbuf,1024,0); // hexdump(rbuf,bytes);
  if ( 0 == bytes ) exit(0);
  else if ( 0 < bytes ){
    if ( rbuf[0] == '\0' ) exit(0);
    if ( rbuf[0] == '\1' ) exit(1);
    write(STDERR_FILENO,rbuf,bytes);
    for (int i = 0; i < bytes; i++){
      if ( rbuf[i] == '\0' ) exit(0);
      if ( rbuf[i] == '\1' ) exit(1); }}
  goto TRY_READ; }

void rpc_init(void){
  struct sockaddr_un addr;
  char *socket_path = getenv(SOCKET_NAME); if (!socket_path) socket_path = "./socket";
  if ((UNIXFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)             _fatal("[socket] error");
  memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  unlink(socket_path);
  if (bind(UNIXFD, (struct sockaddr*)&addr, sizeof(addr)) == -1)    _fatal("[socket] bind error");
  if (listen(UNIXFD, 128) == -1)                                    _fatal("[socket] listen error");
  RPC_CLIENT  = umap_create();
  rpc_autostart_ptr = rpc_autostart;
  io_register(UNIXFD,0,NULL,rpc_accept,NULL,NULL); }
