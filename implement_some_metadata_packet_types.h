# HG changeset patch
# Parent  580a0540d3c0c0486545e06a586a2cf83bb0c3d2
Handle various informational packet types

- MCU state & firmware info;
- WiFi state & firmware info;
- device/runtime info.

diff --git a/core/lightsd.h b/core/lightsd.h
--- a/core/lightsd.h
+++ b/core/lightsd.h
@@ -24,10 +24,33 @@
 #define LGTD_ABS(v) ((v) >= 0 ? (v) : (v) * -1)
 #define LGTD_MIN(a, b) ((a) < (b) ? (a) : (b))
 #define LGTD_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
-#define LGTD_MSECS_TO_TIMEVAL(v) { \
+#define LGTD_MSECS_TO_TIMEVAL(v) {  \
     .tv_sec = (v) / 1000,           \
     .tv_usec = ((v) % 1000) * 1000  \
 }
+#define LGTD_NSECS_TO_USECS(v) ((v) / (unsigned int)10E6)
+#define LGTD_NSECS_TO_SECS(v) ((v) / (unsigned int)10E9)
+#define LGTD_SECS_TO_NSECS(v) ((v) * (unsigned int)10E9)
+#define LGTD_TM_TO_ISOTIME(tm, sbuf, bufsz, usec) do {                          \
+    /* '2015-01-02T10:13:16.132222+00:00' */                                    \
+    if ((usec)) {                                                               \
+        snprintf(                                                               \
+            (sbuf), (bufsz), "%d-%02d-%02dT%02d:%02d:%02d.%jd%c%02ld:%02ld",    \
+            1900 + (tm)->tm_year, 1 + (tm)->tm_mon, (tm)->tm_mday,              \
+            (tm)->tm_hour, (tm)->tm_min, (tm)->tm_sec, (intmax_t)usec,          \
+            (tm)->tm_gmtoff >= 0 ? '+' : '-', /* %+02ld doesn't work */         \
+            LGTD_ABS((tm)->tm_gmtoff / 60 / 60), (tm)->tm_gmtoff % (60 * 60)    \
+        );                                                                      \
+    } else {                                                                    \
+        snprintf(                                                               \
+            (sbuf), (bufsz), "%d-%02d-%02dT%02d:%02d:%02d%c%02ld:%02ld",        \
+            1900 + (tm)->tm_year, 1 + (tm)->tm_mon, (tm)->tm_mday,              \
+            (tm)->tm_hour, (tm)->tm_min, (tm)->tm_sec,                          \
+            (tm)->tm_gmtoff >= 0 ? '+' : '-', /* %+02ld doesn't work */         \
+            LGTD_ABS((tm)->tm_gmtoff / 60 / 60), (tm)->tm_gmtoff % (60 * 60)    \
+        );                                                                      \
+    }                                                                           \
+} while (0)
 
 enum lgtd_verbosity {
     LGTD_DEBUG = 0,
@@ -51,6 +74,11 @@
 void lgtd_sockaddrtoa(const struct sockaddr_storage *, char *buf, int buflen);
 short lgtd_sockaddrport(const struct sockaddr_storage *);
 
+void lgtd_print_duration(uint64_t, char *, int);
+#define LGTD_PRINT_DURATION(secs, arr) do {             \
+    lgtd_print_duration((secs), (arr), sizeof((arr)));  \
+} while (0)
+
 void _lgtd_err(void (*)(int, const char *, ...), int, const char *, ...)
     __attribute__((format(printf, 3, 4)));
 #define lgtd_err(eval, fmt, ...) _lgtd_err(err, (eval), (fmt), ##__VA_ARGS__);
diff --git a/core/log.c b/core/log.c
--- a/core/log.c
+++ b/core/log.c
@@ -52,14 +52,7 @@
     if (!localtime_r(&now.tv_sec, &tm_now)) {
         goto error;
     }
-    // '2015-01-02T10:13:16.132222+00:00'
-    snprintf(
-        strbuf, bufsz, "%d-%02d-%02dT%02d:%02d:%02d.%jd%c%02ld:%02ld",
-        1900 + tm_now.tm_year, 1 + tm_now.tm_mon, tm_now.tm_mday,
-        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
-        (intmax_t)now.tv_usec, tm_now.tm_gmtoff >= 0 ? '+' : '-', // %+02ld doesn't work
-        LGTD_ABS(tm_now.tm_gmtoff / 60 / 60), tm_now.tm_gmtoff % (60 * 60)
-    );
+    LGTD_TM_TO_ISOTIME(&tm_now, strbuf, bufsz, now.tv_usec);
     return;
 error:
     strbuf[0] = '\0';
