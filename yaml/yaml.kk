module yaml/yaml

import yaml/yamlc
import std/core/exn
import std/num/float64
import std/core/undiv


pub extend type exception-info
  YamlExnConvert
  YamlExnParser(event: maybe<marked-event>)

/**
* yaml-json is subset of a Yaml document that is compatible with JSON. This
* type should be enough to convert to any json library that Koka will have
* in the future.
*/
pub type yaml-json
  YamlNull
  YamlBool(b: bool)
  YamlDouble(double: float64)
  YamlString(string: string)
  YamlSequence(sequence: list<yaml-json>)
  YamlMapping(mapping: list<(string, yaml-json)>)


pub type yaml-value
  Scalar(anchor: anchor, tag: tag, value: string, style: style, plain-implicit: bool, quoted-implicit: bool)
  Alias(name: anchor-name)
  Sequence(tag: tag, anchor: anchor, members: list<yaml-value>, implicit: bool)
  Mapping(tag: tag, anchor: anchor, members: list<(yaml-value, yaml-value)>, implicit: bool)


value type mapping-state
  ExpectingKey
  ExpectingValue(current-key: yaml-value)


type parse-node
  MappingBuilder(tag: tag, anchor: anchor, implicit: bool, state: mapping-state, members: list<(yaml-value, yaml-value)>)
  SequenceBuilder(tag: tag, anchor: anchor, implicit: bool, members: list<yaml-value>)


alias anchor-map = list<(string, yaml-value)>

// TODO: actually print yaml string
pub fun yaml-value/show(value: yaml-value): <div> string
  match value
    Scalar(anchor, tag, value, style, plain-implicit, quoted-implicit) ->
      "Scalar(" ++ [show(value), show(tag), show(style), show(anchor), show(plain-implicit), show(quoted-implicit)].join(", ") ++ ")"
    Alias(name) -> "Alias(" ++ show(name) ++ ")"
    Sequence(tag, anchor, members, implicit) ->
      "Sequence(" ++ [show(tag), show(anchor), show(members), show(implicit)].join(", ") ++ ")" 
    Mapping(tag, anchor, members, implicit) ->
      "Mapping(" ++ [show(tag), show(anchor), show(members), show(implicit)].join(", ") ++ ")"


fun char/quote(c: char): string
  match c
    '"' -> "\\\""
    '\\' -> "\\\\"
    '\n' -> "\\n"
    '\r' -> "\\r"
    '\t' -> "\\t"
    _ -> c.string


// quote essential and whitespace characters in a JSON string
fun string/quote(s: string): string
  s.list.map(quote).join


pub fun yaml-json/show(value: yaml-json): string
  match value
    YamlNull -> "null"
    YamlBool(b) -> if b then "true" else "false"
    YamlDouble(d) -> d.show()
    YamlString(s) -> "\"" ++ s.quote ++ "\""
    YamlSequence(xs) -> xs.show(?show=fn(s) s.pretend-decreasing.show)
    YamlMapping(mapping) -> "{" ++ mapping.map(fn((k, v)) "\"" ++ k ++ "\":" ++ v.pretend-decreasing.show).join(",") ++ "}"


fun anchors/lookup(anchors: anchor-map, name: string): maybe<yaml-value>
  val item = anchors.find fn ((key, _))
    key == name
  match item
    Just((_, value)) -> Just(value)
    Nothing -> Nothing 


fun anchors/insert(anchors: anchor-map, name: string, value: yaml-value): anchor-map
  Cons((name, value), anchors)


