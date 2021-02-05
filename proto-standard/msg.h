/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author:Jean-Claude BAU
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __MSG_H
#define __MSG_H

#if __BYTE_ORDER == __BIG_ENDIAN
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) htobe64(x)
# define ntohll(x) be64toh(x)
#endif

extern const int endianess; /* use to check endianess */

extern int msg_unpack_header(struct pp_instance *ppi, void *buf, int len);
extern void msg_init_header(struct pp_instance *ppi, void *buf);
extern void __msg_set_seq_id(struct pp_instance *ppi,
		struct pp_msgtype_info *mf);
extern int __msg_pack_header(struct pp_instance *ppi,
		struct pp_msgtype_info *msg_fmt);
extern void __pack_origin_timestamp(void *buf, struct pp_time *orig_tstamp);
extern void __unpack_origin_timestamp(void *buf, struct pp_time *orig_tstamp);
extern int __msg_pack_header(struct pp_instance *ppi, struct pp_msgtype_info *msg_fmt);

extern int msg_issue_pdelay_req(struct pp_instance *ppi);

#endif /* __MSG_H */