@@ -110,6 +103,26 @@
 }
 
 void
+lgtd_print_duration(uint64_t secs, char *buf, int bufsz)
+{
+    assert(buf);
+    assert(bufsz > 0);
+
+    int days = secs / (60 * 60 * 24);
+    int minutes = secs / 60;
+    int hours = minutes / 60;
+    hours = hours % 24;
+    minutes = minutes % 60;
+
+    int i = 0;
+    if (days) {
+        int n = snprintf(buf, bufsz, "%d days ", days);
+        i = LGTD_MIN(i + n, bufsz);
+    }
+    snprintf(&buf[i], bufsz - i, "%02d:%02d", hours, minutes);
+}
+
+void
 _lgtd_err(void (*errfn)(int, const char *, ...),
            int eval,
            const char *fmt,
diff --git a/lifx/bulb.c b/lifx/bulb.c
--- a/lifx/bulb.c
+++ b/lifx/bulb.c
@@ -154,3 +154,69 @@
 
     bulb->state.tags = tags;
 }
+
+void
+lgtd_lifx_bulb_set_mcu_state(struct lgtd_lifx_bulb *bulb,
+                             const struct lgtd_lifx_ip_state *state,
+                             lgtd_time_mono_t received_at)
+{
+    assert(bulb);
+    assert(state);
+
+    bulb->last_mcu_state_at = received_at;
+    memcpy(&bulb->mcu_state, state, sizeof(bulb->mcu_state));
+}
+
+void
+lgtd_lifx_bulb_set_mcu_firmware_info(struct lgtd_lifx_bulb *bulb,
+                                     const struct lgtd_lifx_ip_firmware_info *info)
+{
+    assert(bulb);
+    assert(info);
+
+    memcpy(&bulb->mcu_fw_info, info, sizeof(bulb->mcu_fw_info));
+}
+
+void
+lgtd_lifx_bulb_set_wifi_state(struct lgtd_lifx_bulb *bulb,
+                              const struct lgtd_lifx_ip_state *state,
+                              lgtd_time_mono_t received_at)
+{
+    assert(bulb);
+    assert(state);
+
+    bulb->last_wifi_state_at = received_at;
+    memcpy(&bulb->wifi_state, state, sizeof(bulb->wifi_state));
+}
+
+void
+lgtd_lifx_bulb_set_wifi_firmware_info(struct lgtd_lifx_bulb *bulb,
+                                      const struct lgtd_lifx_ip_firmware_info *info)
+{
+    assert(bulb);
+    assert(info);
+
+    memcpy(&bulb->wifi_fw_info, info, sizeof(bulb->wifi_fw_info));
+}
+
+void
+lgtd_lifx_bulb_set_product_info(struct lgtd_lifx_bulb *bulb,
+                                const struct lgtd_lifx_product_info *info)
+{
+    assert(bulb);
+    assert(info);
+
+    memcpy(&bulb->product_info, info, sizeof(bulb->product_info));
+}
+
+void
+lgtd_lifx_bulb_set_runtime_info(struct lgtd_lifx_bulb *bulb,
+                                const struct lgtd_lifx_runtime_info *info,
+                                lgtd_time_mono_t received_at)
+{
+    assert(bulb);
+    assert(info);
+
+    bulb->last_runtime_info_at = received_at;
+    memcpy(&bulb->runtime_info, info, sizeof(bulb->runtime_info));
+}
diff --git a/lifx/bulb.h b/lifx/bulb.h
--- a/lifx/bulb.h
+++ b/lifx/bulb.h
@@ -30,17 +30,51 @@
     char        label[LGTD_LIFX_LABEL_SIZE];
     uint64_t    tags;
 };
