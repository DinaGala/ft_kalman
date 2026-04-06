#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>

#include "udp.hpp"
#include "parser.hpp"

// Simple demo: send a handshake, wait for first sensor message, parse it,
// send back an estimation formatted as "X Y Z" (double precision).

int main(int argc, char **argv) {
    if (argc > 3 || argc == 2) {
        std::cerr << "Usage: " << argv[0] << " [host] [port]\n";
        std::cerr << "Example: ./kalman 127.0.0.1 4242\n";
        return 1;
    }

    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    std::string port = (argc > 2) ? argv[2] : "4242";
    std::string handshake = "READY";

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
        if (!client.receiveMessage(msg, 1000)) {
            std::cerr << "no response from sensor-stream within 1s (attempt " << attempt+1 << ")\n";
            continue;
        }
        std::cout << "received: " << msg << "\n";
        vopt = parseVec3(msg);
        if (vopt) break;
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

    // Continue dialogue: receive updates and reply with estimations. This loop
    // demonstrates the parsing and send/receive flow; replace estimation logic
    // with your Kalman filter's output.
    for (int iter = 0; iter < 1000; ++iter) {
        std::string update;
        if (!client.receiveMessage(update, 1000)) {
            std::cerr << "timeout waiting for update (1s). Ending.\n";
            break;
        }
        auto up = parseVec3(update);
        if (!up) {
            std::cerr << "warning: failed to parse update: " << update << "\n";
            continue;
        }
        // placeholder: echo back the received position
        std::ostringstream o2;
        o2 << std::setprecision(17) << up->x << " " << up->y << " " << up->z;
        std::string estimate = o2.str();
        if (!client.sendMessage(estimate)) {
            std::cerr << "failed to send estimate during loop\n";
            break;
        }
    }

    return 0;
}
