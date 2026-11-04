#include <srvros/installed_root.h>
#include <stddef.h>

static bool eq(const uint8_t *a, const uint8_t *b, unsigned n) {
    for (unsigned i=0; i<n; i++) if (a[i]!=b[i]) return false;
    return true;
}
static bool literal(const char *s, unsigned n, const char *v) {
    unsigned i=0; while (v[i] && i<n && s[i]==v[i]) i++;
    return i==n && !v[i];
}
static int hex(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    return -1;
}
static bool uuid(const char *s, unsigned n, uint8_t out[16]) {
    static const uint8_t order[16]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
    if (n!=36) return false;
    unsigned pos=0; bool nonzero=false;
    for (unsigned i=0; i<16; i++) {
        if (pos==8 || pos==13 || pos==18 || pos==23) { if (s[pos++]!='-') return false; }
        int a=hex(s[pos++]), b=hex(s[pos++]);
        if (a<0 || b<0) return false;
        out[order[i]]=(uint8_t)((a<<4)|b); nonzero |= a!=0 || b!=0;
    }
    return nonzero;
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint64_t le64(const uint8_t *p) { return le32(p) | ((uint64_t)le32(p+4)<<32); }
static bool exfat(struct block_device *d) {
    uint8_t b[512];
    if (!block_read(d,0,b,sizeof(b)) || !literal((const char *)b+3,8,"EXFAT   ") ||
        b[510]!=0x55 || b[511]!=0xaa || b[104]!=0 || b[105]!=1 ||
        b[108]<9 || b[108]>12 || b[109]>25-b[108] || (b[110]!=1 && b[110]!=2)) return false;
    uint64_t sector=1ull<<b[108], volume=le64(b+72), fat=le32(b+80), fatlen=le32(b+84);
    uint64_t heap=le32(b+88), clusters=le32(b+92), root=le32(b+96);
    if (sector!=d->block_size || !volume || volume>block_device_size(d)/sector ||
        fat<24 || !fatlen || fat>=volume || fatlen>(volume-fat)/b[110] ||
        heap<fat+fatlen*b[110] || heap>=volume || !clusters ||
        clusters>(volume-heap)/(1ull<<b[109]) || root<2 || root>=clusters+2 ||
        fatlen*sector/4<clusters+2) return false;
    return true;
}
static bool system_name(const uint16_t *name) {
    static const char expected[]="srvros-system-v1";
    unsigned i=0; for (; expected[i]; i++) if (name[i]!=(uint8_t)expected[i]) return false;
    for (; i<36; i++) if (name[i]) return false;
    return true;
}
static enum installed_root_result fail(const char **reason, const char *s) {
    if (reason) *reason=s;
    return INSTALLED_ROOT_INVALID;
}
enum installed_root_result installed_root_select(const char *cmdline,
    struct block_device **out, const char **reason) {
    if (out) *out=NULL;
    if (reason) *reason=NULL;
    if (!out) return fail(reason,"bad-options");
    uint8_t wanted[16]; unsigned fields=0;
    const char *p=cmdline ? cmdline : "";
    while (*p) {
        while (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r') p++;
        const char *s=p; while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') p++;
        unsigned n=(unsigned)(p-s), key=0; while (key<n && s[key]!='=') key++;
        unsigned bit=0;
        if (literal(s,key,"srvros.root")) bit=1;
        else if (literal(s,key,"srvros.root-format")) bit=2;
        else if (literal(s,key,"srvros.root-role")) bit=4;
        if (!bit) continue;
        if (key==n || (fields&bit)) return fail(reason,"bad-options");
        fields |= bit; const char *value=s+key+1; unsigned len=n-key-1;
        if ((bit==1 && !uuid(value,len,wanted)) ||
            (bit==2 && !literal(value,len,"exfat-v1")) ||
            (bit==4 && !literal(value,len,"system"))) return fail(reason,"bad-options");
    }
    if (!fields) return INSTALLED_ROOT_LEGACY;
    if (fields!=7) return fail(reason,"bad-options");
    static const uint8_t basic[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
    uint8_t disks[BLOCK_MAX_DEVICES][16], parts[BLOCK_MAX_DEVICES][16];
    unsigned nd=0,np=0,matches=0; struct block_device *candidate=NULL;
    bool badrole=false;
    /* Registry may grow as views are created; backing views are skipped. */
    for (uint64_t i=0; i<BLOCK_MAX_DEVICES; i++) {
        struct block_device *d=block_device_at(i); if (!d) break;
        if (d->backing || block_parent(d)!=d) continue;
        struct block_gpt_map map;
        if (!block_gpt_scan(d,&map)) return fail(reason,"invalid-map");
        for (unsigned j=0; j<nd; j++) if (eq(disks[j],map.disk_guid,16)) return fail(reason,"ambiguous");
        for (unsigned k=0; k<16; k++) disks[nd][k]=map.disk_guid[k];
        nd++;
        for (unsigned j=0; j<map.count; j++) {
            struct block_gpt_partition *part=&map.partitions[j];
            for (unsigned k=0; k<np; k++) if (eq(parts[k],part->guid,16)) return fail(reason,"ambiguous");
            if (np==BLOCK_MAX_DEVICES) return fail(reason,"invalid-map");
            for (unsigned k=0; k<16; k++) parts[np][k]=part->guid[k];
            np++;
            if (eq(part->guid,wanted,16)) {
                matches++; candidate=part->view;
                badrole=!eq(part->type,basic,16) || !system_name(part->name);
            }
        }
    }
    if (!matches) return fail(reason,"missing");
    if (matches!=1) return fail(reason,"ambiguous");
    if (badrole) return fail(reason,"role");
    if (!exfat(candidate)) return fail(reason,"format");
    *out=candidate; return INSTALLED_ROOT_SELECTED;
}