+
+struct lgtd_lifx_ip_state {
+    float       signal_strength;    // mW
+    uint32_t    tx_bytes;
+    uint32_t    rx_bytes;
+    uint16_t    reserved;           // Temperature?
+};
+
+struct lgtd_lifx_ip_firmware_info {
+    uint64_t    built_at;       // ns since epoch
+    uint64_t    installed_at;   // ns since epoch
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
+    lgtd_time_mono_t                    last_mcu_state_at;
+    lgtd_time_mono_t                    last_wifi_state_at;
+    lgtd_time_mono_t                    last_runtime_info_at;
+    lgtd_time_mono_t                    dirty_at;
+    uint16_t                            expected_power_on;
+    uint8_t                             addr[LGTD_LIFX_ADDR_LENGTH];
+    struct lgtd_lifx_gateway            *gw;
+    struct lgtd_lifx_light_state        state;
+    struct lgtd_lifx_ip_state           mcu_state;
+    struct lgtd_lifx_ip_state           wifi_state;
+    struct lgtd_lifx_ip_firmware_info   mcu_fw_info;
+    struct lgtd_lifx_ip_firmware_info   wifi_fw_info;
+    struct lgtd_lifx_product_info       product_info;
+    struct lgtd_lifx_runtime_info       runtime_info;
 };
 RB_HEAD(lgtd_lifx_bulb_map, lgtd_lifx_bulb);
 SLIST_HEAD(lgtd_lifx_bulb_list, lgtd_lifx_bulb);
@@ -69,3 +103,19 @@
                                     lgtd_time_mono_t);
 void lgtd_lifx_bulb_set_power_state(struct lgtd_lifx_bulb *, uint16_t);
 void lgtd_lifx_bulb_set_tags(struct lgtd_lifx_bulb *, uint64_t);
+
+void lgtd_lifx_bulb_set_mcu_state(struct lgtd_lifx_bulb *,
+                                  const struct lgtd_lifx_ip_state *,
+                                  lgtd_time_mono_t);
+void lgtd_lifx_bulb_set_mcu_firmware_info(struct lgtd_lifx_bulb *,
+                                          const struct lgtd_lifx_ip_firmware_info *);
+void lgtd_lifx_bulb_set_wifi_state(struct lgtd_lifx_bulb *,
+                                   const struct lgtd_lifx_ip_state *,
+                                   lgtd_time_mono_t);
+void lgtd_lifx_bulb_set_wifi_firmware_info(struct lgtd_lifx_bulb *,
+                                           const struct lgtd_lifx_ip_firmware_info *);
+void lgtd_lifx_bulb_set_product_info(struct lgtd_lifx_bulb *,
+                                     const struct lgtd_lifx_product_info *);
+void lgtd_lifx_bulb_set_runtime_info(struct lgtd_lifx_bulb *,
+                                     const struct lgtd_lifx_runtime_info *,
+                                     lgtd_time_mono_t);
diff --git a/lifx/gateway.c b/lifx/gateway.c
--- a/lifx/gateway.c
+++ b/lifx/gateway.c
@@ -489,12 +489,8 @@
         (uintmax_t)pkt->tags
     );
 
-    struct lgtd_lifx_bulb *b = lgtd_lifx_gateway_get_or_open_bulb(
-        gw, hdr->target.device_addr
-    );
-    if (!b) {
-        return;
-    }
+    struct lgtd_lifx_bulb *b;
+    LGTD_LIFX_GATEWAY_GET_BULB_OR_RETURN(b, gw, hdr->target.device_addr);
 
     assert(sizeof(*pkt) == sizeof(b->state));
     lgtd_lifx_bulb_set_light_state(
@@ -559,14 +555,9 @@
         gw->ip_addr, gw->port, lgtd_addrtoa(hdr->target.device_addr), pkt->power
     );
 
-    struct lgtd_lifx_bulb *b = lgtd_lifx_gateway_get_or_open_bulb(
-        gw, hdr->target.device_addr
+    LGTD_LIFX_GATEWAY_SET_BULB_ATTR(
+        gw, hdr->target.device_addr, lgtd_lifx_bulb_set_power_state, pkt->power
     );
-    if (!b) {
-        return;
-    }
-
-    lgtd_lifx_bulb_set_power_state(b, pkt->power);
 }
 
 int
@@ -673,9 +664,10 @@
     }
 }
 
-void lgtd_lifx_gateway_handle_tags(struct lgtd_lifx_gateway *gw,
-                                   const struct lgtd_lifx_packet_header *hdr,
-                                   const struct lgtd_lifx_packet_tags *pkt)
+void
+lgtd_lifx_gateway_handle_tags(struct lgtd_lifx_gateway *gw,
+                             const struct lgtd_lifx_packet_header *hdr,
+                             const struct lgtd_lifx_packet_tags *pkt)
 {
     assert(gw && hdr && pkt);
 
@@ -685,12 +677,8 @@
         (uintmax_t)pkt->tags
     );
 
