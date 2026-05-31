#include "stml_stream.h"
#include "stml.h"
#include <algorithm>

namespace stml {

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

    if (!buffer_.empty() || warnings_.empty()) {
        auto result = loads(buffer_);
        warnings_.insert(warnings_.end(),
                         result.warnings.begin(), result.warnings.end());
        if (result.ast.is_map()) {
            const AstNode* docs = map_find(*result.ast.as_map(), "docs");
            if (docs && docs->is_list()) {
                for (const auto& doc : *docs->as_list()) {
                    if (doc_callback_) doc_callback_(doc);
                }
            }
        }
    }
    buffer_.clear();
}

void STMLStreamer::process_complete_documents() {
    while (true) {
        size_t pos = 0;
        size_t sep_pos = std::string::npos;

        while (pos < buffer_.size()) {
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
                if (next + 3 == buffer_.size() || buffer_[next + 3] == '\n'
                    || buffer_[next + 3] == '\r') {
                    sep_pos = next;
                    break;
                }
            }
            pos = next;
        }

        if (sep_pos == std::string::npos) break;

        std::string doc_text;
        if (sep_pos > 0) doc_text = buffer_.substr(0, sep_pos);

        size_t consume_end = sep_pos + 3;
        if (consume_end < buffer_.size() && buffer_[consume_end] == '\r') ++consume_end;
        if (consume_end < buffer_.size() && buffer_[consume_end] == '\n') ++consume_end;
        buffer_.erase(0, consume_end);

        if (!doc_text.empty() || sep_pos > 0) {
            auto result = loads(doc_text);
            warnings_.insert(warnings_.end(),
                             result.warnings.begin(), result.warnings.end());
            if (result.ast.is_map()) {
                const AstNode* docs = map_find(*result.ast.as_map(), "docs");
                if (docs && docs->is_list()) {
                    for (const auto& doc : *docs->as_list()) {
                        if (doc_callback_) doc_callback_(doc);
                    }
                }
            }
        }
    }
}

} // namespace stml
