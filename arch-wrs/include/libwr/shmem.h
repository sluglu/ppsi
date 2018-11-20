/*
 * This is the shared memory interface for multi-process cooperation
 * within the whiterabbit switch. Everyone exports status information.
 */
#ifndef __WRS_SHM_H__
#define __WRS_SHM_H__
#include <stdint.h>
#include <stdio.h>

#define WRS_SHM_DEFAULT_PATH  "/dev/shm"
#define WRS_SHM_FILE  "wrs-shmem-%i"
#define WRS_SHM_MIN_SIZE    (4*1024)
#define WRS_SHM_MAX_SIZE  (512*1024)

/* Each process "name" (i.e. id) is added to the filename above */
enum wrs_shm_name {
	wrs_shm_ptp,
	wrs_shm_rtu,
	wrs_shm_hal,
	wrs_shm_vlan,
	WRS_SHM_N_NAMES,	/* must be last */
};

/* Each area starts with this process identifier */
struct wrs_shm_head {
	void *mapbase;		/* In writer's addr space (to track ptrs) */
	char name[7 * sizeof(void *)];

	unsigned long stamp;	/* Last modified, w/ CLOCK_MONOTONIC */
	unsigned long data_off;	/* Where the structure lives */
	int fd;			/* So we can enlarge it using fd */
	int pid;		/* The current pid owning the area */

	unsigned pidsequence;	/* Each new pid must increments this */
	unsigned sequence;	/* If we need consistency, this is it. LSB bit
				 * informs whether shmem is locked already */
	unsigned version;	/* Version of the data structure */
	const char *last_write_caller; /* Function of the last wrs_shm_write
				 * call */
	int last_write_line;	/* Line of the last wrs_shm_write call */
	unsigned data_size;	/* Size of it (for binary dumps) */
};

/* flags */
#define WRS_SHM_READ   0x0000
#define WRS_SHM_WRITE  0x0001
#define WRS_SHM_LOCKED 0x0002 /* at init time: writers locks, readers wait  */

#define WRS_SHM_LOCK_MASK 0x0001

/* return values of wrs_shm_get_and_check */
#define WRS_SHM_OPEN_OK           0x0001
#define WRS_SHM_OPEN_FAILED       0x0001
#define WRS_SHM_WRONG_VERSION     0x0002
#define WRS_SHM_INCONSISTENT_DATA 0x0003

/* Set custom path for shmem */
void wrs_shm_set_path(char *new_path);

/* Allow to ignore the flag WRS_SHM_LOCKED
 * If this flag is not ignored then function wrs_shm_get_and_check is not able
 * to open shmem successfully due to lack of process running with the given pid
 */
void wrs_shm_ignore_flag_locked(int ignore_flag);

/* get vs. put, like in the kernel. Errors are in errno (see source) */
struct wrs_shm_head *wrs_shm_get(enum wrs_shm_name name_id, char *name,
				 unsigned long flags);
int wrs_shm_put(struct wrs_shm_head *head);

/* A reader may wait for the writer (polling on version field) */
void wrs_shm_wait(struct wrs_shm_head *head, int msec_step, int retries,
		  FILE *msg);
int wrs_shm_get_and_check(enum wrs_shm_name shm_name,
				 struct wrs_shm_head **head);

/* The writer can allocate structures that live in the area itself */
void *wrs_shm_alloc(struct wrs_shm_head *head, size_t size);

/* The reader can track writer's pointers, if they are in the area */
void *wrs_shm_follow(struct wrs_shm_head *head, void *ptr);

/* Before and after writing a chunk of data, act on sequence and stamp */
#define WRS_SHM_WRITE_BEGIN	1
#define WRS_SHM_WRITE_END	0

/* A helper to pass the name of a caller function and a line of the call */
#define wrs_shm_write(headptr, flags) wrs_shm_write_caller(headptr, flags, \
							   __func__, __LINE__)
extern void wrs_shm_write_caller(struct wrs_shm_head *head, int flags,
				 const char *caller, int line);

/* A reader can rely on the sequence number (in the <linux/seqlock.h> way) */
extern unsigned wrs_shm_seqbegin(struct wrs_shm_head *head);
extern int wrs_shm_seqretry(struct wrs_shm_head *head, unsigned start);

/* A reader can check wether information is current enough */
extern int wrs_shm_age(struct wrs_shm_head *head);

/* A reader can get the information pointer, for a specific version, or NULL */
extern void *wrs_shm_data(struct wrs_shm_head *head, unsigned version);

#endif /* __WRS_SHM_H__ */
