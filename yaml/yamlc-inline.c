#include <kklib.h>
#include <stdint.h>
#include <stdio.h>
#include <yaml.h>

#include "kklib/box.h"
#include "kklib/integer.h"
#include "kklib/platform.h"
#include "kklib/string.h"

/**
 * Common pattern for accessing int32 value
 */
#define DEFINE_YAML_EVENT_INT32_GETTER(UNION_MEMBER, FIELD_NAME, FIELD_TYPE, SUFFIX)    \
  static int32_t kk_yaml_yamlc_get_event_##SUFFIX(kk_box_t bevent, kk_context_t *ctx) { \
    yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);     \
    /* The field's value (enum or int) is safely cast to int32_t */                     \
    int32_t value = (int32_t) ((FIELD_TYPE) event->data.UNION_MEMBER.FIELD_NAME);       \
    return value;                                                                       \
  }

#define DEFINE_YAML_EVENT_STRING_GETTER(UNION_MEMBER, FIELD_NAME, FIELD_TYPE, SUFFIX)          \
  static kk_string_t kk_yaml_yamlc_get_event_##SUFFIX(kk_box_t bevent, kk_context_t *ctx) {    \
    yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);            \
    /* Access the field, cast to const char* for string functions */                           \
    const char *c_str_ptr = (const char *) ((FIELD_TYPE) event->data.UNION_MEMBER.FIELD_NAME); \
                                                                                               \
    if(!c_str_ptr) {            /* Handle NULL pointers from libyaml (e.g., absent             \
                                   optional fields) */                                         \
      return kk_string_empty(); /* Return an empty Koka string */                              \
    }                                                                                          \
    kk_string_t koka_str = kk_string_alloc_from_qutf8(c_str_ptr, ctx);                         \
                                                                                               \
    return koka_str;                                                                           \
  }

static void kk_yaml_parser_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  // kk_unused(ctx);
  yaml_parser_t *parser = (yaml_parser_t *) p;
  // kk_info_message("free yaml parser at %p\n", parser);
  if(parser != NULL) {
    yaml_parser_delete(parser);
    kk_free(parser, ctx);
  }
}

static kk_box_t kk_yaml_parser_create(kk_context_t *ctx) {
  // initialize parser
  yaml_parser_t *parser = kk_malloc(sizeof(yaml_parser_t), ctx);
  // check the result?
  if(!yaml_parser_initialize(parser)) {
    kk_info_message("yaml_parser_initialize failed at %p\n", parser);
  }
  // kk_info_message("create yaml parser at %p\n", parser);
  return kk_cptr_raw_box(&kk_yaml_parser_free, parser, ctx);
}

// For parser events - these need yaml_event_delete()
static void kk_yaml_parser_event_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) p;
  // kk_info_message("freeing parser event at %p\n", event);
  if(event != NULL) {
    yaml_event_delete(event);  // Parser events need this
    kk_free(event, ctx);
  }
}

// For emitter events - these should NOT call yaml_event_delete() after emission
static void kk_yaml_emitter_event_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) p;
  // kk_info_message("freeing emitter event at %p\n", event);
  if(event != NULL) {
    // Don't call yaml_event_delete() - libyaml owns it after successful emission
    kk_free(event, ctx);
  }
}

static kk_unit_t kk_yaml_yamlc_set_input_file(kk_box_t bparser, kk_box_t bfile, kk_context_t *ctx) {
  yaml_parser_t *parser = (yaml_parser_t *) kk_cptr_unbox_borrowed(bparser, ctx);
  FILE *file = (FILE *) kk_cptr_unbox_borrowed(bfile, ctx);
  // kk_info_message("set parser %p input to %p\n", parser, file);
  yaml_parser_set_input_file(parser, file);

  kk_box_drop(bfile, ctx);
  kk_box_drop(bparser, ctx);

  return kk_Unit;
}

