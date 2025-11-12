# ft_kalman

A Kalman Filter in C++ that tracks the position coordinates of a generic vehicle with flawed sensors. This repository contains a simple UDP-based CLI program that
connects to the provided `imu-sensor-stream` (UDP server), performs a handshake, receives the initial sensor packet for filter initialization, and replies with
position estimations in the required format.

This README explains how to build, run and test the program and how the handshake/packet exchange should work.

## What is included

- `Makefile` — builds the `kalman` executable using `-Wall -Wextra -Werror` and generates dependency files to avoid unnecessary relinking.
- `main.cpp` — example CLI which implements handshake, receive/parse first packet, and replies with formatted estimations.
- `udp.cpp` / `udp.hpp` — small UDP client using POSIX sockets with select-based receive timeout.
- `parser.cpp` / `parser.hpp` — simple parser to extract three doubles (X Y Z) from incoming packets.

## Build

Requirements: a reasonably up-to-date GCC or Clang that supports C++17.

To build, run from the project root:

```bash
make
```

This produces the `kalman` binary. The `Makefile` uses `-std=c++17` by default; if you need C++20 change the `CXXFLAGS` in the `Makefile`.

## Running with `imu-sensor-stream`

The test harness you mentioned is `imu-sensor-stream`. Start it first. Example command (as provided):

```bash
./imu-sensor-stream -s 42 -d 42 -p 4242
```

This will open a UDP listener on port 4242 and wait for a client to connect and perform the handshake.

Run the `kalman` program and pass the sensor-stream host and port. By default the program sends the handshake token `START`. You can override it with a third argument; some setups use `READY` instead of `START` as the handshake token.

Example (same machine) using the `READY` token:

```bash
./kalman 127.0.0.1 4242 READY
```

Or using `localhost`:

```bash
./kalman localhost 4242 READY
```

## Handshake and message flow

1. `kalman` sends a handshake message (the default string `START` or another token supplied as the 3rd CLI argument — for example `READY`).
2. The sensor-stream responds with the first sensor packet. This initial packet contains at least three whitespace-separated floating point numbers representing position (X Y Z). Example:

```
1.7325073314060224 -2.2213777837034083 0.49999962025821726
```

3. `kalman` must parse those three doubles, use them to initialize the Kalman filter, then send the first position estimation back to the sensor stream using the same plain-text format: `X Y Z` (space separated, high precision floating point). The estimation should be close to the real coordinates (within 5 meters) and must be returned within 1 second of receiving the input.
4. After the first estimation, the sensor-stream will continue sending updates; your program should reply with a new estimation for each update. If you do not reply within 1 second, or your estimate is farther than 5 meters, the sensor-stream will return an error and stop the run.

## Message format and parser

- Incoming messages are expected to contain at least three whitespace-separated floating point numbers. `parser.cpp` extracts the first three values into a `Vec3 { x, y, z }`.
- Outgoing estimations must be formatted as three doubles separated by spaces. High precision is recommended; the example code uses `std::setprecision(17)` to preserve double precision.

Example outgoing line:

```
1.7325073314060224 -2.2213777837034083 0.49999962025821726
```

## Timeouts and robustness

- The provided client uses a 1 second timeout when waiting for messages; this matches the requirement that estimations be produced within 1 second. If you need to change this value, modify the `receiveMessage` call timeout in `main.cpp`.
- The UDP client performs send retries for the handshake and uses `select()` to implement receive timeouts.

## Replacing the placeholder estimator with your Kalman filter

`main.cpp` currently echoes the parsed position as the estimation (placeholder). To implement the real filter:

1. Implement your Kalman filter in new files (e.g., `kalman_filter.hpp` / `kalman_filter.cpp`).
2. In `main.cpp`, after `parseVec3(msg)`, initialize your filter state with the received `Vec3`.
3. For each incoming update, feed sensor data into your filter and compute the estimated position. Format and send the result as `X Y Z` within 1 second.

Remember to free any heap-allocated memory and avoid leaks; using RAII (std::vector, smart pointers) is recommended.

## Troubleshooting

- If `make` fails with an unsupported C++ standard, update `CXXFLAGS` in `Makefile` to match your compiler's supported standard (e.g., `-std=c++14` or `-std=c++20`).
- If you see socket errors, ensure `imu-sensor-stream` is running and listening on the correct address and port.
- Use `strace` if you need low-level debugging of socket calls.

## Testing tips

- You can test locally by running `imu-sensor-stream` and `kalman` on the same machine using `127.0.0.1` or `localhost`.
- To ensure your estimator responds quickly, profile the estimation code and avoid expensive allocations in the per-packet loop. Pre-allocate buffers and matrices where possible.

## License & Notes

This code is intentionally minimal and educational. The example UDP and parser code is safe for instructional use and avoids global allocations. If you want, I can add a small unit test harness or wire in a simple Kalman filter implementation.
