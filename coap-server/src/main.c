/*
 * Simple CoAP “/storedata” server.
 * Listens for PUTs on coap://[fdde:ad00:beef::1]/storedata
 * ACKs with 2.04 Changed and logs the payload.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_srv, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/net/openthread.h>
#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/coap.h>

#define TEXT_BUF_SZ 256
static char text_buf[TEXT_BUF_SZ];
static size_t text_len;

/* Add ::0001 mesh-local address so clients can reach us */
static void add_meshlocal_routing_id_addr(void)
{
	otInstance *inst = openthread_get_default_instance();
	otNetifAddress addr = { 0 };

	memcpy(&addr.mAddress, otThreadGetMeshLocalPrefix(inst), 8);
	addr.mAddress.mFields.m16[4] = sys_cpu_to_be16(0x0000);
	addr.mAddress.mFields.m16[7] = sys_cpu_to_be16(0x0001);

	addr.mPrefixLength  = 64;
	addr.mPreferred     = true;
	addr.mValid         = true;
	addr.mAddressOrigin = OT_ADDRESS_ORIGIN_THREAD;

	otError err = otIp6AddUnicastAddress(inst, &addr);
	if (err != OT_ERROR_NONE && err != OT_ERROR_ALREADY) {
		LOG_ERR("Add unicast addr failed (%d)", err);
	}
}

static void storedata_reply(const otMessage *req, const otMessageInfo *req_info)
{
	otInstance *inst = openthread_get_default_instance();
	otMessage *rsp = otCoapNewMessage(inst, NULL);

	if (!rsp) {
		LOG_ERR("No mem for CoAP ACK");
		return;
	}

	otError err = otCoapMessageInitResponse(rsp, req,
		OT_COAP_TYPE_ACKNOWLEDGMENT,
		OT_COAP_CODE_CHANGED);
	if (err == OT_ERROR_NONE) {
		err = otCoapSendResponse(inst, rsp, req_info);
	}

	if (err != OT_ERROR_NONE) {
		otMessageFree(rsp);
		LOG_ERR("Send CoAP ACK failed (%d)", err);
	}
}

static void storedata_cb(void *context, otMessage *msg, const otMessageInfo *msg_info)
{
	ARG_UNUSED(context);

	if (otCoapMessageGetCode(msg) != OT_COAP_CODE_PUT) {
		return;
	}

	text_len = otMessageRead(msg, otMessageGetOffset(msg), text_buf, TEXT_BUF_SZ - 1);
	text_buf[text_len] = '\0';

	// LOG_INF("PUT /storedata : \"%s\"", text_buf);
	printk("%s", text_buf);

	if (otCoapMessageGetType(msg) == OT_COAP_TYPE_CONFIRMABLE) {
		storedata_reply(msg, msg_info);
	}
}

static void coap_init(void)
{
	otInstance *inst = openthread_get_default_instance();
	otError err = otCoapStart(inst, OT_DEFAULT_COAP_PORT);

	if (err != OT_ERROR_NONE) {
		LOG_ERR("CoAP start failed (%d)", err);
		return;
	}

	static otCoapResource res = {
		.mUriPath = "storedata",
		.mHandler = storedata_cb,
		.mContext = NULL,
		.mNext = NULL,
	};

	otCoapAddResource(inst, &res);
	LOG_INF("CoAP resource \"/storedata\" registered");
}

int main(void)
{
	LOG_INF("CoAP server start");

	/* Wait until OpenThread reaches 'attached' state */
	struct openthread_context *ot_ctx = openthread_get_default_context();
	while (!ot_ctx || !otThreadGetDeviceRole(ot_ctx->instance)) {
		k_sleep(K_MSEC(100));
	}

	add_meshlocal_routing_id_addr();
	coap_init();

	while (true) {
		k_sleep(K_SECONDS(1));
	}
}
