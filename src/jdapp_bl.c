#include "jdsimple.h"

#ifdef BL


void app_init_services() {}

#define NUM_SERVICES (sizeof(services) / sizeof(services[0]))
static const uint32_t services[] = {JD_SERVICE_CLASS_CTRL, JD_SERVICE_CLASS_BOOTLOADER};

void app_queue_annouce() {
    txq_push(JD_SERVICE_NUMBER_CTRL, JD_CMD_ADVERTISEMENT_DATA, services, sizeof(services));
}

void bl_process(void);
void bl_handle_packet(jd_packet_t *pkt);

void app_handle_packet(jd_packet_t *pkt) {
    bool matched_devid = pkt->device_identifier == device_id();

    if (pkt->flags & JD_FRAME_FLAG_IDENTIFIER_IS_SERVICE_CLASS) {
        for (int i = 0; i < NUM_SERVICES; ++i) {
            if (pkt->device_identifier == services[i]) {
                pkt->service_number = i;
                matched_devid = true;
                break;
            }
        }
    }

    if (!matched_devid)
        return;

    switch (pkt->service_number) {
    case 0:
        ctrl_handle_packet(NULL, pkt);
        break;
    case 1:
        bl_handle_packet(pkt);
        break;
    }
}

void app_process() {
    app_process_frame();

    ctrl_process(NULL);
    bl_process();

    txq_flush();
}

#endif