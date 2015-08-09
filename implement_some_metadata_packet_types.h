# HG changeset patch
# Parent  580a0540d3c0c0486545e06a586a2cf83bb0c3d2
Handle various informational packet types

- MCU state & firmware info;
- WiFi state & firmware info;
- device/runtime info.

diff --git a/lifx/bulb.h b/lifx/bulb.h
--- a/lifx/bulb.h
+++ b/lifx/bulb.h
@@ -30,17 +30,61 @@
     char        label[LGTD_LIFX_LABEL_SIZE];
     uint64_t    tags;
 };
+
+struct lgtd_lifx_mcu_state {
+    float       signal_strength;    // mW
+    uint32_t    tx_bytes;
+    uint32_t    rx_bytes;
+    uint16_t    reserverd;          // Temperature?
+};
+
+struct lgtd_lifx_wifi_state { // same as mcu_state
+    float       signal_strength;
+    uint32_t    tx_bytes;
+    uint32_t    rx_bytes;
+    uint16_t    reserverd;
+};
+
+struct lgtd_lifx_mcu_firmware_info {
+    uint64_t    built_at;       // ns since epoch
+    uint64_t    installed_at;   // ns since epoch
+    uint32_t    version;
+};
+
+struct lgtd_lifx_wifi_firmware_info { // same as mcu_firmware_info
+    uint64_t    built_at;
+    uint64_t    installed_at;
+    uint32_t    version;
+};
+
+struct lgtd_lifx_product_info {
+    uint32_t    vendor_id;
+    uint32_t    product_id;
+    uint32_t    version;
+};
+
+struct lgtd_lifx_runtime_info {
+    uint64_t    time;       // ns since epoch
+    uint64_t    uptime;     // ns
+    uint64_t    downtime;   // ns, last power off period duration
+};
 #pragma pack(pop)
 
 struct lgtd_lifx_bulb {
-    RB_ENTRY(lgtd_lifx_bulb)        link;
-    SLIST_ENTRY(lgtd_lifx_bulb)     link_by_gw;
-    struct lgtd_lifx_gateway        *gw;
-    uint8_t                         addr[LGTD_LIFX_ADDR_LENGTH];
-    struct lgtd_lifx_light_state    state;
-    lgtd_time_mono_t                last_light_state_at;
-    lgtd_time_mono_t                dirty_at;
-    uint16_t                        expected_power_on;
+    RB_ENTRY(lgtd_lifx_bulb)            link;
+    SLIST_ENTRY(lgtd_lifx_bulb)         link_by_gw;
+    lgtd_time_mono_t                    last_light_state_at;
+    lgtd_time_mono_t                    dirty_at;
+    uint16_t                            expected_power_on;
+    uint8_t                             addr[LGTD_LIFX_ADDR_LENGTH];
+    struct lgtd_lifx_gateway            *gw;
+    struct lgtd_lifx_light_state        state;
+    struct lgtd_lifx_mcu_state          mcu_state;
+    struct lgtd_lifx_wifi_state         wifi_state;
+    struct lgtd_lifx_mcu_firmware_info  mcu_fw_info;
+    struct lgtd_lifx_wifi_firmware_info wifi_fw_info;
+    struct lgtd_lifx_product_info       product_info;
+    struct lgtd_lifx_runtime_info       runtime_info;
 };
 RB_HEAD(lgtd_lifx_bulb_map, lgtd_lifx_bulb);
 SLIST_HEAD(lgtd_lifx_bulb_list, lgtd_lifx_bulb);
diff --git a/lifx/wire_proto.c b/lifx/wire_proto.c
--- a/lifx/wire_proto.c
+++ b/lifx/wire_proto.c
@@ -74,12 +74,32 @@
     (void)pkt;
 }
 
