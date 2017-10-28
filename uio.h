#include <errno.h>
#include <string.h>
#include <wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

/*
                                                                         ██   ██ ███████  █████  ██████  ███████ ██████
                                                                         ██   ██ ██      ██   ██ ██   ██ ██      ██   ██
                                                                         ███████ █████   ███████ ██   ██ █████   ██████
                                                                         ██   ██ ██      ██   ██ ██   ██ ██      ██   ██
                                                                         ██   ██ ███████ ██   ██ ██████  ███████ ██   ██
*/

struct ioEvent;
struct ioProcess;
struct ioBind;

typedef struct epoll_event epEvent;
typedef struct ioEvent     ioEvent;
typedef struct ioProcess   ioProcess;
typedef struct ioBind      ioBind;

typedef int (*ioHandler)(ioEvent* e);
//* ret int      : [ -1 == defer < 0 == ok < 1 == error ]
//* arg ioEvent* : ioEvent associated with the triggering fd

#define IOHANDLER(name,args...) int (name)(ioEvent* e,##args)
#define IOHANDLER_PTR(name,args...) int (*name)(ioEvent* e,##args)
#define PROCHANDLER(name) int (name)(ioProcess* p)
#define PROCHANDLER_PTR(name) int (*name)(ioProcess* e)

// io_init
void io_init();
// io_register
ioEvent* io_register(int, unsigned int, void*, ioHandler, ioHandler, ioHandler);
void io_change(ioEvent*, unsigned int, ioHandler, ioHandler, ioHandler);
void io_wantwrite(ioEvent*, ioHandler);
int io_close_fd(int);
int io_close(ioEvent*);
IOHANDLER(io_donewriting);
IOHANDLER(io_unregister);
// io_process
typedef struct ioProcess ioProcess;
ioProcess* ioProcess_new(char**, int, ioHandler, ioHandler, ioHandler,ioHandler);
IOHANDLER(ioProcess_free);
void io_wait_child(pid_t, ioHandler, void*);
void io_wait_child_release(pid_t);
IOHANDLER(ioProcess_child_closed);
IOHANDLER(ioProcess_log);
// io_emit - events
IOHANDLER(io_emit);
IOHANDLER(io_sigread);
IOHANDLER(io_emit_close);
IOHANDLER(io_emit_error);
// utils
void io_exec(char* cmd, ioHandler callback, void* ref);
int io_write_file_sync(char *path, char* data);
int io_append_file_sync(char *path, char* data);
char* io_read_file_sync(char *path,int s);

void io_kill_recursive(pid_t pid, int s);
  IOHANDLER(io_kill_recursive_end);

int unblock (int);

struct ioEvent {
        int fd;
      void* ptr;
   epEvent* evt;
   ioEvent* next;
       bool closed;
  ioHandler read;
  ioHandler write;
  ioHandler close;
  ioHandler error;
  ioHandler free;
      char* type;
      char* name;
      char* data;
        int length;
};

struct ioBind {
        char* name;
        void* value;
};

/*
                                                                            ███████ ████████  █████  ████████ ██  ██████
                                                                            ██         ██    ██   ██    ██    ██ ██
                                                                            ███████    ██    ███████    ██    ██ ██
                                                                                 ██    ██    ██   ██    ██    ██ ██
                                                                            ███████    ██    ██   ██    ██    ██  ██████
*/

static       int IO_EPOLL        = 0;
static       int IO_MAXEVENTS    = 23;
static     umap* IO_EVENTMAP     = NULL;
static     umap* IO_CHILD        = NULL;
static     void* IO_CHILD_REF    = NULL;
static     void* IO_CHILD_STATUS = NULL;
static       int IO_CHILD_PID    = 0;
static       int IO_TIMERFD      = 0;
static       int IO_SIGNALFD     = 0;
static    voidFn PRE_IO          = NULL;
static ioEvent** IO_EVENTS       = NULL;
static  ioEvent* IO_EVENT_COUNT  = 0;
static  ioEvent* IO_EVENT        = NULL;
static  epEvent* IO_EPOLL_EVENT  = NULL;
static   ioEvent IO_CLOSE_EVENT  = {0, NULL, NULL, NULL, false, NULL, NULL, NULL, NULL, NULL, "IO", "EPOLL:CLOSE", NULL, 8};

