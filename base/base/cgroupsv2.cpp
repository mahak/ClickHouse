#include <base/cgroupsv2.h>

#include <base/defines.h>

#include <fstream>
#include <string>

namespace fs = std::filesystem;

bool cgroupsV2Enabled()
{
#if defined(OS_LINUX)
    try
    {
        /// This file exists iff the host has cgroups v2 enabled.
        auto controllers_file = default_cgroups_mount / "cgroup.controllers";
        if (!fs::exists(controllers_file))
            return false;
        return true;
    }
    catch (const fs::filesystem_error &) /// all "underlying OS API errors", typically: permission denied
    {
        return false; /// not logging the exception as most callers fall back to cgroups v1
    }
#else
    return false;
#endif
}

fs::path cgroupV2PathOfProcess()
{
#if defined(OS_LINUX)
    chassert(cgroupsV2Enabled());
    /// All PIDs assigned to a cgroup are in /sys/fs/cgroups/{cgroup_name}/cgroup.procs
    /// A simpler way to get the membership is:
    std::ifstream cgroup_name_file("/proc/self/cgroup");
    if (!cgroup_name_file.is_open())
        return {};
    /// https://docs.kernel.org/admin-guide/cgroup-v2.html says:
    ///   /proc/$PID/cgroup” lists a process’s cgroup membership. If legacy cgroup is in use in the
    ///   system, this file may contain multiple lines, one for each hierarchy. The entry for cgroup
    ///   v2 is always in the format “0::$PATH”.
    ///
    /// So we're basically looking for a (v2) line with prefix "0::/", possibly among lines with
    /// other prefixes belonging v1. Note: It is valid to have an empty name as 'root' cgroup v2.
    static const std::string v2_prefix = "0::/";
    std::string cgroup;
    while (std::getline(cgroup_name_file, cgroup))
    {
        if (cgroup.starts_with(v2_prefix))
        {
            cgroup = cgroup.substr(v2_prefix.length());
            return default_cgroups_mount / cgroup;
        }
    }
    return {};
#else
    return {};
#endif
}

std::optional<std::string> getCgroupsV2PathContainingFile([[maybe_unused]] std::string_view file_name)
{
#if defined(OS_LINUX)
    if (!cgroupsV2Enabled())
        return {};

    fs::path current_cgroup = cgroupV2PathOfProcess();
    if (current_cgroup.empty())
        return {};

    /// Return the bottom-most nested file. If there is no such file at the current
    /// level, try again at the parent level as settings are inherited.
    while (current_cgroup != default_cgroups_mount.parent_path())
    {
        const auto path = current_cgroup / file_name;
        if (fs::exists(path))
            return {current_cgroup};
        current_cgroup = current_cgroup.parent_path();
    }
    return {};
#else
    return {};
#endif
}
