module yaml/yamlc

extern import
  c { vcpkg="libyaml"; library="yaml" }

extern import
  c file "yamlc-inline.c"


pub alias anchor-name = string

pub alias anchor = maybe<anchor-name>

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
  EventSreamStart
  EventStreamEnd
  EventDocumentStart
  EventDocumentEnd
  EventAlias(name: anchor-name)
  EventScalar(value: string, tag: tag, style: style, anchor: anchor)
  EventSequenceStart(tag: tag, style: sequence-style, anchor: anchor)
  EventSequenceEnd
  EventMappingStart(tag: tag, style: mapping-style, anchor: anchor)
  EventMappingEnd

// The pointer position
pub struct yaml-mark
  index: int
  line: int
  column: int

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

// The underlying libyaml parser pointer
alias yaml-parser = any

extern string-yaml-parser(yaml: string): yaml-parser
  c "kk_yaml_string_parser_create"
