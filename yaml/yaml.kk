module yaml/yaml

import yaml/yamlc

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
  YamlSequence(sequence: list<yaml-value>)
  YamlMapping(mapping: list<(string, yaml-value)>)

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


pub fun yaml-value/show(value: yaml-value): <div> string
  match value
    Scalar(anchor, tag, value, style, plain-implicit, quoted-implicit) ->
      "Scalar(" ++ [show(value), show(tag), show(style), show(anchor), show(plain-implicit), show(quoted-implicit)].join(", ") ++ ")"
    Alias(name) -> "Alias(" ++ show(name) ++ ")"
    Sequence(tag, anchor, members, implicit) ->
      "Sequence(" ++ [show(tag), show(anchor), show(members), show(implicit)].join(", ") ++ ")" 
    Mapping(tag, anchor, members, implicit) ->
      "Mapping(" ++ [show(tag), show(anchor), show(members), show(implicit)].join(", ") ++ ")" 


fun anchors/lookup(anchors: anchor-map, name: string): maybe<yaml-value>
  val item = anchors.find fn ((key, _))
    key == name
  match item
    Just((_, value)) -> Just(value)
    Nothing -> Nothing 


fun anchors/insert(anchors: anchor-map, name: string, value: yaml-value): anchor-map
  Cons((name, value), anchors)


pub fun parse-yaml-stream(events: list<marked-event>): <div> either<string, list<yaml-value>>
  match events
    Nil -> Left("Empty events")
    Cons(Marked-event(EventStreamStart, _, _), rest) -> parse-documents-loop(rest, Nil)
    _ -> Left("Expected StreamStart as the first event")

/**
* We implement a basic parser, which is load all event into memory
*/
fun parse-documents-loop(events: list<marked-event>, acc: list<yaml-value>): <div> either<string, list<yaml-value>>
  match events
    Nil -> Left("parse error: Unexpected end of stream, expected StreamEnd")
    Cons(Marked-event(EventStreamEnd, _, _)) -> Right(acc.reverse)
    _ ->
      match events.parse-yaml-document
        Right((doc-value, remaining-events)) -> parse-documents-loop(remaining-events, Cons(doc-value, acc))
        Left(e) -> Left(e)

fun parse-yaml-document(events: list<marked-event>): <div> either<string, (yaml-value, list<marked-event>)>
  match events
    Cons(Marked-event(EventDocumentStart, _, _), rest) ->
      match rest.parse-content-node(Nil, Nil)
        Right((root-node, events-after-content)) ->
          match events-after-content
            Cons(Marked-event(EventDocumentEnd, _, _), remaining-after-doc-end) -> Right((root-node, remaining-after-doc-end))
            _ -> Left("Expected EventDocumentEnd after document content.")
        Left(e) -> Left(e)
    Cons(Marked-event(ev, _, _), _)-> Left("Expecting EventDocumentStart, but got " ++ show(ev))
    _ -> Left("Expecting events, but got empty")

fun parse-content-node(events: list<marked-event>, stack: list<parse-node>, anchors: anchor-map): <div> either<string, (yaml-value, list<marked-event>)>
  match events
    Nil -> Left("Unexpected end of content stream")
    Cons(Marked-event(event, start-mark, _), rest) ->
      match event
        EventScalar(value, tag, style, anchor, plain-implicit, quoted-implicit) ->
          val scalar-node = Scalar(anchor, tag, value, style, plain-implicit, quoted-implicit)
          val updated-anchors = match anchor
            Just(name) -> anchors.insert(name, scalar-node)
            Nothing -> anchors
          scalar-node.add-node-to-parent(stack, updated-anchors, rest, start-mark)
        EventAlias(name) ->
          match anchors.lookup(name)
            Just(_) ->
              val alias-node = Alias(name)
              alias-node.add-node-to-parent(stack, anchors, rest, start-mark)
            Nothing -> Left("Undefined alias " ++ show(name) ++ "at " ++ show(start-mark))
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
                then Right((completed-mapping, rest))
                else completed-mapping.add-node-to-parent(parent-stack, updated-anchors, rest, start-mark)
            _ -> Left("Mismatched MappingEnd event at " ++ show(start-mark))
        EventSequenceEnd ->
          match stack
            Cons(SequenceBuilder(tag, anchor, implicit, members), parent-stack) ->
              val completed-sequence = Sequence(tag, anchor, members.reverse, implicit)
              val updated-anchors = match anchor
                Just(name) -> anchors.insert(name, completed-sequence)
                Nothing -> anchors
              if parent-stack.is-nil
                then Right((completed-sequence, rest))
                else completed-sequence.add-node-to-parent(parent-stack, updated-anchors, rest, start-mark)
            _ -> Left("Mismatched SequenceEnd event at " ++ show(start-mark))
        _ -> Left("Unexpected event type " ++ show(event) ++ "in content stream at " ++ show(start-mark))


fun add-node-to-parent(node: yaml-value, stack: list<parse-node>, anchors: anchor-map, rest: list<marked-event>, mark: yaml-mark): <div> either<string, (yaml-value, list<marked-event>)>
  match stack
    Nil -> Left("Unexpected node outside of a container at " ++ show(mark))
    Cons(MappingBuilder(tag, anchor, implicit, ExpectingKey, members), ps) ->
      val new-mapping-builder = MappingBuilder(tag, anchor, implicit, ExpectingValue(node), members)
      rest.parse-content-node(Cons(new-mapping-builder, ps), anchors)
    Cons(MappingBuilder(tag, anchor, implicit, ExpectingValue(key), members), ps) ->
      val new-mapping-builder = MappingBuilder(tag, anchor, implicit, ExpectingKey, Cons((key, node), members))
      rest.parse-content-node(Cons(new-mapping-builder, ps), anchors)
    Cons(SequenceBuilder(tag, anchor, implicit, members), ps) ->
      val new-sequence-builder = SequenceBuilder(tag, anchor, implicit, Cons(node, members))
      rest.parse-content-node(Cons(new-sequence-builder, ps), anchors)
