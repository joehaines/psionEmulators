// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME's psion_ssd_device + the PACK_MODE branches
// of psion_asic5_device. The two MAME devices are collapsed into a
// single PsionSSD class because SSD is the only consumer of ASIC5 in
// PACK_MODE — the ~280 lines of UART / Centronics / barcode / RS232
// code in MAME's ASIC5 are unreachable for a memory pack and would
// require device_serial_interface machinery we don't have.
//
// Surface mapping vs MAME:
//   psion_ssd_device::data_w(uint16_t) -> writeFrame(uint16_t)
//   psion_ssd_device::data_r()         -> readFrame()
//   psion_ssd_device::call_load(...)   -> attach(bytes, size)
//   psion_ssd_device::call_unload()    -> detach()
//   door_cb                            -> setDoorCb
//
// Wiring on the host side: call writeFrame() from the SIBO channel
// writer callback (asic2.setChannelWriteCb / asic9.setSibChannelWriter)
// and readFrame() from the matching reader callback. The host is
// expected to assert the chip's door-NMI line on attach/detach for
// ~200 ms so EPOC16 re-scans the slot — see series3.cpp / series3c.cpp
// for the cycle-counted countdown.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class PsionSSD {
public:
    PsionSSD() = default;

    // SIBO frame tags (mirror PsionAsic2::NULL_FRAME etc.).
    static constexpr uint16_t NULL_FRAME    = 0x000;
    static constexpr uint16_t CONTROL_FRAME = 0x100;
    static constexpr uint16_t DATA_FRAME    = 0x200;

    // Pack type presented to the host through the info byte (D7-D5).
    // On real hardware this is set by pull-up straps on the pack PCB,
    // NOT by the pack contents — a RAM pack can carry a perfectly valid
    // FEFS structure (0xF1A5 magic included) and still identify as RAM.
    // `Auto` keeps the legacy behaviour of sniffing the 0xF1A5 magic
    // (magic -> Type 1 Flash, otherwise RAM) for callers that only have
    // an image and no type information (e.g. dumped .bin files).
    enum class Type : uint8_t { Auto = 0, Ram = 1, Flash = 2, Protected = 3 };

    // ------------------------------------------------------------------
    // Image management
    // ------------------------------------------------------------------

    // Loads `size` bytes (must be a power of two in [64K, 8M]).
    // `type` selects the info-byte memory type (see Type above).
    // Returns false if `size` is out of range or not power-of-two; in
    // that case the slot remains in its previous state.
    bool attach(const uint8_t *bytes, size_t size, Type type = Type::Auto);

    // Empties the slot; subsequent SerialSelect reads return 0.
    void detach();

    bool   isInserted() const  { return m_infoByte != 0; }
    size_t imageSize()  const  { return m_data.size(); }
    const uint8_t *imageData() const { return m_data.data(); }

    // ------------------------------------------------------------------
    // SIBO serial protocol
    // ------------------------------------------------------------------

    void    writeFrame(uint16_t frame);
    uint8_t readFrame();

    // Door-NMI callback — fires true on attach/detach. The host is
    // responsible for clearing it after ~200 ms via its own scheduler.
    void setDoorCb(std::function<void(bool)> cb) { m_doorCb = std::move(cb); }

private:
    // Info-byte / mem-width assignment from MAME psion_ssd.cpp:243-266.
    void computeInfo(size_t size, Type type);
    uint32_t latchedAddr() const;
    // Counter-mode tick: when port B is in counter mode (the default
    // after reset), each Port A read or write auto-increments the
    // address LSB. Used by both readFrame Port A and writeFrame Port A.
    void tickPortBCounter();
    uint8_t readFrameInner();

    std::vector<uint8_t> m_data;

    uint8_t  m_infoByte      = 0;   // 0 == empty slot
    // True once the guest's SSD driver has spoken to this slot (any
    // SIBO frame). Gates the door-NMI on attach/detach: before the
    // OS's own boot-time slot scan the NMI vector isn't populated, and
    // pulsing it then sends the CPU into the weeds (observed as a
    // device that never paints when a pack is hot-attached <~1 s into
    // a cold boot — the app-library try-flow's fast path). A pack
    // attached before the first scan doesn't need the pulse at all:
    // the scan will find it.
    bool     m_guestAccessed = false;
    int      m_memWidth      = 0;   // log2(per-device size)
    uint8_t  m_siboControl   = 0;   // last CONTROL_FRAME byte
    uint32_t m_portLatch     = 0;   // 24-bit address latch (B/D/C)
    bool     m_portDcSelect  = false;
    uint8_t  m_portBLatch    = 0;   // Port B latch-mode readback
    uint8_t  m_portBMode     = 0;   // ASIC5 port-B mode register
    uint8_t  m_portBCounter  = 0;   // ASIC5 port-B counter

    // Pack-mode pin (PC6=0). Per MAME psion_asic5.h:
    //     enum pc6_state { PACK_MODE = 0, PERIPHERAL_MODE = 1 };
    // Asic5PackId reads return info byte only when sibo_control bit 0
    // matches m_mode. With PACK_MODE=0 we respond to ctrl 0x42 / 0x46
    // (bit 0 = 0); peripheral-mode devices respond to ctrl 0x43 / 0x47.
    // The previous value (1) had this inverted: we responded to 0x43
    // like a peripheral, the kernel's PackId probe at ctrl 0x42 got 0,
    // it concluded the slot was empty, and the System screen reported
    // "Disk [letter] Absent" even though a pack was physically present.
    static constexpr uint8_t PACK_MODE = 0;

    std::function<void(bool)> m_doorCb;
};
