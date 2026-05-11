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

#include <coreinit/dynload.h>
#include <coreinit/memory.h>
#include <coreinit/memorymap.h>
#include <coreinit/title.h>
#include <coreinit/mcp.h>
#include <coreinit/time.h>
#include <cstdio>
#include "Notification.h"
#include <function_patcher/function_patching.h>

#include <nn/act.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <cstddef>

#define SERVICETOKEN_MAX_SIZE 513

static bool check_olv_libs()
{
    OSDynLoad_Module olv_handle = 0;
    OSDynLoad_Error dret;

    dret = OSDynLoad_IsModuleLoaded("nn_olv", &olv_handle);
    if (dret == OS_DYNLOAD_OK && olv_handle != 0)
    {
        return true;
    }

    dret = OSDynLoad_IsModuleLoaded("nn_olv2", &olv_handle);
    if (dret == OS_DYNLOAD_OK && olv_handle != 0)
    {
        return true;
    }

    return false;
}

#define OLV_RPL "nn_olv.rpl"
bool path_is_olv(const char *path)
{
    auto path_len = strlen(path);
    auto *path_suffix = path + path_len - (sizeof(OLV_RPL) - 1); // 1 for null
    return strcmp(path_suffix, OLV_RPL) == 0;
}

static bool check_act_libs()
{
    OSDynLoad_Module act_handle = 0;
    OSDynLoad_Error dret;

    dret = OSDynLoad_IsModuleLoaded("nn_act", &act_handle);
    if (dret == OS_DYNLOAD_OK && act_handle != 0)
    {
        return true;
    }
    return false;
}

void new_rpl_loaded(OSDynLoad_Module module, void *ctx, OSDynLoad_NotifyReason reason, OSDynLoad_NotifyData *rpl)
{
    if (!Config::connect_to_network)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: Miiverse patches skipped.");
        return;
    }

    // Loaded olv?
    if (reason != OS_DYNLOAD_NOTIFY_LOADED)
        return;
    if (!rpl->name || !path_is_olv(rpl->name))
        return;

    replace(rpl->dataAddr, rpl->dataSize, original_url, sizeof(original_url), new_url, sizeof(new_url));
}

#define MIIVERSE_TID_J 0x000500301001600A
#define MIIVERSE_TID_U 0x000500301001610A
#define MIIVERSE_TID_E 0x000500301001620A

static bool isRunningApplet()
{
    uint64_t tid = OSGetTitleID();

    if (tid == 0)
        return false;

    // Applet category: 0x00050030
    if ((tid & 0xFFFFFFFF00000000ULL) == 0x0005003000000000ULL)
        return true;

    return false;
}

static bool doesAppletNeedOlivePatch()
{
    uint64_t tid = OSGetTitleID();

    if (tid == 0)
        return false;

    if (tid == MIIVERSE_TID_J || tid == MIIVERSE_TID_U || tid == MIIVERSE_TID_E)
        return true;

    /*if (tid == MIIVERSE_TID_J || tid == MIIVERSE_TID_U || tid == MIIVERSE_TID_E || tid == TVII_TID_J || tid == TVII_TID_U || tid == TVII_TID_E)
        return true;*/

    return false;
}

extern "C" uint32_t nnactGetCountry(char *country) asm("GetCountry__Q2_2nn3actFPc");
extern "C" uint8_t nnactGetGender() asm("GetGender__Q2_2nn3actFv");
extern "C" uint8_t nnactGetMiiImageUrlEx(char outUrl[257], int slot) asm("GetMiiImageUrlEx__Q2_2nn3actFPcUc");

// is Miiverse client ID global?
#define OLV_CLIENT_ID "87cd32617f1985439ea608c2746e4610"

/**
 * @brief Calculates the maximum Base64 encoded length for a given number of bytes.
 * @details Base64 expands every 3 bytes into 4 characters, plus padding.
 * Add 1 extra for null terminator.
 * @param n Number of input bytes.
 * @return Maximum required output size in bytes.
 */
#define BASE64_ENCODED_SIZE(n) ((((n) + 2) / 3) * 4 + 1)

