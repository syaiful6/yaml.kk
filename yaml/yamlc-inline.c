#include <stdio.h>
#include <yaml.h>

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

static kk_unit_t kk_yaml_c_set_input_string(kk_box_t bparser, kk_string_t yaml, kk_context_t *ctx) {
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

static kk_integer_t kk_yaml_yamlc_get_event_type(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  int event_type = event->type;
  // kk_info_message("Got event type %d\n", event_type);
  kk_box_drop(bevent, ctx);
  return kk_integer_from_int(event_type, ctx);
}

inline kk_yaml_yamlc__yaml_mark yamlc_convert_libyaml_mark(yaml_mark_t *mark,
                                                           kk_context_t *ctx) {
  return kk_yaml_yamlc__new_Yaml_mark(kk_integer_from_size_t(mark->index, ctx),
                                      kk_integer_from_size_t(mark->line, ctx),
                                      kk_integer_from_size_t(mark->column, ctx),
                                      ctx);
}

static kk_yaml_yamlc__yaml_mark kk_yaml_yamlc_get_start_mark(kk_box_t bevent,
                                                      kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->start_mark, ctx);
}

static kk_yaml_yamlc__yaml_mark kk_yaml_yamlc_get_end_mark(kk_box_t bevent,
                                                    kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  kk_box_drop(bevent, ctx);
  return yamlc_convert_libyaml_mark(&event->end_mark, ctx);
}

static kk_string_t kk_yaml_yamlc_get_alias_anchor(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  const char* anchor = (const char *)event->data.alias.anchor;
  if (!anchor) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(anchor, ctx);

  kk_box_drop(bevent, ctx);

  return str;
}

static kk_string_t kk_yaml_yamlc_get_scalar_value(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  const char *value = (const char *)event->data.scalar.value;
  int len = event->data.scalar.length;

  kk_string_t str = kk_string_alloc_from_utf8n(len, value, ctx);

  kk_box_drop(bevent, ctx);

  return str;
}

static kk_integer_t kk_yaml_yamlc_get_scalar_style(kk_box_t bevent,
                                            kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  int style = event->data.scalar.style;

  // kk_info_message("Got scalar style %d\n", style);

  kk_box_drop(bevent, ctx);

  return kk_integer_from_int(style, ctx);
}

static kk_string_t kk_yaml_yamlc_get_scalar_tag(kk_box_t bevent, kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *tag = (const char *)event->data.scalar.tag;
  if (!tag) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(tag, ctx);

  kk_box_drop(bevent, ctx);
  return str;
}

static kk_string_t kk_yaml_yamlc_get_scalar_anchor(kk_box_t bevent,
                                            kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *anchor = (const char *)event->data.scalar.anchor;
  if (!anchor) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(anchor, ctx);

  kk_box_drop(bevent, ctx);

  return str;
}

static kk_string_t kk_yaml_yamlc_get_sequence_start_anchor(kk_box_t bevent,
                                                    kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *anchor = (const char *)event->data.sequence_start.anchor;

  if (!anchor) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(anchor, ctx);

  kk_box_drop(bevent, ctx);

  return str;
}

static kk_integer_t kk_yaml_yamlc_get_sequence_start_style(kk_box_t bevent,
                                                    kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);

  int style = event->data.sequence_start.style;

  kk_box_drop(bevent, ctx);

  return kk_integer_from_int(style, ctx);
}

static kk_string_t kk_yaml_yamlc_get_sequence_start_tag(kk_box_t bevent,
                                                 kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *tag = (const char *)event->data.sequence_start.tag;
  if (!tag) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(tag, ctx);

  kk_box_drop(bevent, ctx);
  return str;
}

static kk_string_t kk_yaml_yamlc_get_mapping_start_anchor(kk_box_t bevent,
                                                   kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *anchor = (const char *)event->data.mapping_start.anchor;
  if (!anchor) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(anchor, ctx);

  kk_box_drop(bevent, ctx);
  return str;
}

static kk_integer_t kk_yaml_yamlc_get_mapping_start_style(kk_box_t bevent,
                                                   kk_context_t *ctx) {
  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  int style = event->data.mapping_start.style;
  kk_box_drop(bevent, ctx);

  return kk_integer_from_int(style, ctx);
}

static kk_string_t kk_yaml_yamlc_get_mapping_start_tag(kk_box_t bevent,
                                                kk_context_t *ctx) {

  yaml_event_t *event = (yaml_event_t *)kk_cptr_raw_unbox_borrowed(bevent, ctx);
  const char *tag = (const char *)event->data.mapping_start.tag;

  if (!tag) {
    kk_box_drop(bevent, ctx);
    return kk_string_empty();
  }

  kk_string_t str = kk_string_alloc_from_qutf8(tag, ctx);

  kk_box_drop(bevent, ctx);
  return str;
}
