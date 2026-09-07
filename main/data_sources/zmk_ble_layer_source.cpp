#include "zmk_ble_layer_source.h"

#include <cstring>

#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"

extern "C" void ble_store_config_init(void);

namespace {

constexpr char kTag[] = "zmk_ble_layer";
constexpr char kPeerName[] = "nickey";

// 3a7d9f10-7d8b-4f2c-9a61-6e7e3c5b1a00
const ble_uuid128_t kLayerServiceUuid = BLE_UUID128_INIT(
    0x00, 0x1a, 0x5b, 0x3c, 0x7e, 0x6e, 0x61, 0x9a,
    0x2c, 0x4f, 0x8b, 0x7d, 0x10, 0x9f, 0x7d, 0x3a);

// 3a7d9f11-7d8b-4f2c-9a61-6e7e3c5b1a00
const ble_uuid128_t kLayerCharacteristicUuid = BLE_UUID128_INIT(
    0x00, 0x1a, 0x5b, 0x3c, 0x7e, 0x6e, 0x61, 0x9a,
    0x2c, 0x4f, 0x8b, 0x7d, 0x11, 0x9f, 0x7d, 0x3a);

ZmkBleLayerSource *s_source = nullptr;
uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
uint16_t s_service_start = 0;
uint16_t s_service_end = 0;
uint16_t s_layer_value_handle = 0;
bool s_connecting = false;

int gap_event(struct ble_gap_event *event, void *arg);

void reset_gatt_state()
{
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_service_start = 0;
    s_service_end = 0;
    s_layer_value_handle = 0;
    s_connecting = false;
}

void start_scan()
{
    uint8_t own_addr_type = 0;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot infer BLE address type: %d", rc);
        return;
    }

    ble_gap_disc_params params = {};
    params.passive = 0;
    params.filter_duplicates = 1;
    params.filter_policy = 0;
    params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, gap_event, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(kTag, "Cannot start BLE scan: %d", rc);
    } else {
        ESP_LOGI(kTag, "Scanning for %s", kPeerName);
    }
}

bool is_nickey(const ble_gap_disc_desc &disc)
{
    if (disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return false;
    }

    ble_hs_adv_fields fields = {};
    if (ble_hs_adv_parse_fields(&fields, disc.data, disc.length_data) != 0 ||
        fields.name == nullptr) {
        return false;
    }

    const size_t expected = std::strlen(kPeerName);
    return fields.name_len == expected &&
           std::memcmp(fields.name, kPeerName, expected) == 0;
}

void disconnect_with_error(const char *stage, int rc)
{
    ESP_LOGE(kTag, "%s failed: %d", stage, rc);
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

int on_layer_read(uint16_t conn_handle, const ble_gatt_error *error,
                  ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)arg;
    if (error->status != 0) {
        ESP_LOGW(kTag, "Initial layer read failed: %d", error->status);
        return 0;
    }

    uint8_t layer = 0;
    if (attr != nullptr && attr->om != nullptr &&
        os_mbuf_copydata(attr->om, 0, sizeof(layer), &layer) == 0 &&
        s_source != nullptr) {
        ESP_LOGI(kTag, "Initial layer: %u", layer);
        s_source->handle_layer(layer);
    }
    return 0;
}

int on_cccd_written(uint16_t conn_handle, const ble_gatt_error *error,
                    ble_gatt_attr *attr, void *arg)
{
    (void)attr;
    (void)arg;
    if (error->status != 0) {
        disconnect_with_error("Notification subscription", error->status);
        return 0;
    }

    ESP_LOGI(kTag, "Subscribed to Nickey layer notifications");
    const int rc = ble_gattc_read(conn_handle, s_layer_value_handle,
                                  on_layer_read, nullptr);
    if (rc != 0) {
        ESP_LOGW(kTag, "Could not read initial layer: %d", rc);
    }
    return 0;
}

