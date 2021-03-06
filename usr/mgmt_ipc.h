/*
 * iSCSI Daemon/Admin Management IPC
 *
 * Copyright (C) 2004 Dmitry Yusupov, Alex Aizman
 * maintained by open-iscsi@googlegroups.com
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
#ifndef MGMT_IPC_H
#define MGMT_IPC_H

#include "types.h"
#include "iscsi_if.h"
#include "config.h"

#define ISCSIADM_NAMESPACE	"ISCSIADM_ABSTRACT_NAMESPACE"
#define PEERUSER_MAX		64

typedef enum mgmt_ipc_err {
	MGMT_IPC_OK			= 0,
	MGMT_IPC_ERR			= 1,
	MGMT_IPC_ERR_NOT_FOUND		= 2,
	MGMT_IPC_ERR_NOMEM		= 3,
	MGMT_IPC_ERR_TRANS_FAILURE	= 4,
	MGMT_IPC_ERR_LOGIN_FAILURE	= 5,
	MGMT_IPC_ERR_IDBM_FAILURE	= 6,
	MGMT_IPC_ERR_INVAL		= 7,
	MGMT_IPC_ERR_TRANS_TIMEOUT	= 8,
	MGMT_IPC_ERR_INTERNAL		= 9,
	MGMT_IPC_ERR_LOGOUT_FAILURE	= 10,
	MGMT_IPC_ERR_PDU_TIMEOUT	= 11,
	MGMT_IPC_ERR_TRANS_NOT_FOUND	= 12,
	MGMT_IPC_ERR_ACCESS		= 13,
	MGMT_IPC_ERR_TRANS_CAPS		= 14,
	MGMT_IPC_ERR_EXISTS		= 15,
	MGMT_IPC_ERR_INVALID_REQ	= 16,
	MGMT_IPC_ERR_ISNS_UNAVAILABLE	= 17,
	MGMT_IPC_ERR_ISCSID_COMM_ERR	= 18,
} mgmt_ipc_err_e;

typedef enum iscsiadm_cmd {
	MGMT_IPC_UNKNOWN		= 0,
	MGMT_IPC_SESSION_LOGIN		= 1,
	MGMT_IPC_SESSION_LOGOUT		= 2,
	MGMT_IPC_SESSION_ACTIVESTAT	= 4,
	MGMT_IPC_CONN_ADD		= 5,
	MGMT_IPC_CONN_REMOVE		= 6,
	MGMT_IPC_SESSION_STATS		= 7,
	MGMT_IPC_CONFIG_INAME		= 8,
	MGMT_IPC_CONFIG_IALIAS		= 9,
	MGMT_IPC_CONFIG_FILE		= 10,
	MGMT_IPC_IMMEDIATE_STOP		= 11,
	MGMT_IPC_SESSION_SYNC		= 12,
	MGMT_IPC_SESSION_INFO		= 13,
	MGMT_IPC_ISNS_DEV_ATTR_QUERY	= 14,
	MGMT_IPC_SEND_TARGETS		= 15,
	MGMT_IPC_SET_HOST_PARAM		= 16,
} iscsiadm_cmd_e;

typedef enum iscsi_conn_state_e {
	STATE_FREE,
	STATE_XPT_WAIT,
	STATE_IN_LOGIN,
	STATE_LOGGED_IN,
	STATE_IN_LOGOUT,
	STATE_LOGOUT_REQUESTED,
	STATE_CLEANUP_WAIT,
} iscsi_conn_state_e;

typedef enum iscsi_session_r_stage_e {
	R_STAGE_NO_CHANGE,
	R_STAGE_SESSION_CLEANUP,
	R_STAGE_SESSION_REOPEN,
	R_STAGE_SESSION_REDIRECT,
} iscsi_session_r_stage_e;

/* IPC Request */
typedef struct iscsiadm_req {
	iscsiadm_cmd_e command;

	union {
		/* messages */
		struct ipc_msg_session {
			int sid;
			node_rec_t rec;
		} session;
		struct ipc_msg_conn {
			int sid;
			int cid;
		} conn;
		struct ipc_msg_send_targets {
			int host_no;
			int do_login;
			struct sockaddr_storage ss;
		} st;
		struct ipc_msg_set_host_param {
			int host_no;
			int param;
			/* TODO: make this variable len to support */
			char value[IFNAMSIZ + 1];

		} set_host_param;
	} u;
} iscsiadm_req_t;

/* IPC Response */
typedef struct iscsiadm_rsp {
	iscsiadm_cmd_e command;
	mgmt_ipc_err_e err;

	union {
#define MGMT_IPC_GETSTATS_BUF_MAX	(sizeof(struct iscsi_uevent) + \
					sizeof(struct iscsi_stats) + \
					sizeof(struct iscsi_stats_custom) * \
						ISCSI_STATS_CUSTOM_MAX)
		struct ipc_msg_getstats {
			struct iscsi_uevent ev;
			struct iscsi_stats stats;
			char custom[sizeof(struct iscsi_stats_custom) *
					ISCSI_STATS_CUSTOM_MAX];
		} getstats;
		struct ipc_msg_config {
			char var[VALUE_MAXLEN];
		} config;
		struct ipc_msg_session_state {
			iscsi_session_r_stage_e session_state;
			iscsi_conn_state_e conn_state;
		} session_state;
	} u;
} iscsiadm_rsp_t;

struct iscsi_ipc *ipc;

void need_reap(void);
void event_loop(struct iscsi_ipc *ipc, int control_fd, int mgmt_ipc_fd, int isns_fd);

struct queue_task;
void mgmt_ipc_write_rsp(struct queue_task *qtask, mgmt_ipc_err_e err);
int mgmt_ipc_listen(void);
void mgmt_ipc_close(int fd);

#endif /* MGMT_IPC_H */