fun yaml-scalar-to-json(t: string): yaml-json
  // lower the string for easier match
  val lower-t = t.to-lower()
  match lower-t
    "null" -> YamlNull
    "" -> YamlNull
    "~" -> YamlNull
    "y" -> YamlBool(True)
    "yes" -> YamlBool(True)
    "true" -> YamlBool(True)
    "on" -> YamlBool(True)
    "n" -> YamlBool(False)
    "no" -> YamlBool(False)
    "false" -> YamlBool(False)
    "off" -> YamlBool(False)
    "-.inf" -> YamlDouble(neginf)
    ".inf" -> YamlDouble(posinf)
    ".nan" -> YamlDouble(nan)
    _ ->
      match t.parse-float64()
        Just(num) -> YamlDouble(num)
        Nothing   -> YamlString(t)


fun simple-key-to-string(key: yaml-value): <exn> string
  match key
    Scalar(_, _, scalar-value) -> scalar-value
    _ -> throw("non-string key is not supported", YamlExnConvert)


pub fun to-json-exn(value: yaml-value): <exn> yaml-json
  match value
    Scalar(_, _, scalar-value, _, _, quoted-implicit) | quoted-implicit -> YamlString(scalar-value)
    Scalar(_, _, scalar-value) -> yaml-scalar-to-json(scalar-value)
    Alias(name) -> throw("Anchors are not supported when serialising to JSON. But got '" ++ show(name) ++ "' alias.", YamlExnConvert)
    Sequence(_, _, members, _) -> YamlSequence(members.map(fn (item) item.pretend-decreasing.to-json-exn))
    Mapping(_, _, members, _) ->
      YamlMapping(members.map fn((key, item)) (key.simple-key-to-string(), item.pretend-decreasing.to-json-exn()))


pub fun to-json(value: yaml-value): <div> either<exception, yaml-json>
  try({ Right(value.to-json-exn())}) fn(exn){ Left(exn) }


pub fun parse-yaml-stream(events: list<marked-event>): <div> either<exception, list<yaml-value>>
  try({ Right(events.parse-yaml-stream-exn()) }) fn(exn){ Left(exn) }


pub fun parse-yaml-stream-exn(events: list<marked-event>): <div,exn> list<yaml-value>
  match events
    Nil -> throw("Empty events", YamlExnParser(Nothing))
    Cons(Marked-event(EventStreamStart, _, _), rest) -> parse-documents-loop(rest, Nil)
    _ -> throw("Expected StreamStart as the first event", YamlExnParser(Nothing))


fun parse-documents-loop(events: list<marked-event>, acc: list<yaml-value>): <div,exn> list<yaml-value>
  match events
    Nil -> throw("parse error: Unexpected end of stream, expected StreamEnd")
    Cons(Marked-event(EventStreamEnd, _, _)) -> acc.reverse
    _ ->
      val (doc-value, remaining-events) = events.parse-yaml-document()
      remaining-events.parse-documents-loop(Cons(doc-value, acc))


fun parse-yaml-document(events: list<marked-event>): <div,exn> (yaml-value, list<marked-event>)
  match events
    Cons(Marked-event(EventDocumentStart, _, _), rest) ->
      val (root-node, events-after-content) = rest.parse-content-node(Nil, Nil)
      match events-after-content
        Cons(Marked-event(EventDocumentEnd, _, _), remaining-after-doc-end) -> (root-node, remaining-after-doc-end)
        Cons(current, _) -> throw("Expecting EventDocumentStart", YamlExnParser(Just(current)))
        Nil -> throw("Expected EventDocumentEnd after document content.", YamlExnParser(Nothing))
    Cons(Marked-event(ev, start, end), _)-> throw("Expecting EventDocumentStart, but got " ++ show(ev), YamlExnParser(Just(Marked-event(ev, start, end))))
    _ -> throw("Expecting events, but got empty", YamlExnParser(Nothing))


