module yaml/yamlc

import std/os/path
import std/num/int32

extern import
  c { vcpkg="libyaml"; library="yaml" }

extern import
  c file "yamlc-inline.c"


pub alias anchor-name = string

pub alias anchor = maybe<anchor-name>


pub type yaml-encoding
  AnyEncoding
  Utf8Encoding
  Utf16LeEncoding
  Utf16BeEncoding


pub fun yaml-encoding/show(enc: yaml-encoding): string
  match enc
    AnyEncoding -> "any"
    Utf8Encoding -> "utf8"
    Utf16LeEncoding -> "utf16le"
    Utf16BeEncoding -> "utf16be"


pub fun yaml-encoding/from-int(i: int): maybe<yaml-encoding>
  match i
    0 -> Just(AnyEncoding)
    1 -> Just(Utf8Encoding)
    2 -> Just(Utf16LeEncoding)
    3 -> Just(Utf16BeEncoding)
    _ -> Nothing


pub value type yaml-version
  YamlVersion(major: int, minor: int)

pub fun yaml-version/show(version: yaml-version): string
  version.major.show() ++ "." ++ version.minor.show()

// yaml-tag-directive constains yaml tag directive handler and prefix
pub value type yaml-tag-directive
  YamlTagDirective(hd: string, pfx: string)


pub fun yaml-tag-directive/show(directive: yaml-tag-directive): string
  "YamlTagDirective(hd = " ++ show(directive.hd) ++ ", pfx = " ++ show(directive.pfx) ++ ")"

// Style for scalars - e.g quoted / folded
pub type style
  Any
  Plain
  SingleQuoted
  DoubleQuoted
  Literal
  Folded
  PlainNoTag


// Style for sequences - e.g. block or flow
pub type sequence-style
  AnySequence
  BlockSequence
  FlowSequence


// Style for mapping - e.g block or flow
pub type mapping-style
  AnyMapping
  BlockMapping
  FlowMapping


pub value type tag
  StrTag
  FloatTag
  NullTag
  BoolTag
  SetTag
  IntTag
  SeqTag
  MapTag
  UriTag(uri:string)
  NoTag


pub type event
  EventStreamStart(encoding: yaml-encoding)
  EventStreamEnd
  EventDocumentStart(version: maybe<yaml-version>, tag-directive: list<yaml-tag-directive>, implicit: bool)
  EventDocumentEnd(implicit: bool)
  EventAlias(name: anchor-name)
  EventScalar(value: string, tag: tag, style: style, anchor: anchor, plain-implicit: bool, quoted-implicit: bool)
  EventSequenceStart(tag: tag, style: sequence-style, anchor: anchor, implicit: bool)
  EventSequenceEnd
  EventMappingStart(tag: tag, style: mapping-style, anchor: anchor, implicit: bool)
  EventMappingEnd


// The pointer position
pub value struct yaml-mark
  index: int
  line: int
  column: int


pub fun yaml-mark/show(mark: yaml-mark): string
  "index = " ++ show(mark.index) ++ " line = " ++ show(mark.line) ++ "column = " ++ show(mark.column)

// Event with start and end marks
pub struct marked-event
  event: event
  start: yaml-mark
  end: yaml-mark

fun tag-supressed(tag: tag) : bool
  match tag
    NoTag -> True
    UriTag("") -> True
    _ -> False

fun tag-to-string(tag: tag) : string
  match tag
    StrTag -> "tag:yaml.org,2002:str"
    FloatTag -> "tag:yaml.org,2002:float"
    NullTag -> "tag:yaml.org,2002:null"
    BoolTag -> "tag:yaml.org,2002:bool"
    SetTag -> "tag:yaml.org,2002:set"
    IntTag -> "tag:yaml.org,2002:int"
    SeqTag -> "tag:yaml.org,2002:seq"
    MapTag -> "tag:yaml.org,2002:map"
    UriTag(uri) -> uri
    NoTag -> ""

fun string-to-tag(tag: string) : tag
  match tag
    "tag:yaml.org,2002:str" -> StrTag
    "tag:yaml.org,2002:float" -> FloatTag
    "tag:yaml.org,2002:null" -> NullTag
    "tag:yaml.org,2002:bool" -> BoolTag
    "tag:yaml.org,2002:set" -> SeqTag
    "tag:yaml.org,2002:map" -> MapTag
    "" -> NoTag
    s   -> UriTag(s)

pub fun tag/show(t: tag): string
  t.tag-to-string

fun style/from-int(v: int) : maybe<style>
  match v
    0 -> Just(Any)
    1 -> Just(Plain)
    2 -> Just(SingleQuoted)
    3 -> Just(DoubleQuoted)
    4 -> Just(Literal)
    5 -> Just(Folded)
    6 -> Just(PlainNoTag)
    _ -> Nothing

