#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LAYOUT_NAME  "uzalfavit"
#define LAYOUT_SHORT "uz"
#define LAYOUT_DESC  "Uzbek (Alfavit, AltGr)"
#define MARKER       "uzkbd-generated"

struct key {
    const char *code;
    const char *sym[4];
};

static const struct key LAYOUT[] = {
    { "AD09", { "o",          "O",          "odiaeresis", "Odiaeresis" } },
    { "AC05", { "g",          "G",          "gbreve",     "Gbreve"     } },
    { "AC02", { "s",          "S",          "scedilla",   "Scedilla"   } },
    { "AB03", { "c",          "C",          "ccedilla",   "Ccedilla"   } },
    { "AC11", { "apostrophe", "quotedbl",   "U02BC",      "U02BC"      } },
};
#define NKEYS (sizeof LAYOUT / sizeof LAYOUT[0])

static char xkb_dir[1024];
static char symbols_path[1200];
static char rules_dir[1200];
static char rules_path[1400];

static void init_paths(void)
{
    const char *cfg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (cfg && *cfg)
        snprintf(xkb_dir, sizeof xkb_dir, "%s/xkb", cfg);
    else if (home && *home)
        snprintf(xkb_dir, sizeof xkb_dir, "%s/.config/xkb", home);
    else {
        fprintf(stderr, "HOME aniqlanmadi\n");
        exit(1);
    }
    snprintf(symbols_path, sizeof symbols_path, "%s/symbols/%s", xkb_dir, LAYOUT_NAME);
    snprintf(rules_dir, sizeof rules_dir, "%s/rules", xkb_dir);
    snprintf(rules_path, sizeof rules_path, "%s/evdev.xml", rules_dir);
}

static void write_symbols(FILE *f)
{
    fprintf(f, "// %s: bu faylni qo'lda o'zgartirmang, `uzkbd install` qayta yozadi.\n", MARKER);
    fprintf(f, "default partial alphanumeric_keys\n");
    fprintf(f, "xkb_symbols \"basic\" {\n");
    fprintf(f, "    include \"us(basic)\"\n");
    fprintf(f, "    name[Group1] = \"%s\";\n", LAYOUT_DESC);
    for (size_t i = 0; i < NKEYS; i++)
        fprintf(f, "    key <%s> { [ %s, %s, %s, %s ] };\n", LAYOUT[i].code,
                LAYOUT[i].sym[0], LAYOUT[i].sym[1], LAYOUT[i].sym[2], LAYOUT[i].sym[3]);
    fprintf(f, "    include \"level3(ralt_switch)\"\n");
    fprintf(f, "};\n");
}

static void write_rules(FILE *f)
{
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<!-- %s -->\n", MARKER);
    fprintf(f, "<xkbConfigRegistry version=\"1.1\">\n");
    fprintf(f, "  <layoutList>\n    <layout>\n      <configItem>\n");
    fprintf(f, "        <name>%s</name>\n", LAYOUT_NAME);
    fprintf(f, "        <shortDescription>%s</shortDescription>\n", LAYOUT_SHORT);
    fprintf(f, "        <description>%s</description>\n", LAYOUT_DESC);
    fprintf(f, "        <languageList><iso639Id>uzb</iso639Id></languageList>\n");
    fprintf(f, "      </configItem>\n    </layout>\n  </layoutList>\n</xkbConfigRegistry>\n");
}

static int is_ours(const char *path)
{
    char line[512];
    FILE *f = fopen(path, "r");
    int ours = 0;
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, MARKER)) { ours = 1; break; }
    fclose(f);
    return ours;
}

static int mkdir_p(const char *path)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    return (mkdir(tmp, 0755) != 0 && errno != EEXIST) ? -1 : 0;
}

static int write_file(const char *path, void (*writer)(FILE *))
{
    if (access(path, F_OK) == 0 && !is_ours(path)) {
        fprintf(stderr, "%s mavjud va bu dasturniki emas, tegmadim.\n", path);
        return -1;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return -1;
    }
    writer(f);
    if (fclose(f) != 0) {
        fprintf(stderr, "%s: yozib bo'lmadi\n", path);
        return -1;
    }
    printf("yozildi: %s\n", path);
    return 0;
}

static int cmd_install(void)
{
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/symbols", xkb_dir);
    if (mkdir_p(dir) != 0 || mkdir_p(rules_dir) != 0) {
        fprintf(stderr, "papka yaratib bo'lmadi: %s\n", strerror(errno));
        return 1;
    }
    if (write_file(symbols_path, write_symbols) != 0 || write_file(rules_path, write_rules) != 0)
        return 1;
    printf("\nTayyor. Sozlamalar -> Keyboard -> Add Input Source -> Other -> Uzbek -> \"%s\".\n"
           "Ro'yxat ko'rinmasa, tizimdan chiqib qayta kiring.\n", LAYOUT_DESC);
    return 0;
}

