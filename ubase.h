extern int isprint(int c);

void* memdup(const void* mem, size_t size) {
  void* out = malloc(size); if(out == NULL) return NULL; return memcpy(out, mem, size); }

typedef struct ASSOC { void* a; void* b; } ASSOC;
ASSOC* assoc_dup(ASSOC* a){ memdup(a,sizeof(ASSOC)); }
ASSOC* assoc_new(ASSOC* a){ return malloc(sizeof(ASSOC)); }
#define ASSOCIATION(label,va,vb) ASSOC* (label) = malloc(sizeof(ASSOC)); (label)->a = (va); (label)->b = (vb);

// Math: MIN, MAX
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

// bool # FIXME: debug atom's gcc linter
#ifndef LINTER
typedef void* bool;
#define true  (void*) 0x01
#define false (void*) NULL
#endif

// callback masks
typedef void (*voidFn)    (void);
typedef int  (*CommandFn) (int argc, int argp, char *argv[]);

static  int  INFD = STDIN_FILENO, OUTFD = STDOUT_FILENO, ERRFD = STDERR_FILENO, TTYFD = STDERR_FILENO;
static bool  DEBUG      = false ;
static bool  VERBOSE    = false ;
static char *STDIO_PATH = NULL  ;
static char *LOG_LINE;

#ifndef ULOG
#define ULOG(args...) { int len = asprintf(&LOG_LINE,##args); write(2,LOG_LINE,len); }
#endif

#ifndef UVRB
#define UVRB(args...) { if( VERBOSE || DEBUG ) { int len = asprintf(&LOG_LINE,##args); write(2,LOG_LINE,len); } }
#endif

#ifndef UDBG
#define UDBG(args...) { if( DEBUG ) { int len = asprintf(&LOG_LINE,##args); write(2,LOG_LINE,len); } }
#endif

#ifndef _FN_FATAL
#define _FN_FATAL
void _fatal(char const*message){
  write(1,"ERROR ",6);
  write(1,message,strlen(message));
  write(1,"\n",1);
  // exit(1);
}
#define UASSERT(state,args...) if ( !(state) ) { int len = asprintf(&LOG_LINE,##args); write(2,LOG_LINE,len); } // exit(1)
#endif

void hexdump(void *mem, unsigned int length){
	char  line[80];
	char *src = (char*)mem;
	unsigned i;
	int j;
	// printf("\x1b[1;37mdumping %u bytes from %p\x1b[0m\r\n\x1b[1;36m       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F    0123456789ABCDEF\x1b[0m\r\n", length, src);
	for (i=0; i<length; i+=16, src+=16){
		char *t = line;t += sprintf(t, "%04x:  ", i);
		for(j=0; j<16; j++){
			if (i+j < length)t += sprintf(t, "%02X", src[j] & 0xff);
			else t += sprintf(t, "  ");t+=sprintf(t, j%2 ? " " : " ");}
		t+=sprintf(t, "  ");
		for(j=0; j<16; j++){
			if(i+j<length){
				if(isprint((unsigned char)src[j]))t+=sprintf(t, "%c", src[j]);
				else t += sprintf(t, ".");}
			else{t+=sprintf(t, " ");}}
	t+=sprintf(t, "\r\n");
	// printf("%s", line);
}}
