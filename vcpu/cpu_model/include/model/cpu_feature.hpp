/**
 * Copyright (C) 2019-2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/cpu_feature.hpp>
#include <platform/atomic.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <platform/vm_types.hpp>

namespace Model {
    class CpuFeature;
    class CpuFlag;
};

namespace Request {
    enum Requestor : uint32 {
        VMM = 0,
        VMI = 1,
        MAX_REQUESTORS,
    };
}

/**
 * Important note:
 * - Clients that *read* the configuration MUST call `clean`
 *   *before* calling `read`.
 */
class Model::CpuFeature {
public:
    bool is_requested_by(Request::Requestor requestor) const {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        return (_requests[requestor] & ENABLE_MASK) != 0u;
    }
    bool is_requested() const {
        return is_requested_by(Request::VMM) || is_requested_by(Request::VMI);
    }

    /**
     * \brief Reads at the current configuration without committing to
     *        act on it.
     */
    void read(bool &enabled, Reg_selection &regs) const {
        uint64 conf = _requests[0] | _requests[1];

        enabled = ((conf & ENABLE_MASK) != 0u);

        if (enabled)
            regs = conf & ~ENABLE_MASK;
        else
            regs = 0; // Force empty registers when the feature is disabled
    }

    /**
     * \brief checks the dirty status and reads if the values are dirty.
     * This function is provided for convenience.
     *
     * This function is marked `nodiscard`, if you do not need the return value,
     * use `clean_read`.
     *
     * \param always if true, read the configuration even if it is not dirty.
     * \returns true if the configuration has been updated since the last read.
     */
    [[nodiscard]] bool check_clean_read(bool &enabled, Reg_selection &regs, bool always = false) {
        bool dirty = check_clean();

        if (always or dirty)
            read(enabled, regs);

        return dirty;
    }

    void clean_read(bool &enabled, Reg_selection &regs) {
        clean();
        read(enabled, regs);
    }

    // Each requestor is responsible of maintaining the consistency of its config
    void request(bool enable, Request::Requestor requestor, Reg_selection regs = 0) {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        ASSERT(!(ENABLE_MASK & regs)); // Reg_selection doesn't use the highest bit for now
        _requests[requestor] = enable ? regs | ENABLE_MASK : 0;
        // it is necessary that setting the dirty bits happens after
        // updating the value.
        force_reconfiguration();
    }

    void force_reconfiguration() { _dirty = true; }
    bool needs_reconfiguration() const { return _dirty; }

    /**
     * The read of the configuration (through `read`) *must* be done
     * *after* this operation.
     * For a safer interface, use `clean_read`.
     */
    [[nodiscard]] bool check_clean() {
        bool b = _dirty;
        if (b)
            // this test is not necessary, it trades a branch for a memory fence
            _dirty = false;
        return b;
        // alternative implementation:
        //   return _dirty.fetch_and(0);
        // on ARM, this is a cas-loop which is less efficient.
        // on x86, this might be more efficient
    }
    void clean() { _dirty = false; }

private:
    static constexpr uint8 ENABLE_SHIFT = 63;
    static constexpr uint64 ENABLE_MASK = 1ull << ENABLE_SHIFT;

    atomic<uint64> _requests[Request::MAX_REQUESTORS] = {0ull, 0ull};
    // The `_dirty` bit could use release writes and acquire reads as a more
    // performant solution than SEQ_CST.
    atomic<bool> _dirty{false};
};

class Model::CpuFlag {
public:
    bool is_requested_by(Request::Requestor requestor) const {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        return (_requests & requestor_bit(requestor)) != 0;
    }
    bool is_requested() const { return (_requests & ONLY_DATA) != 0; }

    /**
     * \brief Reads at the current configuration without committing to
     *        act on it.
     */
    void read(bool &enabled) const { enabled = ((_requests & ONLY_DATA) != 0); }

    /**
     * \brief checks the dirty status and reads if the values are dirty.
     * This function is provided for convenience.
     *
     * This function is marked `nodiscard`, if you do not need the return value,
     * use `clean_read`.
     *
     * \param always if true, read the configuration even if it is not dirty.
     * \returns true if the configuration has been updated since the last read.
     */
    [[nodiscard]] bool check_clean_read(bool &enabled, bool = false) {
        auto result = _requests.fetch_and(ONLY_DATA);
        enabled = ((result & ONLY_DATA) != 0);
        return (result & DIRTY) != 0;
    }

    void clean_read(bool &enabled) { enabled = (_requests.and_fetch(ONLY_DATA) != 0u); }

    // Each requestor is responsible of maintaining the consistency of its config
    void request(bool enable, Request::Requestor requestor) {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        if (enable) {
            _requests.fetch_or(requestor_bit(requestor));
        } else {
            _requests.fetch_and(~requestor_bit(requestor));
        }
        // reconfiguration seems to be necessary even if the configuration did not change.
        force_reconfiguration();
    }

    void force_reconfiguration() { _requests.or_fetch(DIRTY); }
    bool needs_reconfiguration() const { return (_requests & DIRTY) != 0; }

    /**
     * The read of the configuration (through `read`) *must* be done
     * *after* this operation.
     * For a safer interface, use `clean_read`.
     */
    [[nodiscard]] bool check_clean() { return (_requests.fetch_and(ONLY_DATA) & DIRTY) != 0; }
    void clean() { _requests.and_fetch(ONLY_DATA); }

private:
    static char requestor_bit(Request::Requestor req) { return static_cast<char>(1 << req); }
    enum : char { DIRTY = 0x4, ONLY_DATA = 0x3 };

    atomic<char> _requests{0};
};
