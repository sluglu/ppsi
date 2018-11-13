/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __PPSI_PPSI_H__
#define __PPSI_PPSI_H__
#include <generated/autoconf.h>

#include <stdint.h>
#include <stdarg.h>
#include <float.h>
#include <stddef.h>
#include <ppsi/lib.h>
#include <ppsi/ieee1588_types.h>
#include <ppsi/constants.h>
#include <ppsi/jiffies.h>

#include <ppsi/pp-instance.h>
#include <ppsi/diag-macros.h>

#include <arch/arch.h> /* ntohs and so on -- and wr-api.h for wr archs */

#if CONFIG_EXT_WR==1
#include "../proto-ext-whiterabbit/wr-api.h"
#endif

#if CONFIG_EXT_L1SYNC==1
#include "../proto-ext-l1sync/l1e-api.h"
#endif

/* At this point in time, we need ARRAY_SIZE to conditionally build vlan code */
#undef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef CONFIG_WRPC_FAULTS
#   define CONFIG_HAS_WRPC_FAULTS 1
#else
#   define CONFIG_HAS_WRPC_FAULTS 0
#endif

/* Default values for code optimization. Can be redefined for each targets in arch/arch.h*/
#ifndef CODEOPT_BMCA
	#define CODEOPT_BMCA        0  /* Code optimization for BMCA. If set to 0, remove all code optimizations  */
#endif
#ifndef CODEOPT_ONE_PORT
#define CODEOPT_ONE_PORT()               (0 && CODEOPT_BMCA==1)  /* Code optimization when only one port is used. */
#endif
#ifndef CODEOPT_ONE_FMASTER
#define CODEOPT_ONE_FMASTER()            ((PP_NR_FOREIGN_RECORDS==1) && CODEOPT_BMCA==1)  /* Code optimization when only one foreign master. */
#endif
#ifndef CODEOPT_ROLE_MASTER_SLAVE_ONLY
#define CODEOPT_ROLE_MASTER_SLAVE_ONLY() ( 0  && CODEOPT_BMCA==1)  /* Code optimization when role auto not allowed. */
#endif

#ifdef CONFIG_ARCH_WRPC
#define ARCH_IS_WRPC (1)
#else
#define ARCH_IS_WRPC (0)
#endif

#ifdef CONFIG_ARCH_WRS
#define ARCH_IS_WRS (1)
#else
#define ARCH_IS_WRS (0)
#endif

/* We can't include pp-printf.h when building freestading, so have it here */
extern int pp_printf(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));
extern int pp_vprintf(const char *fmt, va_list args)
	__attribute__((format(printf, 1, 0)));
extern int pp_sprintf(char *s, const char *fmt, ...)
	__attribute__((format(printf,2,3)));
extern int pp_vsprintf(char *buf, const char *, va_list)
	__attribute__ ((format (printf, 2, 0)));

/* This structure is never defined, it seems */
struct pp_vlanhdr {
	uint8_t h_dest[6];
	uint8_t h_source[6];
	uint16_t h_tpid;
	uint16_t h_tci;
	uint16_t h_proto;
};

/* Factorize some random information in this table */
struct pp_msgtype_info {
	enum pp_std_messages msg_type;
	uint16_t msglen;
	unsigned char chtype;
	unsigned char is_pdelay;
	unsigned char controlField;		/* Table 23 */
	unsigned char logMessageInterval;	/* Table 24, see defines */

#define PP_LOG_ANNOUNCE  0
#define PP_LOG_SYNC      1
#define PP_LOG_REQUEST   2
};

/* Used as index for the pp_msgtype_info array */

enum pp_msg_format {
	PPM_SYNC_FMT		= 0x0,
	PPM_DELAY_REQ_FMT,
	PPM_PDELAY_REQ_FMT,
	PPM_PDELAY_RESP_FMT,
	PPM_FOLLOW_UP_FMT,
	PPM_DELAY_RESP_FMT,
	PPM_PDELAY_R_FUP_FMT,
	PPM_ANNOUNCE_FMT,
	PPM_SIGNALING_FMT,
	PPM_SIGNALING_NO_FWD_FMT,
	PPM_MANAGEMENT_FMT,
	PPM_MSG_FMT_MAX
};