-    struct lgtd_lifx_bulb *b = lgtd_lifx_gateway_get_or_open_bulb(
-        gw, hdr->target.device_addr
-    );
-    if (!b) {
-        return;
-    }
+    struct lgtd_lifx_bulb *b;
+    LGTD_LIFX_GATEWAY_GET_BULB_OR_RETURN(b, gw, hdr->target.device_addr);
 
     int tag_id;
     LGTD_LIFX_WIRE_FOREACH_TAG_ID(tag_id, pkt->tags) {
@@ -707,3 +695,142 @@
 
     lgtd_lifx_bulb_set_tags(b, pkt->tags);
 }
+
+void
+lgtd_lifx_gateway_handle_ip_state(struct lgtd_lifx_gateway *gw,
+                                  const struct lgtd_lifx_packet_header *hdr,
+                                  const struct lgtd_lifx_packet_ip_state *pkt)
+{
+    assert(gw && hdr && pkt);
+
+    const char  *type;
+    void        (*bulb_fn)(struct lgtd_lifx_bulb *,
+                           const struct lgtd_lifx_ip_state *,
+                           lgtd_time_mono_t);
+
+    switch (hdr->packet_type) {
+    case LGTD_LIFX_MESH_INFO:
+        type = "MCU_STATE";
+        bulb_fn = lgtd_lifx_bulb_set_mcu_state;
+        break;
+    case LGTD_LIFX_WIFI_INFO:
+        type = "WIFI_STATE";
+        bulb_fn = lgtd_lifx_bulb_set_wifi_state;
+        break;
+    default:
+        lgtd_info("invalid ip state packet_type %#hx", hdr->packet_type);
+#ifndef NDEBUG
+        abort();
+#endif
+        return;
+    }
+
+    lgtd_debug(
+        "%s <-- [%s]:%hu - %s "
+        "signal_strength=%f, rx_bytes=%u, tx_bytes=%u, reserved=%hu",
+        type, gw->ip_addr, gw->port, lgtd_addrtoa(hdr->target.device_addr),
+        pkt->signal_strength, pkt->rx_bytes, pkt->tx_bytes, pkt->reserved
+    );
+
+    LGTD_LIFX_GATEWAY_SET_BULB_ATTR_WITH_RECV_AT(
+        gw,
+        hdr->target.device_addr,
+        bulb_fn,
+        (const struct lgtd_lifx_ip_state *)pkt
+    );
+}
+
+void
+lgtd_lifx_gateway_handle_ip_firmware_info(struct lgtd_lifx_gateway *gw,
+                                          const struct lgtd_lifx_packet_header *hdr,
+                                          const struct lgtd_lifx_packet_ip_firmware_info *pkt)
+{
+    assert(gw && hdr && pkt);
+
+    const char  *type;
+    void        (*bulb_fn)(struct lgtd_lifx_bulb *,
+                           const struct lgtd_lifx_ip_firmware_info *);
+
+    switch (hdr->packet_type) {
+    case LGTD_LIFX_MESH_FIRMWARE:
+        type = "MCU_FIRMWARE_INFO";
+        bulb_fn = lgtd_lifx_bulb_set_mcu_firmware_info;
+        break;
+    case LGTD_LIFX_WIFI_FIRMWARE_STATE:
+        type = "WIFI_FIRMWARE_INFO";
+        bulb_fn = lgtd_lifx_bulb_set_wifi_firmware_info;
+        break;
+    default:
+        lgtd_info("invalid ip firmware packet_type %#hx", hdr->packet_type);
+#ifndef NDEBUG
+        abort();
+#endif
+        return;
+    }
+
+    char built_at[64], installed_at[64];
+    LGTD_LIFX_WIRE_PRINT_NSEC_TIMESTAMP(pkt->built_at, built_at);
+    LGTD_LIFX_WIRE_PRINT_NSEC_TIMESTAMP(pkt->installed_at, installed_at);
+
+    lgtd_debug(
+        "%s <-- [%s]:%hu - %s "
+        "built_at=%s, installed_at=%s, version=%u",
+        type, gw->ip_addr, gw->port, lgtd_addrtoa(hdr->target.device_addr),
+        built_at, installed_at, pkt->version
+    );
+
+    LGTD_LIFX_GATEWAY_SET_BULB_ATTR(
+        gw,
+        hdr->target.device_addr,
+        bulb_fn,
+        (const struct lgtd_lifx_ip_firmware_info *)pkt
+    );
+}
+
+void
+lgtd_lifx_gateway_handle_product_info(struct lgtd_lifx_gateway *gw,
+                                      const struct lgtd_lifx_packet_header *hdr,
+                                      const struct lgtd_lifx_packet_product_info *pkt)
+{
+    assert(gw && hdr && pkt);
+
+    lgtd_debug(
+        "PRODUCT_INFO <-- [%s]:%hu - %s "
+        "vendor_id=%#x, product_id=%#x, version=%u",
+        gw->ip_addr, gw->port, lgtd_addrtoa(hdr->target.device_addr),
+        pkt->vendor_id, pkt->product_id, pkt->version
+    );
+
+    LGTD_LIFX_GATEWAY_SET_BULB_ATTR(
+        gw,
+        hdr->target.device_addr,
+        lgtd_lifx_bulb_set_product_info,
+        (const struct lgtd_lifx_product_info *)pkt
+    );
+}
+
+void
+lgtd_lifx_gateway_handle_runtime_info(struct lgtd_lifx_gateway *gw,
+                                      const struct lgtd_lifx_packet_header *hdr,
+                                      const struct lgtd_lifx_packet_runtime_info *pkt)
+{
+    assert(gw && hdr && pkt);
+
+    char device_time[64], uptime[64], downtime[64];
+    LGTD_LIFX_WIRE_PRINT_NSEC_TIMESTAMP(pkt->time, device_time);
+    LGTD_PRINT_DURATION(LGTD_NSECS_TO_SECS(pkt->uptime), uptime);
+    LGTD_PRINT_DURATION(LGTD_NSECS_TO_SECS(pkt->downtime), downtime);
+
+    lgtd_debug(
+        "PRODUCT_INFO <-- [%s]:%hu - %s time=%s, uptime=%s, downtime=%s",
+        gw->ip_addr, gw->port, lgtd_addrtoa(hdr->target.device_addr),
+        device_time, uptime, downtime
+    );
+
+    LGTD_LIFX_GATEWAY_SET_BULB_ATTR_WITH_RECV_AT(
+        gw,
+        hdr->target.device_addr,
+        lgtd_lifx_bulb_set_runtime_info,
+        (const struct lgtd_lifx_runtime_info *)pkt
+    );
+}
diff --git a/lifx/gateway.h b/lifx/gateway.h
--- a/lifx/gateway.h
+++ b/lifx/gateway.h
@@ -36,6 +36,12 @@
 struct lgtd_lifx_gateway {
     LIST_ENTRY(lgtd_lifx_gateway)   link;
     struct lgtd_lifx_bulb_list      bulbs;
+#define LGTD_LIFX_GATEWAY_GET_BULB_OR_RETURN(b, gw, bulb_addr)  do {    \
+    (b) = lgtd_lifx_gateway_get_or_open_bulb((gw), (bulb_addr));        \
+    if (!(b)) {                                                         \
+        return;                                                         \
+    }                                                                   \
+} while (0)
     // Multiple gateways can share the same site (that happens when bulbs are
     // far away enough that ZigBee can't be used). Moreover the SET_PAN_GATEWAY
     // packet doesn't include the device address in the header (i.e: site and