/**
 * @brief Encodes a binary buffer into Base64 text.
 * @param input Pointer to raw input bytes.
 * @param len Number of bytes in input.
 * @param output Pointer to destination buffer (must be at least BASE64_ENCODED_SIZE(len)).
 */
static void base64_encode(const unsigned char *input, size_t len, char *output)
{
    static const char cBase64Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    size_t outIndex = 0;
    size_t i = 0;

    while (i + 2 < len)
    {
        // Take 3 bytes and split into 4 groups of 6 bits.
        int triple = (input[i] << 16) | (input[i + 1] << 8) | input[i + 2];
        output[outIndex++] = cBase64Alphabet[(triple >> 18) & 0x3F];
        output[outIndex++] = cBase64Alphabet[(triple >> 12) & 0x3F];
        output[outIndex++] = cBase64Alphabet[(triple >> 6) & 0x3F];
        output[outIndex++] = cBase64Alphabet[triple & 0x3F];
        i += 3;
    }

    // Handle remaining 1 or 2 bytes with padding.
    if (i < len)
    {
        int triple = input[i] << 16;
        if (i + 1 < len)
        {
            triple |= input[i + 1] << 8;
        }

        output[outIndex++] = cBase64Alphabet[(triple >> 18) & 0x3F];
        output[outIndex++] = cBase64Alphabet[(triple >> 12) & 0x3F];

        if (i + 1 < len)
        {
            output[outIndex++] = cBase64Alphabet[(triple >> 6) & 0x3F];
            output[outIndex++] = '=';
        }
        else
        {
            output[outIndex++] = '=';
            output[outIndex++] = '=';
        }
    }

    // Null terminate.
    output[outIndex] = '\0';
}

static char cached_slot_base64[SERVICETOKEN_MAX_SIZE];
static int cached_slot = -1;
static char cached_code_serial[21];
static bool cachedMCP = false;

static void initMCP(void)
{
    if (cachedMCP)
        return;

    int handle = MCP_Open();
    if (handle < 0)
        return;

    MCPSysProdSettings settings alignas(0x40);
    memset(&settings, 0, sizeof(settings));

    if (MCP_GetSysProdSettings(handle, &settings) == 0)
    {
        size_t code_len = strnlen(settings.code_id, sizeof(settings.code_id));
        size_t serial_len = strnlen(settings.serial_id, sizeof(settings.serial_id));

        memcpy(cached_code_serial, settings.code_id, code_len);
        memcpy(cached_code_serial + code_len, settings.serial_id, serial_len);
        cached_code_serial[code_len + serial_len] = '\0';

        cachedMCP = true;
    }

    MCP_Close(handle);
}

// Generate random alphanumeric password
static void gen_olive_user_key(char *out, size_t out_len, size_t length = 20)
{
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static const size_t charset_len = sizeof(charset) - 1;

    if (length >= out_len)
        length = out_len - 1;

    OSTick seed = OSGetTick(); // always safe on Wii U
    for (size_t i = 0; i < length; i++)
    {
        seed = seed * 1664525 + 1013904223; // LCG step
        out[i] = charset[seed % charset_len];
    }
    out[length] = '\0';
}

static void obfuscate_string(char *str, size_t len)
{
    static const char secret[] = "placeholder";
#define SECRET_LEN (sizeof(secret) - 1) // exclude null terminator

    for (size_t i = 0; i < len; ++i)
    {
        str[i] ^= secret[i % SECRET_LEN]; // XOR each byte with secret
    }
}

