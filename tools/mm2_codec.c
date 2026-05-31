#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITEM_RECORD_SIZE 20
#define ITEM_NAME_SIZE 12

static void usage(void) {
    fprintf(stderr,
            "MM2 codec tool for items.dat and str.dat\n"
            "Usage:\n"
            "  mm2_codec items-decode <items.dat> <items.csv>\n"
            "  mm2_codec items-encode <items.csv> <items.dat>\n"
            "  mm2_codec str-decode <str.dat> <text.txt>\n"
            "  mm2_codec str-encode <text.txt> <str.dat>\n");
}

static long file_size(FILE *fp) {
    long cur = ftell(fp);
    if (cur < 0) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        return -1;
    }
    long size = ftell(fp);
    if (size < 0) {
        return -1;
    }
    if (fseek(fp, cur, SEEK_SET) != 0) {
        return -1;
    }
    return size;
}

static int parse_u8(const char *s, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0' || v < 0 || v > 255) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_u16(const char *s, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0' || v < 0 || v > 65535) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int cmd_items_decode(const char *src_path, const char *dst_path) {
    FILE *in = fopen(src_path, "rb");
    FILE *out = NULL;
    if (!in) {
        fprintf(stderr, "open failed: %s\n", src_path);
        return 1;
    }
    out = fopen(dst_path, "wb");
    if (!out) {
        fprintf(stderr, "open failed: %s\n", dst_path);
        fclose(in);
        return 1;
    }

    long size = file_size(in);
    if (size < 0 || (size % ITEM_RECORD_SIZE) != 0) {
        fprintf(stderr, "invalid items.dat size: %ld\n", size);
        fclose(in);
        fclose(out);
        return 1;
    }

    fprintf(out,
            "index,name,separator,forbidden_classes,bonus_type,bonus_amount,effect_type,effect_amount,damage,pad,gold\n");

    uint8_t rec[ITEM_RECORD_SIZE];
    int index = 0;
    while (fread(rec, 1, ITEM_RECORD_SIZE, in) == ITEM_RECORD_SIZE) {
        char name[ITEM_NAME_SIZE + 1];
        for (int i = 0; i < ITEM_NAME_SIZE; i++) {
            name[i] = (char)rec[i];
        }
        name[ITEM_NAME_SIZE] = '\0';
        for (int i = ITEM_NAME_SIZE - 1; i >= 0; i--) {
            if (name[i] == ' ') {
                name[i] = '\0';
            } else {
                break;
            }
        }

        int separator = rec[12];
        int forbidden_classes = rec[13];  /* set bit = class CANNOT use */
        int bonus_type = (rec[14] >> 4) & 0x0F;
        int bonus_amount = rec[14] & 0x0F;
        int effect_type = (rec[15] >> 4) & 0x0F;
        int effect_amount = rec[15] & 0x0F;
        int damage = rec[16];
        int pad = rec[17];
        int gold = (int)rec[18] | ((int)rec[19] << 8);

        fprintf(out, "%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                index, name, separator, forbidden_classes, bonus_type, bonus_amount,
                effect_type, effect_amount, damage, pad, gold);
        index++;
    }

    fclose(in);
    fclose(out);
    return 0;
}