static int cmd_uninstall(void)
{
    const char *paths[] = { symbols_path, rules_path };
    for (int i = 0; i < 2; i++) {
        if (access(paths[i], F_OK) != 0)
            continue;
        if (!is_ours(paths[i])) {
            fprintf(stderr, "%s bizniki emas, o'chirmadim.\n", paths[i]);
            continue;
        }
        if (unlink(paths[i]) == 0)
            printf("o'chirildi: %s\n", paths[i]);
    }
    return 0;
}

static int run(const char *cmd)
{
    return system(cmd);
}

static int cmd_enable(int on)
{
    char buf[1024];
    FILE *p = popen("gsettings get org.gnome.desktop.input-sources sources", "r");
    if (!p || !fgets(buf, sizeof buf, p)) {
        if (p) pclose(p);
        fprintf(stderr, "gsettings o'qib bo'lmadi\n");
        return 1;
    }
    pclose(p);
    buf[strcspn(buf, "\n")] = '\0';

    const char *entry = "('xkb', '" LAYOUT_NAME "')";
    int present = strstr(buf, entry) != NULL;
    char newval[1200], cmd[1400];

    if (on) {
        if (present) { puts("allaqachon qo'shilgan"); return 0; }
        if (strncmp(buf, "@a(ss)", 6) == 0 || strcmp(buf, "[]") == 0)
            snprintf(newval, sizeof newval, "[('xkb', 'us'), %s]", entry);
        else {
            size_t n = strlen(buf);
            if (n < 2 || buf[n - 1] != ']') { fprintf(stderr, "kutilmagan qiymat: %s\n", buf); return 1; }
            buf[n - 1] = '\0';
            snprintf(newval, sizeof newval, "%s, %s]", buf, entry);
        }
    } else {
        if (!present) { puts("ro'yxatda yo'q"); return 0; }
        char *at = strstr(buf, entry);
        size_t elen = strlen(entry);
        char *after = at + elen;
        if (strncmp(after, ", ", 2) == 0) after += 2;
        else if (at - buf >= 2 && strncmp(at - 2, ", ", 2) == 0) at -= 2;
        memmove(at, after, strlen(after) + 1);
        snprintf(newval, sizeof newval, "%s", buf);
    }
    snprintf(cmd, sizeof cmd, "gsettings set org.gnome.desktop.input-sources sources \"%s\"", newval);
    return run(cmd) == 0 ? 0 : 1;
}

static int cmd_test(void)
{
    char keymap[1100], cmd[3400];
    snprintf(keymap, sizeof keymap, "%s/.test-keymap.xkb", xkb_dir);
    FILE *f = fopen(keymap, "w");
    if (!f) { fprintf(stderr, "%s: %s\n", keymap, strerror(errno)); return 1; }
    fprintf(f,
        "xkb_keymap {\n"
        "  xkb_keycodes { include \"evdev+aliases(qwerty)\" };\n"
        "  xkb_types    { include \"complete\" };\n"
        "  xkb_compat   { include \"complete\" };\n"
        "  xkb_symbols  { include \"pc+%s+inet(evdev)\" };\n"
        "};\n", LAYOUT_NAME);
    fclose(f);
    snprintf(cmd, sizeof cmd,
             "xkbcomp -I'%s' -I/usr/share/X11/xkb -xkb '%s' - 2>/dev/null | grep -E 'ccedilla|scedilla|gbreve|odiaeresis|U02BC' | head -5",
             xkb_dir, keymap);
    int rc = run(cmd);
    unlink(keymap);
    return rc == 0 ? 0 : 1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Ishlatish: %s <buyruq>\n"
        "  install    maketni ~/.config/xkb ichiga o'rnatadi\n"
        "  uninstall  o'rnatilgan fayllarni o'chiradi\n"
        "  enable     maketni GNOME kiritish manbalariga qo'shadi\n"
        "  disable    maketni kiritish manbalaridan olib tashlaydi\n"
        "  test       maket kompilyatsiya bo'lishini tekshiradi (xkbcomp)\n"
        "  print      hosil bo'ladigan symbols faylini ekranga chiqaradi\n", prog);
}

int main(int argc, char **argv)
{
    if (argc != 2) { usage(argv[0]); return 2; }
    init_paths();
    if (!strcmp(argv[1], "install"))   return cmd_install();
    if (!strcmp(argv[1], "uninstall")) return cmd_uninstall();
    if (!strcmp(argv[1], "enable"))    return cmd_enable(1);
    if (!strcmp(argv[1], "disable"))   return cmd_enable(0);
    if (!strcmp(argv[1], "test"))      return cmd_test();
    if (!strcmp(argv[1], "print"))     { write_symbols(stdout); return 0; }
    usage(argv[0]);
    return 2;
}
