# HG changeset patch
# Parent  99f9eb93b992753a17a5e334e2c242631ea800cb
Add missing (uintmax_t) casts in a test file

diff --git a/tests/lifx/gateway/test_gateway_deallocate_tag_id_from_lifx_network.c b/tests/lifx/gateway/test_gateway_deallocate_tag_id_from_lifx_network.c
--- a/tests/lifx/gateway/test_gateway_deallocate_tag_id_from_lifx_network.c
+++ b/tests/lifx/gateway/test_gateway_deallocate_tag_id_from_lifx_network.c
@@ -46,7 +46,7 @@
     if (gw.tag_ids != LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)) {
         errx(
             1, "unexpected gw.tag_ids value = %jx (expected %jx)",
-            gw.tag_ids, LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)
+            (uintmax_t)gw.tag_ids, (uintmax_t)LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)
         );
     }
     if (!tagging_decref_called) {
@@ -61,7 +61,7 @@
     if (gw.tag_ids != LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)) {
         errx(
             1, "unexpected gw.tag_ids value = %jx (expected %jx)",
-            gw.tag_ids, LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)
+            (uintmax_t)gw.tag_ids, (uintmax_t)LGTD_LIFX_WIRE_TAG_ID_TO_VALUE(42)
         );
     }
     if (tagging_decref_called) {