fun sequence-style/from-int(v: int): maybe<sequence-style>
  match v
    0 -> Just(AnySequence)
    1 -> Just(BlockSequence)
    2 -> Just(FlowSequence)
    _ -> Nothing


fun mapping-style/from-int(v: int): maybe<mapping-style>
  match v
    0 -> Just(AnyMapping)
    1 -> Just(BlockMapping)
    2 -> Just(FlowMapping)
    _ -> Nothing


pub fun style/show(s: style): string
  match s
    Any -> "Any"
    Plain -> "Plain"
    SingleQuoted -> "SingleQuoted"
    DoubleQuoted -> "DoubleQuoted"
    Literal -> "Literal"
    Folded -> "Folded"
    PlainNoTag -> "PlainNoTag"


pub fun sequence-style/show(s: sequence-style): string
  match s
    AnySequence -> "AnySequence"
    BlockSequence -> "BlockSequence"
    FlowSequence -> "FlowSequence"


pub fun mapping-style/show(s: mapping-style): string
  match s
    AnyMapping -> "AnyMapping"
    BlockMapping -> "BlockMapping"
    FlowMapping -> "FlowMapping"

pub fun event/show(ev: event): string
  match ev
    EventStreamStart(encoding) -> "EventStreamStart(" ++ show(encoding) ++ ")"
    EventStreamEnd -> "EventStreamEnd"
    EventDocumentStart(version, tag-directive, implicit) -> "EventDocumentStart(" ++ [show(version), show(tag-directive), show(implicit)].join(", ") ++ ")"
    EventDocumentEnd(implicit) -> "EventDocumentEnd(" ++ show(implicit) ++ ")"
    EventAlias(name) -> "EventAlias(" ++ show(name) ++ ")"
    EventScalar(value, tag, style, anchor, plain-implicit, quoted-implicit) -> "EventScalar(" ++ [show(value), show(tag), show(style), show(anchor), show(plain-implicit), show(quoted-implicit)].join(", ") ++ ")"
    EventSequenceStart(tag, style, anchor, implicit) -> "EventSequenceStart(" ++ [show(tag), show(style), show(anchor), show(implicit)].join(", ") ++ ")"
    EventSequenceEnd -> "EventSequenceEnd"
    EventMappingStart(tag, style, anchor, implicit) -> "EventMappingStart(" ++ [show(tag), show(style), show(anchor), show(implicit)].join(", ") ++ ")"
    EventMappingEnd -> "EventMappingEnd"


// The underlying libyaml parser pointer
alias libyaml-parser = any

extern create-yaml-parser(): libyaml-parser
  c "kk_yaml_parser_create"

// The underlying yaml event
alias libyaml-event = any

extern yaml-open-file(path: string): any
  c "kk_yaml_yamlc_open_file"

extern yaml-close-file(file: any): ()
  c "kk_yaml_yamlc_close_file"

extern yaml-set-input-file(parser: libyaml-parser, file: any): ()
  c "kk_yaml_yamlc_set_input_file"

extern yaml-set-input-string(parser: libyaml-parser, yaml: string): ()
  c "kk_yaml_c_set_input_string"

extern yaml-parser-parse-one(parser: libyaml-parser): <div> maybe<libyaml-event>
  c "kk_yaml_parse_one"