static  sigset_t IO_SIGSET       = {};
static       int IO_SIGMAP[]     = {SIGWINCH,SIGQUIT,SIGUSR1,SIGUSR2,SIGPIPE,SIGCHLD,-1};
static     char* SIGNAME[]       = {"INVALID","SIGHUP","SIGINT","SIGQUIT","SIGILL","SIGTRAP","SIGABRT","SIGBUS","SIGFPE",
                                    "SIGKILL","SIGUSR1","SIGSEGV","SIGUSR2","SIGPIPE","SIGALRM","SIGTERM","SIGSTKFLT",
                                    "SIGCHLD","SIGCONT","SIGSTOP","SIGTSTP","SIGTTIN","SIGTTOU","SIGURG","SIGXCPU",
                                    "SIGXFSZ","SIGVTALRM","SIGPROF","SIGWINCH","SIGPOLL","SIGPWR","SIGSYS",NULL};

static      char IO_SPARSE[1024];

static IOHANDLER_PTR(rpc_autostart_ptr);

/*
                                                                            ██  ██████          ██ ███    ██ ██ ████████
                                                                            ██ ██    ██         ██ ████   ██ ██    ██
                                                                            ██ ██    ██         ██ ██ ██  ██ ██    ██
                                                                            ██ ██    ██         ██ ██  ██ ██ ██    ██
                                                                            ██  ██████  ███████ ██ ██   ████ ██    ██
*/

void io_init(){
  IO_EPOLL    = epoll_create(1);
  IO_EVENTMAP = umap_create();
  IO_CHILD    = umap_create();
  if ( 0 != sigemptyset(&IO_SIGSET)) _fatal("ERROR IO SIG_EMPTY_SET");
  int i,sig; for ( sig=IO_SIGMAP[i=0]; sig!=-1; sig=IO_SIGMAP[++i] ){
  if (  0 != sigaddset(&IO_SIGSET,sig) ) _fatal("can't catch SIG\n"); }
  if (  0 != sigprocmask(SIG_BLOCK,&IO_SIGSET,NULL)) _fatal("ERROR IO SIGPROCMASK");
  UASSERT( -1 != (IO_SIGNALFD = signalfd(-1, &IO_SIGSET, 0)),"ERROR IO SIGNALFD\n");
  io_register(IO_SIGNALFD,0,(void*)0x1,io_sigread,NULL,NULL); }

/*
                                        ██  ██████          ██████  ███████  ██████  ██ ███████ ████████ ███████ ██████
                                        ██ ██    ██         ██   ██ ██      ██       ██ ██         ██    ██      ██   ██
                                        ██ ██    ██         ██████  █████   ██   ███ ██ ███████    ██    █████   ██████
                                        ██ ██    ██         ██   ██ ██      ██    ██ ██      ██    ██    ██      ██   ██
                                        ██  ██████  ███████ ██   ██ ███████  ██████  ██ ███████    ██    ███████ ██   ██
*/