void init_olive_token()
{
    //i fixed the stupid mother fucker.
    //apparently. i forgot to initialize act, i was scared by how limited i was so i never thought of it?
    //i also remember getting Miiverse to crash (applet) maybe my coding sucked or my implementation was incorrect.

    //nonetheless, its fixed. enjoy :p - david "Jay" Soda
    //I DID NOT VIBE CODE THIS! I MADE IT ALL MYSELF! 5 HOURS OF CODING, RESEARCHING, TESTING! ALL FUCKING WORTH IT!
    nn::act::Initialize();
    int slotNo = nn::act::GetSlotNo();

    if (slotNo == cached_slot)
    {
        DEBUG_FUNCTION_LINE("Slot number is the same than last! %i", slotNo);
        //Lets not clean the token, since this is not an error, just finish act early...
        //if we dont finish act im fucking scared were gonna memory leak or sum shit
        nn::act::Finalize();
        return;
    }

    if (slotNo == 0)
    {
        DEBUG_FUNCTION_LINE("Unloaded account! or Unexistant!");
        cached_slot_base64[0] = '\0';
        nn::act::Finalize();
        return;
    }

    unsigned int pid = nn::act::GetPrincipalId();
    if (pid == 0)
    {
        DEBUG_FUNCTION_LINE("PID at slot no %i is invalid!", slotNo);
        cached_slot_base64[0] = '\0';
        nn::act::Finalize();
        return;
    }

    initMCP();

    const char *folderPath = "fs:/vol/external01/wiiu/olive";
    if (mkdir(folderPath, 0777) != 0 && errno != EEXIST)
    {
        DEBUG_FUNCTION_LINE("Failed to create olive folder");
        ShowNotification("Failed to create folder to store Roséverse auth data.");
        cached_slot_base64[0] = '\0';
        nn::act::Finalize();
        return;
    }

    char filePath[512];

    int pathLen = snprintf(
        filePath,
        sizeof(filePath),
        "%s/acc_%u_key.txt",
        folderPath,
        pid);

    if (pathLen < 0 || pathLen >= (int)sizeof(filePath))
    {
        DEBUG_FUNCTION_LINE("File path overflow");
        ShowNotification("Internal path error for Roséverse.");
        cached_slot_base64[0] = '\0';
        nn::act::Finalize();
        return;
    }

    // Load/generate Olive password
    char password[256] = {0};

    FILE *in = fopen(filePath, "r");

    if (in)
    {
        if (!fgets(password, sizeof(password), in))
        {
            DEBUG_FUNCTION_LINE("Failed to read token file");
            ShowNotification("Failed to read access key for Roséverse.");
            fclose(in);
            cached_slot_base64[0] = '\0';
            nn::act::Finalize();
            return;
        }

        fclose(in);

        size_t len = strlen(password);

        if (len > 0 && password[len - 1] == '\n')
            password[len - 1] = '\0';
    }
    else
    {
        gen_olive_user_key(password, sizeof(password));

        if (password[0] == '\0')
        {
            DEBUG_FUNCTION_LINE("Generated empty password");
            ShowNotification("Failed to generate account key for Roséverse.");
            cached_slot_base64[0] = '\0';
            nn::act::Finalize();
            return;
        }

        FILE *out = fopen(filePath, "w");

        if (!out)
        {
            DEBUG_FUNCTION_LINE("Failed to create token file");
            ShowNotification("Failed to save access key for Roséverse.");
            cached_slot_base64[0] = '\0';
            nn::act::Finalize();
            return;
        }

        if (fputs(password, out) < 0)
        {
            DEBUG_FUNCTION_LINE("Failed writing token file");
            ShowNotification("Failed to write access key for Roséverse.");
            fclose(out);
            cached_slot_base64[0] = '\0';
            nn::act::Finalize();
            return;
        }

        fclose(out);
    }

    char country[3] = {0};
    uint16_t y;
    uint8_t m;
    uint8_t d;

    nnactGetCountry(country);
    uint8_t gender = nnactGetGender();

    // Do after country and gender.
    nn::act::GetBirthday(&y, &m, &d);

    char miiImageUrl[257];
    nnactGetMiiImageUrlEx(miiImageUrl, slotNo);

    //nn::act is normally not utilized anymore after this.
    nn::act::Finalize();

    // Check if URL contains the Nintendo Account secure domain anywhere
    int isNNAcc = strstr(miiImageUrl,
                         "https://mii-secure.account.nintendo.net/") != NULL;

    // Build token string
    char combined[SERVICETOKEN_MAX_SIZE];
    size_t offset = 0;
    offset += snprintf(combined + offset, sizeof(combined) - offset,
                       "%u,%s,%s,%u,%u,%u/%u/%u,%s,0",
                       pid, password, country, gender, isNNAcc, y, m, d,
                       cached_code_serial);

    obfuscate_string(combined, offset);

    // Base64 on top
    base64_encode((const uint8_t *)combined, offset, cached_slot_base64);

    // Validate encoded token
    if (cached_slot_base64[0] == '\0' || strlen(cached_slot_base64) == 0)
    {
        DEBUG_FUNCTION_LINE("Invalid base64 token data");
        ShowNotification("Could not generate auth header for Roséverse.");
        cached_slot_base64[0] = '\0';
        return;
    }

    cached_slot = slotNo;
    DEBUG_FUNCTION_LINE("New slot number is %i", slotNo);

    DEBUG_FUNCTION_LINE("Token initialized, Base64 length: %zu", strlen(cached_slot_base64));
}

