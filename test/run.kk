import std/os/file
import std/os/path
import yaml/yamlc

fun test-parse-string(): <io> list<marked-event>
  val file = cwd() / "test/example.yaml"
  with collect-stream-event
  decode-file(file)

fun main()
  val events = test-parse-string()
  val evs = events.map(fn(c)
    c.event
  )
  evs.show
