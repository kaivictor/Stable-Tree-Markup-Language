#ifndef STML_STML_STREAM_H
#define STML_STML_STREAM_H

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
/// \file stml_stream.h
/// Streaming API for incremental STML parsing.

#include "ast/ast.h"
#include "diagnostics/error.h"
#include <functional>
<<<<<<< Updated upstream
=======
=======
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast.h"
#include "diagnostics/error.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <string>
#include <vector>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// STMLStreamer — incrementally feed text and receive callbacks.
=======
<<<<<<< Updated upstream
// =========================================================================
// STMLStreamer — incrementally feed text and receive callbacks.
=======
// ============================================================
// STMLStreamer — 流式便利包装
>>>>>>> Stashed changes
>>>>>>> Stashed changes
//
// 将 Lexer 和 Parser 串联：
//   feed(text) → Lexer → Parser
//   finish()  → 返回 AST
//
// 用法:
//   STMLStreamer streamer;
<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
//   streamer.on_document([](const AstNode& doc) { ... });
//   streamer.feed(chunk1);
//   streamer.feed(chunk2);
//   streamer.finish();
//   for (auto& w : streamer.warnings()) { ... }
// =========================================================================
class STMLStreamer {
public:
    using DocumentCallback = std::function<void(const AstNode& doc)>;

    STMLStreamer() = default;

    /// Set a callback invoked for each completed document.
    void on_document(DocumentCallback cb);

    /// Feed a chunk of text. Complete documents trigger the callback.
    void feed(const std::string& chunk);

    /// Signal end of input. Flushes any remaining document.
    void finish();

    /// Get accumulated warnings.
    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    DocumentCallback doc_callback_;
    std::vector<Warning> warnings_;
    std::string buffer_;
    bool finished_ = false;

    void process_complete_documents();
<<<<<<< Updated upstream
=======
=======
//   streamer.feed(chunk1);
//   streamer.feed(chunk2);
//   AstList docs = streamer.finish();
// ============================================================
class STMLStreamer {
public:
    STMLStreamer();

    void feed(const std::string& text);
    AstList finish();

    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    Lexer lexer_;
    Parser parser_;
    std::vector<Warning> warnings_;
    bool finished_ = false;

    void collect_warnings();
>>>>>>> Stashed changes
>>>>>>> Stashed changes
};

} // namespace stml

#endif // STML_STML_STREAM_H