extern struct pp_msgtype_info pp_msgtype_info[];
extern char  *pp_msgtype_name[];

/* Helpers for the fsm (fsm-lib.c) */
extern int pp_lib_may_issue_sync(struct pp_instance *ppi);
extern int pp_lib_may_issue_announce(struct pp_instance *ppi);
extern int pp_lib_may_issue_request(struct pp_instance *ppi);

/* We use data sets a lot, so have these helpers */
static inline struct pp_globals *GLBS(struct pp_instance *ppi)
{
	return ppi->glbs;
}

static inline struct pp_instance *INST(struct pp_globals *ppg,
							int n_instance)
{
	return ppg->pp_instances + n_instance;
}

static inline struct pp_runtime_opts *GOPTS(struct pp_globals *ppg)
{
	return ppg->rt_opts;
}

static inline struct pp_runtime_opts *OPTS(struct pp_instance *ppi)
{
	return GOPTS(GLBS(ppi));
}

static inline defaultDS_t *GDSDEF(struct pp_globals *ppg)
{
	return ppg->defaultDS;
}

static inline  defaultDS_t *DSDEF(struct pp_instance *ppi)
{
	return GDSDEF(GLBS(ppi));
}

static inline currentDS_t *DSCUR(struct pp_instance *ppi)
{
	return GLBS(ppi)->currentDS;
}

static inline parentDS_t *DSPAR(struct pp_instance *ppi)
{
	return GLBS(ppi)->parentDS;
}

static inline portDS_t *DSPOR(struct pp_instance *ppi)
{
	return ppi->portDS;
}

static inline timePropertiesDS_t *DSPRO(struct pp_instance *ppi)
{
	return GLBS(ppi)->timePropertiesDS;
}

/* We used to have a "netpath" structure. Keep this until we merge pdelay */
static struct pp_instance *NP(struct pp_instance *ppi)
	__attribute__((deprecated));

static inline struct pp_instance *NP(struct pp_instance *ppi)
{
	return ppi;
}

static inline struct pp_servo *SRV(struct pp_instance *ppi)
{
	return ppi->servo;
}

extern void pp_prepare_pointers(struct pp_instance *ppi);

/*
 * Each extension should fill this structure that is used to augment
 * the standard states and avoid code duplications. Please remember
 * that proto-standard functions are picked as a fall-back when non
 * extension-specific code is provided. The set of hooks here is designed
 * based on what White Rabbit does. If you add more please remember to
 * allow NULL pointers.
 */
struct pp_ext_hooks {
	int (*init)(struct pp_instance *ppi, void *buf, int len);
	int (*open)(struct pp_instance *ppi, struct pp_runtime_opts *rt_opts);
	int (*close)(struct pp_instance *ppi);
	int (*listening)(struct pp_instance *ppi, void *buf, int len);
	int (*master_msg)(struct pp_instance *ppi, void *buf,
			  int len, int msgtype);
	int (*new_slave)(struct pp_instance *ppi, void *buf, int len);
	int (*handle_resp)(struct pp_instance *ppi);
	void (*s1)(struct pp_instance *ppi, struct pp_frgn_master *frgn_master);
	int (*execute_slave)(struct pp_instance *ppi);
	int (*handle_announce)(struct pp_instance *ppi);
	int (*handle_sync)(struct pp_instance *ppi, struct pp_time *orig);
	int (*handle_followup)(struct pp_instance *ppi, struct pp_time *orig);
	int (*handle_preq) (struct pp_instance * ppi);
	int (*handle_presp) (struct pp_instance * ppi);
	int (*handle_signaling) (struct pp_instance * ppi, void *buf, int len);
	int (*pack_announce)(struct pp_instance *ppi);
	void (*unpack_announce)(void *buf, MsgAnnounce *ann);
	int (*ready_for_slave)(struct pp_instance *ppi); /* returns: 0=Not ready 1=ready */
	int (*state_decision)(struct pp_instance *ppi, int next_state);
	void (*state_change)(struct pp_instance *ppi);
	int (*run_ext_state_machine) (struct pp_instance *ppi);
};

/*
 * Network methods are encapsulated in a structure, so each arch only needs
 * to provide that structure. This simplifies management overall.
 */