static kk_unit_t kk_yaml_c_set_input_string(kk_box_t bparser, kk_addr_t yaml, kk_ssize_t len,
                                            kk_context_t *ctx) {
  yaml_parser_t *parser = (yaml_parser_t *) kk_cptr_unbox_borrowed(bparser, ctx);
  yaml_parser_set_input_string(parser, (uint8_t *) yaml, len);
  // kk_string_drop(yaml, ctx);
  kk_box_drop(bparser, ctx);

  return kk_Unit;
}

static kk_box_t kk_yaml_yamlc_with_c_string(kk_string_t s, kk_function_t f, kk_context_t *_ctx) {
  kk_ssize_t len;
  kk_addr_t cptr = (kk_addr_t) kk_string_cbuf_borrow(s, &len, kk_context());
  return kk_function_call(kk_box_t, (kk_function_t, kk_addr_t, kk_ssize_t, kk_context_t *), f,
                          (f, cptr, len, kk_context()), kk_context());
}

static kk_box_t kk_yaml_yamlc_open_file(kk_string_t path, kk_context_t *ctx) {
  kk_ssize_t len;
  const char *cpat = kk_string_cbuf_borrow(path, &len, ctx);

  FILE *file = fopen(cpat, "rb");
  kk_string_drop(path, ctx);

  return kk_cptr_box(file, ctx);
}

static kk_box_t kk_yaml_yamlc_open_write_file(kk_string_t path, kk_context_t *ctx) {
  kk_ssize_t len;
  const char *cpat = kk_string_cbuf_borrow(path, &len, ctx);

  FILE *file = fopen(cpat, "wb");
  kk_string_drop(path, ctx);

  return kk_cptr_box(file, ctx);
}

static void kk_yaml_yamlc_close_file(kk_box_t bfile, kk_context_t *ctx) {
  FILE *file = (FILE *) kk_cptr_unbox_borrowed(bfile, ctx);

  fclose(file);

  kk_box_drop(bfile, ctx);
}

static kk_std_core_types__either kk_yaml_parse_one(kk_box_t bparser, kk_context_t *ctx) {
  yaml_parser_t *parser = (yaml_parser_t *) kk_cptr_unbox_borrowed(bparser, ctx);
  yaml_event_t *event = kk_malloc(sizeof(yaml_event_t), ctx);
  // kk_info_message("kk_yaml_parse_one at %p\n", parser);
  if(!yaml_parser_parse(parser, event)) {
    // Create error struct with detailed information
    kk_string_t error_msg =
        kk_string_alloc_from_qutf8(parser->problem ? parser->problem : "Unknown parse error", ctx);
    kk_integer_t line =
        kk_integer_from_size_t(parser->problem_mark.line + 1, ctx);  // Convert to 1-based
    kk_integer_t column =
        kk_integer_from_size_t(parser->problem_mark.column + 1, ctx);  // Convert to 1-based
    kk_integer_t index = kk_integer_from_size_t(parser->problem_mark.index, ctx);

    struct kk_yaml_yamlc_Yaml_parse_error error_struct =
        kk_yaml_yamlc__new_Yaml_parse_error(error_msg, line, column, index, ctx);

    yaml_event_delete(event);
    kk_free(event, ctx);
    kk_box_drop(bparser, ctx);
    return kk_std_core_types__new_Left(kk_yaml_yamlc__yaml_parse_error_box(error_struct, ctx), ctx);
  }
  // kk_info_message("kk_yaml_parse_one initialize event at %p\n", event);
  kk_box_drop(bparser, ctx);
  // we got the event, wrap to Right
  return kk_std_core_types__new_Right(kk_cptr_raw_box(&kk_yaml_parser_event_free, event, ctx), ctx);
}

static int32_t kk_yaml_yamlc_get_event_type(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);
  int32_t event_type = event->type;
  // kk_info_message("Got event type %d\n", event_type);
  // we borrow the event, no need to drop
  // kk_box_drop(bevent, ctx);
  return event_type;
}

