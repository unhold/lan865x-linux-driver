/* SPDX-License-Identifier: GPL-2.0+ */
/* ---------------------------------------------------------------------------------------------- */
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
#ifndef tc6_h_
#define tc6_h_

#include <linux/types.h>

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                          USER ADJUSTABLE                             */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief Platform specific assertion call. Maybe defined to nothing
 */
#ifndef TC6_ASSERT
//#define TC6_ASSERT(condition)  BUG_ON(condition ? false : true)
#endif

/**
 * \brief Set the maximum amount of parallel TC6 instances. 1 means a single
 * MACPHY hardware is attached
 */
#ifndef TC6_MAX_INSTANCES
#define TC6_MAX_INSTANCES   (2u)
#endif

/**
 * \brief Defines the additional payload caused by the TC6 protocol
 * (Footer or Header)
 * \note This value is defined by the Open Alliance, do not modify.
 */
#ifndef TC6_HEADER_SIZE
#define TC6_HEADER_SIZE     (4u)
#endif

/**
 * \brief Defines the data payload length of a single TC6 chunk
 * \note The only valid values are either 32 or 64
 */
#ifndef TC6_CHUNK_SIZE
#define TC6_CHUNK_SIZE      (64u)
#endif

/**
 * \brief Defines the entire payload length of a single TC6 chunk including
 * header / footer.
 * \note Do not modify.
 */
#ifndef TC6_CHUNK_BUF_SIZE
#define TC6_CHUNK_BUF_SIZE  (TC6_CHUNK_SIZE + TC6_HEADER_SIZE)
#endif

/**
 * \brief Set the queue length for control data
 * \note Given length must be power of 2 (2^n).
 */
#ifndef REG_OP_ARRAY_SIZE
#define REG_OP_ARRAY_SIZE   (64u)
#endif

/**
 * \brief Defines the maximum burst length of an SPI transfer. Meaning number
 * of TC6 chunks with a single SPI transaction.
 * \note Depending on the situation, less chunks maybe transferred within a
 * single transaction.
 */
#ifndef TC6_CHUNKS_XACT
#define TC6_CHUNKS_XACT     (22u)
#endif

/**
 * \brief Defines when concatenation of TC6 chunks shall be disabled. Its
 * getting turned off, when the payload length of an Ethernet frame exceeds
 * this given number.
 */
#ifndef TC6_CONCAT_THRESHOLD
#define TC6_CONCAT_THRESHOLD (1024u)
#endif

/**
 * \brief Defines the queue size for holding entire MOSI and MISO data
 */
#ifndef SPI_FULL_BUFFERS
#define SPI_FULL_BUFFERS    (16u)
#endif

/**
 * \brief Defines the queue size for holding pointer to Ethernet frames coming
 * out of the TCP/IP stack.
 * \note Only a reference to the payload is stored, not the entire payload it
 * self.
 */
#ifndef TC6_TX_ETH_QSIZE
#define TC6_TX_ETH_QSIZE    (2u)
#endif

/**
 * \brief Defines the amount of Ethernet segments available to the
 * TC6_GetSendRawSegments and TC6_SendRawEthernetSegments
 * \note If TC6_GetSendRawSegments and TC6_SendRawEthernetSegments are not
 * used set it to 1. Otherwise set the worst case amount of Ethernet segments
 * generated by TCP/IP stack.
 */
#ifndef TC6_TX_ETH_MAX_SEGMENTS
#define TC6_TX_ETH_MAX_SEGMENTS    (1u)
#endif

/**
 * \brief Defines the maximum amount of simultaneous control register
 * operations
 * \note Do not modify.
 */
#ifndef TC6_MAX_CNTRL_VARS
#define TC6_MAX_CNTRL_VARS  (1u)
#endif

struct tc6_t;

