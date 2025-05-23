#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "gear.h"

char *gear_read_entire_file(const char *filename, size_t *size)
{
  char *buffer = NULL;
  FILE *f = fopen(filename, "rb");
  if (f == NULL) {
    fprintf(stderr, "Could not open the file %s: %s\n",
            filename, strerror(errno));
    goto error;
  }

  if (fseek(f, 0, SEEK_END) != 0)
    goto error;
  int n = ftell(f);
  if (fseek(f, 0, SEEK_SET) != 0)
    goto error;

  buffer = malloc(sizeof(char) * n);
  size_t m = fread(buffer, sizeof(char), n, f);
  assert(m == (size_t) n);
  *size = m;
  if (ferror(f) != 0)
    goto error;

  fclose(f);
  return buffer;

error:
  if (f)
    fclose(f);
  if (buffer)
    free(buffer);
  return NULL;
}

// TODO: convert this function to a macro?
bool gear_is_file(const char *filepath)
{
  return access(filepath, F_OK) == 0;
}

bool gear_is_dir(const char *dirpath)
{
  DIR *dir = opendir(dirpath);
  if (dir != NULL) {
    closedir(dir);
    return true;
  }
  return false;
}

bool gear_is_dir_empty(const char *dirpath)
{
  DIR *dir = opendir(dirpath);
  if (dir == NULL)
    return false;
  struct dirent *dirent = readdir(dir);
  while (dirent != NULL) {
    if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
      dirent = readdir(dir);
    else
      return false;
  }
  return true;
}

int gear_makedir(char *dirpath)
{
  GearSplitStrings arr = {0};
  gear_strsplit(&arr, GEAR_MAX_DIRNAME_LENGTH, dirpath, "/");
  char buf[GEAR_MAX_DIRNAME_LENGTH * GEAR_MAX_DIR_CHILDREN] = {0};
  for (size_t i = 0; i < arr.size; i++) {
    gear_strjoin(buf, sizeof(buf), arr.items[i], "/");
    if (mkdir(buf, 0777) != 0) {
      GEAR_ARRAY_DELETE_ALL(&arr);
      return 1;
    }
  }
  GEAR_ARRAY_DELETE_ALL(&arr);
  return 0;
}

char *gear_trim_leading(char *str)
{
  while (isspace(*str)) str++;
  return str;
}

void gear_trim_trailing(char *str)
{
  size_t n = strlen(str) - 1;
  if (n <= 0) return;
  while (isspace(str[n])) n--;
  str[n + 1] = '\0';
}

void gear_strsplit(GearSplitStrings *arr, size_t msize, char *str, const char *delims)
{
  assert(delims != NULL);
  size_t delims_len = strlen(delims);
  char *prev = str;
  char *substr = strstr(str, delims);
  while (substr != NULL) {
    char buf[msize];
    size_t n = substr - prev;
    assert(n < msize);
    memcpy(buf, prev, substr - prev);
    buf[n] = '\0';
    GEAR_ARRAY_APPEND(arr, strdup(buf));
    prev = substr + delims_len;
    substr = strstr(prev, delims);
  }
  char buf[msize];
  size_t n = strlen(prev);
  assert(n < msize);
  memcpy(buf, prev, n);
  buf[n] = '\0';
  GEAR_ARRAY_APPEND(arr, strdup(buf));
}

// TODO: return integer instead of asserting the size
void gear_strappend(char *buf, size_t size, const char *src)
{
  assert(size >= (*buf == 0) ? strlen(src) + 1 :
         strlen(buf) + strlen(src) + 1);
  if (*buf == 0)
    memcpy(buf, src, strlen(src) + 1);
  else
    strcat(buf, src);
}

// TODO: return integer instead of asserting the size
void gear_strjoin(char *buf, size_t size, const char *src, const char *delims)
{
  size_t delims_n = strlen(delims);
  assert(size >= (*buf == 0) ? strlen(src) + delims_n + 1 :
         strlen(buf) + strlen(src) + delims_n + 1);
  if (*buf == 0)
    gear_strappend(buf, size, src);
  else {
    gear_strappend(buf, size, delims);
    gear_strappend(buf, size, src);
  }
}

bool gear_strstartswith(const char *str, const char *prefix)
{
  assert(strlen(prefix) <= strlen(str));
  for (size_t i = 0; i < strlen(prefix); i++)
    if (str[i] != prefix[i]) return false;
  return true;
}

bool gear_strendswith(const char *str, const char *suffix)
{
  int n = strlen(str);
  int m = strlen(suffix);
  assert(m <= n);
  for (int i = 0; i > -m; i--)
    if (str[n - i] != suffix[m - i]) return false;
  return true;
}

void gear_strreplace(char *buf, size_t size, size_t memsize, char *str, const char *replacee, const char *replacement)
{
  GearSplitStrings arr = {0};
  gear_strsplit(&arr, memsize, str, replacee);
  for (size_t i = 0; i < arr.size; i++)
    gear_strjoin(buf, size, arr.items[i], replacement);
  GEAR_ARRAY_DELETE_ALL(&arr);
}

GearMap *gear_map_init(GearMapType key_type, const size_t capacity)
{
  GearMap *map = GEAR_NEW(GearMap, 1);
  if (map == NULL) {
    fprintf(stderr, "Could not allocate new map: %s\n",
            strerror(errno));
    exit(1);
  }
  map->key_type = key_type;
  map->capacity = capacity;
  map->size = 0;
  map->slots = GEAR_NEW_CLEAN(GearMapSlot **, map->capacity);
  if (map->slots == NULL) {
    fprintf(stderr, "Could not allocate slots for a new map: %s\n",
            strerror(errno));
    exit(1);
  }
  return map;
}