ioEvent* io_register(int fd, unsigned int mode, void* ref, ioHandler readfn, ioHandler writefn, ioHandler closefn){
  ioEvent *r = (ioEvent*) malloc(sizeof(ioEvent));
  epEvent *e = (epEvent*) malloc(sizeof(epEvent));
  r->closed = 0; r->error = NULL;
  r->fd = fd; r->ptr = ref; r->read = readfn; r->write = writefn; r->close = closefn; r->evt = e;
  r->free = &io_unregister;
  e->data.ptr = (void*) r; e->events = mode;
  if ( NULL != readfn  ) e->events = e->events | EPOLLIN;
  if ( NULL != writefn ) e->events = e->events | EPOLLOUT | EPOLLET;
  if ( NULL != closefn ) e->events = e->events | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP;
  UDBG("\e[33mIO\e[0m REGISTER fd=%i ptr=%llx evt=%lli read=%llx write=%llx close=%llx\n",
    r->fd, r, e->events, readfn, writefn, closefn );
  umap_setI(IO_EVENTMAP,fd,r,(free_f)NULL);
  int f = fcntl(fd, F_GETFL, 0); if ( f == -1 ) goto fail; f = fcntl(fd, F_SETFL, f | O_NONBLOCK | FD_CLOEXEC);
  UASSERT ( -1 != epoll_ctl(IO_EPOLL, EPOLL_CTL_ADD, fd, r->evt),"ERROR IO REGISTER %i %s\n",errno,strerror(errno));
  return r;
  fail:
  free(r); free(e);
  return NULL; }

void io_change(ioEvent *r, unsigned int mode, ioHandler readfn, ioHandler writefn, ioHandler closefn){
  int fd = r->fd; epEvent* e = r->evt;
  r->read = readfn; r->write = writefn; r->close = closefn; e->events = mode;
  if ( NULL != readfn  ) e->events = e->events | EPOLLIN;
  if ( NULL != writefn ) e->events = e->events | EPOLLOUT;
  if ( NULL != closefn ) e->events = e->events | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP;
  UDBG("\e[33mIO\e[0m CHANGE fd=%i ptr=%llx evt=%lli read=%llx write=%llx close=%llx\n",r->fd,r,e->events,readfn,writefn,closefn);
  if ( -1 == epoll_ctl(IO_EPOLL, EPOLL_CTL_MOD, fd, e)) { UDBG("ERROR IO CHANGE %i %s\n",errno,strerror(errno)); exit(1); }}

IOHANDLER(io_unregister){
  int fd = e->fd;
  if ( fd > -1 && fd < 3 ){
    ULOG("\e[33mIO\e[0m FD %i closed: reopen '%s'",fd,STDIO_PATH);
    tty_reopen(); return 1; }
  //UASSERT( fd != 0, "FATAL IO CLOSE fd=0\n");
  e->evt->data.ptr = NULL;
  UDBG("\e[33mIO\e[0m UNREGISTER fd=%i\n",fd);
  int r = epoll_ctl( IO_EPOLL, EPOLL_CTL_DEL, fd, NULL);
  if ( -1 == r ){
    UDBG("\e[33mIO\e[0m ERROR FULLUNREGISTER failed fd=%i err=%i %s\n",fd,errno,strerror(errno)); }
  else { UDBG("\e[31mIO\e[0m \e[32mUNREGISTER\e[0m \e[32msuccess\e[0m fd=%i\n",fd); }
  close(fd); // PROVEME
  IO_CLOSE_EVENT.fd = fd; io_emit(&IO_CLOSE_EVENT);
  umap_delI(IO_EVENTMAP,fd,NULL);
  free(e->evt);
  free(e);
  UDBG("\e[33mIO\e[0m \x1b[31mCLOSED\x1b[0m fd=%i\n",fd); }

void io_wantwrite(ioEvent* r, ioHandler fn){
  epEvent* e = r->evt;
  r->write = fn; e->events = e->events | EPOLLOUT | EPOLLET;
  UASSERT ( -1 != epoll_ctl(IO_EPOLL, EPOLL_CTL_MOD, r->fd, r->evt),"ERROR IO REGISTER %i %s\n",errno,strerror(errno)); }

IOHANDLER(io_donewriting){
  epEvent* ep = e->evt;
  e->write = NULL; ep->events = ep->events ^ EPOLLOUT ^ EPOLLET;
  UASSERT ( -1 != epoll_ctl(IO_EPOLL, EPOLL_CTL_MOD, e->fd, e->evt),"ERROR IO REGISTER %i %s\n",errno,strerror(errno)); }

