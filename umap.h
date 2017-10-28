static char* LICENSE_UMAP[] = {
  "  umap.h - light general purpose map",
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

#define NULL_HASH 0x0
typedef void (*free_f)(void*);

              typedef uint32_t    uh32;
struct ubuf;  typedef struct ubuf ubuf;
struct umap;  typedef struct umap umap;
struct urec;  typedef struct urec urec;
struct upair; typedef struct upair upair;

inline uh32   UFNV32a      (char *str);

inline upair* upair_create(char* key, void* value);
inline void   upair_free(upair* p);

inline ubuf*  ubuf_create  (char* data,int len);
inline ubuf*  ubuf_append  (ubuf* p, char* data, int len);

inline umap*  umap_create  (void);
inline void   umap_destroy (umap* map, free_f vfree);
inline void   umap_free    (umap* map, urec* rec, free_f vfree);
inline urec*  umap_lookupI (umap* map, uh32 hash);
inline urec*  umap_setI    (umap* map, uh32 hash, void* value, free_f vfree);
inline void*  umap_getI    (umap* map, uh32 hash);
inline void   umap_delI    (umap* map, uh32 hash, free_f vfree);
inline urec*  umap_lookup  (umap* map, char* key, uh32* gethash);
inline void   umap_set     (umap* map, char* key, void* value, free_f vfree);
inline void*  umap_get     (umap* map, char* key);
inline void   umap_del     (umap* map, char* key, free_f vfree);
inline void   umap_join    (urec* prev, urec* next);
inline void   umap_insert  (urec* prev, urec* item, urec* next);

/*
                                                                     ██    ██ ███████ ███    ██ ██    ██ ██████  ██████
                                                                     ██    ██ ██      ████   ██ ██    ██      ██      ██
                                                                     ██    ██ █████   ██ ██  ██ ██    ██  █████   █████
                                                                     ██    ██ ██      ██  ██ ██  ██  ██       ██ ██
                                                                      ██████  ██      ██   ████   ████   ██████  ███████
  Placed into public domain by :chongo: <Landon C. Noll>
  Released as is. Many thanks.
*/

uh32 UFNV32a(char *str){
  unsigned char *s = (unsigned char *) str; uh32 hash = 0x811c9dc5;
  while (*s){
    hash ^= (uh32)*s++;
    hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24); }
  return hash; }

/*
                                                                                     ██    ██ ██████   █████  ██ ██████
                                                                                     ██    ██ ██   ██ ██   ██ ██ ██   ██
                                                                                     ██    ██ ██████  ███████ ██ ██████
                                                                                     ██    ██ ██      ██   ██ ██ ██   ██
                                                                                      ██████  ██      ██   ██ ██ ██   ██
*/

struct upair {
  void* value;
  char* key;
};

upair* upair_create(char* key, void* value){
  upair* i = (upair*) malloc(sizeof(upair));
  i->key = key;
  i->value = value;
  return i; }

void upair_free(upair* p){
  if ( p ){ if (p->key) free(p->key);
    if (p->value) free(p->value); }}

  /*
                                                                                         ██    ██ ██████  ██    ██ ███████
                                                                                         ██    ██ ██   ██ ██    ██ ██
                                                                                         ██    ██ ██████  ██    ██ █████
                                                                                         ██    ██ ██   ██ ██    ██ ██
                                                                                          ██████  ██████   ██████  ██
  */

struct ubuf{
  int len;
  int total;
  ubuf* next;
  char* data; };

ubuf* ubuf_create(char* data,int len){
  ubuf* b = (ubuf*) malloc(sizeof(ubuf));
  b->len = len;
  b->total = len;
  b->data = data;
  return b; }

ubuf* ubuf_append(ubuf* p, char* data, int len){
  ubuf* b = ubuf_create(data,len);
  b->len = len;
  p->next = b;
  b->data = data;
  b->total += len;
  return b; }

/*
                                                                                     ██    ██ ███    ███  █████  ██████
                                                                                     ██    ██ ████  ████ ██   ██ ██   ██
                                                                                     ██    ██ ██ ████ ██ ███████ ██████
                                                                                     ██    ██ ██  ██  ██ ██   ██ ██
                                                                                      ██████  ██      ██ ██   ██ ██
*/

struct umap {
  urec* first;
  urec* last;
  uint64_t count;
};

struct urec {
  uh32 hash;
  char* key;
  void* value;
  urec* prev;
  urec* next;
};

#define umap_each(map,rec,nxt,i) \
  int i; urec *rec,*nxt; if ( map->first ) for ( \
    i=0,rec=map->first,nxt=map->first->next; \
    i<map->count && NULL!=nxt; \
    i++,rec=nxt,nxt=nxt->next )

#define umap_each_sorted(map,rec,nxt,i) \
  int i; urec *rec,*nxt; if ( map->first ) for ( \
    i=0,rec=map->first,nxt=map->first->sort; \
    i<map->count && NULL!=nxt; \
    i++,rec=nxt,nxt=nxt->sort )

int umap_keysort(const void *a, const void *b){
  const urec *ar = *(urec**)a, *br = *(urec**)b;
  return strcmp(ar->key,br->key); }

void** umap_sort(umap *map){
  if ( !map->first      ) return;
  if (  map->count == 0 ) return;
  int i; urec *rec, *nxt;
  urec **o = malloc((map->count+1)*sizeof(urec*));
  umap_each(map,ra,na,ia){
    o[ia] = ra; }
  qsort(o,map->count,sizeof(urec*),umap_keysort);
  return o; }