fun parse-content-node(events: list<marked-event>, stack: list<parse-node>, anchors: anchor-map): <div,exn> (yaml-value, list<marked-event>)
  match events
    Nil -> throw("Unexpected end of content stream")
    Cons(Marked-event(event, start-mark, end-mark), rest) ->
      match event
        EventScalar(value, tag, style, anchor, plain-implicit, quoted-implicit) ->
          val scalar-node = Scalar(anchor, tag, value, style, plain-implicit, quoted-implicit)
          val updated-anchors = match anchor
            Just(name) -> anchors.insert(name, scalar-node)
            Nothing -> anchors
          scalar-node.add-node-to-parent(stack, updated-anchors, rest, Marked-event(event, start-mark, end-mark))
        EventAlias(name) ->
          match anchors.lookup(name)
            Just(_) ->
              val alias-node = Alias(name)
              alias-node.add-node-to-parent(stack, anchors, rest, Marked-event(event, start-mark, end-mark))
            Nothing -> throw("Undefined alias " ++ show(name), YamlExnParser(Just(Marked-event(event, start-mark, end-mark))))
        EventMappingStart(tag, _, anchor, implicit) ->
          val new-builder = MappingBuilder(tag, anchor, implicit, ExpectingKey, Nil)
          rest.parse-content-node(Cons(new-builder, stack), anchors)
        EventSequenceStart(tag, _, anchor, implicit) ->
          val new-builder = SequenceBuilder(tag, anchor, implicit, Nil)
          rest.parse-content-node(Cons(new-builder, stack), anchors)
        EventMappingEnd ->
          match stack
            Cons(MappingBuilder(tag, anchor, implicit, _, members), parent-stack) ->
              val completed-mapping = Mapping(tag, anchor, members.reverse, implicit)
              val updated-anchors = match anchor
                Just(name) -> anchors.insert(name, completed-mapping)
                Nothing -> anchors
              if parent-stack.is-nil
                then (completed-mapping, rest)
                else completed-mapping.add-node-to-parent(parent-stack, updated-anchors, rest, Marked-event(event, start-mark, end-mark))
            _ -> throw("Mismatched MappingEnd event at " ++ show(start-mark), YamlExnParser(Just(Marked-event(event, start-mark, end-mark))))
        EventSequenceEnd ->
          match stack
            Cons(SequenceBuilder(tag, anchor, implicit, members), parent-stack) ->
              val completed-sequence = Sequence(tag, anchor, members.reverse, implicit)
              val updated-anchors = match anchor
                Just(name) -> anchors.insert(name, completed-sequence)
                Nothing -> anchors
              if parent-stack.is-nil
                then (completed-sequence, rest)
                else completed-sequence.add-node-to-parent(parent-stack, updated-anchors, rest, Marked-event(event, start-mark, end-mark))
            _ -> throw("Mismatched SequenceEnd event at " ++ show(start-mark),YamlExnParser(Just(Marked-event(event, start-mark, end-mark))))
        _ -> throw("Unexpected event type " ++ show(event) ++ "in content stream", YamlExnParser(Just(Marked-event(event, start-mark, end-mark))))


fun add-node-to-parent(node: yaml-value, stack: list<parse-node>, anchors: anchor-map, rest: list<marked-event>, current: marked-event): <div,exn> (yaml-value, list<marked-event>)
  match stack
    Nil -> throw("Unexpected node outside of a container at " ++ show(current.start), YamlExnParser(Just(current)))
    Cons(MappingBuilder(tag, anchor, implicit, ExpectingKey, members), ps) ->
      val new-mapping-builder = MappingBuilder(tag, anchor, implicit, ExpectingValue(node), members)
      rest.parse-content-node(Cons(new-mapping-builder, ps), anchors)
    Cons(MappingBuilder(tag, anchor, implicit, ExpectingValue(key), members), ps) ->
      val new-mapping-builder = MappingBuilder(tag, anchor, implicit, ExpectingKey, Cons((key, node), members))
      rest.parse-content-node(Cons(new-mapping-builder, ps), anchors)
    Cons(SequenceBuilder(tag, anchor, implicit, members), ps) ->
      val new-sequence-builder = SequenceBuilder(tag, anchor, implicit, Cons(node, members))
      rest.parse-content-node(Cons(new-sequence-builder, ps), anchors)
