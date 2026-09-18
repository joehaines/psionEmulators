#!/usr/bin/env bash
# Validates that each supported EPOC device's kernel autostarts the
# PLP handshake and proceeds through NCP-Info + LINK.* Connect.
#
# Drives the link-layer handshake using harness --serial-auto-rules:
#   0x21 (Req_Req_Pdu) → reply Req_Con_Pdu + fake 4-byte magic
#   0x24 (Req_Con_Pdu) → reply Ack_Pdu
#
# Then asserts that within the polling window we observe:
#   - a 0x31 frame (NCP Info,  payload `00 00 06 06 XX XX YY YY`)
#     — version=6 corresponds to EPOC ER3 (Windermere) or ER5 (netBook)
#   - a 0x32 frame whose payload contains the ASCII "LINK.*" connect string
#
# The Conan is the exception and gets the opposite assertion: its ROM speaks
# the ER5u escaped transport (ESC 0x19; ENQ `19 23` / ACK `19 24`) instead of
# our PLP data link, so the row proves it answers a host ENQ with an ACK and
# emits NO PLP frame. That is what the frontend's linkProtocol=0 for this
# device encodes; if a ROM or emulator change ever made it talk PLP after
# all, this row fails and the profile should be revisited. See
# docs/conan-remote-link.md.
#
# Windermere (Revo / 5mx / 5mxpro / mc218) routes the cable serial port
# to UART2 and the kernel comes up quickly, so those rows use
# `--serial-attach 2 8 --serial-poll-until 14`.  The SA-1100 netBook
# routes the cable to UART3 and needs the full EPOC R5 OS image
# attached via CF before RemoteLinkServer4 starts listening; that row
# uses the normal boot + post-attach phasing so --card-path is
# honoured (--serial-poll-until forces skipCard).
#
# Usage:
#   scripts/test-remote-link.sh           # run all devices
#   scripts/test-remote-link.sh revo      # run just one

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROMS="$REPO_ROOT/roms"

if [ ! -x "$HARNESS" ]; then
    bash "$REPO_ROOT/harness/build.sh" >&2
fi

DEVICES=(
    "revo:Revo_v1.06(390)_eng.bin"
    "5mx:5mx_v1.05(260)_eng.bin"
    "5mxpro:5mxPRO_v1.05(319)_patch_eng.bin"
    "mc218:MC218_v1.05(259)_eng.bin"
    "osaris:Osaris_v1.02(209)_eng.bin"
    "series5:series5_v1.01(144)_eng.bin"
    "geofox:Geofox_v1.01(146)_eng.bin"
    "netbook:netBook_BL_v011_eng.bin"
    "series7:series7_v1.05(254)_b756_eng.bin"
    "netpad:Netpad.img"
    "conan:conan_s2_2201.engbuild.IMG"
)