/**
 * \brief callback when ever a transmission of raw ethernet packet was
 * finished.
 * \note this function may be implemented by the integrator and passed as
 * argument with tc6_sendrawethernetpacket().
 * \param pinst - the pointer returned by tc6_init.
 * \param ptx - exact the same pointer as has been given along with the
 * tc6_sendrawethernetpacket function.
 * \param len - exact the same length as has been given along with the
 * tc6_sendrawethernetpacket function.
 * \param ptag - tag pointer which was given along tc6_sendrawethernetpacket
 * function.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
typedef void (*tc6_rawtxcallback_t)(struct tc6_t *pinst, const u8 *ptx, u16 len,
				    void *ptag, void *pglobaltag);

/**
 * \brief callback when ever a register access was finished.
 * \note this function may be implemented by the integrator and passed as
 * argument with tc6_readregister() or tc6_writeregister() or
 * tc6_readmodifywriteregister().
 * \note it is safe inside this callback to call again tc6_readregister() or
 * tc6_writeregister() or tc6_readmodifywriteregister().
 * \param pinst - the pointer returned by tc6_init.
 * \param success - true, if the register could be accessed without errors.
 * false, there was an error while trying to access the register.
 * \param addr - the register address, as passed a long with tc6_readregister()
 * or tc6_writeregister() or tc6_readmodifywriteregister().
 * \param value - the register value. if this belongs to a write request, it is
 * the same value as given along with tc6_writeregister(). in case of a read,
 * this holds the register read value. in case of tc6_readmodifywriteregister()
 * it holds the final value, which was written back into the mac/phy.
 * \param ptag - tag pointer which was given along with the register access
 * functions.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
typedef void (*tc6_regcallback_t)(struct tc6_t *pinst, bool success, u32 addr,
				  u32 value, void *ptag,
				  void *pglobaltag);

enum memoryop_t {
	memop_write = 0,
	memop_readmodifywrite = 1,
	memop_read = 2
};

/**
 * \brief structure holding a register read/write
 */
struct memorymap_t {
	u32 address;
	u32 value;
	u32 mask;
	enum memoryop_t op;
	bool secure;
};

struct tc6_rawtxsegment {
	const u8 *peth;        /** pointer to the ethernet packet segment */
	u16 seglen;       /** length of the ethernet packet segment */
};

struct qtxeth {
	void *priv;
	tc6_rawtxcallback_t txcallback;
	struct tc6_rawtxsegment ethsegs[TC6_TX_ETH_MAX_SEGMENTS];
	u16 totallen;
	u8 segcount;
	u8 tsc;
};

#define TC6_CNTRL_BUF_SIZE  ((2u + (TC6_MAX_CNTRL_VARS * 2u)) * 4u)
#define TC6_SPI_BUF_SIZE    (TC6_CHUNKS_XACT * TC6_CHUNK_BUF_SIZE)

struct qspibuf {
	u8 txbuff[TC6_SPI_BUF_SIZE];
	u8 rxbuff[TC6_SPI_BUF_SIZE];
	u16 length;
};

enum register_op_type {
	REGISTER_OP_INVALID,
	REGISTER_OP_WRITE,
	REGISTER_OP_READ,
	REGISTER_OP_READWRITE_STAGE1,
	REGISTER_OP_READWRITE_STAGE2
};

struct register_operation {
	u8 tx_buf[TC6_CNTRL_BUF_SIZE];
	u8 rx_buf[TC6_CNTRL_BUF_SIZE];
	tc6_regcallback_t callback;
	void *tag;
	enum register_op_type op;
	u32 modifyvalue;
	u32 modifymask;
	u32 regaddr;
	u16 length;
	bool secure;
};

/*
 * namespace: qtxeth
 * type: "struct qtxeth"
 * stages: stage1_enqueue stage2_convert
 */

struct qtxeth_queue {
	struct qtxeth *buffer_;
	u32 size_;
	u32 stage1_enqueue_;
	u32 stage2_convert_;
};

/*
 * namespace: qspibuf
 * type: "struct qspibuf"
 * stages: stage1_transfer stage2_int stage3_process
 */

struct qspibuf_queue {
	struct qspibuf *buffer_;
	u32 size_;
	u32 stage1_transfer_;
	u32 stage2_int_;
	u32 stage3_process_;
};

struct regop_queue {
	struct register_operation *buffer_;
	u32 size_;
	u32 stage1_enqueue_;
	u32 stage2_send_;
	u32 stage3_int_;
	u32 stage4_modify_;
	u32 stage5_send_;
	u32 stage6_int_;
	u32 stage7_event_;
};

enum spiop_t {
	spi_op_invalid,
	spi_op_data,
	spi_op_reg
};

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                            definitions                               */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

