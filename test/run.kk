import std/os/file
import std/os/path
import yaml/yaml


fun map-eff-event(action: () -> <yaml-stream<marked-event>,yaml-stream<event>|e> ()): <yaml-stream<event>|e> ()
  with fun yield(ev: marked-event) yield(ev.event)
  action()

fun parse-file(name: string): <io> list<marked-event>
  val file = cwd() / name
  with collect-stream-event
  decode-file(file)

fun parse-string(): <io> list<marked-event>
  val file = cwd() / "test/example.yaml"
  val yaml = read-text-file(file)
  with collect-stream-event
  decode-string(yaml)

fun parse-as-yaml(name: string): <io> list<yaml-document>
  val streams = parse-file(name)
  streams.parse-yaml-stream-exn

fun test-collect-stream()
  val marked-events = "test/example.yaml".parse-file
  assert("correctly collect stream", list/length(marked-events) > 0)
  val events = marked-events.map fn(c)
    c.event
  events.show.println

fun parse-file-events(name: string): <io,yaml-stream<event>> ()
  val file = cwd() / name
  with map-eff-event
  decode-file(file)

fun emit-to-string(name: string): <io> string
  with encode-string
  name.parse-file-events

fun main()
  ["test/example.yaml", "test/directive.yaml"].foreach(fn(name)
    val events = parse-file(name)
    val evs = events.map(fn(c)
      c.event
    )

    evs.show.println;
  )

  val events2 = parse-string()
  val evs2 = events2.map fn(c)
    c.event

  evs2.show.println;

  with name <- ["test/example.yaml", "test/strings.yaml", "test/mapping.yaml", "test/directive.yaml"].foreach
  val parsed = name.parse-as-yaml
  parsed.show.println()
  "Json representation".println()
  with yaml <- parsed.foreach
  yaml.to-json.show.println()

  val yaml-str = name.emit-to-string
  ("file name, encoded-decoded is: \n" ++ yaml-str).println
  // yaml-str.println
