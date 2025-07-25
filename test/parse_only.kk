import std/os/file
import std/os/path
import yaml/yamlc

fun parse-file(name: string): <io> list<marked-event>
  val file = cwd() / name
  with collect-stream-event
  decode-file(file)

fun main()
  val marked-events = "example.yaml".parse-file
  assert("correctly collect stream", list/length(marked-events) > 0)
  "Parse successful!".println