struct pp_network_operations {
	int (*init)(struct pp_instance *ppi);
	int (*exit)(struct pp_instance *ppi);
	int (*recv)(struct pp_instance *ppi, void *pkt, int len,
		    struct pp_time *t);
	int (*send)(struct pp_instance *ppi, void *pkt, int len, enum pp_msg_format msg_fmt);
	int (*check_packet)(struct pp_globals *ppg, int delay_ms);
};

/* This is the struct pp_network_operations to be provided by time- dir */
extern struct pp_network_operations DEFAULT_NET_OPS;

/* These can be liked and used as fallback by a different timing engine */
extern struct pp_network_operations unix_net_ops;

/*
 * Time operations, like network operations above, are encapsulated.
 * They may live in their own time-<name> subdirectory.
 *
 * If "set" receives a NULL time value, it should update the TAI offset.
 */
struct pp_time_operations {
	int (*get_utc_time)(struct pp_instance *ppi, int *hours, int *minutes, int *seconds);
	int (*get_utc_offset)(struct pp_instance *ppi, int *offset, int *leap59, int *leap61);
	int (*set_utc_offset)(struct pp_instance *ppi, int offset, int leap59, int leap61);
	int (*get)(struct pp_instance *ppi, struct pp_time *t);
	int (*set)(struct pp_instance *ppi, const struct pp_time *t);
	/* freq_ppb is parts per billion */
	int (*adjust)(struct pp_instance *ppi, long offset_ns, long freq_ppb);
	int (*adjust_offset)(struct pp_instance *ppi, long offset_ns);
	int (*adjust_freq)(struct pp_instance *ppi, long freq_ppb);
	int (*init_servo)(struct pp_instance *ppi);
	int (*get_servo_state)(struct pp_instance *ppi, int *state);
	unsigned long (*calc_timeout)(struct pp_instance *ppi, int millisec);
};

/* This is the struct pp_time_operations to be provided by time- dir */
extern struct pp_time_operations DEFAULT_TIME_OPS;

/* These can be liked and used as fallback by a different timing engine */
extern struct pp_time_operations unix_time_ops;


/* FIXME this define is no more used; check whether it should be
 * introduced again */
#define  PP_ADJ_NS_MAX		(500*1000)

/* FIXME Restored to value of ptpd. What does this stand for, exactly? */
#define  PP_ADJ_FREQ_MAX	512000

/*
 * Timeouts.
 *
 * A timeout, is just a number that must be compared with the current counter.
 * So we don't need struct operations, as it is one function only,
 * which is folded into the "pp_time_operations" above.
 */
extern void pp_timeout_init(struct pp_instance *ppi);
extern void __pp_timeout_set(struct pp_instance *ppi, int index, int millisec);
extern void pp_timeout_clear(struct pp_instance *ppi, int index);
extern void pp_timeout_set(struct pp_instance *ppi, int index);
extern void pp_timeout_setall(struct pp_instance *ppi);
extern int pp_timeout(struct pp_instance *ppi, int index)
	__attribute__((warn_unused_result));
extern int pp_next_delay_1(struct pp_instance *ppi, int i1);
extern int pp_next_delay_2(struct pp_instance *ppi, int i1, int i2);
extern int pp_next_delay_3(struct pp_instance *ppi, int i1, int i2, int i3);

/* The channel for an instance must be created and possibly destroyed. */
extern int pp_init_globals(struct pp_globals *ppg, struct pp_runtime_opts *opts);
extern int pp_close_globals(struct pp_globals *ppg);

extern int pp_parse_cmdline(struct pp_globals *ppg, int argc, char **argv);

/* platform independent timespec-like data structure */
struct pp_cfg_time {
	long tv_sec;
	long tv_nsec;
};

/* Data structure used to pass just a single argument to configuration
 * functions. Any future new type for any new configuration function can be just
 * added inside here, without redefining cfg_handler prototype */
union pp_cfg_arg {
	int i;
	int i2[2];
	int64_t i64;
	double d;
	Boolean b;
	char *s;
	struct pp_cfg_time ts;
};

/*
 * Configuration: we are structure-based, and a typedef simplifies things
 */
struct pp_argline;

typedef int (*cfg_handler)(struct pp_argline *l, int lineno,
			   struct pp_globals *ppg, union pp_cfg_arg *arg);

