/*
 * iSCSI Kernel/User Interface
 *
 * Copyright (C) 2004 Dmitry Yusupov, Alex Aizman
 * maintained by open-iscsi@@googlegroups.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * See the file COPYING included with this distribution for more details.
 */

#ifndef ISCSI_U_H
#define ISCSI_U_H

#ifndef ulong_t
#define ulong_t unsigned long
#endif

#define ISCSI_PROVIDER_NAME_MAXLEN	64
#define ISCSI_PROVIDER_MAX		16
#define UEVENT_BASE			10
#define KEVENT_BASE			100
#define ISCSI_DP_ERR_BASE		1000

typedef enum {
	ISCSI_OK			= 0,

	ISCSI_ERR_BAD_TARGET		= ISCSI_DP_ERR_BASE + 1,
	ISCSI_ERR_DATASN		= ISCSI_DP_ERR_BASE + 2,
	ISCSI_ERR_DATA_OFFSET		= ISCSI_DP_ERR_BASE + 3,
	ISCSI_ERR_MAX_CMDSN		= ISCSI_DP_ERR_BASE + 4,
	ISCSI_ERR_EXP_CMDSN		= ISCSI_DP_ERR_BASE + 5,
	ISCSI_ERR_BAD_OPCODE		= ISCSI_DP_ERR_BASE + 6,
	ISCSI_ERR_DATALEN		= ISCSI_DP_ERR_BASE + 7,
	ISCSI_ERR_AHSLEN		= ISCSI_DP_ERR_BASE + 8,
	ISCSI_ERR_PROTO			= ISCSI_DP_ERR_BASE + 9,
	ISCSI_ERR_LUN			= ISCSI_DP_ERR_BASE + 10,
	ISCSI_ERR_BAD_ITT		= ISCSI_DP_ERR_BASE + 11,
	ISCSI_ERR_CNX_FAILED		= ISCSI_DP_ERR_BASE + 12,
	ISCSI_ERR_R2TSN			= ISCSI_DP_ERR_BASE + 13,
	ISCSI_ERR_SNX_FAILED		= ISCSI_DP_ERR_BASE + 14,
	ISCSI_ERR_HDR_DGST		= ISCSI_DP_ERR_BASE + 15,
	ISCSI_ERR_DATA_DGST		= ISCSI_DP_ERR_BASE + 16,
} iscsi_err_e;

typedef enum {
	ISCSI_PARAM_MAX_RECV_DLENGTH	= 0,
	ISCSI_PARAM_MAX_XMIT_DLENGTH	= 1,
	ISCSI_PARAM_HDRDGST_EN		= 2,
	ISCSI_PARAM_DATADGST_EN		= 3,
	ISCSI_PARAM_INITIAL_R2T_EN	= 4,
	ISCSI_PARAM_MAX_R2T		= 5,
	ISCSI_PARAM_IMM_DATA_EN		= 6,
	ISCSI_PARAM_FIRST_BURST		= 7,
	ISCSI_PARAM_MAX_BURST		= 8,
	ISCSI_PARAM_PDU_INORDER_EN	= 9,
	ISCSI_PARAM_DATASEQ_INORDER_EN	= 10,
	ISCSI_PARAM_ERL			= 11,
	ISCSI_PARAM_IFMARKER_EN		= 12,
	ISCSI_PARAM_OFMARKER_EN		= 13,
} iscsi_param_e;

typedef enum iscsi_uevent_e {
	ISCSI_UEVENT_UNKNOWN		= 0,

	/* down events */
	ISCSI_UEVENT_CREATE_SESSION	= UEVENT_BASE + 1,
	ISCSI_UEVENT_DESTROY_SESSION	= UEVENT_BASE + 2,
	ISCSI_UEVENT_CREATE_CNX		= UEVENT_BASE + 3,
	ISCSI_UEVENT_DESTROY_CNX	= UEVENT_BASE + 4,
	ISCSI_UEVENT_BIND_CNX		= UEVENT_BASE + 5,
	ISCSI_UEVENT_SEND_PDU_BEGIN	= UEVENT_BASE + 6,
	ISCSI_UEVENT_SEND_PDU_END	= UEVENT_BASE + 7,
	ISCSI_UEVENT_RECV_PDU_BEGIN	= UEVENT_BASE + 8,
	ISCSI_UEVENT_RECV_PDU_END	= UEVENT_BASE + 9,
	ISCSI_UEVENT_RECV_REQ		= UEVENT_BASE + 10,
	ISCSI_UEVENT_SET_PARAM		= UEVENT_BASE + 11,
	ISCSI_UEVENT_START_CNX		= UEVENT_BASE + 12,
	ISCSI_UEVENT_STOP_CNX		= UEVENT_BASE + 13,
	ISCSI_UEVENT_CNX_ERROR		= UEVENT_BASE + 14,
	ISCSI_UEVENT_SEND_PDU		= UEVENT_BASE + 15,

	/* up events */
	ISCSI_KEVENT_RECV_PDU		= KEVENT_BASE + 1,
	ISCSI_KEVENT_CNX_ERROR		= KEVENT_BASE + 2,
} iscsi_uevent_e;

typedef struct iscsi_uevent {
	int type; /* k/u events type */
	int provider_id;

	union {
		/* messages u -> k */
		struct msg_create_session {
			ulong_t		session_handle;
			unsigned int	sid;
			unsigned int	initial_cmdsn;
		} c_session;
		struct msg_destroy_session {
			ulong_t		session_handle;
		} d_session;
		struct msg_create_cnx {
			ulong_t		session_handle;
			ulong_t		cnx_handle;
			int		socket_fd;
			unsigned int	cid;
		} c_cnx;
		struct msg_bind_cnx {
			ulong_t		session_handle;
			ulong_t		cnx_handle;
			int		is_leading;
		} b_cnx;
		struct msg_destroy_cnx {
			ulong_t		cnx_handle;
		} d_cnx;
		struct msg_send_pdu {
			int		hdr_size;
			int		data_size;
			ulong_t		cnx_handle;
		} send_pdu;
		struct msg_sp_begin {
			int		hdr_size;
			int		data_size;
			ulong_t		cnx_handle;
		} sp_begin;
		struct msg_sp_end {
			ulong_t		cnx_handle;
		} sp_end;
		struct msg_rp_begin {
			ulong_t		cpcnx_handle;
			ulong_t		recv_handle;
		} rp_begin;
		struct msg_rp_end_req {
			ulong_t		cpcnx_handle;
			ulong_t		pdu_handle;
		} rp_end;
		struct msg_set_param {
			ulong_t		cnx_handle;
			iscsi_param_e	param;
			unsigned int	value;
		} set_param;
		struct msg_start_cnx {
			ulong_t		cnx_handle;
		} start_cnx;
		struct msg_stop_cnx {
			ulong_t		cnx_handle;
		} stop_cnx;
		struct msg_cnxerror_ack {
			ulong_t		cpcnx_handle;
			ulong_t		recv_handle;
		} cnxerror_ack;
	} u;
	union {
		/* messages k -> u */
		ulong_t			handle;
		int			retcode;
		struct msg_recv_req {
			ulong_t		recv_handle;
			ulong_t		cnx_handle;
		} recv_req;
		struct msg_cnx_error {
			ulong_t		cnx_handle;
			iscsi_err_e	error;
		} cnxerror;
		struct msg_rp_begin_rsp {
			ulong_t		pdu_handle;
			unsigned int	pdu_size;
		} rp_begin;
	} r;
} iscsi_uevent_t;

#endif /* ISCSI_U_H */
