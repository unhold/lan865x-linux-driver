// SPDX-License-Identifier: GPL-2.0+
/*------------------------------------------------------------------------------------------------*/
/* low level driver for openalliance tc6 10base-t1s macphy via spi protocol                       */
/* copyright 2023, microchip technology inc. and its subsidiaries.                                */
/*                                                                                                */
/* redistribution and use in source and binary forms, with or without                             */
/* modification, are permitted provided that the following conditions are met:                    */
/*                                                                                                */
/* 1. redistributions of source code must retain the above copyright notice, this                 */
/*    list of conditions and the following disclaimer.                                            */
/*                                                                                                */
/* 2. redistributions in binary form must reproduce the above copyright notice,                   */
/*    this list of conditions and the following disclaimer in the documentation                   */
/*    and/or other materials provided with the distribution.                                      */
/*                                                                                                */
/* 3. neither the name of the copyright holder nor the names of its                               */
/*    contributors may be used to endorse or promote products derived from                        */
/*    this software without specific prior written permission.                                    */
/*                                                                                                */
/* this software is provided by the copyright holders and contributors "as is"                    */
/* and any express or implied warranties, including, but not limited to, the                      */
/* implied warranties of merchantability and fitness for a particular purpose are                 */
/* disclaimed. in no event shall the copyright holder or contributors be liable                   */
/* for any direct, indirect, incidental, special, exemplary, or consequential                     */
/* damages (including, but not limited to, procurement of substitute goods or                     */
/* services; loss of use, data, or profits; or business interruption) however                     */
/* caused and on any theory of liability, whether in contract, strict liability,                  */
/* or tort (including negligence or otherwise) arising in any way out of the use                  */
/* of this software, even if advised of the possibility of such damage.                           */
/*------------------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include "tc6.h"

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                          user adjustable                             */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

#define TC6_CHUNKS_PER_ISR  (2u)

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                    internal defines and variables                    */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/*
 * tx data header: 32-bit spi tx data chunk command header
 */

/* data fields {{{ */

#define hdr_dnc   fld(31u, 1u) /* data, not control */
#define hdr_seq   fld(30u, 1u) /* data chunk sequence */
/*#define hdr_norx  fld(29u, 1u) */ /* no receive */
#define hdr_dv    fld(21u, 1u) /* data valid */
#define hdr_sv    fld(20u, 1u) /* start of frame valid */
#define hdr_swo   fld(16u, 4u) /* start of frame word offset */
#define hdr_ev    fld(14u, 1u) /* end of frame valid */
#define hdr_ebo   fld(8u, 6u) /* end of frame byte offset */
#define hdr_tsc   fld(6u, 2u) /* transmit frame timestamp capture */
#define hdr_p     fld(0u, 1u) /* header parity bit */

/* }}} */

/* control transaction fields {{{ */

#define hdr_c_dnc    fld(31u, 1u) /* data, not control */
/*#define hdr_c_hdrb   fld(30u, 1u) */ /* tx header bad */
#define hdr_c_wnr    fld(29u, 1u) /* write, not read */
#define hdr_c_aid    fld(28u, 1u) /* address increment disable */
#define hdr_c_mms    fld(24u, 4u) /* memory map selector */
#define hdr_c_addr   fld(8u, 16u) /* address */
#define hdr_c_len    fld(1u, 7u) /* length */
#define hdr_c_p      fld(0u, 1u) /* parity bit */

/* }}} */

/*
 * rx data footer: 36-bit spi rx data chunk command footer
 */

#define ftr_exst     fld(31u, 1u) /* extended status */
#define ftr_hdrb     fld(30u, 1u) /* tx header bad */
#define ftr_sync     fld(29u, 1u) /* configuration synchronized */
#define ftr_rca      fld(24u, 5u) /* receive chunks available */
/*#define ftr_vs       fld(22u, 2u) */ /* vendor specific */
#define ftr_dv       fld(21u, 1u) /* data valid */
#define ftr_sv       fld(20u, 1u) /* start of frame valid */
#define ftr_swo      fld(16u, 4u) /* start of frame word offset */
#define ftr_fd       fld(15u, 1u) /* frame drop */
#define ftr_ev       fld(14u, 1u) /* end of frame valid */
#define ftr_ebo      fld(8u, 6u) /* end of frame byte offset */
#define ftr_rtsa     fld(7u, 1u) /* receive frame timestamp added */
#define ftr_rtsp     fld(6u, 1u) /* receive frame timestamp parity */
#define ftr_txc      fld(1u, 5u) /* transmit credits */
/*#define ftr_p        fld(0u, 1u) */ /* footer parity bit */

#define ftr_data_mask  \
	(mk_mask(ftr_sv, 1u) | mk_mask(ftr_dv, 1u) | mk_mask(ftr_ev, 1u))

/** integrator needs to allocate this structure. but the elements must not be accessed.
 *  they are getting filled via function calls only.
 */

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                      private function prototypes                     */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static inline u32 mask(u32 width);
static inline u32 fld(u32 pos, u32 width);
static inline u32 fld_pos(u32 f);
static inline u32 fld_mask(u32 f);
static inline u32 get_val(u32 _field, u32 _v32);
static inline u32 mk_mask(u32 _field, u32 _val);
static void initializespientry(struct qspibuf *newentry);
static u16 gettrail(struct tc6_t *g, bool enqueueempty);
static void addemptychunks(struct tc6_t *g, struct qspibuf *entry, bool enqueueempty);
static bool servicedata(struct tc6_t *g, bool enqueueempty);
static void servicecontrol(struct tc6_t *g);
static bool spitransaction(struct tc6_t *g, u8 *ptx, u8 *prx, u16 len, enum spiop_t op);
static bool modify(struct tc6_t *g, u32 value);
static bool accessregisters(struct tc6_t *g, enum register_op_type op, u32 addr,
			    u32 value, bool secure, u32 modifymask,
			    tc6_regcallback_t callback, void *tag);
static void processdatarx(struct tc6_t *g);

/* protocol implementation */
static u16 mk_ctrl_req(bool wnr, bool aid, u32 addr, u8 num_regs,
		       const u32 *regs, u8 *buff, u16 size_of_buff);
static u16 mk_secure_ctrl_req(bool wnr, bool aid, u32 addr, u8 num_regs,
			      const u32 *regs, u8 *buff, u16 size_of_buff);
static u16 read_rx_ctrl_buffer(const u8 *rx_buf, u16 rx_buf_size,
			       u32 *regs_buf, u8 regs_buf_size, bool secure);
static u16 mk_data_tx(struct tc6_t *g, u8 *tx_buf, u16 tx_buf_len);
static void enqueue_rx_spi(struct tc6_t *g, const u8 *buff, u16 buf_len);
static void update_credit_cnt(struct tc6_t *g, const u8 *buff, u16 buf_len);