target="${1:-all}"
overall=0
for entry in "${DEVICES[@]}"; do
    dev="${entry%%:*}"
    rom="${entry##*:}"
    if [ "$target" != "all" ] && [ "$target" != "$dev" ]; then continue; fi
    rom_path="$ROMS/$rom"
    if [ ! -f "$rom_path" ]; then
        echo "SKIP $dev: ROM $rom not present" >&2
        continue
    fi

    if [ "$dev" = "conan" ]; then
        # Conan (EPOC ER5u): no PLP. Send an ESC ENQ (19 23) at t=10 and a
        # framed Req_Req_Pdu at t=14; the ROM answers the ENQ with an ESC ACK
        # (19 24) and ignores the PLP frame, probing with its own ENQ ~2 s
        # after any inbound byte. Cable is on UART2 like the rest of the
        # Windermere family.
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --serial-attach 2 8 \
            --serial-tx 2 10 "19,23" \
            --serial-tx-framed 2 14 "21" \
            --serial-poll-until 24 2>&1) || true
    elif [ "$dev" = "series7" ]; then
        # Series 7 boots from its 16 MB ROM directly (no CF card).
        # RemoteLinkServer4 sends Req_Req_Pdu at sim time ~0.9 s.
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --boot-seconds 15 \
            --serial-attach 3 0.5 \
            --serial-auto-rule "21:24 de ad be ef" \
            --serial-auto-rule "24:00" 2>&1) || true
    elif [ "$dev" = "netpad" ]; then
        # netpad boots its own 12.3 MB EPOC R5 ROM straight from flash
        # (no CF handoff) and routes the cable to UART3 like its SA-1100
        # siblings.  RemoteLinkServer4 sends its Req_Req_Pdu at ~1.9 s,
        # so attach before that.  Unlike the Series 7 / netBook this ROM
        # does drain UTDR, so the handshake gets all the way to NCP Info.
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --boot-seconds 15 --skip-card \
            --serial-attach 3 0.5 \
            --serial-auto-rule "21:24 de ad be ef" \
            --serial-auto-rule "24:00" 2>&1) || true
    elif [ "$dev" = "netbook" ]; then
        # netBook needs the EPOC R5 OS image on CF — the bootloader
        # alone never starts RemoteLinkServer4.  CF attach happens at
        # boot-seconds=3, EPOC R5 then emits a single Req_Req_Pdu on
        # UART3 at sim time ~4 s and (if no Req_Con_Pdu comes back)
        # never retries.  So the bridge must be attached BEFORE that
        # first frame — t=3.5s puts us right after CF handoff but
        # before EPOC's RemoteLinkServer4 transmits.
        os_img="$ROMS/netbook_os.img"
        if [ ! -f "$os_img" ]; then
            echo "SKIP $dev: OS image $os_img not present" >&2
            continue
        fi
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --boot-seconds 3 --post-attach-seconds 20 \
            --card-path "$os_img" \
            --serial-attach 3 3.5 \
            --serial-auto-rule "21:24 de ad be ef" \
            --serial-auto-rule "24:00" 2>&1) || true
    elif [ "$dev" = "geofox" ]; then
        # Geofox One (EPOC R1, CL-PS7110): cable on UART1 and the same
        # 0x22 Req_Con flavour as the Series 5, but the plug is what
        # starts the exchange — the machine ships with Remote Link set to
        # Cable at 115200, its RemoteLinkServer has UART1 open long
        # before this, and it answers the modem-line change with a
        # Req_Req_Pdu burst. So this row attaches at t=20, once the
        # machine has settled, which is the order a user connects in.
        #
        # What it is really gating is the modem-line polarity: this
        # board reads CTS/DSR/DCD inverted from Psion's own CL-PS711x
        # machines (see core/geofox.h). Wire them the Psion way and the
        # server sets up the port and then never transmits a byte, so
        # this row fails with saw_req=0 — which is exactly the bug it
        # exists to catch.
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --serial-attach 1 20 \
            --serial-auto-rule "21:22 de ad be ef" \
            --serial-poll-until 30 2>&1) || true
    elif [ "$dev" = "osaris" ] || [ "$dev" = "series5" ]; then
        # CL-PS711x (Osaris ER4, Series 5 ER3): the cable link rides UART1
        # (shared with the IrDA SIR port — the PS711x serial bridge is
        # UART1-only). Both ROMs autostart a Req_Req_Pdu burst on cable-attach
        # but silently DROP the Revo-flavoured 0x24 Req_Con (and stop retrying,
        # wedging the link); they complete the handshake with the 0x22 form —
        # replying Ack_Pdu and proceeding to NCP Info (which satisfies
        # saw_info below). They then re-send NCP Info awaiting the host's
        # Data-PDU acks, which the real PlpClient supplies — so LINK.* never
        # appears under these dumb auto-rules and these rows gate on
        # PASS-WIRING+INFO rather than full LINK.*. The Series 5 kernel boots
        # slower, hence the later attach/poll window.
        at=8; until=14
        if [ "$dev" = "series5" ]; then at=25; until=38; fi
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --serial-attach 1 "$at" \
            --serial-auto-rule "21:22 de ad be ef" \
            --serial-poll-until "$until" 2>&1) || true
    else
        log=$("$HARNESS" "$rom_path" --device "$dev" --quiet-logs \
            --serial-attach 2 8 \
            --serial-auto-rule "21:24 de ad be ef" \
            --serial-auto-rule "24:00" \
            --serial-poll-until 14 2>&1) || true
    fi

    if [ "$dev" = "conan" ]; then
        saw_ack=0; saw_enq=0; saw_plp=0
        if echo "$log" | grep -q "serial-rx UART2 (2 bytes): 19 24"; then saw_ack=1; fi
        if echo "$log" | grep -q "serial-rx UART2 (2 bytes): 19 23"; then saw_enq=1; fi
        # Any PLP frame at all from the device would mean the premise of
        # linkProtocol=0 is wrong.
        if echo "$log" | grep -q "serial-frame type="; then saw_plp=1; fi
        if [ "$saw_ack" -eq 1 ] && [ "$saw_enq" -eq 1 ] && [ "$saw_plp" -eq 0 ]; then
            echo "PASS-ER5U $dev — ESC ENQ/ACK observed, no PLP frames (linkProtocol=0 is right)"
        else
            echo "FAIL $dev — saw_ack=$saw_ack saw_enq=$saw_enq saw_plp=$saw_plp" >&2
            echo "$log" | grep -E "serial-rx|serial-frame" | head -20 >&2
            overall=1
        fi
        continue
    fi

    saw_info=0
    saw_link=0
    saw_req=0
    if echo "$log" | grep -q "serial-frame type=31 len=9: 31 00 00 06 06"; then
        saw_info=1
    fi
    if echo "$log" | grep -qE "serial-frame type=32 .*: 32 00 01 03 4c 49 4e 4b 2e 2a 00"; then
        saw_link=1
    fi
    # serial-frame type=21 proves the device transmitted Req_Req_Pdu on
    # the attached UART — i.e. the host bridge is wired to the right
    # port for that device.  Independent of whether the upper handshake
    # completes.
    if echo "$log" | grep -q "serial-frame type=21"; then
        saw_req=1
    fi

    if [ "$saw_info" -eq 1 ] && [ "$saw_link" -eq 1 ]; then
        echo "PASS $dev — NCP Info + LINK.* Connect observed"
    elif ([ "$dev" = "osaris" ] || [ "$dev" = "series5" ] || [ "$dev" = "geofox" ]) \
         && [ "$saw_info" -eq 1 ]; then
        # 0x22 Req_Con accepted: the device Ack'd and sent NCP Info — the
        # handshake works; only the ack-driven continuation needs the real
        # client (see the CL-PS711x row comment above).
        echo "PASS-WIRING+INFO $dev — 0x22 handshake accepted, NCP Info observed on UART1"
    elif [ "$dev" = "netpad" ] && [ "$saw_info" -eq 1 ]; then
        # netpad: link-layer handshake completes and the device sends NCP
        # Info; LINK.* Connect needs the real PlpClient (same limitation
        # as the CL-PS711x rows).
        echo "PASS-WIRING+INFO $dev — link handshake accepted, NCP Info observed on UART3"
    elif ([ "$dev" = "netbook" ] || [ "$dev" = "series7" ] || [ "$dev" = "osaris" ]) && [ "$saw_req" -eq 1 ]; then
        # Bridge wiring proven (the device transmitted Req_Req_Pdu on the
        # attached UART) but the full handshake doesn't complete under the
        # auto-rule harness:
        #  • netBook / Series 7 (UART3): the EPOC R5 kernel never drains UTDR
        #    after acking the SERn IRQ, so our Req_Con_Pdu sits in the RX FIFO
        #    forever — the EKA1 scheduler / Active-Object pathology tracked in
        #    docs/series7-screen-freeze-2026-05-22.md.
        #  • Osaris (UART1): the R5 ROM answers the link Req/Con but does not
        #    send NCP Info spontaneously — the real PlpClient sends it first, so
        #    the harness (which only auto-replies at the link layer) can't drive
        #    NCP. The link-layer routing this row gates is still correct.
        echo "PASS-WIRING $dev — Req_Req_Pdu observed on attached UART (upstream handshake needs the real client; see comment)"
    else
        echo "FAIL $dev — saw_info=$saw_info saw_link=$saw_link saw_req=$saw_req" >&2
        echo "$log" | grep -E "serial-frame|auto-rule" | head -20 >&2
        overall=1
    fi
done
exit "$overall"