@@ -77,6 +83,18 @@
 
 extern struct lgtd_lifx_gateway_list lgtd_lifx_gateways;
 
+#define LGTD_LIFX_GATEWAY_SET_BULB_ATTR(gw, bulb_addr, bulb_fn, payload) do {   \
+    struct lgtd_lifx_bulb *b;                                                   \
+    LGTD_LIFX_GATEWAY_GET_BULB_OR_RETURN(b, gw, bulb_addr);                     \
+    (bulb_fn)(b, (payload));                                                    \
+} while (0)
+
+#define LGTD_LIFX_GATEWAY_SET_BULB_ATTR_WITH_RECV_AT(gw, bulb_addr, bulb_fn, payload) do {  \
+    struct lgtd_lifx_bulb *b;                                                               \
+    LGTD_LIFX_GATEWAY_GET_BULB_OR_RETURN(b, gw, bulb_addr);                                 \
+    (bulb_fn)(b, (payload), (gw)->last_pkt_at);                                             \
+} while (0)
+
 struct lgtd_lifx_gateway *lgtd_lifx_gateway_get(const struct sockaddr_storage *);
 struct lgtd_lifx_gateway *lgtd_lifx_gateway_open(const struct sockaddr_storage *,
                                                  ev_socklen_t,
@@ -119,3 +137,15 @@
 void lgtd_lifx_gateway_handle_tags(struct lgtd_lifx_gateway *,
                                    const struct lgtd_lifx_packet_header *,
                                    const struct lgtd_lifx_packet_tags *);
+void lgtd_lifx_gateway_handle_ip_state(struct lgtd_lifx_gateway *,
+                                        const struct lgtd_lifx_packet_header *,
+                                        const struct lgtd_lifx_packet_ip_state *);
+void lgtd_lifx_gateway_handle_ip_firmware_info(struct lgtd_lifx_gateway *,
+                                               const struct lgtd_lifx_packet_header *,
+                                               const struct lgtd_lifx_packet_ip_firmware_info *);
+void lgtd_lifx_gateway_handle_product_info(struct lgtd_lifx_gateway *,
+                                           const struct lgtd_lifx_packet_header *,
+                                           const struct lgtd_lifx_packet_product_info *);
+void lgtd_lifx_gateway_handle_runtime_info(struct lgtd_lifx_gateway *,
+                                           const struct lgtd_lifx_packet_header *,
+                                           const struct lgtd_lifx_packet_runtime_info *);
diff --git a/lifx/wire_proto.c b/lifx/wire_proto.c
--- a/lifx/wire_proto.c
+++ b/lifx/wire_proto.c
@@ -24,8 +24,10 @@
 #include <stdarg.h>
 #include <stdbool.h>
 #include <stdint.h>
+#include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
+#include <time.h>
 
 #include <event2/util.h>
 
@@ -74,12 +76,32 @@
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
@@ -90,6 +112,10 @@
 #define REQUEST_ONLY                                        \
     .decode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
     .handle = lgtd_lifx_wire_null_packet_handler
+#define UNIMPLEMENTED                                       \
+    .decode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
+    .encode = lgtd_lifx_wire_null_packet_encoder_decoder,   \
+    .handle = lgtd_lifx_wire_enosys_packet_handler
 
     static struct lgtd_lifx_packet_info packet_table[] = {
         // Gateway packets:
@@ -187,6 +213,211 @@
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
+            .size = sizeof(struct lgtd_lifx_packet_ip_state),
+            .decode = DECODER(lgtd_lifx_wire_decode_ip_state),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_ip_state)
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
+            .size = sizeof(struct lgtd_lifx_packet_ip_firmware_info),
+            .decode = DECODER(lgtd_lifx_wire_decode_ip_firmware_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_ip_firmware_info)
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
+            .size = sizeof(struct lgtd_lifx_packet_ip_state),
+            .decode = DECODER(lgtd_lifx_wire_decode_ip_state),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_ip_state)
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
+            .size = sizeof(struct lgtd_lifx_packet_ip_firmware_info),
+            .decode = DECODER(lgtd_lifx_wire_decode_ip_firmware_info),
+            .handle = HANDLER(lgtd_lifx_gateway_handle_ip_firmware_info)
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
+        // Unimplemented but "known" packets
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
 
