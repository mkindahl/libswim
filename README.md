# SWIM Failure Detector for Embedding

An implementation of a [SWIM][SWIM] (Das, Gupta and Motivala, 2002) failure
detector that can be embedded into other application, for example, into IoT
devices.

SWIM detects failed nodes through a ping / ping-req protocol. Each member
periodically pings a random peer and, on timeout, asks other members to probe
the target before declaring it suspect. A suspicion sub-protocol with
incarnation numbers lets falsely suspected nodes refute the claim, so only
persistently unreachable nodes are marked dead. All membership updates, such
as joins, suspects, and failures, are disseminated by piggybacking them on
the protocol messages themselves, avoiding the need for a separate broadcast
channel.

Tools like [Consul][Consul] and [Serf][Serf] from HashiCorp also build on the
SWIM protocol, but they ship as standalone services that applications
communicate with over RPC. In contrast, libswim is a library designed to be
embedded directly into an application, making it suitable for environments
where running a separate daemon is impractical, such as IoT devices or
resource-constrained systems.

## Dependencies

- CMake (>= 3.20)
- Ninja (or another CMake-supported generator)
- `libuuid`
- `ncurses` (optional — needed for the TUI example)

On Ubuntu:

```sh
sudo apt install cmake ninja-build uuid-dev libncurses-dev
```

## Building

To build:

```sh
cmake -B build -G Ninja
cmake --build build
```

To run the tests:

```sh
ctest --test-dir build
```

[Consul]: https://www.consul.io/
[Serf]: https://www.serf.io/
[SWIM]: https://www.cs.cornell.edu/projects/Quicksilver/public_pdfs/SWIM.pdf
