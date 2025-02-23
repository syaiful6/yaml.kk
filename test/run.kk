import std/os/file
import std/os/path
import yaml/yamlc

fun test-parse-file(): <io> list<marked-event>
  val file = cwd() / "test/example.yaml"
  with collect-stream-event
  decode-file(file)

fun test-parse-string(): <io> list<marked-event>
  val file = cwd() / "test/example.yaml"
  val yaml = read-text-file(file)
  with collect-stream-event
  decode-string(yaml)

fun main()
  val events = test-parse-file()
  val evs = events.map(fn(c)
    c.event
  )

  evs.show.println;

  val events2 = test-parse-string()
  val evs2 = events2.map fn(c)
    c.event

  evs2.show.println
