#ifndef YUAN_LSP_JSONRPC_H
#define YUAN_LSP_JSONRPC_H

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <iostream>

using json = nlohmann::json;

namespace yuan {
namespace lsp {

/// Read a single JSON-RPC message from an input stream.
/// Returns std::nullopt if the stream is closed or an error occurs.
std::optional<json> readMessage(std::istream& in);

/// Write a single JSON-RPC message to an output stream.
void writeMessage(std::ostream& out, const json& msg);

} // namespace lsp
} // namespace yuan

#endif // YUAN_LSP_JSONRPC_H
