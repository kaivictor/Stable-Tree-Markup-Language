#include "stml_stream.h"
<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
#include "stml.h"
#include <algorithm>

namespace stml {

// =========================================================================
// STMLStreamer implementation
// =========================================================================

void STMLStreamer::on_document(DocumentCallback cb) {
    doc_callback_ = std::move(cb);
}

void STMLStreamer::feed(const std::string& chunk) {
    if (finished_) return;
    buffer_ += chunk;
    process_complete_documents();
}

void STMLStreamer::finish() {
    if (finished_) return;
    finished_ = true;

    // Parse the remaining buffer
    if (!buffer_.empty() || warnings_.empty()) {
        // If buffer_ is empty but this is the first call (empty input),
        // we still need to produce the empty docs list.
        auto result = loads(buffer_);
        warnings_.insert(warnings_.end(),
                         result.warnings.begin(), result.warnings.end());

        // Extract individual documents from {"docs": [...]}
        if (result.ast.is_map()) {
            const AstNode* docs = map_find(*result.ast.as_map(), "docs");
            if (docs && docs->is_list()) {
                for (const auto& doc : *docs->as_list()) {
                    if (doc_callback_) {
                        doc_callback_(doc);
                    }
                }
            }
        }
    }
    buffer_.clear();
}

void STMLStreamer::process_complete_documents() {
    // Search for document separator "---" at the start of a line.
    // We only split when we have a complete document ending with \n---\n.
    while (true) {
        // Find "---" at the start of a line (after \n or at position 0)
        size_t pos = 0;
        size_t sep_pos = std::string::npos;

        while (pos < buffer_.size()) {
            // Check for "\n---\n" or start-of-buffer "---\n"
            // or "\n---" at end, or "---" at start with end
            if (pos == 0 && buffer_.size() >= 4
                && buffer_[0] == '-' && buffer_[1] == '-' && buffer_[2] == '-'
                && buffer_[3] == '\n') {
                sep_pos = 0;
                break;
            }
            size_t nl = buffer_.find('\n', pos);
            if (nl == std::string::npos) break;
            size_t next = nl + 1;
            if (next + 3 <= buffer_.size()
                && buffer_[next] == '-' && buffer_[next + 1] == '-'
                && buffer_[next + 2] == '-') {
                // Check that "---" is at start of line and followed by \n or EOF
                if (next + 3 == buffer_.size() || buffer_[next + 3] == '\n'
                    || buffer_[next + 3] == '\r') {
                    sep_pos = next;
                    break;
                }
            }
            pos = next;
        }

        if (sep_pos == std::string::npos) {
            // No complete document separator found; wait for more input
            break;
        }

        // Extract text before the separator
        std::string doc_text;
        if (sep_pos > 0) {
            doc_text = buffer_.substr(0, sep_pos);
        } // else: sep at position 0 → empty document (leading ---)

        // Remove the extracted text and separator from buffer
        size_t consume_end = sep_pos + 3; // "---"
        if (consume_end < buffer_.size() && buffer_[consume_end] == '\r') {
            ++consume_end;
        }
        if (consume_end < buffer_.size() && buffer_[consume_end] == '\n') {
            ++consume_end;
        }
        buffer_.erase(0, consume_end);

        // Parse the document text
        if (!doc_text.empty() || sep_pos > 0) {
            auto result = loads(doc_text);
            warnings_.insert(warnings_.end(),
                             result.warnings.begin(), result.warnings.end());

            // Extract individual documents from {"docs": [...]}
            if (result.ast.is_map()) {
                const AstNode* docs = map_find(*result.ast.as_map(), "docs");
                if (docs && docs->is_list()) {
                    for (const auto& doc : *docs->as_list()) {
                        if (doc_callback_) {
                            doc_callback_(doc);
                        }
                    }
                }
            }
        }
    }
<<<<<<< Updated upstream
=======
=======

namespace stml {

STMLStreamer::STMLStreamer() {
    // Lexer 产出 Token → 直接喂给 Parser
    lexer_.on_token([this](Token t) {
        parser_.feed_token(t);
    });
}

void STMLStreamer::feed(const std::string& text) {
    if (finished_) return;
    lexer_.feed(text);
}

AstList STMLStreamer::finish() {
    if (finished_) {
        throw ParseError(0, 0, "STMLStreamer already finished");
    }
    finished_ = true;

    lexer_.finish();
    AstList docs = parser_.finish();

    collect_warnings();
    return docs;
}

void STMLStreamer::collect_warnings() {
    for (const auto& w : lexer_.warnings()) {
        warnings_.push_back(w);
    }
    for (const auto& w : parser_.warnings()) {
        warnings_.push_back(w);
    }
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