static int cmd_items_encode(const char *src_path, const char *dst_path) {
    FILE *in = fopen(src_path, "rb");
    FILE *out = NULL;
    if (!in) {
        fprintf(stderr, "open failed: %s\n", src_path);
        return 1;
    }
    out = fopen(dst_path, "wb");
    if (!out) {
        fprintf(stderr, "open failed: %s\n", dst_path);
        fclose(in);
        return 1;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), in)) {
        fprintf(stderr, "empty CSV\n");
        fclose(in);
        fclose(out);
        return 1;
    }

    int expected_index = 0;
    while (fgets(line, sizeof(line), in)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0) {
            continue;
        }

        char *fields[11];
        int field_count = 0;
        char *tok = strtok(line, ",");
        while (tok && field_count < 11) {
            fields[field_count++] = tok;
            tok = strtok(NULL, ",");
        }
        if (field_count != 11) {
            fprintf(stderr, "bad CSV row, expected 11 columns\n");
            fclose(in);
            fclose(out);
            return 1;
        }

        int idx = 0;
        int separator = 0;
        int forbidden_classes = 0;
        int bonus_type = 0;
        int bonus_amount = 0;
        int effect_type = 0;
        int effect_amount = 0;
        int damage = 0;
        int pad = 0;
        int gold = 0;

        if (parse_u16(fields[0], &idx) != 0 || idx != expected_index) {
            fprintf(stderr, "index mismatch at row %d\n", expected_index);
            fclose(in);
            fclose(out);
            return 1;
        }
        expected_index++;

        const char *name = fields[1];
        size_t name_len = strlen(name);
        if (name_len > ITEM_NAME_SIZE) {
            fprintf(stderr, "name too long: %s\n", name);
            fclose(in);
            fclose(out);
            return 1;
        }

        if (parse_u8(fields[2], &separator) != 0 ||
            parse_u8(fields[3], &forbidden_classes) != 0 ||
            parse_u8(fields[4], &bonus_type) != 0 ||
            parse_u8(fields[5], &bonus_amount) != 0 ||
            parse_u8(fields[6], &effect_type) != 0 ||
            parse_u8(fields[7], &effect_amount) != 0 ||
            parse_u8(fields[8], &damage) != 0 ||
            parse_u8(fields[9], &pad) != 0 ||
            parse_u16(fields[10], &gold) != 0) {
            fprintf(stderr, "numeric parse error\n");
            fclose(in);
            fclose(out);
            return 1;
        }

        if (bonus_type > 0x0F || bonus_amount > 0x0F ||
            effect_type > 0x0F || effect_amount > 0x0F) {
            fprintf(stderr, "nibble fields must be 0..15\n");
            fclose(in);
            fclose(out);
            return 1;
        }

        uint8_t rec[ITEM_RECORD_SIZE];
        memset(rec, 0, sizeof(rec));
        memcpy(rec, name, name_len);
        for (size_t i = name_len; i < ITEM_NAME_SIZE; i++) {
            rec[i] = ' ';
        }
        rec[12] = (uint8_t)separator;
        rec[13] = (uint8_t)forbidden_classes;
        rec[14] = (uint8_t)((bonus_type << 4) | bonus_amount);
        rec[15] = (uint8_t)((effect_type << 4) | effect_amount);
        rec[16] = (uint8_t)damage;
        rec[17] = (uint8_t)pad;
        rec[18] = (uint8_t)(gold & 0xFF);
        rec[19] = (uint8_t)((gold >> 8) & 0xFF);

        if (fwrite(rec, 1, ITEM_RECORD_SIZE, out) != ITEM_RECORD_SIZE) {
            fprintf(stderr, "write failed\n");
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static int cmd_str_decode(const char *src_path, const char *dst_path) {
    FILE *in = fopen(src_path, "rb");
    FILE *out = NULL;
    int ch;
    if (!in) {
        fprintf(stderr, "open failed: %s\n", src_path);
        return 1;
    }
    out = fopen(dst_path, "wb");
    if (!out) {
        fprintf(stderr, "open failed: %s\n", dst_path);
        fclose(in);
        return 1;
    }
    while ((ch = fgetc(in)) != EOF) {
        uint8_t b = (uint8_t)ch;
        uint8_t o = (b == 0x01) ? 0x0A : (uint8_t)((b + 0x1C) & 0xFF);
        if (fputc(o, out) == EOF) {
            fprintf(stderr, "write failed\n");
            fclose(in);
            fclose(out);
            return 1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int cmd_str_encode(const char *src_path, const char *dst_path) {
    FILE *in = fopen(src_path, "rb");
    FILE *out = NULL;
    int ch;
    if (!in) {
        fprintf(stderr, "open failed: %s\n", src_path);
        return 1;
    }
    out = fopen(dst_path, "wb");
    if (!out) {
        fprintf(stderr, "open failed: %s\n", dst_path);
        fclose(in);
        return 1;
    }
    while ((ch = fgetc(in)) != EOF) {
        uint8_t b = (uint8_t)ch;
        if (b == 0x0D) {
            continue;
        }
        uint8_t o = (b == 0x0A) ? 0x01 : (uint8_t)((b - 0x1C) & 0xFF);
        if (fputc(o, out) == EOF) {
            fprintf(stderr, "write failed\n");
            fclose(in);
            fclose(out);
            return 1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        usage();
        return 1;
    }

    const char *cmd = argv[1];
    const char *src = argv[2];
    const char *dst = argv[3];

    if (strcmp(cmd, "items-decode") == 0) {
        return cmd_items_decode(src, dst);
    }
    if (strcmp(cmd, "items-encode") == 0) {
        return cmd_items_encode(src, dst);
    }
    if (strcmp(cmd, "str-decode") == 0) {
        return cmd_str_decode(src, dst);
    }
    if (strcmp(cmd, "str-encode") == 0) {
        return cmd_str_encode(src, dst);
    }

    usage();
    return 1;
}
