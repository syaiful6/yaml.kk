#include <kklib.h>
#include <yaml.h>

typedef struct kk_yamlc_buffer_s {
  unsigned char *buffer;
  unsigned int size, used;
} kk_yamlc_buffer_t;

static void kk_yaml_emitter_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  yaml_emitter_t *emitter = (yaml_emitter_t *)p;
  if (emitter != NULL) {
    yaml_emitter_delete(emitter);
    kk_free(emitter, ctx);
  }
}

static kk_box_t kk_yaml_emitter_create(kk_context_t *ctx) {
  yaml_emitter_t *emitter = kk_malloc(sizeof(yaml_emitter_t), ctx);
  if (!yaml_emitter_initialize(emitter)) {
    kk_info_message("yaml_emitter_initialize failed at %p\n", emitter);
  }

  return kk_cptr_raw_box(&kk_yaml_emitter_free, emitter, ctx);
}

static kk_box_t kk_yamlc_buffer_create(void *p, kk_block_t *b,
                                       kk_context_t *ctx) {
  kk_yamlc_buffer_t *buffer = kk_malloc(sizeof(kk_yamlc_buffer_t), ctx);
  buffer->buffer = 0;
  buffer->size = buffer->used = 0;

  return kk_cptr_box(buffer, ctx);
}

static kk_unit_t kk_yaml_yamlc_emitter_set_output_file(kk_box_t bemitter,
                                                       kk_box_t bfile,
                                                       kk_context_t *ctx) {
  yaml_emitter_t *emitter =
      (yaml_emitter_t *)kk_cptr_unbox_borrowed(bemitter, ctx);
  FILE *file = (FILE *)kk_cptr_unbox_borrowed(bfile, ctx);

  yaml_emitter_set_output_file(emitter, file);

  kk_box_drop(bfile, ctx);
  kk_box_drop(bemitter, ctx);

  return kk_Unit;
}

static int kk_yamlc_buffer_append(void *ext, unsigned char *str, size_t size) {
  kk_yamlc_buffer_t *b = ext;
  int new_size, new_used;
  unsigned char *tmp;

  new_used = b->used + size;
  for (new_size = b->size ? b->size : 8; new_size < new_used; new_size *= 2)
    ;

  if (new_size != b->size) {
    tmp = realloc(b->buffer, new_size);
    if (!tmp)
      return 0;
    b->buffer = tmp;
    b->size = new_size;
  }

  memcpy(b->buffer + b->used, str, size);
  b->used = new_size;

  return 1;
}

static kk_unit_t kk_yaml_yamlc_emitter_set_output_buffer(kk_box_t bemitter,
                                                         kk_box_t bbufer,
                                                         kk_context_t *ctx) {
  yaml_emitter_t *emitter =
      (yaml_emitter_t *)kk_cptr_unbox_borrowed(bemitter, ctx);
  kk_yamlc_buffer_t *buffer =
      (kk_yamlc_buffer_t *)kk_cptr_unbox_borrowed(bbufer, ctx);

  yaml_emitter_set_output(emitter, kk_yamlc_buffer_append, buffer);

  kk_box_drop(bbufer, ctx);
  kk_box_drop(bemitter, ctx);

  return kk_Unit;
}

static kk_string_t kk_yaml_yamlc_get_buffer_string(kk_box_t bbufer,
                                                   kk_context_t *ctx) {
  kk_yamlc_buffer_t *buffer =
      (kk_yamlc_buffer_t *)kk_cptr_unbox_borrowed(bbufer, ctx);

  const char *buff = (const char *)buffer->buffer;
  kk_string_t str = kk_string_alloc_from_utf8n(buffer->used, buff, ctx);

  return str;
}