size_t hash_long(long long n, const size_t max)
{
  n *= 2;
  for (int _ = 0; _ < 10; _++) n = (n - 213) * 2;
  return (size_t) n % max;
}

size_t hash_integer(int n, const size_t max)
{
  n *= 2;
  for (int _ = 0; _ < 10; _++) n = (n - 213) * 2;
  return (size_t) n % max;
}

size_t hash_string(const char *str, const size_t max)
{
  size_t m = 2008;
  for (size_t i = 0; i < strlen(str); i++) m = (m - str[i]) * 3;
  return m % max;
}

size_t hash_double(double n, const size_t max)
{
  for (int _ = 0; _ < 10; _++) n = (n + 512) * 0.69;
  return (size_t) n % max;
}

bool cmpkeys(const GearMapType type, void *key1, void *key2)
{
  switch (type) {
    case MAP_TYPE_STRING: {
      return strcmp(key1, key2) == 0;
    } break;

    case MAP_TYPE_INTEGER: {
      return *((int *)key1) == *((int *)key2);
    } break;

    case MAP_TYPE_LONG: {
      return *((long long *)key1) == *((long long *)key2);
    } break;

    case MAP_TYPE_DOUBLE: {
      return *((double *)key1) == *((double *)key2);
    } break;

    default: {
      assert(0 && "unreachable");
    } break;
  }
}

size_t hash_key(const GearMapType ktype, const size_t capacity, const void *key)
{
  switch (ktype) {
    case MAP_TYPE_STRING: {
      return hash_string(key, capacity);
    } break;

    case MAP_TYPE_INTEGER: {
      return hash_integer(*((int *)key), capacity);
    } break;

    case MAP_TYPE_LONG: {
      return hash_long(*((long long *)key), capacity);
    } break;

    case MAP_TYPE_DOUBLE: {
      return hash_double(*((double *)key), capacity);
    } break;

    default: {
      assert(0 && "unreachable");
    } break;
  }
}

void gear_map_resize(GearMap *map, const size_t nsize)
{
  GearMapSlot **new = GEAR_NEW_CLEAN(GearMapSlot *, nsize);
  if (new == NULL) {
    fprintf(stderr, "Could not allocate memory for resizing hashmap: %s\n",
            strerror(errno));
    exit(1);
  }

  for (size_t i = 0; i < map->capacity; i++) {
    GearMapSlot *slot = map->slots[i];
    if (slot == NULL) continue;

    size_t hash = hash_key(map->key_type, nsize, slot->key);
    if (new[hash] == NULL)
      new[hash] = slot;
    else {
      while (new[hash] != NULL)
        hash = (hash + 1 >= nsize) ? 0 : hash + 1;
      new[hash] = slot;
    }
  }

  free(map->slots);
  map->slots = new;
  map->capacity = nsize;
}

// TODO: return int with an error?
void gear_map_insert(GearMap *map, void *key, void *value)
{
  if (map->size++ >= map->capacity)
    gear_map_resize(map, map->capacity * 2);

  size_t hash = hash_key(map->key_type, map->capacity, key);
  
  GearMapSlot *slot = GEAR_NEW(GearMapSlot, 1);
  slot->key = key;
  slot->value = value;
  
  if (map->slots[hash] == NULL)
    map->slots[hash] = slot;
  else {
    while (map->slots[hash] != NULL)
      hash = (hash + 1 >= map->capacity) ? 0 : hash + 1;
    map->slots[hash] = slot;
  }
}

int gear_map_remove(const GearMap *map, void *key)
{
  size_t hash = hash_key(map->key_type, map->capacity, key);
  if (cmpkeys(map->key_type, key, map->slots[hash]->key)) {
    free(map->slots[hash]->value);
    free(map->slots[hash]);
  }
  else {
    hash = (hash + 1 >= map->capacity) ? 0 : hash + 1;
    size_t start = hash;
    while (!cmpkeys(map->key_type, key, map->slots[hash]->key)) {
      if (hash == start) return 1; // second lap
      hash = (hash + 1 >= map->capacity) ? 0 : hash + 1;
    }
    free(map->slots[hash]->value);
    free(map->slots[hash]);
  }
  return 0;
}

void *gear_map_find(const GearMap *map, void *key)
{
  size_t hash = hash_key(map->key_type, map->capacity, key);
  if (cmpkeys(map->key_type, key, map->slots[hash]->key))
    return map->slots[hash]->value;
  else {
    size_t start = hash;
    hash = (hash + 1 >= map->capacity) ? 0 : hash + 1;
    while (!cmpkeys(map->key_type, key, map->slots[hash]->key)) {
      if (hash == start) return NULL; // second lap
      hash = (hash + 1 >= map->capacity) ? 0 : hash + 1;
    }
    return map->slots[hash]->value;
  }
  return NULL;
}

void gear_map_delete_kvpairs(GearMap *map)
{
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->slots[i] == NULL) continue;
    free(map->slots[i]->value);
    free(map->slots[i]->key);
    free(map->slots[i]);
  }
}

void gear_map_delete_keys(GearMap *map)
{
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->slots[i] == NULL) continue;
    free(map->slots[i]->key);
    free(map->slots[i]);
  }
}

void gear_map_delete_values(GearMap *map)
{
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->slots[i] == NULL) continue;
    free(map->slots[i]->value);
    free(map->slots[i]);
  }
}

void gear_map_delete_slots(GearMap *map)
{
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->slots[i] == NULL) continue;
    free(map->slots[i]);
  }
}