DECL_FUNCTION(nn::Result, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, nn::act::SlotNo slot, nn::act::ACTLoadOption unk1, char const *unk2, bool unk3)
{
    // we should load first
    DEBUG_FUNCTION_LINE("requesting account switch");
    nn::Result ret = real_LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb(slot, unk1, unk2, unk3);
    init_olive_token();
    return ret;
}

// Override AIST to just return cached token
DECL_FUNCTION(int, AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4,
              uint8_t *token, const char *client_id, uint32_t cacheDurationInSeconds, bool unk1, bool unk2)
{
    DEBUG_FUNCTION_LINE("Miiverse client id requested");

    if (client_id != NULL && strcmp(client_id, OLV_CLIENT_ID) == 0)
    {
        // memset(token, 0, SERVICETOKEN_MAX_SIZE); // string gets terminated below
        size_t len = strlen(cached_slot_base64);
        memcpy(token, cached_slot_base64, len);
        token[len] = 0;
        return 0;
    }

    // fallback to real function
    return real_AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4(
        token, client_id, cacheDurationInSeconds, unk1, unk2);
}

static PatchedFunctionHandle LCAPatchHandle = 0;
static PatchedFunctionHandle AistPatchHandle = 0;
static PatchedFunctionHandle AistPatchHandleApplet = 0;

static PatchedFunctionHandle addFunctionPatch_AIST(uint32_t address)
{
    uint32_t physicalAddress = OSEffectiveToPhysical(address);
    DEBUG_FUNCTION_LINE("effective %08X, physical %08X", address, physicalAddress);

    PatchedFunctionHandle patchedFunctionHandle = 0;
    function_replacement_data_t patchData REPLACE_FUNCTION_VIA_ADDRESS(AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4, physicalAddress, address);
    FunctionPatcherStatus result = FunctionPatcher_AddFunctionPatch(&patchData, &patchedFunctionHandle, nullptr);

    if (result != FUNCTION_PATCHER_RESULT_SUCCESS)
    {
        DEBUG_FUNCTION_LINE("AddFunctionPatch returned %d", result);
        return 0;
    }

    return patchedFunctionHandle;
}

static PatchedFunctionHandle addFunctionPatch_LCA(uint32_t address)
{
    uint32_t physicalAddress = OSEffectiveToPhysical(address);
    DEBUG_FUNCTION_LINE("effective %08X, physical %08X", address, physicalAddress);

    PatchedFunctionHandle patchedFunctionHandle = 0;
    function_replacement_data_t patchData REPLACE_FUNCTION_VIA_ADDRESS(LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, physicalAddress, address);
    FunctionPatcherStatus result = FunctionPatcher_AddFunctionPatch(&patchData, &patchedFunctionHandle, nullptr);

    if (result != FUNCTION_PATCHER_RESULT_SUCCESS)
    {
        DEBUG_FUNCTION_LINE("AddFunctionPatch returned %d", result);
        return 0;
    }

    return patchedFunctionHandle;
}

