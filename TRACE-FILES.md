# Trace Files

The Amalgam runtime uses a simple line-oriented format to record and replay API operations.  If you run the Amalgam CLI tool like `amalgam-st --trace`, you can type these commands directly into the tool to run top-level Amalgam API operations without necessarily writing a C program.

The [Amalgam Python client library](https://github.com/howsoai/amalgam-lang-py) can generate trace files if an option `trace=True` is passed to the `Amalgam()` constructor.  The Amalgam CLI tool can consume these trace files via the `amalgam-st --tracefile [file]` option.  Trace files can be hand-written if needed.

## Syntax

Blank lines and lines that begin with a comment marker, `#` followed by at least one space, are ignored.  Lines with unknown commands or syntactic errors are ignored, and do not stop execution.

Command lines contain a command name followed by some number of space-separated arguments.  An argument may appear in double quotes; if it does, the argument continues through the next matching double-quote.  If a double-quote is needed in an argument value, it may be preceded by a backslash.  No other escaping is possible or allowed, and backslashes that are not followed by double quotes are literally read as backslashes.  If ASCII double-quote bytes 0x22 are escaped by preceding them with a backslash byte 0x5C then an item can otherwise contain arbitrary binary data.

Some commands end with a JSON object.  This is passed directly as a string, and is not quoted or escaped in any way.  Other commands can take a JSON object as a parameter, and this must be quoted and escaped as described above.  Compare:

```none
LOAD_ENTITY "handle" "path" "" false "{\"executeOnLoad\": true}"
SET_ENTITY_PERMISSIONS "handle" {"load": true, "store": true}
```

Several of the entity-related operations take a `persistent` parameter.  This is a Boolean value, true if the string is exactly `true` and false otherwise.  This is conventionally not quoted but double quotes will be recognized and removed.

## Commands

The commands mirror the C API in `Amalgam.h`.  Commands are as follows:

```none
LOAD_ENTITY "handle" "path" "file_type" persistent "json_payload" "transaction_listener_path" "print_listener_path" "rand_seed" "entity_path"
```

Load an entity from a file on disk.  `handle` and `path` are required, all other parameters are optional.  If `entity_path` is provided then it contains a space-separated ordered list of entity IDs, and the entity is loaded into an interior entity underneath an existing `handle`.

```none
LOAD_ENTITY_FROM_MEMORY "handle" "base64_data" "file_type" "persistent" "json_payload" "transaction_listener_path" "print_listener_path" "rand_seed" "entity_path"
```

Load an entity from a base64-encoded data string.  `handle`, `base64_data`, and `file_type` are all required.  Other parameters are handled as `LOAD_ENTITY`.

```none
GET_ENTITY_PERMISSIONS "handle"
```

Get the current permissions of the specified entity.  `handle` is required.

```none
SET_ENTITY_PERMISSIONS "handle" json
```

Set the permissions of an entity.  `handle` is required, and is followed by an unquoted JSON string with the actual permissions.

```none
CLONE_ENTITY "from_handle" "to_handle" "file_type" persistent "json_payload" "transaction_listener_path" "print_listener_path"
```

Make a copy of an existing entity.  The two handles are required.

```none
STORE_ENTITY "handle" "path" "file_type" persistent "json_payload" "entity_path"
```

Store an entity to a file on disk.  `handle` and `path` are required, other options default to the options provided when the entity was created.

```none
STORE_ENTITY_TO_MEMORY "handle" "file_type" persistent "json_payload" "entity_path"
```

Store an entity to memory.  `handle` is required, and the output of the trace operation is the base64-encoded entity data.

```none
DESTROY_ENTITY "handle"
```

Destroy an entity.  `handle` is required.

```none
SET_JSON_TO_LABEL "handle" "label" json
```

Set the value at some label within an existing entity.  `handle` and `label` are required, and are followed by an unquoted JSON string with the content to set.

```none
GET_JSON_FROM_LABEL "handle" "label"
```

Get the value at some label within an existing entity.  `handle` and `label` are required, and the output of the trace operation is the JSON-encoded value.

```none
EXECUTE_ENTITY_JSON "handle" "label" json
```

Execute some label within an existing entity, passing a JSON object as context.  `handle` and `label` are required, and are followed by an unquoted JSON string with values to set in the execution context.  If the JSON string is empty or absent, it is treated as an empty object `{}`.  The executing Amalgam code will see the keys and values of the JSON object as ordinary variables.  The output of the trace operation is the JSON-encoded result of executing the label.

```none
EXECUTE_ENTITY_JSON_LOGGED "handle" "label" json
```

As `EXECUTE_ENTITY_JSON`, but also writes out an Amalgam-syntax description of how the execution modified the entity.

```none
EVAL_ON_ENTITY "handle" "amlg"
```

Execute arbitrary Amalgam code in the context of an existing entity.  `handle` and `amlg` are required, and `amlg` must be correctly quoted.  The output of the trace operation is the JSON-encoded result of evaluating the expression.

```none
SET_RANDOM_SEED "handle" content
```

Sets the random seed of an existing entity to arbitrary binary data.  `handle` is required, and `content` are any bytes remaining after any whitespace through the end of the line.

```none
VERSION
```

The output of the trace operation is the current Amalgam library version.  Takes no parameters.

```none
VERIFY_ENTITY "path"
```

Check that a filesystem path can be loaded as an entity.  `path` is required.

```none
GET_MAX_NUM_THREADS
```

The output of the trace operation is the number of concurrent threads the Amalgam interpreter can use.  Error on non-multithreaded Amalgam runtimes such as `amalgam-st`.

```none
SET_MAX_NUM_THREADS n
```

Set the maximum number of threads in a multithreaded Amalgam runtime.  The parameter is an unquoted number with the number of threads, and must be greater than zero.  Error on non-multithreaded Amalgam runtimes such as `amalgam-st`.

```none
EXIT
```

End the trace file.