@@ -234,6 +465,24 @@
     return LGTD_LIFX_WAVEFORM_INVALID;
 }
 
+void
+lgtd_lifx_wire_print_nsec_timestamp(uint64_t nsec_ts, char *buf, int bufsz)
+{
+    assert(buf);
+    assert(bufsz > 0);
+
+    time_t ts = LGTD_NSECS_TO_SECS(nsec_ts);
+
+    struct tm tm_utc;
+    if (gmtime_r(&ts, &tm_utc)) {
+        int64_t usecs = LGTD_NSECS_TO_USECS(nsec_ts - LGTD_SECS_TO_NSECS(ts));
+        LGTD_TM_TO_ISOTIME(&tm_utc, buf, bufsz, usecs);
+        return;
+    }
+
+    buf[0] = '\0';
+}
+
 static void
 lgtd_lifx_wire_encode_header(struct lgtd_lifx_packet_header *hdr, int flags)
 {
@@ -423,3 +672,44 @@
 
     pkt->tags = le64toh(pkt->tags);
 }
+
+void
+lgtd_lifx_wire_decode_ip_state(struct lgtd_lifx_packet_ip_state *pkt)
+{
+    assert(pkt);
+
+    pkt->signal_strength = lgtd_lifx_wire_lefloattoh(pkt->signal_strength);
+    pkt->tx_bytes = le32toh(pkt->tx_bytes);
+    pkt->rx_bytes = le32toh(pkt->rx_bytes);
+    pkt->reserved = le16toh(pkt->reserved);
+}
+
+void
+lgtd_lifx_wire_decode_ip_firmware_info(struct lgtd_lifx_packet_ip_firmware_info *pkt)
+{
+    assert(pkt);
+
+    pkt->built_at = le64toh(pkt->built_at);
+    pkt->installed_at = le64toh(pkt->installed_at);
+    pkt->version = le32toh(pkt->version);
+}
+
+void
+lgtd_lifx_wire_decode_product_info(struct lgtd_lifx_packet_product_info *pkt)
+{
+    assert(pkt);
+
+    pkt->vendor_id = le32toh(pkt->vendor_id);
+    pkt->product_id = le32toh(pkt->product_id);
+    pkt->version = le32toh(pkt->version);
+}
+
+void
+lgtd_lifx_wire_decode_runtime_info(struct lgtd_lifx_packet_runtime_info *pkt)
+{
+    assert(pkt);
+
+    pkt->time = le64toh(pkt->time);
+    pkt->uptime = le64toh(pkt->uptime);
+    pkt->downtime = le64toh(pkt->downtime);
+}
diff --git a/lifx/wire_proto.h b/lifx/wire_proto.h
--- a/lifx/wire_proto.h
+++ b/lifx/wire_proto.h
@@ -28,7 +28,7 @@
 static inline floatle_t
 lgtd_lifx_wire_htolefloat(float f)
 {
-    union { float f; uint32_t i; } u  = { .f = f };
+    union { float f; uint32_t i; } u = { .f = f };
     htole32(u.i);
     return u.f;
 }