static inline void init_qtxeth_queue(struct qtxeth_queue *q,
				     struct qtxeth *buffer, u32 size)
{
	q->buffer_ = buffer;
	q->size_ = size;
	q->stage1_enqueue_ = 0u;
	q->stage2_convert_ = 0u;
}

/* stage1_enqueue */
static inline bool qtxeth_stage1_enqueue_ready(struct qtxeth_queue const *q)
{
	return 0u != ((q->stage1_enqueue_ - q->stage2_convert_) < q->size_);
}

static inline struct qtxeth *qtxeth_stage1_enqueue_ptr(struct qtxeth_queue const *q)
{
	return &q->buffer_[(q->stage1_enqueue_ & (q->size_ - 1u))];
}

static inline void qtxeth_stage1_enqueue_done(struct qtxeth_queue *q)
{
	++q->stage1_enqueue_;
}

static inline void qtxeth_stage1_enqueue_undo(struct qtxeth_queue *q)
{
	--q->stage1_enqueue_;
}

static inline u32 qtxeth_stage1_enqueue_cap(struct qtxeth_queue const *q)
{
	return q->stage2_convert_ + q->size_ - q->stage1_enqueue_;
}

/* stage2_convert */
static inline bool qtxeth_stage2_convert_ready(struct qtxeth_queue const *q)
{
	return (q->stage1_enqueue_ - q->stage2_convert_ - 1u) < q->size_;
}

static inline struct qtxeth *qtxeth_stage2_convert_ptr(struct qtxeth_queue const *q)
{
	return &q->buffer_[(q->stage2_convert_ & (q->size_ - 1u))];
}

static inline void qtxeth_stage2_convert_done(struct qtxeth_queue *q)
{
	++q->stage2_convert_;
}

static inline void qtxeth_stage2_convert_undo(struct qtxeth_queue *q)
{
	--q->stage2_convert_;
}

static inline u32 qtxeth_stage2_convert_cap(struct qtxeth_queue const *q)
{
	return q->stage1_enqueue_ - q->stage2_convert_;
}

static inline void init_qspibuf_queue(struct qspibuf_queue *q,
				      struct qspibuf *buffer, u32 size)
{
	q->buffer_ = buffer;
	q->size_ = size;
	q->stage1_transfer_ = 0u;
	q->stage2_int_ = 0u;
	q->stage3_process_ = 0u;
}

/* stage1_transfer */
static inline bool qspibuf_stage1_transfer_ready(struct qspibuf_queue const *q)
{
	return 0u != ((q->stage1_transfer_ - q->stage3_process_) < q->size_);
}

static inline struct qspibuf *qspibuf_stage1_transfer_ptr(struct qspibuf_queue const *q)
{
	return &q->buffer_[(q->stage1_transfer_ & (q->size_ - 1u))];
}

static inline void qspibuf_stage1_transfer_done(struct qspibuf_queue *q)
{
	++q->stage1_transfer_;
}

static inline void qspibuf_stage1_transfer_undo(struct qspibuf_queue *q)
{
	--q->stage1_transfer_;
}

static inline u32 qspibuf_stage1_transfer_cap(struct qspibuf_queue const *q)
{
	return q->stage3_process_ + q->size_ - q->stage1_transfer_;
}

/* stage2_int */
static inline bool qspibuf_stage2_int_ready(struct qspibuf_queue const *q)
{
	return (q->stage1_transfer_ - q->stage2_int_ - 1u) < q->size_;
}

static inline struct qspibuf *qspibuf_stage2_int_ptr(struct qspibuf_queue const *q)
{
	return &q->buffer_[(q->stage2_int_ & (q->size_ - 1u))];
}

static inline void qspibuf_stage2_int_done(struct qspibuf_queue *q)
{
	++q->stage2_int_;
}

static inline void qspibuf_stage2_int_undo(struct qspibuf_queue *q)
{
	--q->stage2_int_;
}

static inline u32 qspibuf_stage2_int_cap(struct qspibuf_queue const *q)
{
	return q->stage1_transfer_ - q->stage2_int_;
}

/* stage3_process */
static inline bool qspibuf_stage3_process_ready(struct qspibuf_queue const *q)
{
	return (q->stage2_int_ - q->stage3_process_ - 1u) < q->size_;
}

static inline struct qspibuf *qspibuf_stage3_process_ptr(struct qspibuf_queue const *q)
{
	return &q->buffer_[(q->stage3_process_ & (q->size_ - 1u))];
}

static inline void qspibuf_stage3_process_done(struct qspibuf_queue *q)
{
	++q->stage3_process_;
}

static inline void qspibuf_stage3_process_undo(struct qspibuf_queue *q)
{
	--q->stage3_process_;
}

static inline u32 qspibuf_stage3_process_cap(struct qspibuf_queue const *q)
{
	return q->stage2_int_ - q->stage3_process_;
}

/*
 * namespace: regop
 * type: "struct register_operation"
 * stages: stage1_enqueue stage2_send stage3_int stage4_modify stage5_send stage6_int stage7_event
 */

static inline void init_regop_queue(struct regop_queue *q,
				    struct register_operation *buffer, u32 size)
{
	q->buffer_ = buffer;
	q->size_ = size;
	q->stage1_enqueue_ = 0u;
	q->stage2_send_ = 0u;
	q->stage3_int_ = 0u;
	q->stage4_modify_ = 0u;
	q->stage5_send_ = 0u;
	q->stage6_int_ = 0u;
	q->stage7_event_ = 0u;
}

/* stage1_enqueue */
static inline bool regop_stage1_enqueue_ready(struct regop_queue const *q)
{
	return 0u != ((q->stage1_enqueue_ - q->stage7_event_) < q->size_);
}

static inline struct register_operation *regop_stage1_enqueue_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage1_enqueue_ & (q->size_ - 1u))];
}

static inline void regop_stage1_enqueue_done(struct regop_queue *q)
{
	++q->stage1_enqueue_;
}

static inline void regop_stage1_enqueue_undo(struct regop_queue *q)
{
	--q->stage1_enqueue_;
}

static inline u32 regop_stage1_enqueue_cap(struct regop_queue const *q)
{
	return q->stage7_event_ + q->size_ - q->stage1_enqueue_;
}

/* stage2_send */
static inline bool regop_stage2_send_ready(struct regop_queue const *q)
{
	return (q->stage1_enqueue_ - q->stage2_send_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage2_send_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage2_send_ & (q->size_ - 1u))];
}

static inline void regop_stage2_send_done(struct regop_queue *q)
{
	++q->stage2_send_;
}

