/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "hstub_impl.h"
#include "odisk_priv.h"

#include "blast_channel_client.h"

/* This is called when adiskd sends a new object */
static void
recv_object(void *conn_data, struct mrpc_message *msg,  object_x *object)
{
	sdevice_state_t *dev = (sdevice_state_t *)conn_data;
	conn_info_t	*cinfo = &dev->con_data;
	obj_data_t	*obj;
	void		*obj_data = NULL;
	attribute_x	*attr;
	size_t		hdr_len, attr_len = 0;
	unsigned int	i;
	int		err;

	/* Allocate storage for the data. */
	if (object->data.data_len) {
		obj_data = (char *) malloc(object->data.data_len);
		if (obj_data == NULL) {
			log_message(LOGT_NET, LOGL_CRIT,
				    "recv_object: malloc failed");
			return;
		}
	}

	/* allocate an obj_data_t structure to hold the object. */
	obj = odisk_null_obj();
	assert(obj != NULL);

	memcpy(obj_data, object->data.data_val, object->data.data_len);

	obj->data_len = object->data.data_len;
	obj->data = obj_data;

	hdr_len = sizeof(object_x);

	for (i = 0; i < object->attrs.attrs_len; i++) {
		attr = &object->attrs.attrs_val[i];
		err = obj_write_attr(&obj->attr_info, attr->name,
				     attr->data.data_len,
				     (unsigned char *)attr->data.data_val);
		if (err) {
			log_message(LOGT_NET, LOGL_CRIT,
				    "recv_object: obj_write_attr failed");
			return;
		}
		hdr_len += sizeof(attribute_x);
		attr_len += strlen(attr->name) + attr->data.data_len;
	}

	cinfo->stat_obj_rx++;
	cinfo->stat_obj_attr_byte_rx += attr_len;
	cinfo->stat_obj_hdr_byte_rx += hdr_len;
	cinfo->stat_obj_data_byte_rx += object->data.data_len;
	cinfo->stat_obj_total_byte_rx += object->data.data_len +
	    hdr_len + attr_len;

	if (obj->data_len == 0 && attr_len == 0)
	{
		(*dev->cb.search_done_cb) (dev->hcookie);
		odisk_release_obj(obj);
	} else {
		/* XXX put it into the object ring */
		err = ring_enq(dev->obj_ring, obj);
		assert(err == 0);
		cinfo->flags |= CINFO_PENDING_CREDIT;
	}
	return;
}

static const struct blast_channel_client_operations ops = {
	.send_object = recv_object,
};
const struct blast_channel_client_operations *hstub_blast_ops = &ops;


/* This is called when we want to write the credit count. */
void hstub_send_credits(sdevice_state_t *dev)
{
	conn_info_t *cinfo = &dev->con_data;
	credit_x credit = { .credits = 0 };
	mrpc_status_t rc;

	if ((cinfo->flags & CINFO_PENDING_CREDIT) == 0)
		return;

	if (cinfo->obj_limit > ring_count(dev->obj_ring))
		credit.credits = cinfo->obj_limit - ring_count(dev->obj_ring);

	rc = blast_channel_update_credit(cinfo->blast_conn, &credit);

	/* if successful, clear the flag */
	cinfo->flags &= ~CINFO_PENDING_CREDIT;
}

