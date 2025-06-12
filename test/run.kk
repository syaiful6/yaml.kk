import std/os/file
import std/os/path
import yaml/yamlc
import yaml/yaml

fun test-parse-file(name: string): <io> list<marked-event>
  val file = cwd() / name
  with collect-stream-event
  decode-file(file)

fun test-parse-string(): <io> list<marked-event>
  val file = cwd() / "test/example.yaml"
  val yaml = read-text-file(file)
  with collect-stream-event
  decode-string(yaml)

fun test-parse-as-yaml(name: string): <io> either<string, list<yaml-value>>
  val streams = test-parse-file(name)
  streams.parse-yaml-stream

fun main()
  val events = test-parse-file("test/example.yaml")
  val evs = events.map(fn(c)
    c.event
  )

  evs.show.println;

  val events2 = test-parse-string()
  val evs2 = events2.map fn(c)
    c.event

  evs2.show.println;

  with name <- ["test/example.yaml", "test/strings.yaml", "test/mapping.yaml"].foreach
  val parsed = name.test-parse-as-yaml
  parsed.show.println
