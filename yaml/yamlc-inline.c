#include <stdio.h>
#include <yaml.h>

/**
 * Common pattern for accessing int32 value
 */
#define DEFINE_YAML_EVENT_INT32_GETTER(UNION_MEMBER, FIELD_NAME, FIELD_TYPE, SUFFIX) \
static int32_t kk_yaml_yamlc_get_event_##SUFFIX(kk_box_t bevent, \
                                                kk_context_t *ctx) { \
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx); \
  /* The field's value (enum or int) is safely cast to int32_t */ \
  int32_t value = (int32_t)((FIELD_TYPE)event->data.UNION_MEMBER.FIELD_NAME); \
  return value; \
}

#define DEFINE_YAML_EVENT_STRING_GETTER(UNION_MEMBER, FIELD_NAME, FIELD_TYPE, SUFFIX) \
static kk_string_t kk_yaml_yamlc_get_event_##SUFFIX(kk_box_t bevent, \
                                                    kk_context_t *ctx) { \
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx); \
  /* Access the field, cast to const char* for string functions */ \
  const char *c_str_ptr = (const char *)((FIELD_TYPE)event->data.UNION_MEMBER.FIELD_NAME); \
                                                                            \
  if (!c_str_ptr) { /* Handle NULL pointers from libyaml (e.g., absent optional fields) */ \
    return kk_string_empty(); /* Return an empty Koka string */ \
  } \
  kk_string_t koka_str = kk_string_alloc_from_qutf8(c_str_ptr, ctx); \
  \
  return koka_str; \
}

static void kk_yaml_parser_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  // kk_unused(ctx);
  yaml_parser_t *parser = (yaml_parser_t *)p;
  // kk_info_message("free yaml parser at %p\n", parser);
  if (parser != NULL) {
    yaml_parser_delete(parser);
    kk_free(parser, ctx);
  }
}

static kk_box_t kk_yaml_parser_create(kk_context_t *ctx) {
  // initialize parser
  yaml_parser_t *parser = kk_malloc(sizeof(yaml_parser_t), ctx);
  // check the result?
  if (!yaml_parser_initialize(parser)) {
    kk_info_message("yaml_parser_initialize failed at %p\n", parser);
  }
  // kk_info_message("create yaml parser at %p\n", parser);
  return kk_cptr_raw_box(&kk_yaml_parser_free, parser, ctx);
}

static void kk_yaml_event_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  // kk_unused(ctx);
  yaml_event_t *event = (yaml_event_t *)p;
  // kk_info_message("free yaml event at %p\n", event);
  if (event != NULL) {
    yaml_event_delete(event);
    kk_free(event, ctx);
  }
}

static kk_unit_t kk_yaml_yamlc_set_input_file(kk_box_t bparser, kk_box_t bfile,
                                              kk_context_t *ctx) {
  yaml_parser_t *parser = (yaml_parser_t *)kk_cptr_unbox_borrowed(bparser, ctx);
  FILE *file = (FILE *)kk_cptr_unbox_borrowed(bfile, ctx);
  // kk_info_message("set parser %p input to %p\n", parser, file);
  yaml_parser_set_input_file(parser, file);

  kk_box_drop(bfile, ctx);
  kk_box_drop(bparser, ctx);

  return kk_Unit;
}

static kk_unit_t kk_yaml_c_set_input_string(kk_box_t bparser, kk_string_t yaml,
                                            kk_context_t *ctx) {
  yaml_parser_t *parser = (yaml_parser_t *)kk_cptr_unbox_borrowed(bparser, ctx);
  kk_ssize_t len;
  const u_int8_t *cyaml = kk_string_buf_borrow(yaml, &len, ctx);
  yaml_parser_set_input_string(parser, cyaml, len);

  // kk_string_drop(yaml, ctx);
  kk_box_drop(bparser, ctx);

  return kk_Unit;
}

kk_box_t kk_yaml_yamlc_open_file(kk_string_t path, kk_context_t *ctx) {
  kk_ssize_t len;
  const char *cpat = kk_string_cbuf_borrow(path, &len, ctx);

  FILE *file = fopen(cpat, "rb");
  kk_string_drop(path, ctx);

  return kk_cptr_box(file, ctx);
}

static void kk_yaml_yamlc_close_file(kk_box_t bfile, kk_context_t *ctx) {
  FILE *file = (FILE *)kk_cptr_unbox_borrowed(bfile, ctx);

  fclose(file);

  kk_box_drop(bfile, ctx);
}

