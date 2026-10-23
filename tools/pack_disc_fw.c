/**
 * @file pack_disc_fw.c
 * @brief Flashable `.fw` Firmware Update Package Generator & Verifier for FiiO Snowsky DISC
 *
 * Packs the compiled `dialos-disc` binary and the default `/data/dialos/config.json`
 * payload into a structured `.fw` update image with IEEE 802.3 CRC32 verification.
 *
 * Usage:
 *   ./build/pack_disc_fw <input_disc_bin> <output_fw_path>
 *   ./build/pack_disc_fw --verify <output_fw_path>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAL_FW_MAGIC "DIALFW10"

typedef struct {
    char     magic[8];        /* "DIALFW10" */
    char     version[16];     /* "1.0.0" */
    char     target[24];      /* "snowsky-disc-x2000" */
    uint32_t bin_size;        /* Size of dialos-disc binary in bytes */
    uint32_t bin_crc32;       /* CRC32 of dialos-disc binary */
    uint32_t cfg_size;        /* Size of default config.json in bytes */
    uint32_t cfg_crc32;       /* CRC32 of default config.json */
} dial_fw_header_t;

static const char k_default_config_json[] =
    "{\n"
    "  \"system\": {\n"
    "    \"version\": \"1.0.0\",\n"
    "    \"theme_mode\": \"night\",\n"
    "    \"theme_accent\": \"moss\",\n"
    "    \"brightness_pct\": 70,\n"
    "    \"screen_timeout_sec\": 30,\n"
    "    \"haptic_strength\": \"crisp\",\n"
    "    \"vinyl_spinning\": true\n"
    "  },\n"
    "  \"audio\": {\n"
    "    \"master_volume\": 82,\n"
    "    \"high_gain\": false,\n"
    "    \"dac_filter\": 0,\n"
    "    \"gapless_enabled\": true,\n"
    "    \"eq_preset\": \"acoustic\",\n"
    "    \"usb_charge_isolated\": true\n"
    "  },\n"
    "  \"connectivity\": {\n"
    "    \"wifi_enabled\": true,\n"
    "    \"last_ssid\": \"Studio 5G\",\n"
    "    \"bt_enabled\": true,\n"
    "    \"bt_codec_pref\": \"LDAC_990\"\n"
    "  }\n"
    "}\n";

static uint32_t crc32_compute(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static int verify_fw(const char *fw_path) {
    FILE *fp = fopen(fw_path, "rb");
    if (!fp) {
        fprintf(stderr, "[FW] Failed to open %s\n", fw_path);
        return 1;
    }

    dial_fw_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[FW] Truncated header in %s\n", fw_path);
        fclose(fp);
        return 2;
    }

    if (memcmp(hdr.magic, DIAL_FW_MAGIC, 8) != 0) {
        fprintf(stderr, "[FW] Invalid magic in %s\n", fw_path);
        fclose(fp);
        return 3;
    }

    if (hdr.bin_size == 0 || hdr.cfg_size == 0) {
        fprintf(stderr, "[FW] Empty payload in %s\n", fw_path);
        fclose(fp);
        return 4;
    }

    uint8_t *bin_buf = (uint8_t *)malloc(hdr.bin_size);
    uint8_t *cfg_buf = (uint8_t *)malloc(hdr.cfg_size);
    if (!bin_buf || !cfg_buf) {
        free(bin_buf);
        free(cfg_buf);
        fclose(fp);
        return 5;
    }

    if (fread(bin_buf, 1, hdr.bin_size, fp) != hdr.bin_size ||
        fread(cfg_buf, 1, hdr.cfg_size, fp) != hdr.cfg_size) {
        fprintf(stderr, "[FW] Payload read mismatch in %s\n", fw_path);
        free(bin_buf);
        free(cfg_buf);
        fclose(fp);
        return 6;
    }
    fclose(fp);

    uint32_t calc_bin_crc = crc32_compute(bin_buf, hdr.bin_size);
    uint32_t calc_cfg_crc = crc32_compute(cfg_buf, hdr.cfg_size);
    free(bin_buf);
    free(cfg_buf);

    if (calc_bin_crc != hdr.bin_crc32 || calc_cfg_crc != hdr.cfg_crc32) {
        fprintf(stderr, "[FW] CRC32 mismatch in %s (bin: 0x%08X vs 0x%08X, cfg: 0x%08X vs 0x%08X)\n",
                fw_path, calc_bin_crc, hdr.bin_crc32, calc_cfg_crc, hdr.cfg_crc32);
        return 7;
    }

    printf("[PASS] pack_disc_fw: Verified %s (target=%s, v%s, bin=%u B, CRC32=0x%08X)\n",
           fw_path, hdr.target, hdr.version, hdr.bin_size, hdr.bin_crc32);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--verify") == 0) {
        return verify_fw(argv[2]);
    }
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_disc_bin> <output_fw_path> | --verify <fw_path>\n", argv[0]);
        return 1;
    }

    const char *bin_path = argv[1];
    const char *fw_path  = argv[2];

    FILE *bfp = fopen(bin_path, "rb");
    if (!bfp) {
        fprintf(stderr, "[FW] Cannot open input binary %s\n", bin_path);
        return 1;
    }
    fseek(bfp, 0, SEEK_END);
    long bsz = ftell(bfp);
    fseek(bfp, 0, SEEK_SET);
    if (bsz <= 0) {
        fclose(bfp);
        return 2;
    }

    uint8_t *bin_data = (uint8_t *)malloc((size_t)bsz);
    if (!bin_data || fread(bin_data, 1, (size_t)bsz, bfp) != (size_t)bsz) {
        free(bin_data);
        fclose(bfp);
        return 3;
    }
    fclose(bfp);

    size_t cfg_len = strlen(k_default_config_json);
    dial_fw_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, DIAL_FW_MAGIC, 8);
    strncpy(hdr.version, "1.0.0", sizeof(hdr.version) - 1);
    strncpy(hdr.target, "snowsky-disc-x2000", sizeof(hdr.target) - 1);
    hdr.bin_size  = (uint32_t)bsz;
    hdr.bin_crc32 = crc32_compute(bin_data, (size_t)bsz);
    hdr.cfg_size  = (uint32_t)cfg_len;
    hdr.cfg_crc32 = crc32_compute((const uint8_t *)k_default_config_json, cfg_len);

    FILE *ofp = fopen(fw_path, "wb");
    if (!ofp) {
        free(bin_data);
        return 4;
    }
    fwrite(&hdr, sizeof(hdr), 1, ofp);
    fwrite(bin_data, 1, (size_t)bsz, ofp);
    fwrite(k_default_config_json, 1, cfg_len, ofp);
    fclose(ofp);
    free(bin_data);

    printf("[DialOS FW] Packaged %s (%u bytes binary + %u bytes config, CRC32=0x%08X)\n",
           fw_path, hdr.bin_size, hdr.cfg_size, hdr.bin_crc32);
    return 0;
}