inline kk_yaml_yamlc__yaml_mark yamlc_convert_libyaml_mark(yaml_mark_t *mark, kk_context_t *ctx) {
  return kk_yaml_yamlc__new_Yaml_mark(kk_integer_from_size_t(mark->index, ctx),
                                      kk_integer_from_size_t(mark->line, ctx),
                                      kk_integer_from_size_t(mark->column, ctx), ctx);
}

static kk_yaml_yamlc__yaml_mark kk_yaml_yamlc_get_start_mark(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  // kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->start_mark, ctx);
}

static kk_yaml_yamlc__yaml_mark kk_yaml_yamlc_get_end_mark(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  // kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->end_mark, ctx);
}

DEFINE_YAML_EVENT_STRING_GETTER(alias, anchor, yaml_char_t *, alias_anchor);

static kk_string_t kk_yaml_yamlc_get_event_scalar_value(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  const char *value = (const char *) event->data.scalar.value;
  size_t len = event->data.scalar.length;

  kk_string_t str = kk_string_alloc_from_utf8n(len, value, ctx);

  return str;
}

// accessor for stream_start
DEFINE_YAML_EVENT_INT32_GETTER(stream_start, encoding, yaml_encoding_t, stream_start_encoding);

// accessor for document start
static kk_std_core_types__maybe kk_yaml_yamlc_get_event_document_start_version_directive(
    kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  if(!event->data.document_start.version_directive) {
    // null, convert to nothing
    return kk_std_core_types__new_Nothing(ctx);
  }

  int major = (int) (event->data.document_start.version_directive->major);
  int minor = (int) (event->data.document_start.version_directive->minor);
  return kk_std_core_types__new_Just(
      kk_std_core_types__tuple2_box(
          kk_std_core_types__new_Tuple2(kk_integer_box(kk_integer_from_int(major, ctx), ctx),
                                        kk_integer_box(kk_integer_from_int(minor, ctx), ctx), ctx),
          ctx),
      ctx);
}

static kk_std_core_types__list kk_yaml_yamlc_get_event_document_start_tag_directives(
    kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  kk_std_core_types__list hd = kk_std_core_types__new_Nil(ctx);

  if(event->data.document_start.tag_directives.start ==
     event->data.document_start.tag_directives.end) {
    return hd;
  }

  yaml_tag_directive_t *tag;
  kk_std_core_types__tuple2 directive;
  const char *handler;
  const char *prefix;

  for(tag = event->data.document_start.tag_directives.start;
      tag != event->data.document_start.tag_directives.end; tag++) {
    handler = (const char *) ((yaml_char_t *) tag->handle);
    prefix = (const char *) ((yaml_char_t *) tag->prefix);

    directive =
        kk_std_core_types__new_Tuple2(kk_string_box(kk_string_alloc_from_qutf8(handler, ctx)),
                                      kk_string_box(kk_string_alloc_from_qutf8(prefix, ctx)), ctx);
    hd = kk_std_core_types__new_Cons(kk_reuse_null, 0,
                                     kk_std_core_types__tuple2_box(directive, ctx), hd, ctx);
  }

  return hd;
}
DEFINE_YAML_EVENT_INT32_GETTER(document_start, implicit, int, document_start_implicit);

// accessor for scalar field
DEFINE_YAML_EVENT_INT32_GETTER(scalar, style, yaml_scalar_style_t, scalar_style);
DEFINE_YAML_EVENT_INT32_GETTER(scalar, plain_implicit, int, scalar_plain_implicit);
DEFINE_YAML_EVENT_INT32_GETTER(scalar, quoted_implicit, int, scalar_quoted_implicit);
DEFINE_YAML_EVENT_STRING_GETTER(scalar, tag, yaml_char_t *, scalar_tag);
DEFINE_YAML_EVENT_STRING_GETTER(scalar, anchor, yaml_char_t *, scalar_anchor);

