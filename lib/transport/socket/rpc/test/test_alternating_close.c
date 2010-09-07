/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#define ITERS 100

#include <glib.h>
#include "common.h"

gint serv_can_close;

static mrpc_status_t do_ping(void *conn)
{
	if (g_atomic_int_get(&serv_can_close))
		expect(mrpc_conn_close(conn), 0);
	return MINIRPC_OK;
}

static const struct proto_server_operations ops = {
	.ping = do_ping,
};

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int i;
	int a, b;

	g_thread_init(NULL);
	for (i=0; i<ITERS; i++) {
		get_conn_pair(&a, &b);
		expect(mrpc_conn_create(&sconn, proto_server, a, NULL), 0);
		proto_server_set_operations(sconn, &ops);
		start_monitored_dispatcher(sconn);
		expect(mrpc_conn_create(&conn, proto_client, b, NULL), 0);
		g_atomic_int_set(&serv_can_close, i % 2);
		if (i % 2)
			expect(proto_ping(conn), MINIRPC_NETWORK_FAILURE);
		else
			expect(proto_ping(conn), 0);
		mrpc_conn_free(conn);
		dispatcher_barrier();
	}
	return 0;
}