struct pp_argname {
	char *name;
	int value;
};
enum pp_argtype {
	ARG_NONE,
	ARG_INT,
	ARG_INT2,
	ARG_STR,
	ARG_NAMES,
	ARG_TIME,
	ARG_DOUBLE,
	ARG_INT64
};

/* This enumeration gives the list of run-time options that should be marked when they are set in the configuration */
enum {
	OPT_RT_NO_UPDATE=0,
};


typedef struct {
	union min {
		int min_int;
		Integer64 min_int64;
		double min_double;
	}min;
	union max{
		int max_int;
		Integer64 max_int64;
		double max_double;
	}max;
}pp_argline_min_max_t;

struct pp_argline {
	cfg_handler f;
	char *keyword;	/* Each line starts with a keyword */
	enum pp_argtype t;
	struct pp_argname *args;
	size_t field_offset;
	int needs_port;
	pp_argline_min_max_t min_max;
};

/* Below are macros for setting up pp_argline arrays */
#define OFFS(s,f) offsetof(s, f)

#define OPTION_OPEN() {
#define OPTION_CLOSE() }
#define OPTION(s,func,k,typ,a,field,np)	\
		.f = func,						\
		.keyword = k,						\
		.t = typ,						\
		.args = a,						\
		.field_offset = OFFS(s,field),				\
		.needs_port = np,

#define LEGACY_OPTION(func,k,typ)					\
	{								\
		.f = func,						\
		.keyword = k,						\
		.t = typ,						\
	}

#define INST_OPTION(func,k,t,a,field)					\
    OPTION_OPEN() \
	OPTION(struct pp_instance,func,k,t,a,field,1) \
	OPTION_CLOSE()

#define INST_OPTION_FCT(func,k,t)					\
	    OPTION_OPEN() \
		OPTION(struct pp_instance,func,k,t,NULL,cfg,1) \
		OPTION_CLOSE()

#define INST_OPTION_STR(k,field)					\
	INST_OPTION(f_string,k,ARG_STR,NULL,field)

#define INST_OPTION_INT_RANGE(k,t,a,field,mn,mx)					\
	OPTION_OPEN() \
	OPTION(struct pp_instance,f_simple_int,k,t,a,field,1) \
	.min_max.min.min_int = mn,\
	.min_max.max.max_int = mx,\
	OPTION_CLOSE()

#define INST_OPTION_INT(k,t,a,field)					\
		INST_OPTION_INT_RANGE(k,t,a,field,INT_MIN,INT_MAX)


#define INST_OPTION_BOOL(k,field)					\
	INST_OPTION(f_simple_bool,k,ARG_NAMES,arg_bool,field)

#define INST_OPTION_INT64_RANGE(k,t,a,field,mn,mx)					\
	OPTION_OPEN() \
	OPTION(struct pp_instance,f_simple_int64,k,t,a,field,1) \
	.min_max.min.min_int64 = mn,\
	.min_max.max.max_int64 = mx,\
	OPTION_CLOSE()

#define INST_OPTION_INT64(k,t,a,field)					\
		INST_OPTION_INT64_RANGE(k,t,a,field,INT64_MIN,INT64_MAX)

#define INST_OPTION_DOUBLE_RANGE(k,t,a,field,mn,mx)					\
	OPTION_OPEN() \
	OPTION(struct pp_instance,f_simple_double,k,t,a,field,1) \
	.min_max.min.min_double = mn,\
	.min_max.max.max_double = mx,\
	OPTION_CLOSE()

#define INST_OPTION_DOUBLE(k,t,a,field)					\
		INST_OPTION_DOUBLE_RANGE(k,t,a,field,-DBL_MAX,DBL_MAX)

#define RT_OPTION(func,k,t,a,field)					\
	OPTION_OPEN() \
	OPTION(struct pp_runtime_opts,func,k,t,a,field,0)\
	OPTION_CLOSE()

#define RT_OPTION_INT_RANGE(k,t,a,field,mn,mx)					\
	OPTION_OPEN() \
	OPTION(struct pp_runtime_opts,f_simple_int,k,t,a,field,0) \
	.min_max.min.min_int = mn,\
	.min_max.max.max_int = mx,\
	OPTION_CLOSE()

#define RT_OPTION_INT(k,t,a,field)					\
	RT_OPTION_INT_RANGE(k,t,a,field,INT_MIN,INT_MAX)