struct tc6_t {
	struct qtxeth tx_eth_buffer[TC6_TX_ETH_QSIZE];
	struct qspibuf spibuf[SPI_FULL_BUFFERS];
	struct qtxeth_queue eth_q;
	struct qspibuf_queue qspi;
	struct register_operation regop_storage[REG_OP_ARRAY_SIZE];
	struct regop_queue regop_q;
	void *gtag;
	u64 ts;
	enum spiop_t currentop;
	u32 magic;
	u16 buf_len;
	u16 offseteth;
	u16 offsetrx;
	u16 segoffset;
	u8 segcurr;
	u8 instance;
	u8 seq_num;
	u8 txc;
	u8 rca;
	bool alreadyincontrolservice;
	bool alreadyindataservice;
	bool initdone;
	bool enabledata;
	bool synced;
	bool exst_locked;
	bool eth_started;
	bool eth_error;
};

enum tc6_error_t {
	tc6error_succeeded,         /** no error occurred */
	tc6error_nohardware,        /** no macphy hardware available */
	tc6error_unexpectedsv,      /** unexpected start valid flag  */
	tc6error_unexpecteddvev,    /** unexpected data valid or end valid */
	tc6error_badchecksum,       /** checksum in footer is wrong */
	tc6error_unexpectedctrl,    /** unexpected control packet received */
	tc6error_badtxdata,         /** header bad flag received */
	tc6error_synclost,          /** sync flag is no longer set */
	tc6error_spierror,          /** spi transaction failed */
	tc6error_controltxfail,     /** control tx failure */
};

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                            public api                                */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief initializes the lwip interface driver for tc6.
 * \param pglobaltag - this pointer will be returned back with any callback of
 * this component. maybe set to null.
 * \note must be called before any other functions of this component.
 * \return filled structure for further usage with other functions of this
 * component. or null, if there was an error.
 */
void tc6_init(struct tc6_t *pinst);

/**
 * \brief reset all internal state machines and queues
 * \param pinst - the pointer returned by tc6_init.
 */
void tc6_reset(struct tc6_t *pinst);

/**
 * \brief services the hardware and the protocol stack.
 * \note must be called when a) tc6 interrupt became active (level triggered)
 * or b) when the tc6_cb_onneedservice() callback was raised.
 * \param pinst - the pointer returned by tc6_init.
 * \param interruptlevel - the level of the interrupt pin. false is
 * interpreted as interrupt active. true is interpreted as currently no
 * interrupt issued.
 * \return true, if all pending work has been done. false, there is still work
 * to do, but currently not possible.
 */
bool tc6_service(struct tc6_t *pinst, bool interruptlevel);

/**
 * \brief enables or disables outgoing / incoming data traffic.
 * \note it make sense to enable data transfer, when configuring the registers
 * is done.
 * \param pinst - the pointer returned by tc6_init.
 * \param interruptlevel - the level of the interrupt pin. false is
 * interpreted as interrupt active. true is interpreted as currently no
 * interrupt issued.
 */
void tc6_enabledata(struct tc6_t *pinst, bool enable);

/**
 * \brief returns the current instance number of the given tc6 pointer
 * \param pinst - the pointer returned by tc6_init.
 * \return instance number, starting with 0 for the first instance
 */
u8 tc6_getinstance(struct tc6_t *pinst);

/**
 * \brief returns the tc6 related status variables such as tx and rx credit
 * counter and if the controller is in sync state.
 * \param pinst - the pointer returned by tc6_init.
 * \param ptxcredit - pointer to a tx credit variable. this function writes
 * the current tx credit value into this variable. null pointer is accepted.
 * \param prxcredit - pointer to a rx credit variable. this function writes
 * the current rx value into this variable. null pointer is accepted.
 * \param psynced - pointer to a synced variable. this function writes the
 * current synced state into this variable. null pointer is accepted.
 */
void tc6_getstate(struct tc6_t *pinst, u8 *ptxcredit, u8 *prxcredit, bool *psynced);

/**
 * \brief sends a raw ethernet packet.
 * \param pinst - the pointer returned by tc6_init.
 * \param ptx - filled byte array holding an entire ethernet packet. warning,
 * the buffer must stay valid until tc6_cb_ontxrawethernetpacket callback with
 * this pointer as parameter was called.
 * \param len - length of the byte array.
 * \param tsc - a tsc field value of zero indicates to the macphy that it
 * shall not capture a timestamp for this packet.
 * if tsc is [1..3], a timestamp will be captured for this packet
 * will be captured into the corresponding ttscax register.
 * \param txcallback - callback function if desired, null otherwise.
 * \param ptag - any pointer the integrator wants to give. it will be returned
 * in tc6_cb_ontxrawethernetpacket.
 * \return true, on success. false, otherwise.
 */
