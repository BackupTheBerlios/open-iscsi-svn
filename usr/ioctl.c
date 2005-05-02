/*
 * iSCSI Ioctl/Unix Interface
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "iscsi_if.h"
#include "iscsi_ifev.h"
#include "iscsid.h"
#include "log.h"
#include "iscsi_ipc.h"

#define CTL_DEVICE	"/dev/iscsictl"

static int ctrl_fd;
static void *xmitbuf = NULL;
static int xmitlen = 0;
static void *recvbuf = NULL;
static int recvlen = 0;
static void *ioctl_sendbuf;
static void *ioctl_recvbuf;
static void *pdu_sendbuf;

#define IOCTL_BUF_DEFAULT_MAX \
	(DEFAULT_MAX_RECV_DATA_SEGMENT_LENGTH + sizeof(struct iscsi_hdr))

#define PDU_SENDBUF_DEFAULT_MAX \
	(DEFAULT_MAX_RECV_DATA_SEGMENT_LENGTH + sizeof(struct iscsi_hdr))

static int
kread(char *data, int count)
{
	log_debug(7, "in %s", __FUNCTION__);
	return count;
}

static int
kwritev(enum iscsi_uevent_e type, struct iovec *iovp, int count)
{
	log_debug(7, "in %s", __FUNCTION__);
	return 0;
}

/*
 * __kipc_call() should never block
 */
static int
__kipc_call(void *iov_base, int iov_len)
{
	log_debug(7, "in %s", __FUNCTION__);
	return 0;
}

static int
kcreate_session(uint64_t transport_handle, uint32_t initial_cmdsn,
		uint64_t *out_handle, uint32_t *out_sid)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_CREATE_SESSION;
	ev.transport_handle = transport_handle;
	ev.u.c_session.initial_cmdsn = initial_cmdsn;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}
	if (!ev.r.c_session_ret.session_handle || ev.r.c_session_ret.sid < 0)
		return -EIO;

	*out_handle = ev.r.c_session_ret.session_handle;
	*out_sid = ev.r.c_session_ret.sid;

	return 0;
}

static int
kdestroy_session(uint64_t transport_handle, uint64_t snxh, uint32_t sid)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_DESTROY_SESSION;
	ev.transport_handle = transport_handle;
	ev.u.d_session.session_handle = snxh;
	ev.u.d_session.sid = sid;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	return 0;
}

static int
kcreate_cnx(uint64_t transport_handle, uint64_t snxh, uint32_t sid,
	    uint32_t cid, uint64_t *out_handle)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_CREATE_CNX;
	ev.transport_handle = transport_handle;
	ev.u.c_cnx.session_handle = snxh;
	ev.u.c_cnx.cid = cid;
	ev.u.c_cnx.sid = sid;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}
	if (!ev.r.handle)
		return -EIO;

	*out_handle = ev.r.handle;
	return 0;
}

static int
kdestroy_cnx(uint64_t transport_handle, uint64_t cnxh, int cid)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_DESTROY_CNX;
	ev.transport_handle = transport_handle;
	ev.u.d_cnx.cnx_handle = cnxh;
	ev.u.d_cnx.cid = cid;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	return 0;
}

static int
kbind_cnx(uint64_t transport_handle, uint64_t snxh, uint64_t cnxh,
	  uint32_t transport_fd, int is_leading, int *retcode)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_BIND_CNX;
	ev.transport_handle = transport_handle;
	ev.u.b_cnx.session_handle = snxh;
	ev.u.b_cnx.cnx_handle = cnxh;
	ev.u.b_cnx.transport_fd = transport_fd;
	ev.u.b_cnx.is_leading = is_leading;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	*retcode = ev.r.retcode;

	return 0;
}

static void
ksend_pdu_begin(uint64_t transport_handle, uint64_t cnxh,
			int hdr_size, int data_size)
{
	struct iscsi_uevent *ev;

	log_debug(7, "in %s", __FUNCTION__);

	if (xmitbuf) {
		log_error("send's begin state machine bug?");
		exit(-EIO);
	}

	xmitbuf = pdu_sendbuf;
	memset(xmitbuf, 0, sizeof(*ev) + hdr_size + data_size);
	xmitlen = sizeof(*ev);
	ev = xmitbuf;
	memset(ev, 0, sizeof(*ev));
	ev->type = ISCSI_UEVENT_SEND_PDU;
	ev->transport_handle = transport_handle;
	ev->u.send_pdu.cnx_handle = cnxh;
	ev->u.send_pdu.hdr_size = hdr_size;
	ev->u.send_pdu.data_size = data_size;

	log_debug(3, "send PDU began for hdr %d bytes and data %d bytes",
		hdr_size, data_size);
}

static int
ksend_pdu_end(uint64_t transport_handle, uint64_t cnxh, int *retcode)
{
	int rc;
	struct iscsi_uevent *ev;
	struct iovec iov;

	log_debug(7, "in %s", __FUNCTION__);

	if (!xmitbuf) {
		log_error("send's end state machine bug?");
		exit(-EIO);
	}
	ev = xmitbuf;
	if (ev->u.send_pdu.cnx_handle != cnxh) {
		log_error("send's end state machine corruption?");
		exit(-EIO);
	}

	iov.iov_base = xmitbuf;
	iov.iov_len = xmitlen;

	if ((rc = __kipc_call(xmitbuf, xmitlen)) < 0)
		goto err;
	if (ev->r.retcode) {
		*retcode = ev->r.retcode;
		goto err;
	}
	if (ev->type != ISCSI_UEVENT_SEND_PDU) {
		log_error("bad event: bug on send_pdu_end?");
		exit(-EIO);
	}

	log_debug(3, "send PDU finished for cnx (handle %p)", iscsi_ptr(cnxh));

	xmitbuf = NULL;
	return 0;

err:
	xmitbuf = NULL;
	xmitlen = 0;
	return rc;
}

