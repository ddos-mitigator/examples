/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

ENTRYPOINT enum Result
sorb(Context ctx) {
    if (packet_transport_proto(ctx) == IP_PROTO_TCP) {
        return RESULT_SORB;
    }
    return RESULT_PASS;
}

PROGRAM_DISPLAY_ID("SORB Example v0.1")