bool setup_olv_libs()
{
    if (!Config::connect_to_network)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Inkay: Miiverse patches skipped.");
        return false;
    }

    OSDynLoad_AddNotifyCallback(&new_rpl_loaded, nullptr);

    auto actLoaded = check_act_libs();
    if (!actLoaded)
    {
        DEBUG_FUNCTION_LINE("Inkay: no act loaded, fuck\n");
    }
    else
    {
        OSDynLoad_Module NN_ACT = 0;
        // Acquire the nn_act module
        if (OSDynLoad_Acquire("nn_act.rpl", &NN_ACT) != OS_DYNLOAD_OK)
        {
            DEBUG_FUNCTION_LINE("Failed to acquire nn_act.rpl module.");
        }
        else
        {
            uint32_t LCAaddressVIR = 0;
            uint32_t AISTaddressVIR = 0;

            if (OSDynLoad_FindExport(NN_ACT, OS_DYNLOAD_EXPORT_FUNC, "LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb", (void **)&LCAaddressVIR) != OS_DYNLOAD_OK)
            { // Get the physical address
                DEBUG_FUNCTION_LINE("Failed to find LoadConsoleAccount function in nn_act.rpl");
            }
            else
            {
                if (!isRunningApplet())
                {
                    if (LCAPatchHandle != 0)
                    {
                        DEBUG_FUNCTION_LINE("Removing previous LCA patch (handle %08X)", LCAPatchHandle);
                        FunctionPatcherStatus removeRes = FunctionPatcher_RemoveFunctionPatch(LCAPatchHandle);
                        DEBUG_FUNCTION_LINE("RemoveFunctionPatch returned %d", removeRes);
                        LCAPatchHandle = 0;
                    }
                    DEBUG_FUNCTION_LINE("Adding new LCA patch at %08X", LCAaddressVIR);
                    LCAPatchHandle = addFunctionPatch_LCA(LCAaddressVIR);
                    DEBUG_FUNCTION_LINE("New LCA patch handle = %08X", LCAPatchHandle);
                }
            }

            if (OSDynLoad_FindExport(NN_ACT, OS_DYNLOAD_EXPORT_FUNC, "AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4", (void **)&AISTaddressVIR) != OS_DYNLOAD_OK)
            { // Get the physical address
                DEBUG_FUNCTION_LINE("Failed to find AcquireIndependentServiceToken function in nn_act.rpl");
            }
            else
            {
                if (isRunningApplet() && doesAppletNeedOlivePatch())
                {
                    // check if an applet is running
                    // to avoid unpatching background game
                    if (AistPatchHandleApplet != 0)
                    {
                        DEBUG_FUNCTION_LINE("Removing previous AIST applet patch (handle %08X)", AistPatchHandleApplet);
                        FunctionPatcher_RemoveFunctionPatch(AistPatchHandleApplet);
                        AistPatchHandleApplet = 0;
                    }

                    AistPatchHandleApplet = addFunctionPatch_AIST(AISTaddressVIR);
                    DEBUG_FUNCTION_LINE("New AIST applet patch handle = %08X", AistPatchHandleApplet);
                }
                else
                {
                    // Removes previous patch and patches when game/menu starts
                    if (AistPatchHandle != 0)
                    {
                        DEBUG_FUNCTION_LINE("Removing previous AIST patch (handle %08X)", AistPatchHandle);
                        FunctionPatcher_RemoveFunctionPatch(AistPatchHandle);
                        AistPatchHandle = 0;
                    }
                    DEBUG_FUNCTION_LINE("Adding new AIST patch at %08X", AISTaddressVIR);
                    AistPatchHandle = addFunctionPatch_AIST(AISTaddressVIR);
                    DEBUG_FUNCTION_LINE("New AIST patch handle = %08X", AistPatchHandle);
                }
            }
        }
    }

    auto olvLoaded = check_olv_libs();
    if (!olvLoaded)
    {
        DEBUG_FUNCTION_LINE("Inkay: no olv, quitting for now\n");
        return false;
    }

    // wish there was a better way than "blow through MEM2"
    uint32_t base_addr, size;
    if (OSGetMemBound(OS_MEM2, &base_addr, &size))
    {
        DEBUG_FUNCTION_LINE("Inkay: OSGetMemBound failed!");
        return false;
    }

    return replace(base_addr, size, original_url, sizeof(original_url), new_url, sizeof(new_url));
}

void remove_aist_patches()
{
    FunctionPatcher_RemoveFunctionPatch(LCAPatchHandle);
    FunctionPatcher_RemoveFunctionPatch(AistPatchHandle);
    FunctionPatcher_RemoveFunctionPatch(AistPatchHandleApplet);
    LCAPatchHandle = 0;
    AistPatchHandle = 0;
    AistPatchHandleApplet = 0;
}