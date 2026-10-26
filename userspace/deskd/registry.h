/* Session-owned storage. Records never move; pointer/scratch tables may grow.
 * Kept separate from the pure scene/widget layer, which borrows these arrays. */
#ifndef DESKD_STORAGE_ALLOC
static void *desktop_storage_alloc(size_t bytes) {
    void *p=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    return p==MAP_FAILED?NULL:p;
}
static void desktop_storage_free(void *p,size_t bytes) { if(p)munmap(p,bytes); }
#define DESKD_STORAGE_ALLOC desktop_storage_alloc
#define DESKD_STORAGE_FREE desktop_storage_free
#endif
static int registry_reserve(struct session *s,size_t bytes) {
    if(!s->dynamic || s->registry_bytes>s->registry_budget || bytes>s->registry_budget-s->registry_bytes)return 0;
    s->registry_bytes+=bytes;
    if(s->registry_bytes>s->registry_peak)s->registry_peak=s->registry_bytes;
    return 1;
}
static void registry_free(struct session *s,void *p,size_t bytes) {
    if(p) { DESKD_STORAGE_FREE(p,bytes);s->registry_bytes-=bytes; }
}
static int windows_grow(struct session *s) {
    unsigned old=session_capacity(s),capacity=old*2;
    /* Widget row geometry and all byte arithmetic must remain representable.
     * Normal admission hits the metadata budget long before this ABI bound. */
    if(capacity<old || capacity>DESK_LIST_MAX_ITEMS-DESK_APP_COUNT ||
       !s->scene.menu.items || !s->scene.launcher.items)return 0;
    size_t menu_count=capacity+DESK_MENU_ITEMS,palette_count=capacity+DESK_APP_COUNT;
    if(s->scene.menu.count>menu_count || s->scene.launcher.count>palette_count)return 0;
    size_t table_bytes=capacity*(sizeof(struct window_record *)+sizeof(struct desktop_window *)+sizeof(int))+
        2*menu_count*sizeof(struct desk_menu_item)+2*palette_count*sizeof(struct desk_palette_item)+palette_count*sizeof(struct desk_list_item);
    size_t chunk_bytes=sizeof(struct window_chunk)+(capacity-old)*sizeof(struct window_record);
    if(!registry_reserve(s,table_bytes+chunk_bytes))return 0;
    void *table=DESKD_STORAGE_ALLOC(table_bytes);
    struct window_chunk *chunk=table?DESKD_STORAGE_ALLOC(chunk_bytes):NULL;
    if(!chunk) {
        if(table)DESKD_STORAGE_FREE(table,table_bytes);
        s->registry_bytes-=table_bytes+chunk_bytes;return 0;
    }
    memset(table,0,table_bytes);memset(chunk,0,chunk_bytes);
    struct window_record **records=table;
    struct desktop_window **scene=(void *)(records+capacity);
    int *order=(void *)(scene+capacity);
    struct desk_menu_item *menu=(void *)(order+capacity),*menu_build=menu+menu_count;
    struct desk_palette_item *palette=(void *)(menu_build+menu_count),*palette_build=palette+palette_count;
    struct desk_list_item *visible=(void *)(palette_build+palette_count);
    for(unsigned i=0;i<old;i++) {
        records[i]=i<DESKD_WINDOWS?NULL:s->records[i];scene[i]=desktop_window_at(&s->scene,i);
    }
    for(unsigned i=old;i<capacity;i++) { records[i]=&chunk->records[i-old];scene[i]=&records[i]->window; }
    chunk->bytes=chunk_bytes;chunk->next=s->window_chunks;
    /* Capacities above cover all existing snapshots. These binding operations
     * cannot allocate/fail under the checked invariants, and cancel stale presses. */
    (void)desk_menu_bind_storage(&s->scene.menu,menu,menu_count);
    (void)desk_palette_bind_storage(&s->scene.launcher,palette,visible,palette_count);
    void *old_table=s->window_table;size_t old_bytes=s->window_table_bytes;
    s->records=records;s->window_chunks=chunk;s->window_table=table;s->window_table_bytes=table_bytes;
    s->scene.records=scene;s->scene.capacity=capacity;s->scene.order=order;
    s->menu_build=menu_build;s->palette_build=palette_build;
    registry_free(s,old_table,old_bytes);return 1;
}
static int children_grow(struct session *s) {
    unsigned old=children_capacity(s),capacity=old*2;
    if(capacity<old || capacity>INT32_MAX/2)return 0;
    size_t table_bytes=capacity*sizeof(struct child_record *);
    size_t chunk_bytes=sizeof(struct child_chunk)+(capacity-old)*sizeof(struct child_record);
    if(!registry_reserve(s,table_bytes+chunk_bytes))return 0;
    struct child_record **table=DESKD_STORAGE_ALLOC(table_bytes);
    struct child_chunk *chunk=table?DESKD_STORAGE_ALLOC(chunk_bytes):NULL;
    if(!chunk) {
        if(table)DESKD_STORAGE_FREE(table,table_bytes);
        s->registry_bytes-=table_bytes+chunk_bytes;return 0;
    }
    memset(table,0,table_bytes);memset(chunk,0,chunk_bytes);
    for(unsigned i=DESKD_WINDOWS;i<old;i++)table[i]=s->child_records[i];
    for(unsigned i=old;i<capacity;i++)table[i]=&chunk->records[i-old];
    chunk->bytes=chunk_bytes;chunk->next=s->child_chunks;
    void *old_table=s->child_records;size_t old_bytes=s->child_table_bytes;
    s->child_records=table;s->child_chunks=chunk;s->child_table_bytes=table_bytes;s->child_capacity=capacity;
    registry_free(s,old_table,old_bytes);return 1;
}
static int child_slot(struct session *s) {
    for(int i=0;i<children_capacity(s);i++)if(!*session_children(s,i) && !life_reserved(s->worker,i))return i;
    int i=children_capacity(s);return children_grow(s)?i:-1;
}
/* Cache/clipboard retirement and child reaping precede releasing these records. */
static void registry_destroy(struct session *s) {
    while(s->window_chunks) {
        struct window_chunk *chunk=s->window_chunks;s->window_chunks=chunk->next;registry_free(s,chunk,chunk->bytes);
    }
    while(s->child_chunks) {
        struct child_chunk *chunk=s->child_chunks;s->child_chunks=chunk->next;registry_free(s,chunk,chunk->bytes);
    }
    registry_free(s,s->window_table,s->window_table_bytes);registry_free(s,s->child_records,s->child_table_bytes);
    s->window_table=NULL;s->records=NULL;s->child_records=NULL;s->scene.records=NULL;s->scene.order=NULL;
    s->window_table_bytes=s->child_table_bytes=0;s->scene.capacity=s->child_capacity=0;
    s->menu_build=NULL;s->palette_build=NULL;
    desk_menu_init(&s->scene.menu);desk_palette_init(&s->scene.launcher);
}
