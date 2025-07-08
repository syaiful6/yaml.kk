#include <kklib.h>
#include <yaml.h>
#include <stdlib.h>
#include <string.h>


// Macro for one-argument event constructors (e.g., stream_end)
#define KK_YAML_EVENT_SIMPLE_BUILDER(NAME, FUNC) \
  static kk_box_t kk_yaml_make_##NAME##_event(kk_context_t* ctx) { \
    yaml_event_t* ev = kk_malloc(sizeof(yaml_event_t), ctx); \
    if (!FUNC(ev)) { kk_free(ev, ctx); return kk_box_null(); } \
    return kk_cptr_raw_box(&kk_yaml_event_free, ev, ctx); \
  }

// Macro for custom-arg event constructors
#define KK_YAML_EVENT_BUILDER(NAME, FUNC, ARGS, CALL_ARGS) \
  static kk_box_t kk_yaml_make_##NAME##_event ARGS { \
    yaml_event_t* ev = kk_malloc(sizeof(yaml_event_t), ctx); \
    if (!FUNC CALL_ARGS) { kk_free(ev, ctx); return kk_box_null(); } \
    return kk_cptr_raw_box(&kk_yaml_event_free, ev, ctx); \
  }

// Event builders (add more as needed):

KK_YAML_EVENT_BUILDER(
  stream_start, yaml_stream_start_event_initialize,
  (yaml_encoding_t encoding, kk_context_t* ctx),
  (ev, encoding)
)

KK_YAML_EVENT_SIMPLE_BUILDER(stream_end, yaml_stream_end_event_initialize)

KK_YAML_EVENT_BUILDER(
  document_start, yaml_document_start_event_initialize,
  (yaml_version_directive_t* version, yaml_tag_directive_t* tag_directives, int implicit, kk_context_t* ctx),
  (ev, version, tag_directives, implicit)
)

KK_YAML_EVENT_BUILDER(
  document_end, yaml_document_end_event_initialize,
  (int implicit, kk_context_t* ctx),
  (ev, implicit)
)

KK_YAML_EVENT_BUILDER(
  alias, yaml_alias_event_initialize,
  (yaml_char_t* anchor, kk_context_t* ctx),
  (ev, anchor)
)

KK_YAML_EVENT_BUILDER(
  scalar, yaml_scalar_event_initialize,
  (yaml_char_t* anchor, yaml_char_t* tag, yaml_char_t* value, int length, int plain_implicit, int quoted_implicit, yaml_scalar_style_t style, kk_context_t* ctx),
  (ev, anchor, tag, value, length, plain_implicit, quoted_implicit, style)
)

KK_YAML_EVENT_BUILDER(
  sequence_start, yaml_sequence_start_event_initialize,
  (yaml_char_t* anchor, yaml_char_t* tag, int implicit, yaml_sequence_style_t style, kk_context_t* ctx),
  (ev, anchor, tag, implicit, style)
)

KK_YAML_EVENT_SIMPLE_BUILDER(sequence_end, yaml_sequence_end_event_initialize)

KK_YAML_EVENT_BUILDER(
  mapping_start, yaml_mapping_start_event_initialize,
  (yaml_char_t* anchor, yaml_char_t* tag, int implicit, yaml_mapping_style_t style, kk_context_t* ctx),
  (ev, anchor, tag, implicit, style)
)

KK_YAML_EVENT_SIMPLE_BUILDER(mapping_end, yaml_mapping_end_event_initialize)

// Helper to emit and drop event (optional)
static kk_unit_t kk_yaml_emit_event(kk_box_t bemitter, kk_box_t bevent, kk_context_t* ctx) {
  yaml_emitter_t* emitter = (yaml_emitter_t*)kk_cptr_unbox_borrowed(bemitter, ctx);
  yaml_event_t* event = (yaml_event_t*)kk_cptr_unbox_borrowed(bevent, ctx);

  int ok = yaml_emitter_emit(emitter, event);
  // event will be freed by GC (via kk_yaml_event_free)
  kk_box_drop(bevent, ctx);
  kk_box_drop(bemitter, ctx);
  if (!ok) {
    // Optionally print error
    // fprintf(stderr, "yaml_emitter_emit failed\n");
  }
  return kk_Unit;
}