int io_close(ioEvent* e){
  UDBG("\e[33mIO\e[0m CLOSE fd=%i\n",e->fd);
  e->closed = true;
  return close(e->fd); }

int io_close_fd(int fd){
  UASSERT( fd != 0, "FATAL IO CLOSE fd=0\n"); UDBG("\e[33mIO\e[0m CLOSE FD fd=%i\n",fd);
  ioEvent* r = (ioEvent*) umap_getI(IO_EVENTMAP,fd);
  if (r) return io_close(r);
  return close(fd); }

/*
                                                                      ██  ██████          ███████ ███    ███ ██ ████████
                                                                      ██ ██    ██         ██      ████  ████ ██    ██
                                                                      ██ ██    ██         █████   ██ ████ ██ ██    ██
                                                                      ██ ██    ██         ██      ██  ██  ██ ██    ██
                                                                      ██  ██████  ███████ ███████ ██      ██ ██    ██
*/

IOHANDLER(io_emit){
  ioHandler        callback = (ioHandler) umap_get(IO_EVENTMAP,e->name);
  if ( !callback ) callback = (ioHandler) umap_get(IO_EVENTMAP,e->type);
  if ( VERBOSE )
    if      ( "IO"  == e->type ) { UDBG("\e[33mIO\e[0m EMIT \e[0m\e[1;32mIO\e[0m '%s' data=0x%llx callback=%llx\e[0m\n", e->name, (void*)e->data, callback); }
    else if ( "SIG" == e->type ) { UDBG("\e[33mIO\e[0m EMIT \e[0m\e[1;32mSIG\e[0m name=%s sig=%i callback=%llx\e[0m\n", e->name, IO_CHILD_PID, callback); }
    else if ( "KEY" == e->type ) { char *k = e->data; UDBG("\e[33mIO\e[0m EMIT \e[0m\e[1;32mKEY\e[0m '%s' 0x%02x%02x%02x%02x%02x%02x%02x%02x [%llx] \e[0m\n", e->name, k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7], callback); }
    else                         { UDBG("\e[33mIO\e[0m EMIT type=%s name=%s args=%llx\n", e->type, e->name, (void*)e->data ); }
  if ( !callback && VERBOSE ) { UVRB("ERROR IO EMIT fd=%i err=-1 No callback for: %s\n",e->fd,e->name); return -1; }
  if ( callback ) return callback(e);
  return 1; }

IOHANDLER(io_emit_close){
  ioHandler callback;
  if ( NULL != ( callback = e->close )){
    UDBG("\e[33mIO\e[0m CALL CLOSE fd=%i cb=%llx\n",e->fd,callback);
    callback(e); }
  else { io_close(e); }
}

IOHANDLER(io_emit_error){
  if ( NULL != e->error ){
    ioHandler callback = e->error;
    UDBG("\e[33mIO\e[0m CALL ERROR fd=%i cb=%llx\n",e->fd,callback);
    callback(e); }
  else {
    int err = errno;
    if (err == 11) return -1; // try again
    UDBG("\e[33mIO\e[0m ERROR fd=%i errno=%i: %s\n",e->fd,err,strerror(err));
    io_close(e);
}}


/*
                                            ██  ██████          ██████  ██████   ██████   ██████ ███████ ███████ ███████
                                            ██ ██    ██         ██   ██ ██   ██ ██    ██ ██      ██      ██      ██
                                            ██ ██    ██         ██████  ██████  ██    ██ ██      █████   ███████ ███████
                                            ██ ██    ██         ██      ██   ██ ██    ██ ██      ██           ██      ██
                                            ██  ██████  ███████ ██      ██   ██  ██████   ██████ ███████ ███████ ███████
*/

#define READ 0
#define WRITE 1

