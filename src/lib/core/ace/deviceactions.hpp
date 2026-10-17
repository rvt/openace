#pragma once

#include <stdint.h>
#include "etl/vector.h"

namespace GATAS
{
    enum class DeviceAction : uint8_t { Status = 0, Save = 1, Restart = 2, UsbBoot = 3 };
    enum class DeviceActionPhase : uint8_t { Prepare = 0, Execute = 1 };
    enum class DeviceUpdatePolicy : uint8_t { RequireSaved = 0, SaveFirst = 1, AllowUnsaved = 2 };
    enum class DeviceActionResult : uint8_t
    {
        Ok = 0, Accepted = 1, Unsupported = 2, Invalid = 3, Busy = 4,
        SaveFailed = 5, StaleBoot = 6, UnsavedChanges = 7, Expired = 8
    };

    struct DeviceActionRequest
    {
        DeviceAction action = DeviceAction::Status;
        DeviceActionPhase phase = DeviceActionPhase::Prepare;
        uint32_t requestId = 0;
        uint64_t bootId = 0;
        DeviceUpdatePolicy updatePolicy = DeviceUpdatePolicy::RequireSaved;

        bool sameOperation(const DeviceActionRequest &other) const
        {
            return action == other.action && requestId == other.requestId &&
                   bootId == other.bootId && updatePolicy == other.updatePolicy;
        }
    };

    struct DeviceActionResponse
    {
        DeviceAction action;
        uint32_t requestId;
        DeviceActionResult result;
        uint8_t capabilities;
        uint64_t bootId;
        bool dirty;
    };

    inline constexpr uint8_t DEVICE_ACTION_CAPABILITIES = 7;

    // Own no Configuration reference. Config calls this while holding its
    // recursive mutex and supplies a synchronous, testable execution callback.
    class DeviceActionCoordinator
    {
        struct Entry
        {
            DeviceActionRequest request;
            uint32_t session;
            uint64_t deadline;
            DeviceActionResult result;
            bool executed;
        };
        etl::vector<Entry, 16> entries;

    public:
        void disconnect(uint32_t session)
        {
            for (auto &entry : entries)
            {
                if (entry.session == session && !entry.executed)
                {
                    entry.executed = true;
                    entry.result = DeviceActionResult::Expired;
                }
            }
        }

        template <typename Execute>
        DeviceActionResult handle(const DeviceActionRequest &request, uint32_t session,
                                  uint64_t now, uint64_t bootId, Execute execute)
        {
            if (session == 0 || request.requestId == 0 ||
                static_cast<uint8_t>(request.phase) > 1 || static_cast<uint8_t>(request.updatePolicy) > 2)
            {
                return DeviceActionResult::Invalid;
            }
            if (static_cast<uint8_t>(request.action) > 3)
            {
                return DeviceActionResult::Unsupported;
            }
            if (request.action != DeviceAction::UsbBoot && request.updatePolicy != DeviceUpdatePolicy::RequireSaved)
            {
                return DeviceActionResult::Invalid;
            }
            if (request.action == DeviceAction::Status)
            {
                return request.phase == DeviceActionPhase::Prepare ? DeviceActionResult::Ok : DeviceActionResult::Invalid;
            }
            if (request.bootId != bootId)
            {
                return DeviceActionResult::StaleBoot;
            }
            for (auto &entry : entries)
            {
                if (!entry.executed && now >= entry.deadline)
                {
                    entry.executed = true;
                    entry.result = DeviceActionResult::Expired;
                }
            }
            for (auto &entry : entries)
            {
                if (entry.session != session || entry.request.requestId != request.requestId)
                {
                    continue;
                }
                if (!entry.request.sameOperation(request))
                {
                    return DeviceActionResult::Invalid;
                }
                if (entry.executed || request.phase == DeviceActionPhase::Prepare)
                {
                    return entry.result;
                }
                // Mark before the callback: a reentrant/duplicate Execute must
                // never start another flash write or reset.
                entry.executed = true;
                entry.result = DeviceActionResult::Busy;
                entry.result = execute(request);
                return entry.result;
            }
            if (request.phase != DeviceActionPhase::Prepare)
            {
                return DeviceActionResult::Invalid;
            }
            for (const auto &entry : entries)
            {
                if (!entry.executed)
                {
                    return DeviceActionResult::Busy;
                }
            }
            if (entries.full())
            {
                entries.erase(entries.begin());
            }
            entries.push_back({request, session, now + 10'000, DeviceActionResult::Accepted, false});
            return DeviceActionResult::Accepted;
        }
    };
}
