 #include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>

#include "udp.hpp"
#include "parser.hpp"

// Simple demo: send a handshake, wait for first sensor message, parse it,
// send back an estimation formatted as "X Y Z" (double precision).

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> [handshake]\n";
        std::cerr << "Example: ./kalman 127.0.0.1 4242 START\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];
    std::string handshake = (argc >= 4) ? argv[3] : "READY";

    UDPClient client(host, port);

    // Send handshake to start the trajectory generation. Retry a few times if
    // send fails initially.
    bool ok = false;
    for (int i = 0; i < 3; ++i) {
        if (client.sendMessage(handshake)) { ok = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!ok) {
        std::cerr << "failed to send handshake to " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "handshake sent: \"" << handshake << "\"\n";

    // Wait for the first input that contains three doubles. The sensor stream
    // may print informational lines first (e.g. "Trajectory Generation..."),
    // so keep reading until we can parse a Vec3 or until a total timeout.
    std::string msg;
    const int max_attempts = 10; // up to ~10 seconds total (1s per attempt)
    std::optional<Vec3> vopt;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto recv_time = std::chrono::steady_clock::now();
        if (!client.receiveMessage(msg, 1000)) {
            std::cerr << "no response from sensor-stream within 1s (attempt " << attempt+1 << ")\n";
            continue;
        }
        std::cout << "received: " << msg << "\n";
        vopt = parseVec3(msg);
        if (vopt) {
            // report latency for the initial packet receipt
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - recv_time).count();
            std::cout << "info: parsed initial Vec3 (recv->parse) latency: " << ms << " ms\n";
            break;
        }
        // otherwise ignore non-numeric informational messages and keep waiting
    }
    if (!vopt) {
        std::cerr << "failed to receive a numeric initial sensor message within timeout\n";
        return 1;
    }

    Vec3 initial = *vopt;

    // For demo purposes we send initial position back as estimation. In your
    // actual filter, compute an estimate before sending.
    std::ostringstream oss;
    oss << std::setprecision(17) << initial.x << " " << initial.y << " " << initial.z;
    std::string estimation = oss.str();

    if (!client.sendMessage(estimation)) {
        std::cerr << "failed to send estimation\n";
        return 1;
    }
    std::cout << "sent estimation: " << estimation << "\n";

    // compute and print distance between initial parsed position and sent
    // estimation (useful diagnostic for "Delta is too high" errors)
    {
        Vec3 parsed = initial;
        // parse back the estimation we sent (should match parsed values for
        // the placeholder estimator). This calculates the Euclidean distance
        // between parsed input and sent output.
        double dx = parsed.x - initial.x;
        double dy = parsed.y - initial.y;
        double dz = parsed.z - initial.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        std::cout << "info: initial distance between parsed and sent estimate: " << dist << "\n";
    }

    // Continue dialogue: receive updates and reply with estimations. This loop
    // demonstrates the parsing and send/receive flow; replace estimation logic
    // with your Kalman filter's output.
    for (int iter = 0; iter < 1000; ++iter) {
        std::string update;
        auto recv_time = std::chrono::steady_clock::now();
        if (!client.receiveMessage(update, 1000)) {
            std::cerr << "timeout waiting for update (1s). Ending.\n";
            break;
        }
        auto up = parseVec3(update);
        if (!up) {
            // Many sensor-stream messages are informational or single-value
            // updates (e.g. SPEED, MSG_START/MSG_END or timestamped info). Try
            // to handle SPEED values (which may appear on the same datagram or
            // on the next one). For other known informational messages, log
            // them at info level.
            if (update.find("SPEED") != std::string::npos) {
                // Try to extract a double from this message first
                auto dopt = parseDouble(update);
                if (!dopt) {
                    // maybe the numeric value came in the next packet; try a
                    // short follow-up receive (non-blocking-ish, 200ms)
                    std::string extra;
                    if (client.receiveMessage(extra, 200)) {
                        auto d2 = parseDouble(extra);
                        if (d2) {
                            std::cout << "info: SPEED " << *d2 << "\n";
                            continue;
                        }
                        // if the extra packet isn't numeric, treat it as info
                        std::cout << "info: non-pos message: " << extra << "\n";
                        continue;
                    }
                    std::cout << "info: non-pos message: " << update << "\n";
                    continue;
                }
                std::cout << "info: SPEED " << *dopt << "\n";
                continue;
            }

            if (update.find("MSG_START") != std::string::npos ||
                update.find("MSG_END") != std::string::npos ||
                update.find("Trajectory") != std::string::npos ||
                update.find("TRUE POSITION") != std::string::npos) {
                std::cout << "info: non-pos message: " << update << "\n";
                continue;
            }

            // Otherwise warn so user can see unexpected messages that failed
            // to parse into a Vec3.
            std::cerr << "warning: failed to parse update: " << update << "\n";
            continue;
        }
        // placeholder: echo back the received position
        std::ostringstream o2;
        o2 << std::setprecision(17) << up->x << " " << up->y << " " << up->z;
        std::string estimate = o2.str();
        // compute and print timing and distance diagnostics
        if (!client.sendMessage(estimate)) {
            std::cerr << "failed to send estimate during loop\n";
            break;
        }
        auto send_end = std::chrono::steady_clock::now();
        auto recv_to_send_ms = std::chrono::duration_cast<std::chrono::milliseconds>(send_end - recv_time).count();
        double dx = up->x - up->x; // placeholder estimator echoes position
        double dy = up->y - up->y;
        double dz = up->z - up->z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        std::cout << "sent estimate: " << estimate << "\n";
        std::cout << "info: recv->send latency: " << recv_to_send_ms << " ms, distance: " << dist << "\n";
    }

    return 0;
}