bool tc6_sendrawethernetpacket(struct tc6_t *pinst, const u8 *ptx, u16 len,
			       u8 tsc, tc6_rawtxcallback_t txcallback,
			       void *ptag);

/**
 * \brief delivers an array of raw segments. integrator can link all ethernet
 * segments to form an entire ethernet frame.
 * \note  after filling out the array structure call
 * tc6_sendrawethernetsegments to send out all segments at once.
 * \param pinst - the pointer returned by tc6_init.
 * \param psegments - pointer of the raw segments will be written to the given
 * address. null if there is no buffer available.
 * \return tc6_tx_eth_max_segments if send buffer is available. 0, otherwise.
 */
u8 tc6_getrawsegments(struct tc6_t *pinst, struct tc6_rawtxsegment **psegments);

/**
 * \brief sends a raw ethernet packet out of several ethernet segments.
 * \param pinst - the pointer returned by tc6_init.
 * \param psegments - filled raw segment array, initial got by calling
 * tc6_getsendrawsegments function.
 * \param segmentcount - amount of ethernet segments. at least 1, maximum
 * tc6_tx_eth_max_segments.
 * \param totallen - total length of entire ethernet frame.
 * \param tsc - a tsc field value of zero indicates to the macphy that it
 * shall not capture a timestamp for this packet.
 * if tsc is [1..3], a timestamp will be captured for this packet
 * will be captured into the corresponding ttscax register.
 * \param txcallback - callback function if desired, null otherwise. note,
 * callback tx pointer will point to the first segment only.
 * \param ptag - any pointer the integrator wants to give. it will be returned
 * in tc6_cb_ontxrawethernetpacket.
 * \return true, on success. false, otherwise.
 */
bool tc6_sendrawethernetsegments(struct tc6_t *pinst,
				 const struct tc6_rawtxsegment *psegments,
				 u8 segmentcount, u16 totallen, u8 tsc,
				 tc6_rawtxcallback_t txcallback, void *ptag);

/**
 * \brief reads from mac / phy registers
 * \param pinst - the pointer returned by tc6_init.
 * \param addr - the 32 bit register offset.
 * \param rxcallback - pointer to a callback handler. may left null.
 * \param ptag - any pointer. will be given back in given rxcallback. may left
 * null.
 * \return true, on success. false, otherwise.
 */
bool tc6_readregister(struct tc6_t *pinst, u32 addr,
		      tc6_regcallback_t rxcallback, void *ptag);

/**
 * \brief writes to mac / phy registers
 * \param pinst - the pointer returned by tc6_init.
 * \param addr - the 32 bit register offset.
 * \param value - the new 32 bit register value.
 * \param txcallback - pointer to a callback handler. may left null.
 * \param ptag - any pointer. will be given back in given txcallback. may left
 * null.
 * \return true, on success. false, otherwise.
 */
bool tc6_writeregister(struct tc6_t *pinst, u32 addr, u32 value,
		       tc6_regcallback_t txcallback, void *ptag);

/**
 * \brief reads, modifies and writes back the changed value to mac / phy
 * registers
 * \param pinst - the pointer returned by tc6_init.
 * \param addr - the 32 bit register offset.
 * \param value - the 32 bit register bit values to be changed. this value
 * will be set to register only if mask on the corresponding position is set to
 * 1.
 * \param mask - the 32 bit register bit mask. only bits set to 1 will be
 * changed accordingly to value.
 * \param modifycallback - pointer to a callback handler. may left null.
 * \param ptag - any pointer. will be given back in given modifycallback. may
 * left null.
 * \return true, on success. false, otherwise.
 */
bool tc6_readmodifywriteregister(struct tc6_t *pinst, u32 addr, u32 value,
				 u32 mask,
				 tc6_regcallback_t modifycallback, void *ptag);

/**
 * \brief execute a list of register commands
 * \param pinst - the pointer returned by tc6_init.
 * \param pmap - array of memory commands
 * \param maplength - the length of the given array.
 * \param multiplecallback - pointer to a callback handler, it will be called
 * for every single entry of the memory map. may left null.
 * \param ptag - any pointer. will be given back in given modifycallback. may
 * left null.
 * \return the amount of register commands enqueued. may return 0 when queue
 * is total full. may return less then maplength when queue is partly full.
 */
u16 tc6_multipleregisteraccess(struct tc6_t *pinst, const struct memorymap_t *pmap,
			       u16 maplength,
			       tc6_regcallback_t multiplecallback, void *ptag);

