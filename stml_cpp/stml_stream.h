#pragma once

#include <string>
#include <vector>
#include <functional>

#include "diagnostics/error.h"
#include "ast/ast.h"
#include "stml.h"

namespace stml {

// =========================================================================
// STMLStreamer — streaming STML parser for incremental input.
// =========================================================================
class STMLStreamer {
public:
    STMLStreamer() = default;

    /// Callback type: invoked for each complete document parsed.
    using DocumentCallback = std::function<void(const AstNode&)>;

    /// Set a callback to receive each document as it's parsed.
    void on_document(DocumentCallback cb);

    /// Feed a text chunk.
    void feed(const std::string& chunk);

    /// Signal end of input. Parses any remaining text.
    void finish();

    /// Convenience: signal end of input and return the complete result.
    /// This is a non-streaming convenience; the callback-based finish() is preferred.
    LoadResult finalize();

    /// Accumulated warnings.
    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    void process_complete_documents();

    std::string buffer_;
    bool finished_ = false;
    std::vector<Warning> warnings_;
    DocumentCallback doc_callback_;
};

/// Convenience: stream-parse complete text in one call.
LoadResult stream_parse(const std::string& text);

} // namespace stml
