/* Optional libFuzzer entry point. Build with address/undefined sanitizers. */
#include <stddef.h>
#include <stdint.h>

#include "tdx_frame.h"
#include "tdx_json.h"
#include "tdx_jsn.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    tdx_json_doc json = {0};
    tdx_jsn_document jsn = {0};
    tdx_buf out = {0};
    if (size > 1024u * 1024u)
        return 0;
    (void)tdx_json_parse(data, size, &json, NULL);
    (void)tdx_json_parse(data, size, &json, NULL);
    tdx_json_doc_free(&json);
    (void)tdx_jsn_parse(data, size, &jsn, NULL);
    if (jsn.group_count && jsn.groups[0].row_count && jsn.groups[0].column_count)
        (void)tdx_jsn_cell_json(&jsn, &jsn.groups[0], 0, 0, &out, NULL);
    (void)tdx_jsn_parse(data, size, &jsn, NULL);
    tdx_jsn_document_free(&jsn);
    if (size >= 2 && size - 2 <= UINT16_MAX) {
        size_t decoded = (size_t)data[0] | ((size_t)data[1] << 8);
        (void)tdx_frame_decode_body(data + 2, size - 2, decoded, &out, NULL);
        (void)tdx_frame_decode_body(data + 2, size - 2, decoded, &out, NULL);
    }
    tdx_buf_free(&out);
    return 0;
}
