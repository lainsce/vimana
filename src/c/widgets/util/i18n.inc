// Cogito i18n — simple key-value translation catalog.
// Load translations from a JSON file, then _(key) returns the translated string.
// Falls back to the key itself if no translation is found.

#include <stdlib.h>
#include <string.h>

// ---- Catalog entry (hash map bucket) ----
typedef struct CogitoI18nEntry {
  char *key;
  char *value;
  struct CogitoI18nEntry *next;
} CogitoI18nEntry;

#define COGITO_I18N_BUCKETS 256

static CogitoI18nEntry *cogito_i18n_table[COGITO_I18N_BUCKETS];
static char cogito_i18n_locale[64] = "";
static char cogito_i18n_domain[128] = "";

// djb2 hash
static unsigned int cogito_i18n_hash(const char *s) {
  unsigned int h = 5381;
  while (*s)
    h = h * 33 + (unsigned char)*s++;
  return h;
}

static void cogito_i18n_clear(void) {
  for (int i = 0; i < COGITO_I18N_BUCKETS; i++) {
    CogitoI18nEntry *e = cogito_i18n_table[i];
    while (e) {
      CogitoI18nEntry *next = e->next;
      free(e->key);
      free(e->value);
      free(e);
      e = next;
    }
    cogito_i18n_table[i] = NULL;
  }
}

static void cogito_i18n_set(const char *key, const char *value) {
  unsigned int idx = cogito_i18n_hash(key) % COGITO_I18N_BUCKETS;
  CogitoI18nEntry *e = cogito_i18n_table[idx];
  while (e) {
    if (strcmp(e->key, key) == 0) {
      free(e->value);
      e->value = strdup(value);
      return;
    }
    e = e->next;
  }
  e = (CogitoI18nEntry *)calloc(1, sizeof(*e));
  e->key = strdup(key);
  e->value = strdup(value);
  e->next = cogito_i18n_table[idx];
  cogito_i18n_table[idx] = e;
}

static const char *cogito_i18n_get(const char *key) {
  if (!key)
    return "";
  unsigned int idx = cogito_i18n_hash(key) % COGITO_I18N_BUCKETS;
  CogitoI18nEntry *e = cogito_i18n_table[idx];
  while (e) {
    if (strcmp(e->key, key) == 0)
      return e->value;
    e = e->next;
  }
  return key; // fallback to key itself
}

// ---- JSON loader (simple flat {"key":"value",...} parser) ----
static void cogito_i18n_load_json(const char *path) {
  if (!path)
    return;
  FILE *f = fopen(path, "r");
  if (!f)
    return;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len <= 0) {
    fclose(f);
    return;
  }

  char *buf = (char *)malloc((size_t)len + 1);
  size_t rd = fread(buf, 1, (size_t)len, f);
  buf[rd] = '\0';
  fclose(f);

  cogito_i18n_clear();

  // Minimal flat JSON object parser: {"key":"value", ...}
  const char *p = buf;
  // skip to opening brace
  while (*p && *p != '{')
    p++;
  if (*p == '{')
    p++;

  while (*p) {
    // skip whitespace, commas
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')
      p++;
    if (*p == '}' || *p == '\0')
      break;

    // parse key string
    if (*p != '"') break;
    p++;
    const char *ks = p;
    while (*p && *p != '"') {
      if (*p == '\\') p++; // skip escaped char
      if (*p) p++;
    }
    size_t klen = (size_t)(p - ks);
    char *key = (char *)malloc(klen + 1);
    memcpy(key, ks, klen);
    key[klen] = '\0';
    if (*p == '"') p++;

    // skip colon
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;

    // parse value string
    if (*p != '"') { free(key); break; }
    p++;
    const char *vs = p;
    while (*p && *p != '"') {
      if (*p == '\\') p++;
      if (*p) p++;
    }
    size_t vlen = (size_t)(p - vs);
    char *val = (char *)malloc(vlen + 1);
    memcpy(val, vs, vlen);
    val[vlen] = '\0';
    if (*p == '"') p++;

    cogito_i18n_set(key, val);
    free(key);
    free(val);
  }

  free(buf);
}

// ---- Set locale + auto-load catalog ----
static void cogito_i18n_set_locale(const char *locale) {
  if (!locale)
    return;
  size_t len = strlen(locale);
  if (len >= sizeof(cogito_i18n_locale))
    len = sizeof(cogito_i18n_locale) - 1;
  memcpy(cogito_i18n_locale, locale, len);
  cogito_i18n_locale[len] = '\0';

  // Auto-load: if domain is set, try <domain>/<locale>.json
  if (cogito_i18n_domain[0]) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", cogito_i18n_domain, locale);
    cogito_i18n_load_json(path);
  }
}

static void cogito_i18n_set_domain(const char *domain) {
  if (!domain)
    return;
  size_t len = strlen(domain);
  if (len >= sizeof(cogito_i18n_domain))
    len = sizeof(cogito_i18n_domain) - 1;
  memcpy(cogito_i18n_domain, domain, len);
  cogito_i18n_domain[len] = '\0';
}

static const char *cogito_i18n_get_locale(void) {
  if (cogito_i18n_locale[0])
    return cogito_i18n_locale;
  // Detect from environment
  const char *env = getenv("LANG");
  if (!env) env = getenv("LC_ALL");
  if (!env) env = getenv("LC_MESSAGES");
  if (env) return env;
  return "en";
}