// accessor for sequence field
DEFINE_YAML_EVENT_STRING_GETTER(sequence_start, anchor, yaml_char_t *, sequence_start_anchor);
DEFINE_YAML_EVENT_INT32_GETTER(sequence_start, style, yaml_sequence_style_t, sequence_start_style);
DEFINE_YAML_EVENT_STRING_GETTER(sequence_start, tag, yaml_char_t *, sequence_start_tag);
DEFINE_YAML_EVENT_INT32_GETTER(sequence_start, implicit, int, sequence_start_implicit);

// accessor for mapping field
DEFINE_YAML_EVENT_STRING_GETTER(mapping_start, anchor, yaml_char_t *, mapping_start_anchor);
DEFINE_YAML_EVENT_STRING_GETTER(mapping_start, tag, yaml_char_t *, mapping_start_tag);
DEFINE_YAML_EVENT_INT32_GETTER(mapping_start, style, yaml_mapping_style_t, mapping_start_style);
DEFINE_YAML_EVENT_INT32_GETTER(mapping_start, implicit, int, mapping_start_implicit);

// accessor for document end
DEFINE_YAML_EVENT_INT32_GETTER(document_end, implicit, int, document_end_implicit);

typedef struct kk_yamlc_buffer_s {
  unsigned char *buffer;
  unsigned int size, used;
} kk_yamlc_buffer_t;

static void kk_yaml_emitter_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  yaml_emitter_t *emitter = (yaml_emitter_t *) p;
  if(emitter != NULL) {
    yaml_emitter_delete(emitter);
    kk_free(emitter, ctx);
  }
}

static kk_box_t kk_yaml_emitter_create(kk_context_t *ctx) {
  yaml_emitter_t *emitter = kk_malloc(sizeof(yaml_emitter_t), ctx);
  if(!yaml_emitter_initialize(emitter)) {
    kk_info_message("yaml_emitter_initialize failed at %p\n", emitter);
  }

  return kk_cptr_raw_box(&kk_yaml_emitter_free, emitter, ctx);
}

static void kk_yamlc_buffer_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  kk_yamlc_buffer_t *buffer = (kk_yamlc_buffer_t *) p;
  // kk_info_message("freeing buffer at %p\n", buffer);
  if(buffer != NULL) {
    if(buffer->buffer) {
      free(buffer->buffer);  // Use standard free for buffer allocated with realloc
    }
    kk_free(buffer, ctx);
  }
}

static kk_box_t kk_yamlc_buffer_create(kk_context_t *ctx) {
  kk_yamlc_buffer_t *buffer = kk_malloc(sizeof(kk_yamlc_buffer_t), ctx);
  buffer->buffer = NULL;
  buffer->size = buffer->used = 0;

  return kk_cptr_raw_box(&kk_yamlc_buffer_free, buffer, ctx);
}

static kk_unit_t kk_yaml_yamlc_emitter_set_output_file(kk_box_t bemitter, kk_box_t bfile,
                                                       kk_context_t *ctx) {
  yaml_emitter_t *emitter = (yaml_emitter_t *) kk_cptr_unbox_borrowed(bemitter, ctx);
  FILE *file = (FILE *) kk_cptr_unbox_borrowed(bfile, ctx);

  yaml_emitter_set_output_file(emitter, file);

  kk_box_drop(bfile, ctx);
  kk_box_drop(bemitter, ctx);

  return kk_Unit;
}

static int kk_yamlc_buffer_append(void *ext, unsigned char *str, size_t size) {
  // kk_info_message("kk_yamlc_buffer_append start\n");
  kk_yamlc_buffer_t *b = ext;
  int new_size, new_used;
  unsigned char *tmp;

  new_used = b->used + size;
  for(new_size = b->size ? b->size : 8; new_size < new_used; new_size *= 2);

  if(new_size != (int) b->size) {
    // kk_info_message("realloc\n");
    tmp = realloc(b->buffer, new_size);
    if(!tmp) return 0;
    b->buffer = tmp;
    b->size = new_size;
  }

  memcpy(b->buffer + b->used, str, size);
  b->used = new_used;

  // kk_info_message("kk_yamlc_buffer_append succeed\n");

  return 1;
}

