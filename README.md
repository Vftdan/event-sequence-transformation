## Decription

(Currently unnamed) event stream transformation tool.

Transformation rules are represented in a graph form through which events flow. Events are strictly ordered (initially by their creation time), and the first event that causes the system to advance is always processed.

## Building

### Dependencies

- gcc, GNU Make, pkg-config
- libevdev2
- libconfig9

```sh
apt-get install gcc make libevdev-dev libconfig-dev
```

### Compiling

```sh
make
```

## Events

Each event has:

- Namespace code: used to distinguish events originating from different nodes
- Major code: generally corresponds to the `type` field of a linux event (see `event_type` enum)
- Minor code: generally corresponds to the `code` field of a linux event (standard values depend on the major code, see `*_event` enums)
- Payload: generally corresponds to the `value` field of a linux event (can e. g. be button press state or pointing device position/speed coordinate)
- Modifiers: (bit-)set of (preferably small) natural numbers that can be used to store additional flags
- TTL: used to catch situations of an event travelling through the graph indefinitely
- Time: stores the original event creation time

## Configuration

Transformation graph file should be written in [libconfig](https://hyperrealm.github.io/libconfig/libconfig_manual.html#Configuration-Files) format. The file has multiple sections: `constants`, `enums`, `predicates`, `nodes`, `channels`.

Example configurations: [config.cfg](config.cfg), [quadtap-both-click.cfg](quadtap-both-click.cfg).

`constants` and `enums` define integer constants that can be referenced in integer fields. `enums` create dot-separated name and automatically increments elements of the same enum. There are builtin constants for `linux/input-event-codes.h` with their original name and also in enum format with their prefix replaced with `input_property.`, `event_type.`, `synchronization_event.`, `key_event.`, `button_event.`, `relative_axis.`, `absolute_axis.`, `switch_event.`, `misc_event.`, `led_event.`, `repeat_event.`, `sound_event.`, `force_feedback_status.`, `force_feedback.` (the list is defined in [`event_code_names.cc`](event_code_names.cc)).

`predicates` defines predicates that can be applied to events in isolation. Each one has a required `type` field, optional `enabled` (when set to zero the result of this event is treated as rejections, unless another behavior is specified, defaults to `1`), optional `inverted` (rejects events that would have been accepted and vice versa, defaults to `0`), and type-specific. Possible predicate types:

- `accept`: accepts the event
- `code_ns`, `code_major`, `code_minor`, `payload`: accepts the event if the corresponding field lies in the range defined with `min` and `max` predicate fields (inclusively)
- `input_index`: the same range check, but for the connector at which the event arrived to the current node
- `conjunction`/`and`: accepts the event if all the predicates in the `args` predicate field accept the event, disabled predicated are skipped and thus treated as accepting
- `disjunction`/`or`: accepts the event if any of the predicates in the `args` predicate field accept the event, disabled predicated are skipped and thus treated as rejecting
- `modifier`: accepts the event if the modifier flag at the index specified in the `single_modifier` predicate field is set for the event

`nodes` defines nodes of the graph, each one has `type` and `options` fields. `options` stores type-specific fields. Possible types can be viewed using `--list-modules` command-line option, type-specific fields can be viewed using `--module-help` command-line option.

`channels` defines the connections between graph nodes. Each one has `from` and `to` fields. `from` is a pair of the source node name and it's output connector index, `to` is a pair of the target node name and it's input connector index. Each input and output node connector can have only one connection associated with it.
