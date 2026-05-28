#pragma once

#include <string>
#include <vector>

#include "diagnostics/error.h"
#include "ast/ast.h"

// Include for LoadResult (circular but guarded by #pragma once)
#include "stml.h"

namespace stml {

// Forward declarations
class StreamingLexer;
class StreamingParser;

// =========================================================================
// STMLStreamer — streaming STML parser for incremental input.
//
// Usage:
//   STMLStreamer streamer;
//   for (chunk : llm_output_chunks) {
//       streamer.feed(chunk);
//   }
//   LoadResult result = streamer.finalize();
//   // result.ast → {"docs": [{...}, ...]}
//   // result.warnings → all accumulated warnings
//
// Tokens are piped internally: StreamingLexer → StreamingParser.
// AST is only available after finalize().
// =========================================================================
class STMLStreamer {
public:
    STMLStreamer();
    ~STMLStreamer();

    /// Feed a text chunk. May emit warnings internally (retrievable via warnings()).
    void feed(const std::string& chunk);

    /// Signal end of input. Returns the complete AST + all warnings.
    LoadResult finalize();

    /// Accumulated warnings so far (useful for progress reporting).
    const std::vector<Warning>& warnings() const;

private:
    // Using PIMPL to avoid exposing internal headers
    class Impl;
    Impl* m_impl;
};

/// Convenience: stream-parse complete text in one call.
/// Equivalent to constructing STMLStreamer, feed(text), finalize().
LoadResult stream_parse(const std::string& text);

} // namespace stml
