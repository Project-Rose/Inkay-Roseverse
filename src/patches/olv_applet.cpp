/*  Copyright 2023 Pretendo Network contributors <pretendo.network>
    Copyright 2023 Ash Logan <ash@heyquark.com>
    Copyright 2019 Maschell

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "olv_urls.h"
#include "utils/logger.h"
#include "utils/replace_mem.h"

#include <vector>
#include <optional>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <nsysnet/nssl.h>
#include <function_patcher/function_patching.h>

#include "ca_pem.h" // generated at buildtime

struct olv_allowlist
{
    char scheme[16];
    char domain[128];
    char path[128]; // unverified
    unsigned char flags[5];
};

constexpr struct olv_allowlist original_entry = {
    .scheme = "https",
    .domain = ".nintendo.net",
    .path = "",
    .flags = {1, 1, 1, 1, 1},
};

constexpr struct olv_allowlist new_entry = {
    .scheme = "https",
    .domain = ".projectrose.cafe",
    .path = "",
    .flags = {1, 1, 1, 1, 1},
};

static std::optional<FSFileHandle> rootca_pem_handle{};
static std::optional<FSFileHandle> valid_tld_handle{};
std::vector<PatchedFunctionHandle> olv_patches;
std::vector<PatchedFunctionHandle> vino_patches;

DECL_FUNCTION(int, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, uint32_t *handle,
              int error)
{
    const char *initialOma = "vol/content/initial.oma";

    if (!Config::connect_to_network)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: Miiverse patches skipped.");
        return real_FSOpenFile(client, block, path, mode, handle, error);
    }

    if (strcmp(initialOma, path) == 0)
    {
        // below is a hacky (yet functional!) way to get Inkay to redirect URLs from the Miiverse applet
        // we do it when loading this file since it should only load once, preventing massive lag spikes as it searches all of MEM2 xD
        // WHBLogUdpInit();

        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: hewwo!\n");

        auto olv_ok = setup_olv_libs();
        // Patch applet binary too
        if (olv_ok)
            replace(0x10000000, 0x10000000, (const char *)&original_entry, sizeof(original_entry), (const char *)&new_entry, sizeof(new_entry));
        // Check for root CA file and take note of its handle
    }
    else if (strcmp("vol/content/browser/rootca.pem", path) == 0)
    {
        int ret = real_FSOpenFile(client, block, path, mode, handle, error);
        rootca_pem_handle = *handle;
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: Found Miiverse CA, replacing...");
        return ret;
    }
    else if (strcmp("vol/content/browser/effective_tld_names.dat", path) == 0)
    {
        // we patch the TLD because .cafe isnt on the list (projectrose.cafe)
        int ret = real_FSOpenFile(client, block, path, mode, handle, error);
        valid_tld_handle = *handle;
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: Found Miiverse TLD List, patching...");
        return ret;
    }

    return real_FSOpenFile(client, block, path, mode, handle, error);
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count,
              FSFileHandle handle, uint32_t unk1, uint32_t flags)
{
    if (size != 1)
    {
        DEBUG_FUNCTION_LINE("Inkay: Miiverse CA/LTD replacement failed!");
    }

    if (rootca_pem_handle && *rootca_pem_handle == handle)
    {
        strlcpy((char *)buffer, (const char *)ca_pem, size * count);

        // this can't be done above (in the FSOpenFile hook) since it's not loaded yet.
        // the hardcoded offsets suck but they really are at Random Places In The Heap
        return (FSStatus)count;
    }
    else if (valid_tld_handle && *valid_tld_handle == handle)
    {
        // we patch the tld "aero"...
        FSStatus ret = real_FSReadFile(
            client, block, buffer, size, count, handle, unk1, flags);

        if (ret > 0)
        {
            uint32_t totalSize = size * count;
            char *buf = (char *)buffer;

            for (uint32_t i = 0; i + 4 <= totalSize; i++)
            {
                if (memcmp(&buf[i], "aero", 4) == 0)
                {
                    memcpy(&buf[i], "cafe", 4);

                    DEBUG_FUNCTION_LINE_VERBOSE(
                        "Inkay: patched TLD aero -> cafe");
                }
            }
        }

        return ret;
    }

    return real_FSReadFile(client, block, buffer, size, count, handle, unk1, flags);
}

DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask)
{
    if (handle == rootca_pem_handle)
    {
        rootca_pem_handle.reset();
    }

    if (handle == valid_tld_handle)
    {
        valid_tld_handle.reset();
    }

    return real_FSCloseFile(client, block, handle, errorMask);
}

void patchOlvApplet()
{
    olv_patches.reserve(3);

    auto add_patch = [](function_replacement_data_t repl, const char *name)
    {
        PatchedFunctionHandle handle = 0;
        if (FunctionPatcher_AddFunctionPatch(&repl, &handle, nullptr) != FUNCTION_PATCHER_RESULT_SUCCESS)
        {
            DEBUG_FUNCTION_LINE("Inkay/OLV: Failed to patch %s!", name);
        }
        olv_patches.push_back(handle);
    };

    add_patch(REPLACE_FUNCTION_FOR_PROCESS(FSOpenFile, LIBRARY_COREINIT, FSOpenFile, FP_TARGET_PROCESS_MIIVERSE), "FSOpenFile");
    add_patch(REPLACE_FUNCTION_FOR_PROCESS(FSReadFile, LIBRARY_COREINIT, FSReadFile, FP_TARGET_PROCESS_MIIVERSE), "FSReadFile");
    add_patch(REPLACE_FUNCTION_FOR_PROCESS(FSCloseFile, LIBRARY_COREINIT, FSCloseFile, FP_TARGET_PROCESS_MIIVERSE), "FSCloseFile");
}

void unpatchOlvApplet()
{
    for (auto handle : olv_patches)
    {
        FunctionPatcher_RemoveFunctionPatch(handle);
    }
    olv_patches.clear();
}

//I tried hooking with Skin.dat, and other methods, but either TVii crashed or froze
//I do not know how to properly handle this, im sorry, i hate this.

/*
DECL_FUNCTION(void, NSSLInit)
{
    if (!Config::connect_to_network)
    {
        return real_NSSLInit();
    }

    auto olv_ok = setup_olv_libs();
    // Patch vino applet binary too
    if (olv_ok)
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: tvii olv patched yay!!\n");

    return real_NSSLInit();
}

void patchVinoApplet()
{
    vino_patches.reserve(1);

    auto add_patch = [](function_replacement_data_t repl, const char *name)
    {
        PatchedFunctionHandle handle = 0;
        if (FunctionPatcher_AddFunctionPatch(&repl, &handle, nullptr) != FUNCTION_PATCHER_RESULT_SUCCESS)
        {
            DEBUG_FUNCTION_LINE("Inkay/Vino OLV: Failed to patch %s!", name);
        }
        vino_patches.push_back(handle);
    };

    add_patch(REPLACE_FUNCTION_FOR_PROCESS(NSSLInit, LIBRARY_NSYSNET, NSSLInit, FP_TARGET_PROCESS_TVII), "NSSLInit");
}

void unpatchVinoApplet()
{
    for (auto handle : vino_patches)
    {
        FunctionPatcher_RemoveFunctionPatch(handle);
    }
    vino_patches.clear();
}
*/