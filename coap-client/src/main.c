#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "sensor/sps30/sps30.h"
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include "sensor/scd4x/scd4x.h"

// COAP BEGIN
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <openthread/coap.h>
#include <string.h>

static void coap_init(void)
{
    otInstance *inst = openthread_get_default_instance();
    otError err = otCoapStart(inst, OT_DEFAULT_COAP_PORT);
    if (err != OT_ERROR_NONE)
    {
        printk("Failed to start CoAP: %d\n", err);
    }
}

static void coap_send_data_request(const char *payload)
{
    otInstance *inst = openthread_get_default_instance();
    otError error = OT_ERROR_NONE;
    otMessage *msg = NULL;
    otMessageInfo msg_info;
    const otMeshLocalPrefix *prefix = otThreadGetMeshLocalPrefix(inst);
    uint8_t dst_suffix[8] = {0, 0, 0, 0, 0, 0, 0, 1};

    do
    {
        msg = otCoapNewMessage(inst, NULL);
        if (!msg)
        {
            printk("Failed to allocate CoAP message\n");
            return;
        }

        otCoapMessageInit(msg, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);
        error = otCoapMessageAppendUriPathOptions(msg, "storedata");
        if (error != OT_ERROR_NONE)
            break;

        error = otCoapMessageAppendContentFormatOption(msg, OT_COAP_OPTION_CONTENT_FORMAT_JSON);
        if (error != OT_ERROR_NONE)
            break;

        error = otCoapMessageSetPayloadMarker(msg);
        if (error != OT_ERROR_NONE)
            break;

        error = otMessageAppend(msg, (const void *)payload, strlen(payload));
        if (error != OT_ERROR_NONE)
            break;

        memset(&msg_info, 0, sizeof(msg_info));
        memcpy(&msg_info.mPeerAddr.mFields.m8[0], prefix, 8);
        memcpy(&msg_info.mPeerAddr.mFields.m8[8], dst_suffix, 8);
        msg_info.mPeerPort = OT_DEFAULT_COAP_PORT;

        error = otCoapSendRequest(inst, msg, &msg_info, NULL, NULL);
    } while (false);

    if (error != OT_ERROR_NONE)
    {
        printk("CoAP send failed: %d\n", error);
        if (msg)
            otMessageFree(msg);
    }
    else
    {
        printk("CoAP payload sent.\n");
    }
}
// COAP END

#if !DT_HAS_COMPAT_STATUS_OKAY(sensirion_scd41)
#error "No sensirion,scd4x compatible node found in the device tree"
#endif
#if !DT_HAS_COMPAT_STATUS_OKAY(sensirion_sps30)
#error "No sensirion,sps30 compatible node found in the device tree"
#endif

const struct device *scd41 = DEVICE_DT_GET_ANY(sensirion_scd41);
const struct device *ccs811 = DEVICE_DT_GET_ANY(ams_ccs811);
const struct device *sps30 = DEVICE_DT_GET_ANY(sensirion_sps30);

int main(void)
{
    coap_init(); // COAP INIT CALL

    if (!device_is_ready(scd41) || !device_is_ready(ccs811) || !device_is_ready(sps30))
    {
        printk("Sensor(s) not ready\n");
        return 1;
    }

    struct sensor_value pm_1p0, pm_2p5, pm_10p0, pm_4p0, pm_0p5, pm_1p0_nc, pm_2p5_nc, pm_4p0_nc, pm_10p0_nc, typical_particle_size;
    struct sensor_value co2, temo, humi, eco2, tvoc;
    char json_buf[1024];

    while (true)
    {
        sensor_sample_fetch(scd41);
        sensor_channel_get(scd41, SENSOR_CHAN_CO2_SCD, &co2);
        sensor_channel_get(scd41, SENSOR_CHAN_AMBIENT_TEMP, &temo);
        sensor_channel_get(scd41, SENSOR_CHAN_HUMIDITY, &humi);

        if (sensor_sample_fetch(ccs811) == 0)
        {
            sensor_channel_get(ccs811, SENSOR_CHAN_CO2, &eco2);
            sensor_channel_get(ccs811, SENSOR_CHAN_VOC, &tvoc);
        }

        sensor_sample_fetch(sps30);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_1_0, &pm_1p0);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_2_5, &pm_2p5);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_10, &pm_10p0);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_4_0, &pm_4p0);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_0_5, &pm_0p5);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_1_0_NC, &pm_1p0_nc);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_2_5_NC, &pm_2p5_nc);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_4_0_NC, &pm_4p0_nc);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_10_NC, &pm_10p0_nc);
        sensor_channel_get(sps30, SENSOR_CHAN_PM_TYPICAL_PARTICLE_SIZE, &typical_particle_size);

        snprintf(json_buf, sizeof(json_buf),
                 "<DATA>%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d</DATA>",
                 co2.val1, co2.val2,
                 temo.val1, temo.val2,
                 humi.val1, humi.val2,
                 tvoc.val1, tvoc.val2,
                 pm_2p5.val1, pm_2p5.val2,
                 pm_10p0.val1, pm_10p0.val2);

        printk("%s\n", json_buf);

        // COAP BEGIN
        coap_send_data_request(json_buf);
        // COAP END

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
