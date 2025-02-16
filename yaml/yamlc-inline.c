#include <yaml.h>

static void kk_yaml_parser_free(void *p, kk_block_t *b, kk_context_t *ctx) {
  kk_unused(ctx);
  yaml_parser_t *parser = (yaml_parser_t *)p;
  kk_info_message("free yaml parser at %p\n", parser);
  if (parser != NULL) {
    yaml_parser_delete(parser);
  }
}

static kk_box_t kk_yaml_string_parser_create(kk_string_t source,
                                             kk_context_t *ctx) {
  kk_ssize_t len;
  const u_int8_t *csource = kk_string_buf_borrow(source, &len, ctx);
  // initialize parser
  yaml_parser_t parser;
  // check the result?
  yaml_parser_initialize(&parser);
  yaml_parser_set_input_string(&parser, csource, len);

  kk_info_message("create yaml parser at %p\n", &parser);
  kk_string_drop(source, ctx);
  return kk_cptr_raw_box(&kk_yaml_parser_free, &parser, ctx);
}