static inline void regop_stage2_send_undo(struct regop_queue *q)
{
	--q->stage2_send_;
}

static inline u32 regop_stage2_send_cap(struct regop_queue const *q)
{
	return q->stage1_enqueue_ - q->stage2_send_;
}

/* stage3_int */
static inline bool regop_stage3_int_ready(struct regop_queue const *q)
{
	return (q->stage2_send_ - q->stage3_int_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage3_int_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage3_int_ & (q->size_ - 1u))];
}

static inline void regop_stage3_int_done(struct regop_queue *q)
{
	++q->stage3_int_;
}

static inline void regop_stage3_int_undo(struct regop_queue *q)
{
	--q->stage3_int_;
}

static inline u32 regop_stage3_int_cap(struct regop_queue const *q)
{
	return q->stage2_send_ - q->stage3_int_;
}

/* stage4_modify */
static inline bool regop_stage4_modify_ready(struct regop_queue const *q)
{
	return (q->stage3_int_ - q->stage4_modify_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage4_modify_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage4_modify_ & (q->size_ - 1u))];
}

static inline void regop_stage4_modify_done(struct regop_queue *q)
{
	++q->stage4_modify_;
}

static inline void regop_stage4_modify_undo(struct regop_queue *q)
{
	--q->stage4_modify_;
}

static inline u32 regop_stage4_modify_cap(struct regop_queue const *q)
{
	return q->stage3_int_ - q->stage4_modify_;
}

/* stage5_send */
static inline bool regop_stage5_send_ready(struct regop_queue const *q)
{
	return (q->stage4_modify_ - q->stage5_send_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage5_send_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage5_send_ & (q->size_ - 1u))];
}

static inline void regop_stage5_send_done(struct regop_queue *q)
{
	++q->stage5_send_;
}

static inline void regop_stage5_send_undo(struct regop_queue *q)
{
	--q->stage5_send_;
}

static inline u32 regop_stage5_send_cap(struct regop_queue const *q)
{
	return q->stage4_modify_ - q->stage5_send_;
}

/* stage6_int */
static inline bool regop_stage6_int_ready(struct regop_queue const *q)
{
	return (q->stage5_send_ - q->stage6_int_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage6_int_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage6_int_ & (q->size_ - 1u))];
}

static inline void regop_stage6_int_done(struct regop_queue *q)
{
	++q->stage6_int_;
}

static inline void regop_stage6_int_undo(struct regop_queue *q)
{
	--q->stage6_int_;
}

static inline u32 regop_stage6_int_cap(struct regop_queue const *q)
{
	return q->stage5_send_ - q->stage6_int_;
}

/* stage7_event */
static inline bool regop_stage7_event_ready(struct regop_queue const *q)
{
	return (q->stage6_int_ - q->stage7_event_ - 1u) < q->size_;
}

static inline struct register_operation *regop_stage7_event_ptr(struct regop_queue const *q)
{
	return &q->buffer_[(q->stage7_event_ & (q->size_ - 1u))];
}

static inline void regop_stage7_event_done(struct regop_queue *q)
{
	++q->stage7_event_;
}

static inline void regop_stage7_event_undo(struct regop_queue *q)
{
	--q->stage7_event_;
}