typedef struct ioProcess {
      pid_t pid;
        int ifd;
        int ofd;
        int efd;
   ioEvent* in;
   ioEvent* out;
   ioEvent* err;
     char** cmd;
        int cmdc;
  ioHandler close;
} ioProcess;

ioProcess* ioProcess_new(char** cmd_in, int cmdc_in, ioHandler read, ioHandler write, ioHandler error,ioHandler onclose){
  pid_t pid = 0; int fail = 0, in[2], out[2], err[2];
  ioProcess* p = (ioProcess*) malloc(sizeof(ioProcess));

  // Prepare command array to be passed to execvp
  char** cmd = p->cmd = (char**) malloc(cmdc_in*sizeof(char*));
  size_t cc = cmdc_in-1;
  for (size_t i = 0; i < cc; i++)
    cmd[i] = strdup(cmd_in[i]);
  cmd[ p->cmdc = cc ] = NULL;

  // Prepare io pipes [stdin/stderr/stdout]
  if ( pipe(in) != 0 || pipe(out) != 0 || pipe(err) != 0 ) goto fail;

  // Fork into parent and child process
  if ( 0 > ( pid = p->pid = fork() )) goto fail;

  // Child
  if ( 0 == pid ){
    close(in[WRITE]); dup2(in[READ],   STDIN_FILENO);
    close(out[READ]); dup2(out[WRITE], STDOUT_FILENO);
    close(err[READ]); dup2(err[WRITE], STDERR_FILENO);
    execvp(cmd[0], (char*const*)cmd); perror("execvp failed"); exit(1); }

  // Parent
  close(in[READ]);   p->in  = io_register(p->ifd = in[WRITE], 0, p, NULL,write,NULL);
  close(out[WRITE]); p->out = io_register(p->ofd = out[READ], 0, p, read,NULL,NULL);
  close(err[WRITE]); p->err = io_register(p->efd = err[READ], 0, p, error,NULL,NULL);
  p->close = onclose;
  p->in->free = p->out->free = p->err->free = NULL; // disable free method in favour of sigchild signaling
  io_wait_child( p->pid, ioProcess_child_closed, p );

  UDBG("\e[34mioProcess\e[0m \e[31mOPEN\e[0m ifd=%i ofd=%i efd=%i: cmd[%i]=%s %s %s\n",
    p->ifd,p->ofd,p->efd,cmdc_in,p->cmd[0],p->cmd[1],p->cmd[2]);
  return p;
  fail:
  for (size_t i = 0; i < cmdc_in-1; i++) free(cmd[i]);
  free(p); free(cmd);
  return NULL; }

IOHANDLER(ioProcess_free){
  ioProcess* p = e->ptr;
  UDBG("\e[34mioProcess\e[0m \e[31mCLOSE\e[0m ifd=%i ofd=%i efd=%i\n",p->ifd,p->ofd,p->efd);
  io_wait_child_release(p->pid);
  // io_close(p->in); io_close(p->out); io_close(p->err);
  UDBG("\e[32mTASK\e[0m \e[31mclosed\e[0m\n");
  io_unregister(p->in);
  io_unregister(p->out);
  io_unregister(p->err);
  IOHANDLER_PTR(onclose) = (ioHandler) p->close;
  if ( onclose ) onclose(e);
  for (size_t i = 0; i < p->cmdc; i++) free(p->cmd[i]);
  free(p->cmd); free(p); return 1; }

IOHANDLER(ioProcess_child_closed){
  // UDBG("\e[32mTASK\e[0m \e[31mchild closed\e[0m\n");
  ioProcess* p = (ioProcess*) IO_CHILD_REF;
  if ( p ) return ioProcess_free(p->in);
  else {
    UDBG("ioProcess unknown child closed: %i\n",IO_CHILD_PID);
    return -1; }}