static kk_unit_t kk_yaml_yamlc_emitter_set_output_buffer(kk_box_t bemitter, kk_box_t bbufer,
                                                         kk_context_t *ctx) {
  yaml_emitter_t *emitter = (yaml_emitter_t *) kk_cptr_raw_unbox_borrowed(bemitter, ctx);
  kk_yamlc_buffer_t *buffer = (kk_yamlc_buffer_t *) kk_cptr_raw_unbox_borrowed(bbufer, ctx);

  yaml_emitter_set_output(emitter, kk_yamlc_buffer_append, buffer);

  return kk_Unit;
}

static kk_string_t kk_yaml_yamlc_get_buffer_string(kk_box_t bbufer, kk_context_t *ctx) {
  kk_yamlc_buffer_t *buffer = (kk_yamlc_buffer_t *) kk_cptr_raw_unbox_borrowed(bbufer, ctx);

  kk_string_t str = kk_string_alloc_from_utf8n(buffer->used, (const char *) buffer->buffer, ctx);

  return str;
}

static kk_integer_t kk_yaml_yamlc_emit_event(kk_box_t bemitter, kk_box_t bevent,
                                             kk_context_t *ctx) {
  yaml_emitter_t *emitter = (yaml_emitter_t *) kk_cptr_unbox_borrowed(bemitter, ctx);
  yaml_event_t *event = (yaml_event_t *) kk_cptr_raw_unbox_borrowed(bevent, ctx);

  // kk_info_message("emitting event: %p to emitter %p\n", event, emitter);

  int ok = yaml_emitter_emit(emitter, event);

  // Don't drop the event box if emission succeeded - libyaml owns it now
  return kk_integer_from_int(ok, ctx);
}

// Macro for yaml event that have one argument ie stream_end
#define KK_YAML_EVENT_SIMPLE_BUILDER(NAME, FUNC)                                              \
  static kk_std_core_types__maybe kk_yaml_yamlc_make_##NAME##_event(kk_context_t *ctx) {      \
    yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);                                  \
    if(!FUNC(ev)) {                                                                           \
      kk_free(ev, ctx);                                                                       \
      return kk_std_core_types__new_Nothing(ctx);                                             \
    }                                                                                         \
    return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), \
                                       ctx);                                                  \
  }

// Event builder

