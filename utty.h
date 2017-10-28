#include <termios.h>

/*
                                                                                              ████████ ████████ ██    ██
                                                                                                 ██       ██     ██  ██
                                                                                                 ██       ██      ████
                                                                                                 ██       ██       ██
                                                                                                 ██       ██       ██
     Based on an example by Wolf Holzmann
     see: http://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c */

static struct termios orig_termios;
static   unsigned int COLS = 80;
static   unsigned int ROWS = 24;

int tty_pollsize(void);

int tty_reset(void){
  if (tcsetattr(TTYFD,TCSAFLUSH,&orig_termios) < 0) return -1;
  write(TTYFD,"\e[?1049l",8);
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
  if (tcsetattr(TTYFD,TCSAFLUSH,&raw) < 0) _fatal("can't set raw mode");
  write(TTYFD,"\e[?1049h",8); }

#include <errno.h>

void tty_reopen(){
  int fd = open(STDIO_PATH, O_RDWR|O_NONBLOCK);
  if ( fd == -1 ){
    printf("failed to reopen: %s error: %s\n",STDIO_PATH, strerror(errno)); sleep(5); exit(1); }
  close(0);     close(1);   close(2);
  dup2(fd,0); dup2(fd,1); dup2(fd,2); if ( fd > 2 ) close(fd);
  tty_raw(0); write(TTYFD,"\e[?25l",6);
  ioEvent* i;
  if ( NULL != ( i = umap_getI(IO_EVENTMAP,TTYFD) )){
  epEvent* e = i->evt;
  int r = epoll_ctl( IO_EPOLL, EPOLL_CTL_DEL, TTYFD, NULL);
  int f = fcntl(TTYFD, F_GETFL, 0);
      f = fcntl(TTYFD, F_SETFL, f | O_NONBLOCK | FD_CLOEXEC);
  if ( -1 == epoll_ctl(IO_EPOLL, EPOLL_CTL_ADD, TTYFD, e)){
    printf("ERROR IO REGISTER %i %s\n",errno,strerror(errno)); sleep(5); exit(1); }}}

void tty_getdev(){
  STDIO_PATH = NULL;
  struct stat sb; char *buf; ssize_t nbytes, bufsiz;
  if (lstat("/proc/self/fd/0", &sb) == -1) goto fail;
  bufsiz = sb.st_size + 1;
  if (sb.st_size == 0) bufsiz = PATH_MAX;
  buf = malloc(bufsiz); if (buf == NULL) goto fail;
  nbytes = readlink("/proc/self/fd/0", buf, bufsiz);
  if (nbytes == -1) { free(buf); goto fail; }
  buf[nbytes] = '\0';
  STDIO_PATH = buf;
  fail: if ( STDIO_PATH == NULL ) STDIO_PATH = "/dev/console"; }

void tty_init(){
  tty_getdev();
  if ( !isatty(TTYFD)                      ) _fatal("not on a tty");
  if (  tcgetattr(TTYFD,&orig_termios) < 0 ) _fatal("can't get tty settings");
  if (  atexit(tty_atexit) != 0            ) _fatal("atexit: can't register tty reset");
  tty_raw(0); write(TTYFD,"\e[?25l",6); }

void tty_readsize_kernel(void){
  struct winsize w = {0,0,0,0};
  if ( -1 != ( ioctl(TTYFD, TIOCGWINSZ, &w) )){
    COLS = w.ws_col; ROWS = w.ws_row; }
  else tty_pollsize(); }

// arcane and deprecated; yet still usefull for debugging and fallback
int tty_pollsize(void){ fwrite("\e[24h\e[?25l\e[s\e[99999999C\e[9999999B\e[6n",39,1,stderr); }
int tty_readsize(char* buffer){
  int rows, cols = 0; sscanf(buffer,"[%i;%iR",&rows,&cols); struct winsize ws = {rows, cols, 0, 0};
  if (ioctl(TTYFD, TIOCSWINSZ, &ws) < 0) { UDBG("TTY READSIZE FAILED"); return 0; } ROWS = rows;
  COLS = cols; fwrite("\e[u\e[?25h",9,1,stderr); return 1; }