static inline u32 regop_stage7_event_cap(struct regop_queue const *q)
{
	return q->stage6_int_ - q->stage7_event_;
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                         public functions                             */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

void tc6_init(struct tc6_t *tc6)
{
	(void)memset(tc6, 0, sizeof(struct tc6_t));
	tc6->txc = 24;
	init_qtxeth_queue(&tc6->eth_q, tc6->tx_eth_buffer,
			  TC6_TX_ETH_QSIZE);
	init_qspibuf_queue(&tc6->qspi, tc6->spibuf,
			   SPI_FULL_BUFFERS);
	init_regop_queue(&tc6->regop_q, tc6->regop_storage,
			 REG_OP_ARRAY_SIZE);
}

void tc6_reset(struct tc6_t *g)
{
	struct qtxeth_queue *qeth = &g->eth_q;
	struct regop_queue *qreg = &g->regop_q;

	/* wait for pending spi transactions */
	do {} while (g->currentop != spi_op_invalid);

	/* callback ethernet data event listeners */
	while (qtxeth_stage2_convert_ready(qeth)) {
		struct qtxeth *entry = qtxeth_stage2_convert_ptr(qeth);

		if (entry->txcallback)
			entry->txcallback(g, entry->ethsegs[0].peth,
					  entry->ethsegs[0].seglen,
					  entry->priv, g->gtag);
		qtxeth_stage2_convert_done(qeth);
	}
	while (regop_stage2_send_ready(qreg))
		regop_stage2_send_done(qreg);

	while (regop_stage3_int_ready(qreg))
		regop_stage3_int_done(qreg);

	while (regop_stage4_modify_ready(qreg))
		regop_stage4_modify_done(qreg);

	while (regop_stage5_send_ready(qreg))
		regop_stage5_send_done(qreg);

	while (regop_stage6_int_ready(qreg))
		regop_stage6_int_done(qreg);

	while (regop_stage7_event_ready(qreg)) {
		struct register_operation *entry = regop_stage7_event_ptr(qreg);

		if (entry->callback)
			entry->callback(g, false, entry->regaddr, 0u,
					entry->tag, g->gtag);
		regop_stage7_event_done(qreg);
	}
	if (g->eth_started)
		tc6_cb_onrxethernetpacket(g, false, 0, NULL, g->gtag);

	/* clean all states and queues */
	init_qtxeth_queue(&g->eth_q, g->tx_eth_buffer, TC6_TX_ETH_QSIZE);
	init_qspibuf_queue(&g->qspi, g->spibuf, SPI_FULL_BUFFERS);
	init_regop_queue(&g->regop_q, g->regop_storage, REG_OP_ARRAY_SIZE);

	/* set protocol defaults */
	g->txc = 24;
}

bool tc6_service(struct tc6_t *g, bool interruptlevel)
{
	bool data_sent = false;

	servicecontrol(g);
	if (g->enabledata) {
		processdatarx(g);
		data_sent = servicedata(g, !interruptlevel);
		processdatarx(g);
	}
	return data_sent;
}

void tc6_enabledata(struct tc6_t *g, bool enable)
{
	g->enabledata = enable;
	tc6_cb_onneedservice(g, g->gtag);
}

u8 tc6_getinstance(struct tc6_t *g)
{
	return g ? g->instance : 0xffu;
}

void tc6_getstate(struct tc6_t *g, u8 *ptxcredit, u8 *prxcredit, bool *psynced)
{
	if (ptxcredit)
		*ptxcredit = g->txc;
	if (prxcredit)
		*prxcredit = g->rca;
	if (psynced)
		*psynced = g->synced;
}

bool tc6_sendrawethernetpacket(struct tc6_t *g, const u8 *ptx, u16 len, u8 tsc,
			       tc6_rawtxcallback_t txcallback, void *ptag)
{
	bool success = true;

	if (success && g->enabledata && g->initdone) {
		struct qtxeth_queue *q = &g->eth_q;

		success = false;
		if (qtxeth_stage1_enqueue_ready(q)) {
			struct qtxeth *entry = qtxeth_stage1_enqueue_ptr(q);

			entry->ethsegs[0].peth = ptx;
			entry->ethsegs[0].seglen = len;
			entry->totallen = len;
			entry->segcount = 1;
			entry->tsc = tsc;
			entry->txcallback = txcallback;
			entry->priv = ptag;
			qtxeth_stage1_enqueue_done(q);

			tc6_cb_onneedservice(g, g->gtag);
			success = true;
		}
	} else {
		success = false;
	}
	return success;
}

u8 tc6_getrawsegments(struct tc6_t *g, struct tc6_rawtxsegment **psegments)
{
	bool success = false;

	if (g->enabledata && g->initdone) {
		struct qtxeth_queue *q = &g->eth_q;
		struct qtxeth *entry;

		if (qtxeth_stage1_enqueue_ready(q)) {
			entry = qtxeth_stage1_enqueue_ptr(q);
#ifdef debug
			(void)memset(entry, 0xcdu, sizeof(struct qtxeth));
#endif
			*psegments = entry->ethsegs;
			success = true;
		}
	}
	return (success ? TC6_TX_ETH_MAX_SEGMENTS : 0u);
}

bool tc6_sendrawethernetsegments(struct tc6_t *g, const struct tc6_rawtxsegment *psegments,
				 u8 segmentcount, u16 totallen, u8 tsc,
				 tc6_rawtxcallback_t txcallback, void *ptag)
{
	struct qtxeth_queue *q = &g->eth_q;
	bool success = false;

	if (g->enabledata && g->initdone) {
		struct qtxeth *entry = qtxeth_stage1_enqueue_ptr(q);

		entry->segcount = segmentcount;
		entry->totallen = totallen;
		entry->tsc = tsc;
		entry->txcallback = txcallback;
		entry->priv = ptag;
		qtxeth_stage1_enqueue_done(q);
		tc6_cb_onneedservice(g, g->gtag);
		success = true;
	}
	return success;
}

bool tc6_readregister(struct tc6_t *g, u32 addr, tc6_regcallback_t rxcallback,
		      void *tag)
{
	return accessregisters(g, REGISTER_OP_READ, addr
			, 0    /* value */
			, true /* protected */
			, 0    /* mask */
			, rxcallback
			, tag);
}

bool tc6_writeregister(struct tc6_t *g, u32 addr, u32 value,
		       tc6_regcallback_t txcallback, void *tag)
{
	return accessregisters(g, REGISTER_OP_WRITE, addr
			, value
			, true /* protected */
			, 0    /* mask */
			, txcallback
			, tag);
}

bool tc6_readmodifywriteregister(struct tc6_t *g, u32 addr, u32 value, u32 mask,
				 tc6_regcallback_t modifycallback, void *tag)
{
	return accessregisters(g, REGISTER_OP_READWRITE_STAGE1, addr
			, value
			, true /* protected */
			, mask
			, modifycallback
			, tag);
}

u16 tc6_multipleregisteraccess(struct tc6_t *g, const struct memorymap_t *pmap,
			       u16 maplength,
			       tc6_regcallback_t multiplecallback, void *ptag)
{
	u16 i = 0;
	u16 t = 0;
	bool full = false;

	for (; (i < maplength) && !full; i++) {
		switch (pmap[i].op) {
		case memop_write:
			if (accessregisters(g, REGISTER_OP_WRITE,
					    pmap[i].address,
					    pmap[i].value,
					    pmap[i].secure,
					    0,
					    multiplecallback,
					    ptag))
				t++;
			else
				full = true;
			break;
		case memop_readmodifywrite:
			if (accessregisters(g, REGISTER_OP_READWRITE_STAGE1,
					    pmap[i].address,
					    pmap[i].value,
					    pmap[i].secure,
					    pmap[i].mask,
					    multiplecallback,
					    ptag))
				t++;
			else
				full = true;
			break;
		case memop_read:
			if (accessregisters(g, REGISTER_OP_READ,
					    pmap[i].address,
					    0,
					    pmap[i].secure,
					    0,
					    multiplecallback,
					    ptag))
				t++;
			else
				full = true;
			break;
		default:
			i = 0;
			full = true;
			break;
		}
	}
	return t;
}

void tc6_unlockextendedstatus(struct tc6_t *g)
{
	g->exst_locked = false;
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                  private function implementations                    */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static inline u32 mask(u32 width)
{
	return ((1u << (width)) - 1u);
}

static inline u32 fld(u32 pos, u32 width)
{
	return ((((width) << 8u) | (pos)));
}

static inline u32 fld_pos(u32 f)
{
	return ((f) & 255u);
}

static inline u32 fld_mask(u32 f)
{
	return mask(((f) >> (8u & 255u)));
}

static inline u32 get_val(u32 _field, u32 _v32)
{
	return ((_v32) >> (fld_pos(_field))) & (fld_mask(_field));
}

static inline u32 mk_mask(u32 _field, u32 _val)
{
	return ((_val) & (fld_mask(_field))) << (fld_pos(_field));
}

static void initializespientry(struct qspibuf *newentry)
{
	newentry->length = 0;
#ifdef DEBUG
	(void)memset(newentry->txbuff, 0xccu, sizeof(newentry->txbuff));
	(void)memset(newentry->rxbuff, 0xcdu, sizeof(newentry->rxbuff));
#endif
}

static u16 gettrail(struct tc6_t *g, bool enqueueempty)
{
	u16  trail = 0;

	if (g->rca != 0u) {
		trail = (g->rca * TC6_CHUNK_SIZE);
	} else if (enqueueempty) {
		if (0u == g->txc)
			trail = TC6_CHUNK_SIZE;
		else
			trail = (TC6_CHUNKS_PER_ISR * TC6_CHUNK_SIZE);
	}
	return trail;
}

static void addemptychunks(struct tc6_t *g, struct qspibuf *entry, bool enqueueempty)
{
	u16 trail = gettrail(g, enqueueempty);

	if (trail > 0u) {
		u16 i = 0;

		for (; (i < trail) && (entry->length < sizeof(entry->txbuff));
		     i += TC6_CHUNK_SIZE) {
			u8 *p = &entry->txbuff[entry->length];

			p[0] = 0x80u;
			p[1] = 0x0u;
			p[2] = 0x0u;
			p[3] = 0x0u;
			(void)memset(&p[4], 0x00, TC6_CHUNK_SIZE);
			entry->length += TC6_CHUNK_BUF_SIZE;
		}
	}
}

static bool servicedata(struct tc6_t *g, bool enqueueempty)
{
	bool success = false;

	if (g->enabledata && g->initdone &&
			(g->currentop == spi_op_invalid) &&
			(qspibuf_stage1_transfer_ready(&g->qspi))) {
		u16 maxtxlen;
		/**********************************/
		/* try to enqueue ethernet chunks */
		/**********************************/
		struct qspibuf *entry;

		entry = qspibuf_stage1_transfer_ptr(&g->qspi);

		initializespientry(entry);

		/**************************************/
		/* tx data is getting generated here: */
		/**************************************/
		maxtxlen = g->txc * TC6_CHUNK_BUF_SIZE;
		if (maxtxlen > sizeof(entry->txbuff))
			maxtxlen = sizeof(entry->txbuff);
		entry->length = mk_data_tx(g, entry->txbuff, maxtxlen);
		addemptychunks(g, entry, (enqueueempty || g->rca));

		if (entry->length != 0u) {
			qspibuf_stage1_transfer_done(&g->qspi);
			if (spitransaction(g, entry->txbuff,
						entry->rxbuff,
						entry->length,
						spi_op_data)) {
				success = true;
			} else {
				qspibuf_stage1_transfer_undo(&g->qspi);
			}
		}
	}
	return success;
}

static void servicecontrol(struct tc6_t *g)
{
	/****************************************/
	/* control rx and callback higher layer */
	/****************************************/
	while (regop_stage4_modify_ready(&g->regop_q)) {
		struct register_operation *reg_op = NULL;
		u32 regval = 0xffffffffu;
		u16 num;

		reg_op = regop_stage4_modify_ptr(&g->regop_q);
		num = read_rx_ctrl_buffer(reg_op->rx_buf,
				reg_op->length, &regval,
				sizeof(regval),
				reg_op->secure);
		if ((num == 0u) ||
				(reg_op->op != REGISTER_OP_READWRITE_STAGE1) ||
				!modify(g, regval)) {
			regop_stage4_modify_done(&g->regop_q);
			regop_stage5_send_done(&g->regop_q);
			regop_stage6_int_done(&g->regop_q);
		}
	}
	while (regop_stage7_event_ready(&g->regop_q)) {
		struct register_operation *reg_op = NULL;
		tc6_regcallback_t callback;
		void *tag;
		u32 regval = 0xffffffffu;
		u32 regaddr;
		u16 num;
		bool success;

		reg_op = regop_stage7_event_ptr(&g->regop_q);
		num = read_rx_ctrl_buffer(reg_op->rx_buf,
				reg_op->length, &regval,
				sizeof(regval),
				reg_op->secure);
		callback = reg_op->callback;
		regaddr = reg_op->regaddr;
		tag = reg_op->tag;
		success = (num != 0u);
		regop_stage7_event_done(&g->regop_q);
		if (callback)
			reg_op->callback(g, success, regaddr, regval,
					tag, g->gtag);
		else if (!success)
			tc6_cb_onerror(g, tc6error_nohardware,
					g->gtag);
		if (!g->initdone &&
				!regop_stage2_send_ready(&g->regop_q))
			/* no further registers to be sent, init is finished */
			g->initdone = true;
	}

	/***************/
	/* control tx  */
	/***************/
	if ((g->currentop == spi_op_invalid) &&
			regop_stage5_send_ready(&g->regop_q)) {
		/*********************/
		/* modify control tx */
		/*********************/
		struct register_operation *reg_op;

		reg_op = regop_stage5_send_ptr(&g->regop_q);

		regop_stage5_send_done(&g->regop_q);

		g->currentop = spi_op_reg;
		if (!tc6_cb_onspitransaction(g,
					reg_op->tx_buf,
					reg_op->rx_buf,
					reg_op->length,
					g->gtag)) {
			g->currentop = spi_op_invalid;
			regop_stage5_send_undo(&g->regop_q);
		}
	}
	if ((g->currentop == spi_op_invalid) &&
			regop_stage2_send_ready(&g->regop_q)) {
		/*********************/
		/* normal control tx */
		/*********************/
		struct register_operation *reg_op;

		reg_op = regop_stage2_send_ptr(&g->regop_q);

		regop_stage2_send_done(&g->regop_q);

		g->currentop = spi_op_reg;
		if (!tc6_cb_onspitransaction(g,
					reg_op->tx_buf,
					reg_op->rx_buf,
					reg_op->length,
					g->gtag)) {
			g->currentop = spi_op_invalid;
			regop_stage2_send_undo(&g->regop_q);
		}
	}
}

static bool spitransaction(struct tc6_t *g, u8 *ptx, u8 *prx, u16 len, enum spiop_t op)
{
	bool success = false;

	if (g->currentop == spi_op_invalid) {
		g->currentop = op;
		success = tc6_cb_onspitransaction(g, ptx, prx, len,
						  g->gtag);
		if (!success)
			g->currentop = spi_op_invalid;
	}
	return success;
}

static bool modify(struct tc6_t *g, u32 value)
{
	struct register_operation *reg_op;
	u32 val;
	bool success = false;

	reg_op = regop_stage4_modify_ptr(&g->regop_q);
	reg_op->op = REGISTER_OP_READWRITE_STAGE2;
	val = (value & ~reg_op->modifymask) | reg_op->modifyvalue;
	(void)memset(reg_op->tx_buf, 0x00, sizeof(reg_op->tx_buf));
	reg_op->length = mk_secure_ctrl_req(true, false, reg_op->regaddr, 1,
					    &val, reg_op->tx_buf,
					    sizeof(reg_op->tx_buf));
	if (reg_op->length != 0u) {
		regop_stage4_modify_done(&g->regop_q);
		success = true;
	} else {
		tc6_cb_onerror(g, tc6error_controltxfail, g->gtag);
	}
	return success;
}

static bool accessregisters(struct tc6_t *g, enum register_op_type op, u32 addr,
			    u32 value, bool secure, u32 modifymask,
			    tc6_regcallback_t callback, void *tag)
{
	struct register_operation *reg_op;
	u16 payloadsize = 0;
	bool write = true;
	bool success = false;

	if (regop_stage1_enqueue_ready(&g->regop_q)) {
		success = true;
		switch (op) {
		case REGISTER_OP_WRITE:
			write = true;
			break;
		case REGISTER_OP_READ:
			write = false;
			break;
		case REGISTER_OP_READWRITE_STAGE1:
			write = false;
			break;
		case REGISTER_OP_READWRITE_STAGE2:
			write = true;
			break;
		case REGISTER_OP_INVALID:
		default:
			success = false;
			break;
		}
	}
	if (success) {
		reg_op = regop_stage1_enqueue_ptr(&g->regop_q);
#ifdef debug
		(void)memset(reg_op, 0xccu, sizeof(struct register_operation));
#endif
		(void)memset(reg_op->tx_buf, 0x00, sizeof(reg_op->tx_buf));
		if (secure)
			payloadsize = mk_secure_ctrl_req(write, false, addr, 1,
							 &value,
							 reg_op->tx_buf,
							 sizeof(reg_op->tx_buf));
		else
			payloadsize = mk_ctrl_req(write, false, addr, 1,
						  &value, reg_op->tx_buf,
						  sizeof(reg_op->tx_buf));
		if (payloadsize == 0u) {
			tc6_cb_onerror(g, tc6error_controltxfail, g->gtag);
			success = false;
		}
	}
	if (success) {
		if (!g->enabledata)
			g->initdone = false;
		reg_op->regaddr = addr;
		reg_op->length = payloadsize;
		reg_op->op = op;
		reg_op->secure = secure;
		reg_op->callback = callback;
		reg_op->tag = tag;
		reg_op->modifyvalue = value;
		reg_op->modifymask = modifymask;

		regop_stage1_enqueue_done(&g->regop_q);
		tc6_cb_onneedservice(g, g->gtag);
	}
	return success;
}

static void processdatarx(struct tc6_t *g)
{
	/*******************************/
	/* data rx & free up spi queue */
	/*******************************/
	while (qspibuf_stage3_process_ready(&g->qspi)) {
		struct qspibuf *entry = qspibuf_stage3_process_ptr(&g->qspi);

		enqueue_rx_spi(g, entry->rxbuff, entry->length);
		qspibuf_stage3_process_done(&g->qspi);
	}
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*              callback function from protocol statemachine            */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static void on_tx_eth_done(struct tc6_t *g, const u8 *ptr, u16 len,
			   tc6_rawtxcallback_t txcallback, void *priv)
{
	if (txcallback)
		txcallback(g, ptr, len, priv, g->gtag);
}

static void on_rx_slice(struct tc6_t *g, const u8 *pbuf, u16 offset, u16 buflen,
			bool rtsa, bool rtsp)
{
	const u8 *buff = pbuf; /* because of misra warning */
	u16 buf_len = buflen;  /* because of misra warning */
	/* todo: handle timestamp (rtsp) */
	(void)rtsp;

	if (offset == 0u) {
		g->buf_len = 0;
		g->ts = 0;
	}

	/* handle timestamp (rtsa) */
	/* todo: get timestamp according to selected timestamp length (32bit or 64bit) */
	if (rtsa) {
		g->ts = ((u64)buff[0] << 56) |
			((u64)buff[1] << 48) |
			((u64)buff[2] << 40) |
			((u64)buff[3] << 32) |
			((u64)buff[4] << 24) |
			((u64)buff[5] << 16) |
			((u64)buff[6] << 8)  |
			((u64)buff[7]);

		buff = &buff[8];
		buf_len -= 8u;
	}

	g->buf_len += buf_len;
	tc6_cb_onrxethernetslice(g, buff, offset, buf_len, g->gtag);
}

static void on_rx_done(struct tc6_t *g, u16 buf_len, bool mfd)
{
	bool success = !mfd && !g->eth_error;
	(void)buf_len;

	g->eth_error = false;
	if (success) {
		u64 *pts = (g->ts != 0u) ? &g->ts : NULL;

		tc6_cb_onrxethernetpacket(g, true, g->buf_len, pts, g->gtag);
	} else {
		tc6_cb_onrxethernetpacket(g, false, 0, NULL, g->gtag);
	}
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                         protocol statemachine                        */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static inline void value2net(u32 value, u8 *buf)
{
	buf[0] = (u8)(value >> 24);
	buf[1] = (u8)(value >> 16);
	buf[2] = (u8)(value >> 8);
	buf[3] = (u8)value;
}

static inline u32 net2value(const u8 *buf)
{
	return ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) |
		((u32)buf[3]);
}

static u32 get_parity(u32 p)
{
	u32 v = p;

	v ^= v >> 16;
	v ^= v >> 8;
	v ^= v >> 4;
	v ^= v >> 2;
	v ^= v >> 1;
	return ~v & 1u; /* odd parity */
}

/* control transaction api {{{ */

/*
 * the functions below create regular or secure control transaction buffer
 * ready for an spi tx transfer.  the register values for the 'write' operation
 * are taken from the 'tx_buf'.  the content for the spi transfer is generated
 * into the 'tx_buf'.
 *
 * @wnr: write, not read.
 * @aid: disable address increment.
 * @addr:
 *     upper 16 bit: memory map selector (mms).
 *     lower 16 bit: address.
 * @num_regs: number of registers to read or write.
 * @tx_buf: buffer for spi tx transfer.
 *     if operation is 'write', then the first 'num_regs' elements of the
 *     'tx_buf' must contain values to write.
 * @tx_buf_size: size of the 'tx_buf' in bytes.
 *
 * returns number of the 'tx_buf' bytes to be sent using the spi tx transfer.
 * in case of error the function returns 0 and the buffer remains unchanged.
 *
 * security note:
 *
 * the places for turn-around- or other gaps in the 'tx_buf' are kept
 * unchanged.  if needed, the application must clean up the buffer before
 * transfer it to the function.
 */

static u16 mk_ctrl_req(bool wnr, bool aid, u32 addr, u8 num_regs,
		       const u32 *regs, u8 *tx_buf, u16 tx_buf_size)
{
	const u32 hdr =
		mk_mask(hdr_c_dnc, 0) |
		mk_mask(hdr_c_wnr, wnr) |
		mk_mask(hdr_c_aid, aid) |
		mk_mask(hdr_c_mms, addr >> 16) |
		mk_mask(hdr_c_addr, addr) |
		mk_mask(hdr_c_len, num_regs - 1u);
	bool success = true;

	if ((num_regs < 1u) || (num_regs > 128u))
		success = false;

	if ((num_regs + 2u) > (tx_buf_size / 4u))
		success = false;

	if (success) {
		if (wnr) {
			u8 *dst = tx_buf;
			u16 i;

			dst = &dst[num_regs * 4u];
			for (i = num_regs; i > 0u; i--) {
				u32 v = regs[i - 1u];

				value2net(v, dst);
				dst = &dst[-4];
			}
		}

		value2net(hdr | mk_mask(hdr_c_p, get_parity(hdr)), tx_buf);
	}
	return success ? ((num_regs * 4u) + 8u) : 0u;
}

static u16 mk_secure_ctrl_req(bool wnr, bool aid, u32 addr, u8 num_regs,
			      const u32 *regs, u8 *tx_buf, u16 tx_buf_size)
{
	const u32 hdr =
		mk_mask(hdr_c_dnc, 0) |
		mk_mask(hdr_c_wnr, wnr) |
		mk_mask(hdr_c_aid, aid) |
		mk_mask(hdr_c_mms, addr >> 16) |
		mk_mask(hdr_c_addr, addr) |
		mk_mask(hdr_c_len, num_regs - 1u);
	bool success = true;

	if ((num_regs < 1u) || (num_regs > 128u))
		success = false;

	if (success && (((num_regs * 2u) + 2u) > (tx_buf_size / 4u)))
		success = false;

	if (success) {
		if (wnr) {
			u16 i;
			u8 *dst = tx_buf;

			dst = &dst[num_regs * 8u];
			for (i = num_regs; i > 0u; i--) {
				u32 v = regs[i - 1u];

				value2net(~v, dst);
				value2net(v, &dst[-4]);
				dst = &dst[-8];
			}
		}

		value2net(hdr | mk_mask(hdr_c_p, get_parity(hdr)), tx_buf);
	}
	return success ? ((num_regs * 8u) + 8u) : 0u;
}

static u16 read_rx_ctrl_buffer(const u8 *rx_buf, u16 rx_buf_size,
			       u32 *regs_buf, u8 regs_buf_size, bool secure)
{
	u16 i;
	u16 num = 0;
	const u8 *src;

	if (secure) {
		if ((0u == (rx_buf_size % 8u)) && ((rx_buf_size / 8u) >= 2u)) {
			num = (rx_buf_size / 8u) - 1u;
			if ((regs_buf_size / 4u) < num)
				num = 0;
		}

		src = rx_buf;
		for (i = 0; i < num; i++) {
			src = &src[8];
			if (net2value(src) != ~net2value(&src[4]))
				num = 0;
		}
		src = rx_buf;
		for (i = 0; i < num; i++) {
			src = &src[8];
			regs_buf[i] = net2value(src);
		}
	} else {
		num = (rx_buf_size - 8u) / 4u;
		if ((regs_buf_size / 4u) < num) {
			num = 0;
		} else {
			src = &rx_buf[8];
			for (i = 0; i < num; i++) {
				regs_buf[i] = net2value(src);
				src = &src[4];
			}
		}
	}
	return num;
}

/* }}} */

/* tx api {{{ */

void process_tx2(struct tc6_t *g, u8 *tx_buf, u16 tocopy_len, u32 *header)
{
	struct qtxeth_queue *q = &g->eth_q;
	struct qtxeth *entry;
	u16 copy_pos = 0;

	entry = qtxeth_stage2_convert_ptr(q);
	if (entry->totallen <= TC6_CONCAT_THRESHOLD) {
		u16 remaining_len = TC6_CHUNK_SIZE - tocopy_len;

		if (remaining_len && (entry->totallen > remaining_len)) {
			*header |= mk_mask(hdr_sv, 1) | mk_mask(hdr_swo,
					 (tocopy_len / 4u));
			if (entry->tsc != 0u)
				*header |= mk_mask(hdr_tsc, entry->tsc);

			while (copy_pos < remaining_len) {
				const u8 *pbuf = &entry->ethsegs[g->segcurr].peth[g->segoffset];
				u16 len = entry->ethsegs[g->segcurr].seglen - g->segoffset;
				u16 diff = (remaining_len - copy_pos);

				if (len > diff)
					len = diff;
				(void)memcpy(&tx_buf[TC6_HEADER_SIZE +
					     tocopy_len + copy_pos], pbuf,
					     len);
				g->offseteth += len;
				copy_pos += len;
				g->segoffset += len;
				if (g->segoffset == entry->ethsegs[g->segcurr].seglen) {
					g->segoffset = 0;
					g->segcurr++;
					if (g->segcurr == entry->segcount)
						g->segcurr = 0;
				}
			}
		}
	}
}

/*
 * maps longest possible slice from unprocessed eth payload onto a free spi
 * chunk space.
 */
static u16 process_tx(struct tc6_t *g, u8 *tx_buf)
{
	struct qtxeth_queue *q = &g->eth_q;
	struct qtxeth *entry;
	u32 header = mk_mask(hdr_dnc, 1) | mk_mask(hdr_dv, 1);
	u16 tocopy_len;
	u16 padded_len;
	u16 retval = 0;
	u16 copy_pos = 0;
	bool sv = false;

	if (qtxeth_stage2_convert_ready(q)) {
		entry = qtxeth_stage2_convert_ptr(q);
#ifdef DEBUG
		(void)memset(tx_buf, 0xcdu, TC6_CHUNK_BUF_SIZE);
#endif
		header |= mk_mask(hdr_seq, g->seq_num++);
		if (!g->offseteth) {
			header |= mk_mask(hdr_sv, 1) | mk_mask(hdr_swo, 0);
			sv = true;
			if (entry->tsc != 0u)
				header |= mk_mask(hdr_tsc, entry->tsc);
		}
		tocopy_len = entry->totallen - g->offseteth;
		tocopy_len = (tocopy_len <= TC6_CHUNK_SIZE) ? tocopy_len :
			      TC6_CHUNK_SIZE;
		padded_len = TC6_CHUNK_SIZE - tocopy_len;
		while (copy_pos < tocopy_len) {
			const u8 *pbuf = &entry->ethsegs[g->segcurr].peth[g->segoffset];
			u16 len = entry->ethsegs[g->segcurr].seglen - g->segoffset;
			u16 diff = (tocopy_len - copy_pos);

			if (len > diff)
				len = diff;
			(void)memcpy(&tx_buf[TC6_HEADER_SIZE + copy_pos], pbuf, len);
			copy_pos += len;
			g->segoffset += len;
			if (g->segoffset == entry->ethsegs[g->segcurr].seglen) {
				g->segoffset = 0;
				g->segcurr++;
				if (g->segcurr == entry->segcount)
					g->segcurr = 0;
			}
		}
		if (padded_len != 0u)
			(void)memset(&tx_buf[TC6_HEADER_SIZE + tocopy_len], 0xcc, padded_len);
		g->offseteth += tocopy_len;
		if (g->offseteth == entry->totallen) {
			header |= mk_mask(hdr_ev, 1u) | mk_mask(hdr_ebo, tocopy_len - 1u);
			g->offseteth = 0;
			on_tx_eth_done(g, entry->ethsegs[0].peth,
				       entry->totallen, entry->txcallback,
				       entry->priv);
			qtxeth_stage2_convert_done(q);
			if (!sv && qtxeth_stage2_convert_ready(q)) {
				tocopy_len = (tocopy_len + 3u) >> 2u << 2u;
				process_tx2(g, tx_buf, tocopy_len, &header);
			}
		}
		header |= mk_mask(hdr_p, get_parity(header));
		value2net(header, tx_buf);
		retval = TC6_CHUNK_BUF_SIZE;
	}
	return retval;
}

static u16 mk_data_tx(struct tc6_t *g, u8 *tx_buf, u16 tx_buf_len)
{
	u16 pos = 0;

	while (pos < tx_buf_len) {
		u16 result = process_tx(g, &tx_buf[pos]);

		if (!result)
			break;
		pos += result;
	}
	return pos;
}

/* }}} */

/* rx api {{{ */

static inline void signal_rx_error(struct tc6_t *g, enum tc6_error_t err)
{
	g->eth_error = true;
	g->eth_started = false;
	g->offsetrx = 0;
	tc6_cb_onerror(g, err, g->gtag);
}

static inline void process_rx(struct tc6_t *g, const u8 *buff, u16 buf_len)
{
	const u8 *fptr = &buff[buf_len - TC6_HEADER_SIZE];
	u32 footer = net2value(fptr);

	if ((footer & ftr_data_mask) != 0u) {
		u32 sv;
		u32 ev;
		u32 rtsa = 0;
		u32 rtsp = 0;
		u32 mfd;
		u32 sbo;
		u32 ebo;
		u16 len;
		bool twoframes;
		bool success = true;

		sv = get_val(ftr_sv, footer);
		sbo = sv ? (get_val(ftr_swo, footer) * 4u) : 0u;

		ev = get_val(ftr_ev, footer);
		ebo = ev ? (get_val(ftr_ebo, footer) + 1u) : TC6_CHUNK_SIZE;

		mfd = get_val(ftr_fd, footer);
		twoframes = (ebo <= sbo);

		if (twoframes) {
			/* two eth frames in chunk */
			on_rx_slice(g, buff, g->offsetrx, (u16)ebo, rtsa,
				    rtsp);
			on_rx_done(g, (u16)(g->offsetrx + ebo), mfd);
			g->offsetrx = 0;
			len = (u16)(TC6_CHUNK_SIZE - sbo);
		} else {
			/* single eth frame in chunk */
			len = (u16)(ebo - sbo);
		}

		if (!twoframes && sv && g->eth_started) {
			signal_rx_error(g, tc6error_unexpectedsv);
			success = false;
		}

		if (success && !g->eth_started && !g->eth_error && !sv) {
			signal_rx_error(g, tc6error_unexpecteddvev);
			success = false;
		}

		if ((!sv || twoframes) && g->eth_error)
			/* wait for next start valid flag to clear error flag */
			success = false;

		if (success) {
			if (sv != 0u) {
				rtsa = get_val(ftr_rtsa, footer);
				rtsp = get_val(ftr_rtsp, footer);
			}

			g->eth_started = true;
			g->eth_error = false;
			on_rx_slice(g, &buff[sbo], g->offsetrx, len, rtsa,
				    rtsp);

			/* todo: adjust length according to length of timestamp (32bit or 64bit) */
			if (rtsa != 0u)
				len -= 8u;

			if (!twoframes && ev) {
				u16 offset = g->offsetrx;

				g->eth_started = false;
				g->offsetrx = 0;
				on_rx_done(g, offset + len, mfd);
			} else {
				g->offsetrx += len;
			}
		}
	} else {
		g->eth_error = false;
	}
}

static void enqueue_rx_spi(struct tc6_t *g, const u8 *buff, u16 buf_len)
{
	u32 processed;
	bool success = true;

	if (!buf_len || (buf_len % TC6_CHUNK_BUF_SIZE))
		success = false;

	for (processed = 0; success && (processed < buf_len);
	     processed += TC6_CHUNK_BUF_SIZE) {
		u32 footer = net2value(&buff[processed + TC6_CHUNK_SIZE]);

		if ((footer == 0x0u) || (footer == 0xffffffffu)) {
			signal_rx_error(g, tc6error_nohardware);
			success = false;
		}
		if (success && get_parity(footer)) {
			signal_rx_error(g, tc6error_badchecksum);
			success = false;
		}
		if (success && get_val(ftr_hdrb, footer)) {
			signal_rx_error(g, tc6error_badtxdata);
			success = false;
		}
		g->synced = get_val(ftr_sync, footer);
		if (success && !g->synced) {
			signal_rx_error(g, tc6error_synclost);
			success = false;
		}
		if (success && get_val(ftr_fd, footer)) {
			tc6_cb_onrxethernetpacket(g, false, 0, NULL, g->gtag);
			success = false;
		}
		if (success) {
			if (!g->exst_locked) {
				if (get_val(ftr_exst, footer) != 0u) {
					g->exst_locked = true;
					tc6_cb_onextendedstatus(g, g->gtag);
				}
			}
			process_rx(g, &buff[processed], TC6_CHUNK_BUF_SIZE);
		} else {
			g->offsetrx = 0;
			g->eth_error = false;
		}
	}
}

static void update_credit_cnt(struct tc6_t *g, const u8 *buff, u16 buf_len)
{
	const u8 *const last_fptr = &buff[buf_len - TC6_HEADER_SIZE];
	u32 footer = net2value(last_fptr);
	bool success = true;

	if (get_parity(footer) != 0u)
		success = false;
	if (success && get_val(ftr_hdrb, footer))
		success = false;
	if (success && !get_val(ftr_sync, footer))
		success = false;
	if (success) {
		g->txc = get_val(ftr_txc, footer);
		g->rca = get_val(ftr_rca, footer);
	}
}

/* }}} */

/*
 * this function might be called from the interrupt.
 */
void tc6_spibufferdone(struct tc6_t *g, bool success)
{
	struct qspibuf *entry;

	if (!success)
		signal_rx_error(g, tc6error_spierror);
	switch (g->currentop) {
	case spi_op_data:
		if (!qspibuf_stage2_int_ready(&g->qspi))
			break;

		entry = qspibuf_stage2_int_ptr(&g->qspi);
		update_credit_cnt(g, entry->rxbuff, entry->length);
		qspibuf_stage2_int_done(&g->qspi);
		break;
	case spi_op_reg:
		if (regop_stage3_int_ready(&g->regop_q))
			regop_stage3_int_done(&g->regop_q);
		if (regop_stage6_int_ready(&g->regop_q))
			regop_stage6_int_done(&g->regop_q);
		break;
	case spi_op_invalid:
	default:
		break;
	}
	g->currentop = spi_op_invalid;
	tc6_cb_onneedservice(g, g->gtag);
}