static int
kset_param(uint64_t transport_handle, uint64_t cnxh,
	       enum iscsi_param param, uint32_t value, int *retcode)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_SET_PARAM;
	ev.transport_handle = transport_handle;
	ev.u.set_param.cnx_handle = cnxh;
	ev.u.set_param.param = param;
	ev.u.set_param.value = value;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	*retcode = ev.r.retcode;

	return 0;
}

static int
kstop_cnx(uint64_t transport_handle, uint64_t cnxh, int flag)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_STOP_CNX;
	ev.transport_handle = transport_handle;
	ev.u.stop_cnx.cnx_handle = cnxh;
	ev.u.stop_cnx.flag = flag;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	return 0;
}

static int
kstart_cnx(uint64_t transport_handle, uint64_t cnxh, int *retcode)
{
	int rc;
	struct iscsi_uevent ev;

	log_debug(7, "in %s", __FUNCTION__);

	memset(&ev, 0, sizeof(struct iscsi_uevent));

	ev.type = ISCSI_UEVENT_START_CNX;
	ev.transport_handle = transport_handle;
	ev.u.start_cnx.cnx_handle = cnxh;

	if ((rc = __kipc_call(&ev, sizeof(ev))) < 0) {
		return rc;
	}

	*retcode = ev.r.retcode;
	return 0;
}

static int
krecv_pdu_begin(uint64_t transport_handle, uint64_t cnxh,
		uintptr_t recv_handle, uintptr_t *pdu_handle, int *pdu_size)
{
	log_debug(7, "in %s", __FUNCTION__);

	if (recvbuf) {
		log_error("recv's begin state machine bug?");
		return -EIO;
	}
	recvbuf = (void*)recv_handle + sizeof(struct iscsi_uevent);
	recvlen = 0;
	*pdu_handle = recv_handle;

	log_debug(3, "recv PDU began, pdu handle 0x%p",
		  (void*)*pdu_handle);

	return 0;
}

static int
krecv_pdu_end(uint64_t transport_handle, uintptr_t conn_handle,
	      uintptr_t pdu_handle)
{
	log_debug(7, "in %s", __FUNCTION__);

	if (!recvbuf) {
		log_error("recv's end state machine bug?");
		return -EIO;
	}

	log_debug(3, "recv PDU finished for pdu handle 0x%p",
		  (void*)pdu_handle);

	recvpool_put((void*)conn_handle, (void*)pdu_handle);
	recvbuf = NULL;
	return 0;
}

static int
ktrans_list(struct iscsi_uevent *ev)
{
	int rc;

	log_debug(7, "in %s", __FUNCTION__);

	memset(ev, 0, sizeof(struct iscsi_uevent));

	ev->type = ISCSI_UEVENT_TRANS_LIST;

	if ((rc = __kipc_call(ev, sizeof(*ev))) < 0) {
		return rc;
	}

	return 0;
}

static int
ctldev_handle(void)
{
	log_debug(7, "in %s", __FUNCTION__);
	return 0;
}

static int
ctldev_open(void)
{
	log_debug(7, "in %s", __FUNCTION__);

	ioctl_sendbuf = calloc(1, IOCTL_BUF_DEFAULT_MAX);
	if (!ioctl_sendbuf) {
		log_error("can not allocate ioctl_sendbuf");
		return -1;
	}

	ioctl_recvbuf = calloc(1, IOCTL_BUF_DEFAULT_MAX);
	if (!ioctl_recvbuf) {
		free(ioctl_sendbuf);
		log_error("can not allocate ioctl_recvbuf");
		return -1;
	}

	pdu_sendbuf = calloc(1, PDU_SENDBUF_DEFAULT_MAX);
	if (!pdu_sendbuf) {
		free(ioctl_recvbuf);
		free(ioctl_sendbuf);
		log_error("can not allocate ioctl_sendbuf");
		return -1;
	}

	return ctrl_fd;
}

static void
ctldev_close(void)
{
	free(pdu_sendbuf);
	free(ioctl_recvbuf);
	free(ioctl_sendbuf);
	//close(ctrl_fd);
}

struct iscsi_ipc ioctl_ipc = {
	.name                   = "Open-iSCSI Kernel IPC/IOCTL v.1",
	.ctldev_bufmax		= IOCTL_BUF_DEFAULT_MAX,
	.ctldev_open		= ctldev_open,
	.ctldev_close		= ctldev_close,
	.ctldev_handle		= ctldev_handle,
	.trans_list		= ktrans_list,
	.create_session         = kcreate_session,
	.destroy_session        = kdestroy_session,
	.create_cnx             = kcreate_cnx,
	.destroy_cnx            = kdestroy_cnx,
	.bind_cnx               = kbind_cnx,
	.set_param              = kset_param,
	.get_param              = NULL,
	.start_cnx              = kstart_cnx,
	.stop_cnx               = kstop_cnx,
	.writev			= kwritev,
	.send_pdu_begin         = ksend_pdu_begin,
	.send_pdu_end           = ksend_pdu_end,
	.read			= kread,
	.recv_pdu_begin         = krecv_pdu_begin,
	.recv_pdu_end           = krecv_pdu_end,
};
struct iscsi_ipc *ipc = &ioctl_ipc;
