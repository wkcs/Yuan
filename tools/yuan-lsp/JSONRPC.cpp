#include "JSONRPC.h"
#include <string>
#include <iostream>

namespace yuan {
namespace lsp {

std::optional<json> readMessage(std::istream& in) {
    std::string line;
    size_t contentLength = 0;

    // Read headers until we find an empty line (CRLF)
    while (std::getline(in, line)) {
        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break; // End of headers
        }

        const std::string clHeader = "Content-Length: ";
        if (line.compare(0, clHeader.size(), clHeader) == 0) {
            contentLength = std::stoull(line.substr(clHeader.size()));
        }
    }

    if (contentLength == 0) {
        return std::nullopt; // EOF or invalid header
    }

    // Read payload
    std::string content(contentLength, ' ');
    in.read(&content[0], contentLength);

    if (in.gcount() != static_cast<std::streamsize>(contentLength)) {
        return std::nullopt; // Read error
    }

    try {
        return json::parse(content);
    } catch (const json::parse_error&) {
        return std::nullopt;
    }
}

void writeMessage(std::ostream& out, const json& msg) {
    std::string content = msg.dump();
    out << "Content-Length: " << content.size() << "\r\n\r\n" << content;
    out.flush();
}

} // namespace lsp
} // namespace yuan