static kk_std_core_types__maybe kk_yaml_yamlc_make_stream_start_event(int32_t encoding_i32,
                                                                      kk_context_t *ctx) {
  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_stream_start_event_initialize(ev, (yaml_encoding_t) encoding_i32)) {
    // kk_info_message("kk_yaml_yamlc_make_stream_start_event: failed to initialize event");
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

KK_YAML_EVENT_SIMPLE_BUILDER(stream_end, yaml_stream_end_event_initialize);

static kk_std_core_types__maybe kk_yaml_yamlc_make_document_start_event(
    kk_std_core_types__maybe version, kk_std_core_types__list xs, bool implicit,
    kk_context_t *ctx) {
  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  yaml_version_directive_t *version_ptr = NULL;
  yaml_version_directive_t version_struct;  // Stack allocated

  if(kk_std_core_types__is_Just(version, ctx)) {
    struct kk_yaml_yamlc_YamlVersion vversion =
        kk_yaml_yamlc__yaml_version_unbox(version._cons.Just.value, KK_BORROWED, ctx);
    version_struct.major = (int) kk_integer_clamp(vversion.major, ctx);
    version_struct.minor = (int) kk_integer_clamp(vversion.minor, ctx);
    version_ptr = &version_struct;  // Point to stack allocated struct
    // kk_info_message("using version directive: %d.%d\n", version_struct.major,
    // version_struct.minor);
  }

  // For now, ignore tag directives to isolate the memory issue
  yaml_tag_directive_t *start = NULL;
  yaml_tag_directive_t *end = NULL;

  kk_std_core_types__list_drop(xs, ctx);
  kk_std_core_types__maybe_drop(version, ctx);

  if(!yaml_document_start_event_initialize(ev, version_ptr, start, end, implicit)) {
    // Clean up allocated memory on failure
    // kk_info_message("kk_yaml_yamlc_make_document_start_event failed to initialize: %p\n", ev);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

static kk_std_core_types__maybe kk_yaml_yamlc_make_document_end_event(bool implicit,
                                                                      kk_context_t *ctx) {
  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_document_end_event_initialize(ev, (int) implicit)) {
    // kk_info_message("kk_yaml_yamlc_make_document_end_event failed to initialize: %p\n", ev);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

static kk_std_core_types__maybe kk_yaml_yamlc_make_alias_event(kk_string_t anchor,
                                                               kk_context_t *ctx) {
  // Copy the anchor string - libyaml will own it
  kk_ssize_t len;
  const char *anchor_buf = kk_string_cbuf_borrow(anchor, &len, ctx);
  yaml_char_t *canchor = malloc(len + 1);
  memcpy(canchor, anchor_buf, len);
  canchor[len] = '\0';

  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_alias_event_initialize(ev, canchor)) {
    // kk_info_message("kk_yaml_yamlc_make_alias_event failed to initialize: %p\n", ev);
    free(canchor);
    kk_string_drop(anchor, ctx);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }
  kk_string_drop(anchor, ctx);

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

static kk_std_core_types__maybe kk_yaml_yamlc_make_scalar_event(
    kk_std_core_types__maybe anchor, kk_std_core_types__maybe tag, kk_string_t value,
    bool plain_implicit, bool quoted_implicit, int32_t style, kk_context_t *ctx) {
  yaml_char_t *canchor = NULL;
  if(kk_std_core_types__is_Just(anchor, ctx)) {
    kk_string_t anchor_str = kk_string_unbox(anchor._cons.Just.value);
    kk_ssize_t anchor_len;
    const char *anchor_buf = kk_string_cbuf_borrow(anchor_str, &anchor_len, ctx);
    canchor = kk_malloc(anchor_len + 1, ctx);
    memcpy(canchor, anchor_buf, anchor_len);
    canchor[anchor_len] = '\0';
    // kk_string_drop(anchor_str, ctx);
  }

  yaml_char_t *ctag = NULL;
  if(kk_std_core_types__is_Just(tag, ctx)) {
    kk_string_t tag_str = kk_string_unbox(tag._cons.Just.value);
    kk_ssize_t tag_len;
    const char *tag_buf = kk_string_cbuf_borrow(tag_str, &tag_len, ctx);
    ctag = kk_malloc(tag_len + 1, ctx);
    memcpy(ctag, tag_buf, tag_len);
    ctag[tag_len] = '\0';
    // kk_string_drop(tag_str, ctx);
  }

  // Copy the value string - libyaml will own it
  kk_ssize_t len;
  const char *value_buf = kk_string_cbuf_borrow(value, &len, ctx);
  yaml_char_t *cvalue = kk_malloc(len + 1, ctx);
  memcpy(cvalue, value_buf, len);
  cvalue[len] = '\0';

  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_scalar_event_initialize(ev, canchor, ctag, cvalue, len, (int) plain_implicit,
                                   (int) quoted_implicit, (yaml_scalar_style_t) style)) {
    // kk_info_message("failed to create event scalar\n");
    if(canchor) free(canchor);
    if(ctag) free(ctag);
    free(cvalue);
    kk_std_core_types__maybe_drop(anchor, ctx);
    kk_std_core_types__maybe_drop(tag, ctx);
    kk_string_drop(value, ctx);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  kk_std_core_types__maybe_drop(anchor, ctx);
  kk_std_core_types__maybe_drop(tag, ctx);
  kk_string_drop(value, ctx);

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

// Sequence event builders
static kk_std_core_types__maybe kk_yaml_yamlc_make_sequence_start_event(
    kk_std_core_types__maybe anchor, kk_std_core_types__maybe tag, bool implicit, int32_t style,
    kk_context_t *ctx) {
  yaml_char_t *canchor = NULL;
  if(kk_std_core_types__is_Just(anchor, ctx)) {
    kk_string_t anchor_str = kk_string_unbox(anchor._cons.Just.value);
    kk_ssize_t anchor_len;
    const char *anchor_buf = kk_string_cbuf_borrow(anchor_str, &anchor_len, ctx);
    canchor = kk_malloc(anchor_len + 1, ctx);
    memcpy(canchor, anchor_buf, anchor_len);
    canchor[anchor_len] = '\0';
    // kk_string_drop(anchor_str, ctx);
  }

  yaml_char_t *ctag = NULL;
  if(kk_std_core_types__is_Just(tag, ctx)) {
    kk_string_t tag_str = kk_string_unbox(tag._cons.Just.value);
    kk_ssize_t tag_len;
    const char *tag_buf = kk_string_cbuf_borrow(tag_str, &tag_len, ctx);
    ctag = kk_malloc(tag_len + 1, ctx);
    memcpy(ctag, tag_buf, tag_len);
    ctag[tag_len] = '\0';
    // kk_string_drop(tag_str, ctx);
  }

  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_sequence_start_event_initialize(ev, canchor, ctag, (int) implicit,
                                           (yaml_sequence_style_t) style)) {
    if(canchor) kk_free(canchor, ctx);
    if(ctag) kk_free(ctag, ctx);
    kk_std_core_types__maybe_drop(anchor, ctx);
    kk_std_core_types__maybe_drop(tag, ctx);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  kk_std_core_types__maybe_drop(anchor, ctx);
  kk_std_core_types__maybe_drop(tag, ctx);

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

KK_YAML_EVENT_SIMPLE_BUILDER(sequence_end, yaml_sequence_end_event_initialize);

// Mapping event builders
static kk_std_core_types__maybe kk_yaml_yamlc_make_mapping_start_event(
    kk_std_core_types__maybe anchor, kk_std_core_types__maybe tag, bool implicit, int32_t style,
    kk_context_t *ctx) {
  yaml_char_t *canchor = NULL;
  if(kk_std_core_types__is_Just(anchor, ctx)) {
    kk_string_t anchor_str = kk_string_unbox(anchor._cons.Just.value);
    kk_ssize_t anchor_len;
    const char *anchor_buf = kk_string_cbuf_borrow(anchor_str, &anchor_len, ctx);
    canchor = kk_malloc(anchor_len + 1, ctx);
    memcpy(canchor, anchor_buf, anchor_len);
    canchor[anchor_len] = '\0';
    // kk_string_drop(anchor_str, ctx);
  }

  yaml_char_t *ctag = NULL;
  if(kk_std_core_types__is_Just(tag, ctx)) {
    kk_string_t tag_str = kk_string_unbox(tag._cons.Just.value);
    kk_ssize_t tag_len;
    const char *tag_buf = kk_string_cbuf_borrow(tag_str, &tag_len, ctx);
    ctag = kk_malloc(tag_len + 1, ctx);
    memcpy(ctag, tag_buf, tag_len);
    ctag[tag_len] = '\0';
    // kk_string_drop(tag_str, ctx);
  }

  yaml_event_t *ev = kk_malloc(sizeof(yaml_event_t), ctx);
  if(!yaml_mapping_start_event_initialize(ev, canchor, ctag, (int) implicit,
                                          (yaml_mapping_style_t) style)) {
    if(canchor) kk_free(canchor, ctx);
    if(ctag) kk_free(ctag, ctx);
    kk_std_core_types__maybe_drop(anchor, ctx);
    kk_std_core_types__maybe_drop(tag, ctx);
    kk_free(ev, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }

  kk_std_core_types__maybe_drop(anchor, ctx);
  kk_std_core_types__maybe_drop(tag, ctx);

  return kk_std_core_types__new_Just(kk_cptr_raw_box(&kk_yaml_emitter_event_free, ev, ctx), ctx);
}

KK_YAML_EVENT_SIMPLE_BUILDER(mapping_end, yaml_mapping_end_event_initialize);