#define RT_OPTION_BOOL(k,field)					\
	RT_OPTION(f_simple_bool,k,ARG_NAMES,arg_bool,field)


#define GLOB_OPTION(func,k,t,a,field)					\
	OPTION_OPEN() \
	OPTION(struct pp_globals,func,k,t,a,field,0) \
	OPTION_CLOSE()

#define GLOB_OPTION_INT_RANGE(k,t,a,field,mn,mx)					\
	OPTION_OPEN() \
	OPTION(struct pp_globals,f_simple_int,k,t,a,field,0) \
	.min_max.min.min_int = mn,\
	.min_max.max.max_int = mx,\
	OPTION_CLOSE()

#define GLOB_OPTION_INT(k,t,a,field)					\
	GLOB_OPTION_INT_RANGE(k,t,a,field,INT_MIN,INT_MAX)

/* Both the architecture and the extension can provide config arguments */
extern struct pp_argline pp_arch_arglines[];
extern struct pp_argline pp_ext_arglines[];

/* Note: config_string modifies the string it receives */
extern int pp_config_string(struct pp_globals *ppg, char *s);
extern int pp_config_file(struct pp_globals *ppg, int force, char *fname);
extern int f_simple_int(struct pp_argline *l, int lineno,
			struct pp_globals *ppg, union pp_cfg_arg *arg);

#define PPSI_PROTO_RAW		0
#define PPSI_PROTO_UDP		1
#define PPSI_PROTO_VLAN		2	/* Actually: vlan over raw eth */

#define PPSI_ROLE_AUTO		0
#define PPSI_ROLE_MASTER	1
#define PPSI_ROLE_SLAVE		2

/* Define the PPSI extensions */
#define PPSI_EXT_NONE	0
#define PPSI_EXT_WR		1   /* WR extension */ 
#define PPSI_EXT_L1S	2   /* L1SYNC extension */

/* Define the PPSI profiles */  
#define PPSI_PROFILE_PTP 0 /* Default PTP profile without extensions */
#define PPSI_PROFILE_WR  1 /* WR profile using WR extension */
#define PPSI_PROFILE_HA  2 /* HA profile using L1S extension, masterOnly and externalPortConfiguration options */


/* Servo */
extern void pp_servo_init(struct pp_instance *ppi);
extern void pp_servo_got_sync(struct pp_instance *ppi); /* got t1 and t2 */
extern void pp_servo_got_resp(struct pp_instance *ppi); /* got all t1..t4 */
extern void pp_servo_got_psync(struct pp_instance *ppi); /* got t1 and t2 */
extern void pp_servo_got_presp(struct pp_instance *ppi); /* got all t3..t6 */

/* bmc.c */
extern void bmc_m1(struct pp_instance *ppi);
extern void bmc_m2(struct pp_instance *ppi);
extern void bmc_m3(struct pp_instance *ppi);
extern void bmc_s1(struct pp_instance *ppi, 
			   struct pp_frgn_master *frgn_master);
extern void bmc_p1(struct pp_instance *ppi);
extern void bmc_p2(struct pp_instance *ppi);
extern int bmc_idcmp(struct ClockIdentity *a, struct ClockIdentity *b);
extern int bmc_pidcmp(struct PortIdentity *a, struct PortIdentity *b);
extern int bmc(struct pp_instance *ppi);
extern void bmc_store_frgn_master(struct pp_instance *ppi, 
		       struct pp_frgn_master *frgn_master, void *buf, int len);
extern void bmc_add_frgn_master(struct pp_instance *ppi, void *buf,
			    int len);
extern void bmc_flush_erbest(struct pp_instance *ppi);

/* msg.c */
extern void msg_init_header(struct pp_instance *ppi, void *buf);
extern int msg_from_current_master(struct pp_instance *ppi);
extern int __attribute__((warn_unused_result))
	msg_unpack_header(struct pp_instance *ppi, void *buf, int len);
