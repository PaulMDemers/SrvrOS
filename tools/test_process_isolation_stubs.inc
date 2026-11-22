/* These legacy root-only process fixtures do not exercise isolation policy.
 * Fail loudly if a credential-sensitive route unexpectedly reaches a mock. */
int64_t vfs_check_access(const char *path,uint64_t uid,uint64_t gid,unsigned access) {
    (void)path;(void)uid;(void)gid;(void)access;abort();
}
int64_t vfs_check_parent_access(const char *path,uint64_t uid,uint64_t gid,unsigned access) {
    (void)path;(void)uid;(void)gid;(void)access;abort();
}
bool exfat_write_file_owned(const char *path,const uint8_t *data,uint64_t size,
    uint64_t mode,uint64_t uid,uint64_t gid) {
    (void)path;(void)data;(void)size;(void)mode;(void)uid;(void)gid;abort();
}