@@ -36,7 +36,7 @@
 static inline floatle_t
 lgtd_lifx_wire_lefloattoh(float f)
 {
-    union { float f; uint32_t i; } u  = { .f = f };
+    union { float f; uint32_t i; } u = { .f = f };
     le32toh(u.i);
     return u.f;
 }
@@ -249,6 +249,30 @@
     char        label[LGTD_LIFX_LABEL_SIZE];
 };
 
+struct lgtd_lifx_packet_ip_state {
+    floatle_t   signal_strength;
+    uint32le_t  tx_bytes;
+    uint32le_t  rx_bytes;
+    uint16le_t  reserved;
+};
+
+struct lgtd_lifx_packet_ip_firmware_info {
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
@@ -330,8 +354,11 @@
          (tag_id_varname) != -1;                                                    \
          (tag_id_varname) = lgtd_lifx_wire_next_tag_id((tag_id_varname), (tags)))
 
-
 enum lgtd_lifx_waveform_type lgtd_lifx_wire_waveform_string_id_to_type(const char *, int);
+void lgtd_lifx_wire_print_nsec_timestamp(uint64_t, char *, int);
+#define LGTD_LIFX_WIRE_PRINT_NSEC_TIMESTAMP(ts, arr) do {               \
+    lgtd_lifx_wire_print_nsec_timestamp((ts), (arr), sizeof((arr)));    \
+} while (0)
 
 const struct lgtd_lifx_packet_info *lgtd_lifx_wire_get_packet_info(enum lgtd_lifx_packet_type);
 void lgtd_lifx_wire_load_packet_info_map(void);
@@ -356,3 +383,8 @@
 void lgtd_lifx_wire_decode_tags(struct lgtd_lifx_packet_tags *);
 void lgtd_lifx_wire_encode_tag_labels(struct lgtd_lifx_packet_tag_labels *);
 void lgtd_lifx_wire_decode_tag_labels(struct lgtd_lifx_packet_tag_labels *);
+
+void lgtd_lifx_wire_decode_ip_state(struct lgtd_lifx_packet_ip_state *);
+void lgtd_lifx_wire_decode_ip_firmware_info(struct lgtd_lifx_packet_ip_firmware_info *);
+void lgtd_lifx_wire_decode_product_info(struct lgtd_lifx_packet_product_info *);
+void lgtd_lifx_wire_decode_runtime_info(struct lgtd_lifx_packet_runtime_info *);