extern void msg_unpack_sync(void *buf, MsgSync *sync);
extern int msg_pack_sync(struct pp_instance *ppi, struct pp_time *orig_tstamp);
extern void msg_unpack_announce(struct pp_instance *ppi,void *buf, MsgAnnounce *ann);
extern void msg_unpack_follow_up(void *buf, MsgFollowUp *flwup);
extern void msg_unpack_delay_req(void *buf, MsgDelayReq *delay_req);
extern void msg_unpack_delay_resp(void *buf, MsgDelayResp *resp);
extern int msg_pack_signaling(struct pp_instance *ppi,PortIdentity *target_port_identity,
		UInteger16 tlv_type, UInteger16 tlv_length_field);
extern int msg_pack_signaling_no_fowardable(struct pp_instance *ppi,PortIdentity *target_port_identity,
		UInteger16 tlv_type, UInteger16 tlv_length_field);
void msg_unpack_signaling(void *buf, MsgSignaling *signaling);

/* pdelay */
extern void msg_unpack_pdelay_resp_follow_up(void *buf,
					     MsgPDelayRespFollowUp *
					     pdelay_resp_flwup);
extern void msg_unpack_pdelay_resp(void *buf, MsgPDelayResp * presp);
extern void msg_unpack_pdelay_req(void *buf, MsgPDelayReq * pdelay_req);

/* each of them returns 0 if ok, -1 in case of error in send, 1 if stamp err */
#define PP_SEND_OK		0
#define PP_SEND_ERROR		-1
#define PP_SEND_NO_STAMP	1
#define PP_SEND_DROP		-2
#define PP_RECV_DROP		PP_SEND_DROP

extern void *msg_copy_header(MsgHeader *dest, MsgHeader *src); /* REMOVE ME!! */
extern int msg_issue_announce(struct pp_instance *ppi);
extern int msg_issue_sync_followup(struct pp_instance *ppi);
extern int msg_issue_request(struct pp_instance *ppi);
extern int msg_issue_delay_resp(struct pp_instance *ppi,
				struct pp_time *time);
extern int msg_issue_pdelay_resp_followup(struct pp_instance *ppi,
					  struct pp_time *time);
extern int msg_issue_pdelay_resp(struct pp_instance *ppi, struct pp_time *time);

/* Functions for time math */
extern void normalize_pp_time(struct pp_time *t);
extern void pp_time_add(struct pp_time *t1, struct pp_time *t2);
extern void pp_time_sub(struct pp_time *t1, struct pp_time *t2);
extern void pp_time_div2(struct pp_time *t);
extern TimeInterval pp_time_to_interval(struct pp_time *ts);
extern TimeInterval picos_to_interval(int64_t picos);
extern void pp_time_add_interval(struct pp_time *t1, TimeInterval t2);
extern void pp_time_sub_interval(struct pp_time *t1, TimeInterval t2);
extern int pp_timeout_log_to_ms ( Integer8 logValue);

/* Function for time conversion */
extern int64_t pp_time_to_picos(struct pp_time *ts);
extern void picos_to_pp_time(int64_t picos, struct pp_time *ts);
extern void pp_time_hardwarize(struct pp_time *time, int clock_period_ps,int32_t *ticks, int32_t *picos);
extern int64_t interval_to_picos(TimeInterval interval);
extern int is_timestamps_incorrect(struct pp_instance *ppsi, int *err_count, int ts_mask);


/*
 * The state machine itself is an array of these structures.
 */

/* Use a typedef, to avoid long prototypes */
typedef int pp_action(struct pp_instance *ppi, void *buf, int len);

struct pp_state_table_item {
	int state;
	char *name;
	pp_action *f1;
};

extern struct pp_state_table_item pp_state_table[]; /* 0-terminated */

/* Convert current state as a string value */
char *get_state_as_string(struct pp_instance *ppi, int state);

/* Standard state-machine functions */
extern pp_action pp_initializing, pp_faulty, pp_disabled, pp_listening,
		 pp_master, pp_passive, pp_uncalibrated,
		 pp_slave, pp_pclock;

/* Enforce a state change */
extern int pp_leave_current_state(struct pp_instance *ppi);

/* The engine */
extern int pp_state_machine(struct pp_instance *ppi, void *buf, int len);

/* Frame-drop support -- rx before tx, alphabetically */
extern void ppsi_drop_init(struct pp_globals *ppg, unsigned long seed);
extern int ppsi_drop_rx(void);
extern int ppsi_drop_tx(void);

#endif /* __PPSI_PPSI_H__ */