IOHANDLER(ioProcess_log){
  char buf[1024];
  ioProcess *task = (ioProcess*) e->ptr;
  int bytes = 0, total = 0, fd = e->fd;
  while ( 0 < ( bytes = read( fd, buf, 1024 ))){
    total += bytes;
    buf[bytes] = '\0';
    ULOG("\e[32mTASK\e[0m \e[32mLOG\e[0m %s\n",buf); }
  // if ( bytes == 0 ) { ioProcess_closed(IO_EVENT->evt->ptr); }
  total += bytes;
  return total; }

void io_wait_child(pid_t pid, ioHandler callback, void* ref){
  ASSOCIATION( a, callback, ref ); umap_setI(IO_CHILD,pid,a,(free_f)NULL); }

void io_wait_child_release(pid_t pid){
  umap_delI(IO_CHILD,pid,free); }

ioProcess* io_shell(char* cmd){
  UDBG("\e[32mTASK\e[0m run %s\n",cmd);
  static char *c[] = {"/bin/sh","-c",NULL,NULL}; c[2] = cmd;
  return ioProcess_new(c,4,ioProcess_log,ioProcess_log,NULL,NULL); }

void io_exec(char* cmd, ioHandler callback, void* ref){
  char *c[] = {"/bin/sh","-c",cmd,NULL};
  ioProcess_new(c,4,ioProcess_log,ioProcess_log,NULL,callback); }

void io_kill_recursive(pid_t pid, int s){
  char* cmd; asprintf(&cmd,"pkill -TERM -P %lli",pid);
  UDBG("IO KILL_RECURSIVE pid=%lli '%s'\n",pid,cmd);
  io_exec(cmd,NULL,0); free(cmd); }

IOHANDLER(io_kill_recursive_end){
  UDBG("IO \e[33mWAIT_CHILD_RELEASE\e[0m pid=%lli\n",IO_CHILD_PID);
  io_wait_child_release(IO_CHILD_PID);
  if (IO_CHILD_REF) free(IO_CHILD_REF);
  return 1; }

/*
                                                                     ██  ██████          ███    ███  █████  ██ ███    ██
                                                                     ██ ██    ██         ████  ████ ██   ██ ██ ████   ██
                                                                     ██ ██    ██         ██ ████ ██ ███████ ██ ██ ██  ██
                                                                     ██ ██    ██         ██  ██  ██ ██   ██ ██ ██  ██ ██
                                                                     ██  ██████  ███████ ██      ██ ██   ██ ██ ██   ████
*/

static inline void io_read_drain(int fd){
  UDBG("\e[33mIO\e[0m READ_DRAIN fd=%i\n",fd);
  int result = -1;
  while ( 0 < ( result = read( fd, &IO_SPARSE, 1024 )));
  UDBG("\e[33mIO\e[0m READ_DRAIN_FINISHED fd=%i\n",fd); }

static inline void io_write_drain(int fd){
  UDBG("\e[33mIO\e[0m WRITE_DRAIN fd=%i\n",fd);
  int result = -1;
  while ( 1 < ( result = write( fd, "\0", 1 )));
  UDBG("\e[33mIO\e[0m WRITE_DRAIN_FINISHED fd=%i\n",fd); }

static inline IOHANDLER(io_call_free){
  UDBG("\e[33mIO\e[0m CALL FREE fd=%i\n",e->fd);
  ioHandler free_func = e->free;
  if ( free_func ) free_func(e); }