#define umap_dump(map){ urec* __rec; \
  umap_each(map,__rec,__next,__dump) if ( __rec = __rec->value ) \
  UDBG("[@%llx:%s(#%llx)] @%llx\n",__rec->key,__rec->key,__rec->hash,__rec->value); }

#define umap_dumpI(map){ urec* _rec; \
  umap_each(map,__rec,__next,__dump) UDBG("[%llx] %llx\n",__rec->hash,__rec->value); }

void umap_insert(urec* prev, urec* item, urec* next){
  next->prev = prev->next = item;
  item->prev = prev; item->next = next; }

void umap_join(urec* prev, urec* next){
  next->prev = prev; prev->next = next; }




umap* umap_create(void){
  umap* map = (umap*) malloc(sizeof(umap));
  map->count = 0; map->first = map->last = 0; return map; }

void umap_destroy(umap* map,free_f vfree){
  int i = 0, c = map->count; urec *rec = NULL, *next = NULL;
  if ( map->count == 0 ) return;
  map->last->next; rec = map->first;
  while ( i++ < c ) {
    next = rec->next;
    umap_free(map,rec,vfree);
    rec = next; }
  free(map); }

void umap_free(umap* map, urec* rec, free_f vfree){
  if ( !(map&&rec) ) return;
  if ( map->count == 1 ) map->first = map->last = NULL;
  else {
    umap_join(rec->prev, rec->next);
    if ( map->first == rec ) map->first = rec->next;
    if ( map->last  == rec ) map->last  = rec->prev; }
  if (rec->value && vfree) { vfree(rec->value); rec->value = NULL; }
  if (rec->key) { free(rec->key); rec->key = NULL; }
  free(rec);
  map->count--; }




urec* umap_lookupI(umap* map, uh32 hash){ int i,l; urec* rec = NULL;
  if ( !map ) _fatal("umap_lookupI: empty map");
  if (( map->count == 0 ) || ( hash < map->first->hash ) || ( map->last->hash < hash  )) return NULL;
  for (i=0,l=map->count,rec=map->first; i<l; i++, rec=rec->next)if ( hash == rec->hash ) return rec;
  return NULL; }

void* umap_getI(umap* map, uh32 hash){
  if ( !map ) _fatal("umap_getI: empty map");
  urec* rec = umap_lookupI(map,hash);
  return rec ? rec->value : NULL; }

urec* umap_setI(umap* map, uh32 hash, void* value, free_f vfree){ int i,l; urec *rec,*r;
  if ( !map ) _fatal("umap_setI: empty map");
  rec = umap_lookupI(map, hash);
  if ( rec ){
    if ( vfree ) vfree( rec->value );
    rec->value = value; return rec; }
  rec = (urec*) malloc(sizeof(urec));
  rec->hash  = hash; rec->key = NULL;
  rec->value = value;
       if ( 0 == map->count ){ rec->next = rec->prev = map->first = map->last = rec; }
  else if ( map->last->hash < hash ){  umap_insert(map->last,rec,map->first); map->last  = rec; }
  else if ( map->first->hash > hash ){ umap_insert(map->last,rec,map->first); map->first = rec; }
  else for ( i=0,l=map->count,r=map->first; i<l; i++,r=r->next )
       if ( hash < r->hash ){ umap_insert(r->prev,rec,r); break; }
  //UDBG("SETI map=%llx hash=%llx value=%llx\n",map,hash,value); umap_dumpI(map);
  map->count++; return rec; }

void umap_delI(umap* map, uh32 hash, free_f vfree){
  if ( map->count == 0 ) return;
  urec* rec = umap_lookupI(map,hash);
  umap_free(map,rec,vfree); }




urec* umap_lookup(umap* map, char* key, uh32* gethash){ urec* rec; uh32 hash = NULL_HASH;
  int i, l;
  if ( !map ) _fatal("umap_lookup: empty map");
  if ( !key ) return NULL;
  hash = UFNV32a(key); if ( gethash ) *gethash = hash;
  rec  = umap_lookupI(map,hash);
  if ( rec )
    for ( i=0, l=map->count; i<l; i++, rec = rec->next ){
      if ( ! rec )                      return NULL;
      if ( ! rec->key )                 return NULL;
      if ( 0 == strcmp(key,rec->key) )  return rec;
      if ( hash != rec->next->hash )    return NULL; }
  return NULL; }

void* umap_get(umap* map,char* key){
  if ( !map ) _fatal("umap_get: empty map");
  urec* rec = umap_lookup(map,key,NULL); //UDBG("GET key=%s %llx\n",key,rec ? rec->value : NULL);
  return rec ? rec->value : NULL; }

void umap_set(umap* map, char* key, void* value, free_f vfree){
  if ( !map ) _fatal("umap_set: empty map"); //UDBG("SET %s %llx\n",key,value);
  uh32 hash = NULL_HASH; urec* rec;
  if (( rec = umap_lookup( map, key, &hash ) )){
    if ( rec->value && vfree ) vfree(rec->value);
    rec->value = value; }
  else if (( rec = umap_setI(map,hash,value,vfree) )){
    rec->key = strdup(key); }}

void umap_del(umap* map, char* key, free_f vfree ){
  uh32 hash; urec* rec;
  if (!( rec = umap_lookup(map,key,&hash) )) return;
  umap_free(map, rec, vfree); }