+static void
+lgtd_lifx_wire_enosys_packet_handler(struct lgtd_lifx_gateway *gw,
+                                     const struct lgtd_lifx_packet_header *hdr,
+                                     const void *pkt)
+{
+    (void)pkt;
+
+    const struct lgtd_lifx_packet_info *pkt_info;
+    pkt_info = lgtd_lifx_wire_get_packet_info(hdr->packet_type);
+    bool addressable = hdr->protocol & LGTD_LIFX_PROTOCOL_ADDRESSABLE;
+    bool tagged = hdr->protocol & LGTD_LIFX_PROTOCOL_TAGGED;
+    unsigned int protocol = hdr->protocol & LGTD_LIFX_PROTOCOL_VERSION_MASK;
+    lgtd_info(
+        "%s <-- [%s]:%hu - (Unimplemented, header info: "
+        "addressable=%d, tagged=%d, protocol=%d)",
+        pkt_info->name, gw->ip_addr, gw->port,
+        addressable, tagged, protocol
+    );
+}
+
 void
 lgtd_lifx_wire_load_packet_info_map(void)
 {
 #define DECODER(x)  ((void (*)(void *))(x))
 #define ENCODER(x)  ((void (*)(void *))(x))
-#define HANDLER(x)                                  \
+#define HANDLER(x)                                      \
     ((void (*)(struct lgtd_lifx_gateway *,              \
                const struct lgtd_lifx_packet_header *,  \
                const void *))(x))
@@ -90,6 +110,10 @@
 #define REQUEST_ONLY                                        \
     .decode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
     .handle = lgtd_lifx_wire_null_packet_handler
+#define UNIMPLEMENTED                                       \
+    .decode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
+    .encode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
+    .handle = lgtd_lifx_wire_enosys_packet_handler
 
     static struct lgtd_lifx_packet_info packet_table[] = {
         // Gateway packets:
@@ -187,6 +211,211 @@
             .size = sizeof(struct lgtd_lifx_packet_tags),
             .decode = DECODER(lgtd_lifx_wire_decode_tags),
             .handle = HANDLER(lgtd_lifx_gateway_handle_tags)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_MESH_INFO",
+            .type = LGTD_LIFX_GET_MESH_INFO
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "MESH_INFO",
+            .type = LGTD_LIFX_MESH_INFO,
+            .size = sizeof(struct lgtd_lifx_packet_mcu_state),
+            .decode = DECODER(lgtd_lifx_wire_decode_uc_state),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_mcu_state)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_MESH_FIRMWARE",
+            .type = LGTD_LIFX_GET_MESH_FIRMWARE
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "MESH_FIRMWARE",
+            .type = LGTD_LIFX_MESH_FIRMWARE,
+            .size = sizeof(struct lgtd_lifx_packet_mcu_firmware_info),
+            .decode = DECODER(lgtd_lifx_decode_firmware_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_mcu_firmware_info)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_WIFI_INFO",
+            .type = LGTD_LIFX_GET_WIFI_INFO,
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "WIFI_INFO",
+            .type = LGTD_LIFX_WIFI_INFO,
+            .size = sizeof(struct lgtd_lifx_packet_wifi_state),
+            .decode = DECODER(lgtd_lifx_wire_decode_uc_state),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_wifi_state)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_WIFI_FIRMWARE_STATE",
+            .type = LGTD_LIFX_GET_WIFI_FIRMWARE_STATE
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "WIFI_FIRMWARE_STATE",
+            .type = LGTD_LIFX_WIFI_FIRMWARE_STATE,
+            .size = sizeof(struct lgtd_lifx_packet_wifi_firmware_info),
+            .decode = DECODER(lgtd_lifx_wire_decode_firmware_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_wifi_firmware_info)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_VERSION",
+            .type = LGTD_LIFX_GET_VERSION
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "VERSION_STATE",
+            .type = LGTD_LIFX_VERSION_STATE,
+            .size = sizeof(struct lgtd_lifx_packet_product_info),
+            .decode = DECODER(lgtd_lifx_wire_decode_product_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_product_info)
+        },
+        {
+            REQUEST_ONLY,
+            NO_PAYLOAD,
+            .name = "GET_INFO",
+            .type = LGTD_LIFX_GET_INFO
+        },
+        {
+            RESPONSE_ONLY,
+            .name = "INFO_STATE",
+            .type = LGTD_LIFX_INFO_STATE,
+            .size = sizeof(struct lgtd_lifx_packet_runtime_info),
+            .decode = DECODER(lgtd_lifx_wire_decode_runtime_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_runtime_info)
+        },
+        // Unimplemented packets
+        {
+            UNIMPLEMENTED,
+            .name = "GET_TIME",
+            .type = LGTD_LIFX_GET_TIME
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_TIME",
+            .type = LGTD_LIFX_SET_TIME
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "TIME_STATE",
+            .type = LGTD_LIFX_TIME_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "GET_RESET_SWITCH_STATE",
+            .type = LGTD_LIFX_GET_RESET_SWITCH_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "RESET_SWITCH_STATE",
+            .type = LGTD_LIFX_RESET_SWITCH_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "GET_BULB_LABEL",
+            .type = LGTD_LIFX_GET_BULB_LABEL
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_BULB_LABEL",
+            .type = LGTD_LIFX_SET_BULB_LABEL
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "BULB_LABEL",
+            .type = LGTD_LIFX_BULB_LABEL
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "GET_MCU_RAIL_VOLTAGE",
+            .type = LGTD_LIFX_GET_MCU_RAIL_VOLTAGE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "MCU_RAIL_VOLTAGE",
+            .type = LGTD_LIFX_MCU_RAIL_VOLTAGE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "REBOOT",
+            .type = LGTD_LIFX_REBOOT
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_FACTORY_TEST_MODE",
+            .type = LGTD_LIFX_SET_FACTORY_TEST_MODE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "DISABLE_FACTORY_TEST_MODE",
+            .type = LGTD_LIFX_DISABLE_FACTORY_TEST_MODE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "ACK",
+            .type = LGTD_LIFX_ACK
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "ECHO_REQUEST",
+            .type = LGTD_LIFX_ECHO_REQUEST
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "ECHO_RESPONSE",
+            .type = LGTD_LIFX_ECHO_RESPONSE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_DIM_ABSOLUTE",
+            .type = LGTD_LIFX_SET_DIM_ABSOLUTE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_DIM_RELATIVE",
+            .type = LGTD_LIFX_SET_DIM_RELATIVE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "GET_WIFI_STATE",
+            .type = LGTD_LIFX_GET_WIFI_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_WIFI_STATE",
+            .type = LGTD_LIFX_SET_WIFI_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "WIFI_STATE",
+            .type = LGTD_LIFX_WIFI_STATE
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "GET_ACCESS_POINTS",
+            .type = LGTD_LIFX_GET_ACCESS_POINTS
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "SET_ACCESS_POINTS",
+            .type = LGTD_LIFX_SET_ACCESS_POINTS
+        },
+        {
+            UNIMPLEMENTED,
+            .name = "ACCESS_POINT",
+            .type = LGTD_LIFX_ACCESS_POINT
         }
     };
 