int on_descriptor(uint16_t conn_handle, const ble_gatt_error *error,
                  uint16_t chr_val_handle, const ble_gatt_dsc *dsc, void *arg)
{
    (void)chr_val_handle;
    (void)arg;
    if (error->status == 0 && dsc != nullptr &&
        ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0) {
        const uint8_t enable_notify[2] = {1, 0};
        const int rc = ble_gattc_write_flat(conn_handle, dsc->handle,
                                            enable_notify, sizeof(enable_notify),
                                            on_cccd_written, nullptr);
        if (rc != 0) {
            disconnect_with_error("CCCD write", rc);
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        disconnect_with_error("CCCD discovery", BLE_HS_ENOENT);
    } else if (error->status != 0) {
        disconnect_with_error("Descriptor discovery", error->status);
    }
    return 0;
}

int on_characteristic(uint16_t conn_handle, const ble_gatt_error *error,
                      const ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (error->status == 0 && chr != nullptr) {
        s_layer_value_handle = chr->val_handle;
        const int rc = ble_gattc_disc_all_dscs(conn_handle,
                                               chr->val_handle,
                                               s_service_end,
                                               on_descriptor, nullptr);
        if (rc != 0) {
            disconnect_with_error("Starting descriptor discovery", rc);
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        disconnect_with_error("Layer characteristic discovery", BLE_HS_ENOENT);
    } else if (error->status != 0) {
        disconnect_with_error("Characteristic discovery", error->status);
    }
    return 0;
}

int on_service(uint16_t conn_handle, const ble_gatt_error *error,
               const ble_gatt_svc *service, void *arg)
{
    (void)arg;
    if (error->status == 0 && service != nullptr) {
        s_service_start = service->start_handle;
        s_service_end = service->end_handle;
        return 0;
    }

    if (error->status != BLE_HS_EDONE) {
        disconnect_with_error("Service discovery", error->status);
        return 0;
    }
    if (s_service_start == 0) {
        disconnect_with_error("Nickey layer service not found", BLE_HS_ENOENT);
        return 0;
    }

    const int rc = ble_gattc_disc_chrs_by_uuid(
        conn_handle, s_service_start, s_service_end,
        &kLayerCharacteristicUuid.u, on_characteristic, nullptr);
    if (rc != 0) {
        disconnect_with_error("Starting characteristic discovery", rc);
    }
    return 0;
}

void discover_layer_service(uint16_t conn_handle)
{
    s_service_start = 0;
    s_service_end = 0;
    const int rc = ble_gattc_disc_svc_by_uuid(conn_handle,
                                              &kLayerServiceUuid.u,
                                              on_service, nullptr);
    if (rc != 0) {
        disconnect_with_error("Starting service discovery", rc);
    }
}

void connect_to(const ble_addr_t &address)
{
    if (s_connecting || s_conn_handle != BLE_HS_CONN_HANDLE_NONE) return;

    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(kTag, "Could not stop scan: %d", rc);
        return;
    }

    uint8_t own_addr_type = 0;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot infer address type for connection: %d", rc);
        start_scan();
        return;
    }

    s_connecting = true;
    ESP_LOGI(kTag, "Found %s; connecting", kPeerName);
    rc = ble_gap_connect(own_addr_type, &address, 30000, nullptr,
                         gap_event, nullptr);
    if (rc != 0) {
        s_connecting = false;
        ESP_LOGE(kTag, "Connection start failed: %d", rc);
        start_scan();
    }
}

int gap_event(ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            if (is_nickey(event->disc)) connect_to(event->disc.addr);
            return 0;

        case BLE_GAP_EVENT_CONNECT:
            s_connecting = false;
            if (event->connect.status != 0) {
                ESP_LOGW(kTag, "Connection failed: %d", event->connect.status);
                reset_gatt_state();
                start_scan();
                return 0;
            }
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(kTag, "Connected; securing link");
            if (ble_gap_security_initiate(s_conn_handle) != 0) {
                // A previously bonded link may already be encrypted. Discovery
                // is safe here; encrypted attributes will reject access if not.
                discover_layer_service(s_conn_handle);
            }
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(kTag, "Secure link established");
                discover_layer_service(event->enc_change.conn_handle);
            } else {
                disconnect_with_error("Link security", event->enc_change.status);
            }
            return 0;

        case BLE_GAP_EVENT_NOTIFY_RX: {
            if (event->notify_rx.attr_handle != s_layer_value_handle ||
                event->notify_rx.om == nullptr ||
                OS_MBUF_PKTLEN(event->notify_rx.om) < 1) {
                return 0;
            }
            uint8_t layer = 0;
            if (os_mbuf_copydata(event->notify_rx.om, 0, sizeof(layer), &layer) == 0 &&
                s_source != nullptr) {
                ESP_LOGI(kTag, "Layer notification: %u", layer);
                s_source->handle_layer(layer);
            }
            return 0;
        }

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGW(kTag, "Disconnected: %d", event->disconnect.reason);
            reset_gatt_state();
            start_scan();
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            ble_gap_conn_desc desc = {};
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
            if (!s_connecting && s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                start_scan();
            }
            return 0;

        default:
            return 0;
    }
}

void on_reset(int reason)
{
    ESP_LOGE(kTag, "NimBLE reset: %d", reason);
    reset_gatt_state();
}

void on_sync()
{
    const int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot create BLE identity: %d", rc);
        return;
    }
    start_scan();
}

void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

}  // namespace

esp_err_t ZmkBleLayerSource::start(LayerChangedCallback callback, void *context)
{
    if (callback == nullptr) return ESP_ERR_INVALID_ARG;
    if (s_source != nullptr) return ESP_ERR_INVALID_STATE;

    callback_ = callback;
    context_ = context;
    s_source = this;

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    if (result != ESP_OK) return result;

    result = nimble_port_init();
    if (result != ESP_OK) return result;

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_store_config_init();

    nimble_port_freertos_init(host_task);
    ESP_LOGI(kTag, "Nickey BLE layer source started");
    return ESP_OK;
}

void ZmkBleLayerSource::handle_layer(uint8_t layer)
{
    if (layer > 9) {
        ESP_LOGW(kTag, "Ignoring out-of-range layer %u", layer);
        return;
    }
    if (layer == current_layer_) return;

    current_layer_ = layer;
    if (callback_ != nullptr) callback_(layer, context_);
}