void io_main(void){
  int ready, fd, j, result = -1;
  struct epoll_event *ev, evlist[IO_MAXEVENTS];
  ioHandler callback;

  READ_EVENTS:

  if (PRE_IO) PRE_IO();

  ready = epoll_wait(IO_EPOLL, evlist, IO_MAXEVENTS, -1);
  if ( errno == EINTR ) _fatal("IO EPOLL_ERROR EINTR\n");

  UDBG("\e[33mIO\e[0m \x1b[33mCYCLE\x1b[0m %i\n",ready);
  if ( 0 < ready ) for ( ev=&evlist[j=0]; j < ready; ev=&evlist[++j] ){
    UDBG("\e[33mIO\e[0m \x1b[33mEVENT\x1b[0m ref=%llx\n",ev->data.ptr);
    ioEvent* r = (ioEvent*) ev->data.ptr;

    if ( NULL != r ){
      IO_EVENT = r;
      IO_EPOLL_EVENT = ev;
      bool closing = false;

      UDBG("\e[33mIO\e[0m \e[34mEVENT\e[0m fd=%i ptr=%llx in:%i err:%i hup:%i out:%i\n",
        r->fd,r, ev->events & EPOLLIN, ev->events & EPOLLERR, 0 != ( ev->events & EPOLLHUP ) | ( ev->events & EPOLLRDHUP ), ev->events & EPOLLOUT);

      closing = 0 < ( ev->events & EPOLLHUP ) | ( ev->events & EPOLLRDHUP ) ? true : false;

      if ( ev->events & EPOLLIN ){
        result = 1;
        if ( NULL != ( callback = r->read ) ){
          UDBG("\e[33mIO\e[0m CALL READ fd=%i cb=%llx\n",r->fd,callback);
          result = callback(r); }
        else io_read_drain(r->fd);
        if ( result == -1 ) closing = true; }

      if ( ev->events & EPOLLOUT ){
        result = 1;
        if ( NULL != ( callback = r->write ) ){
          UDBG("\e[33mIO\e[0m CALL WRITE fd=%i cb=%llx\n",r->fd,callback);
          result = callback(r); }
        else io_write_drain(r->fd);
        if ( result == -1 ) closing = true; }

      UDBG("\e[33mIO\e[0m \e[34mEVENT-POST\e[0m fd=%i closed:%i hup:%i\n",r->fd,r->closed,closing);
      if ( EPOLLERR & ev->events ) io_emit_error(r);
      if (    true == closing    ) io_emit_close(r);
      if (    true == r->closed  ) io_call_free(r); }}
  goto READ_EVENTS; }

/*
                                                 ██  ██████          ███████ ██  ██████  ██████  ███████  █████  ██████
                                                 ██ ██    ██         ██      ██ ██       ██   ██ ██      ██   ██ ██   ██
                                                 ██ ██    ██         ███████ ██ ██   ███ ██████  █████   ███████ ██   ██
                                                 ██ ██    ██              ██ ██ ██    ██ ██   ██ ██      ██   ██ ██   ██
                                                 ██  ██████  ███████ ███████ ██  ██████  ██   ██ ███████ ██   ██ ██████
*/

IOHANDLER(io_sigread){
  ioHandler call; ASSOC* a; int bytes;
  struct signalfd_siginfo info;
  TRY_READ:
  // UDBG("\e[33mIO\e[0m SIGNAL READ\n");
  bytes = read(IO_SIGNALFD, &info, sizeof(info));
  if ( bytes == -1 && errno == EAGAIN ) return 0;
  UASSERT(bytes == sizeof(info),"FATAL IO SIGREAD_ODD_READ s=%i b=%i\n",sizeof(info),bytes);
  unsigned sig  = info.ssi_signo;
  unsigned user = info.ssi_uid;
  e->type = "SIG";
  e->name = SIGNAME[sig];
  // UDBG("\e[33mIO\e[0m SIGNAL %i %s\n",sig,SIGNAME[sig]);
  if ( sig == SIGCHLD ){
    pid_t pid; int status;
    // UDBG("\e[33mIO\e[0m SIGNAL SIGCHILD\n");
    while ( 0 < ( pid = waitpid(-1, &status, WNOHANG))){
      UDBG("\e[33mIO\e[0m SIGNAL SIGCHILD pid=%i\n",pid);
      IO_CHILD_REF    = NULL;
      IO_CHILD_STATUS = status;
      IO_CHILD_PID = pid;
      if ( a = umap_getI(IO_CHILD,pid) ){
        call = a->a; IO_CHILD_REF = a->b;
        if ( 1 != call(e)) io_emit(e); }
      else io_emit(e); }
    UDBG("\e[33mIO\e[0m SIGCHILD:DONE pid=%i\n",pid); }
  else {
      IO_CHILD_PID = sig;
      io_emit(e); }
  goto TRY_READ; }

