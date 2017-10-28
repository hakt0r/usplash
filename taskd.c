static char const* LICENSE[] = {
  "  taskd.c - process manager",
  "",
  "  Copyright (C) 2014  Sebastian Glaser ( anx@ulzq.de )",
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
#include <pwd.h>
static char *E_BASE = "/etc/task.d";
static char *E_USER = "unknown";
extern int mkdir(const char*,mode_t);

/*                                                          ###   ###  ########     ##     ####   ##
                                                            ## # # ##  ##    ##     ##     ## ##  ##
                                                            ## ### ##  ########     ##     ## ##  ##
                                                            ##  #  ##  ##    ##     ##     ##  ## ##
                                                            ##     ##  ##    ##     ##     ##   ##*/

static umap *TASK;

typedef struct Task {
  char *name;
  char *dir;
  char *run; struct stat statRun;
  char *log;
  char *err;
  pid_t pid;
  int lofd;
  int lefd;
  bool exists;
  ioProcess* process;
} Task;

bool read_env(){
  register struct passwd *pw; register uid_t uid;
  uid = geteuid();
  pw = getpwuid(uid);
  if (!pw) _fatal("getpwuid failed!\n");
  DEBUG  = false; if ( getenv("DEBUG")   != NULL )   DEBUG = true;
  VERBOSE = true; if ( getenv("VERBOSE") != NULL ) VERBOSE = true;
  E_USER = pw->pw_name;
  asprintf(&E_BASE,"/home/%s/.config/task",E_USER);
  mkdir(E_BASE,0755); }

void  task_free_ptr(Task *t);
Task* task_open(char* name, char* cmd){
  Task* t = NULL;
  if ( NULL != ( t = (Task*) umap_get(TASK,name) )){
    return t; }
  else {
    t = (Task*) malloc(sizeof(Task));
    asprintf(&t->name,"%s",name);
    asprintf(&t->dir,"%s/%s",E_BASE,name);
    asprintf(&t->run,"%s/%s",t->dir,"run");
    asprintf(&t->log,"%s/%s",t->dir,"log");
    asprintf(&t->err,"%s/%s",t->dir,"err"); }

  int status = stat(t->run, &t->statRun);
  t->exists = cmd || ( 0 < strlen(cmd) ) ? true : false;
  if ( !t->exists && ( 0 > status ) ) goto return_free_and_null;

  UDBG("\e[34mTASK\e[0m OPEN name=%s t->name=%s\n",name,t->name);
  mkdir(t->dir,0755);
  umap_set(TASK,t->name,t,NULL);
  UDBG("\e[34mTASK\e[0m NEW name=%s(%llx) t->name=%s(%llx) dir=%s\n",name,&name,t->name,&t->name,t->dir);
  return t;
  return_free_and_null: task_free_ptr(t); return NULL; }

void task_free_ptr(Task *t){
  char *name = t->name;
  UDBG("\e[34mTASK\e[0m \e[31mFREE_PTR\e[0m name=%s fd=%i\n",t->name,t->ofd);
  free(t->log); free(t->err);
  free(t->dir); free(t->run);
  umap_del(TASK,name,free);
  free(name); }

void task_close_ptr(Task *t){
  UDBG("\e[34mTASK\e[0m \e[31mCLOSE_PTR\e[0m name=%s fd=%i\n",t->name,t->ofd);
  io_kill_recursive(t->pid,9);
  UDBG("[\e[32m%s\e[0m] \e[31mkilled\e[0m\n",t->name); }

void task_close(char *name){
  Task* t = NULL; if ( NULL != ( t = (Task*) umap_get(TASK,name) )) task_close_ptr(t); }


#define TASK(name)\
  if ( e == NULL ) { e = IO_EVENT; UDBG("STILL_HAPPENS_1"); } \
  Task* name = (Task*) e->ptr;

IOHANDLER(td_closed){ TASK(t);
  UDBG("TASK FD_CLOSED fd=%i\n",e->fd); }

IOHANDLER(td_child_closed){
  UDBG("TASK CHILD_CLOSED pid=%i\n",IO_CHILD_PID); }

IOHANDLER(td_log){ TASK(t);
  char buf[1024];
  char* name = t->name;
  int bytes = 0, total = 0, fd = e->fd;
  while ( 0 < ( bytes = read( fd, buf, 1024 ))){
    total += bytes;
    buf[bytes] = '\0';
    UVRB("[\e[32m%s\e[0m] \e[32mlog\e[0m %s",t->name,buf);
    write(t->lofd,&buf,bytes); }
  if ( bytes == 0 ) { td_closed(IO_EVENT); }
  total += bytes;
  // if ( bytes == -1 ){ return -1; }
  return total; }

IOHANDLER(td_end){
  if ( e == NULL ) e = IO_EVENT;
  char *name = e->data; char *rest = str_next(name,'\n');
  Task *task = (Task*) umap_get(TASK,name);
  if ( NULL == task ){ rpc_reply_error(e); return -1; }
  int r = kill(task->pid,SIGQUIT);
  UDBG("\e[34mTASK\e[0m \e[31mEND\e[0m name=%s ptr=\e[32m%llx\e[0m r=%i\n",name,task,r);
  r = kill(task->pid,SIGQUIT);
  if ( NULL != task ){ task_close_ptr(task); }
  rpc_reply_done(e); }

IOHANDLER(td_kill){
  if ( e == NULL ) e = IO_EVENT;
  char *name = e->data; char *rest = str_next(name,'\n');
  Task *task = (Task*) umap_get(TASK,name);
  if ( NULL == task ){ rpc_reply_error(e); return -1; }
  int r = kill(task->pid,SIGKILL);
  UDBG("\e[34mTASK\e[0m \e[31mEND\e[0m name=%s ptr=\e[32m%llx\e[0m r=%i\n",name,task,r);
  r = kill(task->pid,SIGQUIT);
  if ( NULL != task ){ task_close_ptr(task); }
  rpc_reply_done(e); }

IOHANDLER(td_zap){
  if ( e == NULL ) e = IO_EVENT;
  char *name = e->data; char *rest = str_next(name,'\n');
  Task *task = (Task*) umap_get(TASK,name);
  if ( NULL != task ){
    UDBG("\e[34mTASK\e[0m \e[31mZAP\e[0m name=%s ptr=\e[32m%llx\e[0m\n",name,task);
    io_kill_recursive(task->pid,SIGKILL);
    // kill(task->pid,SIGHUP);
    // killpg(getpgid(task->pid),SIGHUP);
    // task_close_ptr(task); task_free_ptr(task);
    rpc_reply_done(e); }
  else rpc_reply_error(e); }

IOHANDLER(td_set){
  char *name = e->data;              if (!name) { rpc_reply_error(e); return 0; }
  char *file = str_next(name,'\n');  if (file==name) { rpc_reply_error(e); return 0; }
  char *cmd  = str_next(file,'\n');  if (cmd==name ) { rpc_reply_error(e); return 0; }
  Task *task = task_open(name,cmd);  if (!task) { rpc_reply_error(e); return 0; }
  char *path; asprintf(&path, "%s/%s",task->dir,file);
  int r = io_write_file_sync(path,cmd);
  free(path);
  if ( -1 == r ){ rpc_reply_error(e); }
  else rpc_reply_done(e); }

IOHANDLER(td_get){
  char *name = e->data;              if (!name) { rpc_reply_error(e); return 0; }
  char *file = str_next(name,'\n');  if (file==name) { rpc_reply_error(e); return 0; }
  char *rest = str_next(file,'\n');  if (rest==file) { rpc_reply_error(e); return 0; }
  Task *task = task_open(name,NULL); if (!task) { rpc_reply_error(e); return 0; }
  char *path; asprintf(&path, "%s/%s",task->dir,file);
  UDBG("TD GET name=%s key=%s path=%s\n",name,file,path);
  char *data = io_read_file_sync(path,1024); free(path);
  if ( !data ) { rpc_reply_error(e); return 0; }
  int len = strlen(data);
  rpc_reply(e,data,len); //free(data); free(path);
  rpc_reply_done(e); }

IOHANDLER(td_all){
  char* name; Task* t; upair* p;
  umap_each(TASK,rec,next,i){
    p = rec->value; name = p->key; t = (Task*) p->value;
    write(e->fd,name,strlen(name));
    write(e->fd,"\t",1);
    if ( t->pid > 0 ) {
      write(e->fd,"\e[32mrunning\e[0m",16); }
    else write(e->fd,"\e[31mstopped\e[0m",16);
    write(e->fd,"\n",1); }
  rpc_reply_done(e); }

IOHANDLER(td_run){ // for(int i=0;i<strlen(cmd);i++) if (cmd[i]=='\n') cmd[i] = ' ';
  static FILE *output;

  char *name = &e->data[0];          if ( !name ){ rpc_reply_error(e); return -1; }
  char *cmd  = str_next(name,'\n');
  Task *task = task_open(name,cmd);  if ( !task ){ rpc_reply_error(e); return -2; }

  char *c[] = {"/bin/sh","-c",cmd,NULL}; int cmdc = 3;

  if ( cmd && 0 < strlen(cmd) ) {
    io_write_file_sync(task->run,cmd);
    c[2] = cmd; c[3] = NULL; cmdc = 4;
    UDBG("\e[34mTASK \e[33mRUN:CMD\e[0m %s '%s' (%i bytes)\n",name,cmd,strlen(cmd)); }
  else if ( task->statRun.st_mode & S_IXUSR ) {
    c[0] = task->run; c[1] = NULL; c[2] = NULL; c[3] = NULL; cmdc = 1;
    UDBG("\e[34mTASK \e[35mRUN:EXE\e[0m %s [%s]\n",name,task->run); }
  else {
    c[0] = "/bin/sh"; c[1] = task->run; c[2] = NULL; c[3] = NULL; cmdc = 2;
    UDBG("\e[34mTASK \e[33mRUN:SHL\e[0m %s [%s]\n",name,task->run); }

  if( access( task->log, F_OK ) != -1 )
       task->lofd = open(task->log,O_WRONLY|O_APPEND,0666);
  else task->lofd = creat(task->log,0666);
  if( access( task->err, F_OK ) != -1 )
       task->lefd = open(task->err,O_WRONLY|O_APPEND,0666);
  else task->lefd = creat(task->err,0666);

  ioProcess* p = ioProcess_new(c,cmdc,td_log,td_log,NULL,td_closed);
  task->pid = p->pid;
  task->process = p;
  rpc_reply_done(e); }

IOHANDLER(td_help){
  UDBG("\e[34mTASK\e[0m \e[33mHELP\e[0m\n");
  char* help = "\n\
  DEBUG= VERBOSE= taskd [ACTION] ([ARG] [ARG] ...)\n\
    all\n\
    run [PATH]\n\
    end [PATH]\n\
    zap [PATH]\n\
    get [PATH]\n\
    set [PATH] [VALUE]\n\n";
  rpc_reply_end(e,help,144); }

IOHANDLER(switchroot);
int td_daemon(int argc, int argp, char *argv[]){
  rpc_init();
  umap_set(IO_EVENTMAP,"switchroot",(void*) &switchroot,NULL);
  umap_set(IO_EVENTMAP,"set",(void*) &td_set,NULL);
  umap_set(IO_EVENTMAP,"get",(void*) &td_get,NULL);
  umap_set(IO_EVENTMAP,"run",(void*) &td_run,NULL);
  umap_set(IO_EVENTMAP,"all",(void*) &td_all,NULL);
  umap_set(IO_EVENTMAP,"end",(void*) &td_end,NULL);
  umap_set(IO_EVENTMAP,"zap",(void*) &td_zap,NULL);
  umap_set(IO_EVENTMAP,"kill",(void*) &td_kill,NULL);
  umap_set(IO_EVENTMAP,"help",(void*) &td_help,NULL);
  TASK = umap_create();
  io_main(); }

IOHANDLER(switchroot){
  char *newroot, *postscript; struct stat st; struct statfs stfs; dev_t rootdev;
  postscript = str_next(newroot = e->data,'\n');
  // Change to new root directory and verify it's a different fs
  chdir(newroot);  stat("/", &st); rootdev = st.st_dev; stat(".", &st);
  if ( st.st_dev == rootdev || getpid() != 1 ){
    ULOG("new_root(%s) must be a mountpoint; we must be PID 1",newroot); return -1; }
  if (mount(".", "/", NULL, MS_MOVE, NULL)) { // For example, fails when newroot is not a mountpoint
    ULOG("error moving root"); return -1; }
  chroot("."); chdir("/");                  // The chdir is needed to recalculate "." and ".." links
  ULOG("chroot_complete: %s",postscript);
  if (postscript) io_exec(postscript,NULL,0); }

int main(int argc, char *argv[]){
  CommandFn cmd = NULL;
  read_env();
  io_init();
  if ( argc == 1 ) td_daemon(argc,1,argv);
  else if ( NULL != ( cmd = (CommandFn) umap_get(IO_EVENTMAP,argv[1]) ))
    cmd(argc,1,argv);
  int length = 0, l = 0, i, p = 0;
  char *message = (char*) malloc(1024);
  char *arg, *pos = message;
  for (i=1;i<argc;i++){
    arg = argv[i];
    l = strlen(arg);
    p = length + l + 1;
    message = (char*) realloc(message, p+1);
    memcpy( message + length, arg, l );
    message[p-1] = '\n';
    pos = (char*) message + p;
    length = p; }
  message = (char*) realloc(message, length + 1);
  message[length-1] = '\n';
  message[length] = '\0';
  // printf(">SEND> %i\n%s\n@@@",length,message);
  rpc_send(message,length); }

// write(1,"Usage: taskd [cmd] [args...]\n  run/set/end\n",46); exit(1);