/**
 * \brief reenable the reporting of extended status flag via
 * tc6_cb_onextendedstatus() callback.
 * \note this feature was introduced to not trigger thousands of extended
 * status callbacks, when there is a lot of traffic ongoing.
 * \param pinst - the pointer returned by tc6_init.
 */
void tc6_unlockextendedstatus(struct tc6_t *pinst);

/**
 * \brief the user must call this function once the spi transaction is
 * finished.
 * \note this function may be called direct in the implementation of
 * tc6_cb_onspitransaction, or any timer later and also from interrupt context.
 * \param tc6instance - the instance number of the hardware. starting with 0
 * for the first.
 * \param success - true, if spi transaction was successful. false, otherwise.
 */
void tc6_spibufferdone(struct tc6_t *pinst, bool success);

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                        callback section                              */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief callback when ever this component needs to be serviced by calling
 * tc6_service()
 * \note this function must be implemented by the integrator.
 * \note do not call tc6_service() within this callback! set a flag or raise an
 * event to do it in the cyclic task context.
 * \warning !! this function may get called from task and interrupt context !!
 * \param pinst - the pointer returned by tc6_init.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
void tc6_cb_onneedservice(struct tc6_t *pinst, void *pglobaltag);

/**
 * \brief callback when ever a slice of an ethernet packet was received.
 * \note this function must be implemented by the integrator.
 * \note in most cases this function will not give an entire ethernet frame.
 * it will be only a piece of it. the integrator needs to combine it.
 * \param pinst - the pointer returned by tc6_init.
 * \param prx - filled byte array holding the received ethernet packet
 * \param offset - 0, if this is the start of a new ethernet frame. otherwise
 * the offset to which this slide of payload belongs to.
 * \param len - length of the byte array.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
void tc6_cb_onrxethernetslice(struct tc6_t *pinst, const u8 *prx, u16 offset,
			      u16 len, void *pglobaltag);

/**
 * \brief callback when ever an ethernet packet was received. this will notify
 * the integrator, that now all chunks very reported by
 * tc6_cb_onrxethernetpacket and the data can be processed.
 * \note this function must be implemented by the integrator.
 * \param pinst - the pointer returned by tc6_init.
 * \param success - true, if the received ethernet frame was received without
 * errors. false, if there were errors.
 * \param len - length of the entire ethernet frame. this is all length
 * reported tc6_cb_onrxethernetpacket combined.
 * \param rxtimestamp - pointer to the receive timestamp, if there was any.
 * null, otherwise. pointer will be invalid after returning out of the callback!
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
void tc6_cb_onrxethernetpacket(struct tc6_t *pinst, bool success, u16 len,
			       u64 *rxtimestamp, void *pglobaltag);

/**
 * \brief callback when ever an error occurred.
 * \note this function must be implemented by the integrator.
 * \param pinst - the pointer returned by tc6_init.
 * \param err - enumeration value holding the actual error condition.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
void tc6_cb_onerror(struct tc6_t *pinst, enum tc6_error_t err, void *pglobaltag);

/**
 * \brief callback when ever tc6 packet was received, which had the extended
 * flag set. this means, that the user should read (and clear) at least status0
 * and status1 registers.
 * \note the integrator must call tc6_unlockextendedstatus() whenever he is
 * ready to process the next extended status flag. reenabling it to early may
 * trigger thousands of events per second.
 * \note this function must be implemented by the integrator.
 * \param pinst - the pointer returned by tc6_init.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 */
void tc6_cb_onextendedstatus(struct tc6_t *pinst, void *pglobaltag);

/**
 * \brief users implementation of spi transfer function.
 * \note the implementation may be synchronous or asynchronous. but in any case
 * the tc6_spibufferdone() must be called, when the spi transaction is over!
 * \param tc6instance - the instance number of the hardware. starting with 0
 * for the first.
 * \param ptx - pointer to the mosi data. the pointer stays valid until user
 * calls tc6_spibufferdone()
 * \param prx - pointer to the miso buffer. the pointer stays valid until user
 * calls tc6_spibufferdone()
 * \param len - the length of both buffers (ptx and prx). the entire length
 * must be transferred via spi.
 * \param pglobaltag - the exact same pointer, which was given along with the
 * tc6_init() function.
 * \return true, if the spi data was enqueued/transferred. false, there was an
 * error.
 */
bool tc6_cb_onspitransaction(struct tc6_t *pinst, u8 *ptx, u8 *prx, u16 len,
			     void *pglobaltag);

#endif /* tc6_h_ */
