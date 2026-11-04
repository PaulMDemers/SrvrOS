#ifndef DESKD_PROFILE_CATALOG_H
#define DESKD_PROFILE_CATALOG_H

#define PROFILE_CATALOG_MAX 16
enum profile_state { PROFILE_READY, PROFILE_RECOVERABLE, PROFILE_BLOCKED };
struct profile_entry {
    char home[96], id[37], name[65], detail[160];
    int state;
};
struct profile_catalog {
    struct profile_entry entries[PROFILE_CATALOG_MAX];
    unsigned count;
    int truncated, error;
    char message[160];
};

/* Blocking filesystem/child operations: call from the chooser's I/O worker.
 * Scan never repairs anything. Partial scans remain visible but cannot select a
 * remembered profile. Identity and local binding metadata are not credentials. */
int profile_catalog_scan(struct profile_catalog *catalog, const char *root);
/* 1: exact ready UUID+HOME match; 0: record absent; -1: invalid/unavailable. */
int profile_choice_load(const char *path, const struct profile_catalog *catalog,
                        unsigned *index, char reason[160]);
/* Checked exclusive temporary write + file rename. A flush failure can leave
 * the new record visible; -1 always means persistence was not confirmed. */
int profile_choice_save(const char *path, const struct profile_entry *entry);
#endif