diff --git a/lifx/wire_proto.h b/lifx/wire_proto.h
--- a/lifx/wire_proto.h
+++ b/lifx/wire_proto.h
@@ -249,6 +249,43 @@
     char        label[LGTD_LIFX_LABEL_SIZE];
 };
 
+struct lgtd_lifx_packet_mcu_state {
+    floatle_t   signal_strength;
+    uint32le_t  tx_bytes;
+    uint32le_t  rx_bytes;
+    uint16le_t  reserverd;
+};
+
+struct lgtd_lifx_packet_wifi_state {
+    floatle_t   signal_strength;
+    uint32le_t  tx_bytes;
+    uint32le_t  rx_bytes;
+    uint16le_t  reserverd;
+};
+
+struct lgtd_lifx_packet_mcu_firmware_info {
+    uint64le_t  built_at;
+    uint64le_t  installed_at;
+    uint32le_t  version;
+};
+
+struct lgtd_lifx_packet_wifi_firmware_info {
+    uint64le_t  built_at;
+    uint64le_t  installed_at;
+    uint32le_t  version;
+};
+
+struct lgtd_lifx_packet_product_info {
+    uint32le_t  vendor_id;
+    uint32le_t  product_id;
+    uint32le_t  version;
+};
+
+struct lgtd_lifx_packet_runtime_info {
+    uint64le_t  time;
+    uint64le_t  uptime;
+    uint64le_t  downtime;
+};
 #pragma pack(pop)
 
 enum lgtd_lifx_header_flags {
