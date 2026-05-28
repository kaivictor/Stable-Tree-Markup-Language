/// STMLStreamer — streaming parser implementation.

#include "stml_stream.h"
#include "lexer/streaming_lexer.h"
#include "parser/streaming_parser.h"

namespace stml {

// =========================================================================
// PIMPL implementation
// =========================================================================

class STMLStreamer::Impl {
public:
    StreamingLexer lexer;
    StreamingParser parser;
    std::vector<Warning> all_warnings;
    size_t lexer_warning_count = 0;   // already merged from lexer
    size_t parser_warning_count = 0;  // already merged from parser

    void merge_new_lexer_warnings() {
        const auto& w = lexer.warnings();
        if (w.size() > lexer_warning_count) {
            all_warnings.insert(all_warnings.end(),
                                w.begin() + lexer_warning_count, w.end());
            lexer_warning_count = w.size();
        }
    }

    void merge_new_parser_warnings() {
        const auto& w = parser.warnings();
        if (w.size() > parser_warning_count) {
            all_warnings.insert(all_warnings.end(),
                                w.begin() + parser_warning_count, w.end());
            parser_warning_count = w.size();
        }
    }
};

// =========================================================================
// Construction / Destruction
// =========================================================================

STMLStreamer::STMLStreamer()
    : m_impl(new Impl())
{
}

STMLStreamer::~STMLStreamer()
{
    delete m_impl;
}

// =========================================================================
// Public API
// =========================================================================

void STMLStreamer::feed(const std::string& chunk)
{
    // Lex the chunk → get new tokens
    auto tokens = m_impl->lexer.feed(chunk);
    if (!tokens.empty()) {
        m_impl->parser.feed(std::move(tokens));
    }

    // Collect only new lexer warnings
    m_impl->merge_new_lexer_warnings();
}

LoadResult STMLStreamer::finalize()
{
    // Finalize lexer → remaining tokens (DEDENTs + END)
    auto final_tokens = m_impl->lexer.finalize();
    if (!final_tokens.empty()) {
        m_impl->parser.feed(std::move(final_tokens));
    }

    // Collect remaining lexer and parser warnings
    m_impl->merge_new_lexer_warnings();
    m_impl->merge_new_parser_warnings();

    // Finalize parser → document AST list
    AstList docs_list = m_impl->parser.finalize();

    // Wrap in {"docs": [...]} per spec
    AstMap wrapper;
    wrapper.emplace_back("docs", AstNode(std::move(docs_list)));

    return {AstNode(std::move(wrapper)), std::move(m_impl->all_warnings)};
}

const std::vector<Warning>& STMLStreamer::warnings() const
{
    return m_impl->all_warnings;
}

// =========================================================================
// Convenience free function
// =========================================================================

LoadResult stream_parse(const std::string& text)
{
    STMLStreamer streamer;
    streamer.feed(text);
    return streamer.finalize();
}

} // namespace stml