static kk_std_core_types__maybe kk_yaml_parse_one(kk_box_t bparser,
                                                  kk_context_t *ctx) {

  yaml_parser_t *parser = (yaml_parser_t *)kk_cptr_unbox_borrowed(bparser, ctx);
  yaml_event_t *event = kk_malloc(sizeof(yaml_event_t), ctx);
  // kk_info_message("kk_yaml_parse_one at %p\n", parser);
  if (!yaml_parser_parse(parser, event)) {
    fprintf(stderr, "Parse error: %s\nLine: %lu Column: %lu\n", parser->problem,
            (unsigned long)parser->problem_mark.line + 1,
            (unsigned long)parser->problem_mark.column + 1);

    yaml_event_delete(event);
    kk_free(event, ctx);
    kk_box_drop(bparser, ctx);
    return kk_std_core_types__new_Nothing(ctx);
  }
  // kk_info_message("kk_yaml_parse_one initialize event at %p\n", event);
  kk_box_drop(bparser, ctx);
  // we got the event, wrap to Just
  return kk_std_core_types__new_Just(
      kk_cptr_raw_box(&kk_yaml_event_free, event, ctx), ctx);
}

static int32_t kk_yaml_yamlc_get_event_type(kk_box_t bevent,
                                                 kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  int32_t event_type = event->type;
  // kk_info_message("Got event type %d\n", event_type);
  // we borrow the event, no need to drop
  // kk_box_drop(bevent, ctx);
  return event_type;
}

inline kk_yaml_yamlc__yaml_mark yamlc_convert_libyaml_mark(yaml_mark_t *mark,
                                                           kk_context_t *ctx) {
  return kk_yaml_yamlc__new_Yaml_mark(kk_integer_from_size_t(mark->index, ctx),
                                      kk_integer_from_size_t(mark->line, ctx),
                                      kk_integer_from_size_t(mark->column, ctx),
                                      ctx);
}

static kk_yaml_yamlc__yaml_mark
kk_yaml_yamlc_get_start_mark(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  // kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->start_mark, ctx);
}

static kk_yaml_yamlc__yaml_mark kk_yaml_yamlc_get_end_mark(kk_box_t bevent,
                                                           kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  // kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->end_mark, ctx);
}

DEFINE_YAML_EVENT_STRING_GETTER(alias, anchor, yaml_char_t*, alias_anchor);

static kk_string_t kk_yaml_yamlc_get_event_scalar_value(kk_box_t bevent,
                                                  kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  const char *value = (const char *)event->data.scalar.value;
  size_t len = event->data.scalar.length;

  kk_string_t str = kk_string_alloc_from_utf8n(len, value, ctx);

  return str;
}

// accessor for scalar field
DEFINE_YAML_EVENT_INT32_GETTER(scalar, style, yaml_scalar_style_t, scalar_style);
DEFINE_YAML_EVENT_INT32_GETTER(scalar, plain_implicit, int, scalar_plain_implicit);
DEFINE_YAML_EVENT_INT32_GETTER(scalar, quoted_implicit, int, scalar_quoted_implicit);
DEFINE_YAML_EVENT_STRING_GETTER(scalar, tag, yaml_char_t*, scalar_tag);
DEFINE_YAML_EVENT_STRING_GETTER(scalar, anchor, yaml_char_t*, scalar_anchor);

// accessor for sequence field
DEFINE_YAML_EVENT_STRING_GETTER(sequence_start, anchor, yaml_char_t*, sequence_start_anchor);
DEFINE_YAML_EVENT_INT32_GETTER(sequence_start, style, yaml_sequence_style_t, sequence_start_style);
DEFINE_YAML_EVENT_STRING_GETTER(sequence_start, tag, yaml_char_t*, sequence_start_tag);
DEFINE_YAML_EVENT_INT32_GETTER(sequence_start, implicit, int, sequence_start_implicit);

// accessor for mapping field
DEFINE_YAML_EVENT_STRING_GETTER(mapping_start, anchor, yaml_char_t*, mapping_start_anchor);
DEFINE_YAML_EVENT_STRING_GETTER(mapping_start, tag, yaml_char_t*, mapping_start_tag);
DEFINE_YAML_EVENT_INT32_GETTER(mapping_start, style, yaml_mapping_style_t, mapping_start_style);
DEFINE_YAML_EVENT_INT32_GETTER(mapping_start, implicit, int, mapping_start_implicit);