/*
                                                                                    ██    ██ ████████ ██ ██      ███████
                                                                                    ██    ██    ██    ██ ██      ██
                                                                                    ██    ██    ██    ██ ██      ███████
                                                                                    ██    ██    ██    ██ ██           ██
                                                                                     ██████     ██    ██ ███████ ███████
*/

int io_write_file_sync(char *path, char* data){
  int len = strlen(data);
  // if ( DEBUG ){ printf("IO WRITE_FILE_SYNC path=%s len=%i\n",path,len); hexdump(data,len); }
  FILE* f = fopen(path,"w");
  if ( f == NULL ){
    UDBG("ERROR OPENING %s\n",path);
    return -1; }
  int res = fwrite(data,len,1,f);
  fclose(f);
  return res; }

int io_append_file_sync(char *path, char* data){
  int len = strlen(data);
  // if ( DEBUG ){ printf("IO WRITE_FILE_SYNC path=%s len=%i\n",path,len); hexdump(data,len); }
  FILE* f = fopen(path,"a");
  if ( f == NULL ){
    UDBG("ERROR OPENING %s\n",path);
    return -1; }
  int res = fwrite(data,len,1,f);
  fclose(f);
  return res; }


char* io_read_file_sync(char *path,int s){
  FILE *f; char* d = malloc(s); int b = 0;
  if ( f = fopen(path,"r") ){
    if (NULL == fgets(d,s,f)) return NULL;
    fclose(f);
    return strdup(d); }
  else return NULL; }

void io_bind_event(char *name, void *value){
  umap_set( IO_EVENTMAP, name, value, (free_f)NULL); }

void io_bind(ioBind map[]){
  for ( int i=0; map[i].name; i++ ) umap_set( IO_EVENTMAP, map[i].name, map[i].value, (free_f)NULL); }

int io_connect_unix_stream(char* socket_path){
  int fd, tries=0;
  struct sockaddr_un addr;
  if ( ( fd = socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
    if ( -1 == rpc_autostart_ptr(NULL) ) { UDBG("ERROR IO CONNECT_UNIX %i %s",errno,strerror(errno)); exit(1); }}
  memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  while ( connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
    if ( 3 == tries++ ) {
      UDBG("sock: %s\n",socket_path); return -1; }
    else sleep(0.05);
  return fd; }


int unblock (int fd){ int f;
  f = fcntl(fd, F_GETFL, 0);
  if ( f == -1 ) return -1;
  f |= O_NONBLOCK | FD_CLOEXEC;
  f = fcntl(fd, F_SETFL, f);
  // if ( f & O_RDONLY ) UDBG("fd[%i]: O_RDONLY\n",fd); if ( f & O_WRONLY ) UDBG("fd[%i]: O_WRONLY\n",fd); if ( f & O_RDWR ) UDBG("fd[%i]: O_RDWR\n",fd); if ( f & O_APPEND ) UDBG("fd[%i]: O_APPEND\n",fd); if ( f & O_CREAT ) UDBG("fd[%i]: O_CREAT\n",fd); if ( f & O_EXCL ) UDBG("fd[%i]: O_EXCL\n",fd); if ( f & O_NOCTTY ) UDBG("fd[%i]: O_NOCTTY\n",fd); if ( f & O_NONBLOCK ) UDBG("fd[%i]: O_NONBLOCK\n",fd); if ( f & O_SYNC ) UDBG("fd[%i]: O_SYNC\n",fd); if ( f & O_TRUNC ) UDBG("fd[%i]: O_TRUNC\n",fd);
  if( f == -1 ) return -1;
  return 0; }