extern yaml-get-event-type(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_type"

extern yaml-get-event-stream-start-encoding(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_stream_start_encoding"

extern yaml-get-event-document-start-version-directive(^ev: libyaml-event): maybe<(int, int)>
  c "kk_yaml_yamlc_get_event_document_start_version_directive"

extern kk-yaml-yamlc-get-event-document-start-tag-directives(^ev: libyaml-event): list<(string, string)>
  c "kk_yaml_yamlc_get_event_document_start_tag_directives"

extern kk-yaml-yamlc-get-event-document-start-implicit(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_document_start_implicit"

extern yaml-get-start-mark(^ev: libyaml-event): yaml-mark
  c "kk_yaml_yamlc_get_start_mark"

extern yaml-get-end-mark(^ev: libyaml-event): yaml-mark
  c "kk_yaml_yamlc_get_end_mark"

extern yaml-get-alias-anchor(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_alias_anchor"

extern yaml-get-scalar-value(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_scalar_value"

extern yaml-get-scalar-style(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_scalar_style"

extern yaml-get-scalar-tag(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_scalar_tag"

extern yaml-get-scalar-anchor(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_scalar_anchor"

extern yaml-get-scalar-plain-implicit(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_scalar_plain_implicit"

extern yaml-get-scalar-quoted-implicit(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_scalar_quoted_implicit"

extern yaml-get-sequence-start-anchor(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_sequence_start_anchor"

extern yaml-get-sequence-start-style(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_sequence_start_style"

extern yaml-get-sequence-start-tag(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_sequence_start_tag"

extern yaml-get-sequence-start-implicit(ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_sequence_start_implicit"

extern yaml-get-mapping-start-anchor(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_mapping_start_anchor"

extern yaml-get-mapping-start-style(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_mapping_start_style"

extern yaml-get-mapping-start-tag(^ev: libyaml-event): string
  c "kk_yaml_yamlc_get_event_mapping_start_tag"

extern yaml-get-mapping-start-implicit(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_mapping_start_implicit"

extern yaml-get-event-document-end-implicit(^ev: libyaml-event): int32
  c "kk_yaml_yamlc_get_event_document_end_implicit"

// get event
fun get-event(ev: libyaml-event): maybe<marked-event>
  val et = ev.yaml-get-event-type.int
  val start = yaml-get-start-mark(ev);
  val end = yaml-get-end-mark(ev);

  val event = match et
    0 -> Nothing
    1 ->
      val encoding = match ev.yaml-get-event-stream-start-encoding.int.yaml-encoding/from-int()
        Just(enc) -> enc
        Nothing   -> AnyEncoding
      Just(EventStreamStart(encoding))
    2 -> Just(EventStreamEnd)
    3 ->
      val version = ev.yaml-get-event-document-start-version-directive.map(fn ((major, minor)){ YamlVersion(major, minor) })
      val tag-directive = ev.kk-yaml-yamlc-get-event-document-start-tag-directives.map(fn ((hd, pref)){ YamlTagDirective(hd, pref)})
      Just(EventDocumentStart(version, tag-directive, ev.kk-yaml-yamlc-get-event-document-start-implicit == 1.int32))
    4 -> Just(EventDocumentEnd(ev.yaml-get-event-document-end-implicit == 1.int32))
    5 -> 
      val anchor = ev.yaml-get-alias-anchor
      Just(EventAlias(anchor));
    6 ->
      val value = yaml-get-scalar-value(ev);
      val tag = ev.yaml-get-scalar-tag.string-to-tag
      val anchor = ev.yaml-get-scalar-anchor
      val plain-implicit = ev.yaml-get-scalar-plain-implicit
      val quoted-implicit = ev.yaml-get-scalar-quoted-implicit
      with style <- ev.yaml-get-scalar-style.int.style/from-int.map
      EventScalar(value, tag, style, if anchor != "" then Just(anchor) else Nothing, plain-implicit == 1.int32, quoted-implicit == 1.int32)
    7 ->
      val tag = ev.yaml-get-sequence-start-tag.string-to-tag
      val anchor = ev.yaml-get-sequence-start-anchor
      val implicit = ev.yaml-get-sequence-start-implicit
      with style <- ev.yaml-get-sequence-start-style.int.sequence-style/from-int.map
      EventSequenceStart(tag, style, if anchor != "" then Just(anchor) else Nothing, implicit == 1.int32)
    8 -> Just(EventSequenceEnd)
    9 ->
      val tag = ev.yaml-get-mapping-start-tag.string-to-tag
      val anchor = ev.yaml-get-mapping-start-anchor
      val implicit = ev.yaml-get-mapping-start-implicit
      with style <- ev.yaml-get-mapping-start-style.int.mapping-style/from-int.map
      EventMappingStart(tag, style, if anchor != "" then Just(anchor) else Nothing, implicit == 1.int32)
    10 -> Just(EventMappingEnd)
    _ -> Nothing

  with evn <- event.map
  Marked-event(evn, start, end)

// Effect for libyaml event
pub effect yaml-stream<a>
  fun yield(x: a) : ()

// for someone need pull-based parsing
fun do-parse-one(parser: libyaml-parser): <div> maybe<marked-event>
  val event = parser.yaml-parser-parse-one
  match event
    Nothing -> Nothing
    Just(event) -> event.get-event


fun stream-event(parser: libyaml-parser): <div,yaml-stream<marked-event>> ()
  val x = ref(False)
  while { not(!x) }
    val event = parser.do-parse-one
    match event
      Nothing ->
        x := True
      Just(event) ->
        yield(event)


pub fun decode-file(path: path): <div,yaml-stream<marked-event>> ()
  val parser = create-yaml-parser()
  val file = path.string.yaml-open-file
  with finally { yaml-close-file(file) }
  parser.yaml-set-input-file(file)
  parser.stream-event


pub fun decode-string(yaml: string): <div, yaml-stream<marked-event>> ()
  val parser = create-yaml-parser()
  parser.yaml-set-input-string(yaml)
  parser.stream-event


pub fun print-stream-event(action: () -> <yaml-stream<marked-event>,console|e> a): <console|e> a
  with fun yield(x: marked-event) x.event.println
  action()


pub fun collect-stream-event(action: () -> <yaml-stream<marked-event>|e> ()): e list<marked-event>
  var lines := []
  with handler
    return(x) lines.reverse()
    fun yield(x: marked-event) lines := Cons(x, lines)

  action()
