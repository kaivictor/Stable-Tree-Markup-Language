package com.stml;

import com.stml.ast.*;
import com.stml.diagnostics.Warning;
import com.stml.lexer.StreamingLexer;
import com.stml.parser.StreamingParser;

import java.util.ArrayList;
import java.util.List;

/**
 * STMLStreamer — streaming STML parser for incremental input.
 *
 * Usage:
 *   STMLStreamer streamer = new STMLStreamer();
 *   for (String chunk : llmOutputChunks) {
 *       streamer.feed(chunk);
 *   }
 *   LoadResult result = streamer.finish();
 */
public class STMLStreamer {

    private final StreamingLexer lexer;
    private final StreamingParser parser;
    private final List<Warning> allWarnings = new ArrayList<>();
    private int lexerWarningCount = 0;
    private int parserWarningCount = 0;

    public STMLStreamer() {
        this.lexer = new StreamingLexer();
        this.parser = new StreamingParser();
    }

    /** Feed a text chunk. May emit warnings internally (retrievable via getWarnings()). */
    public void feed(String chunk) {
        var tokens = lexer.feed(chunk);
        if (!tokens.isEmpty()) {
            parser.feed(tokens);
        }
        mergeNewLexerWarnings();
    }

    /** Signal end of input. Returns the complete AST + all warnings. */
    public LoadResult finish() {
        // Finalize lexer → remaining tokens (DEDENTs + END)
        var finalTokens = lexer.finish();
        if (!finalTokens.isEmpty()) {
            parser.feed(finalTokens);
        }

        // Collect remaining warnings
        mergeNewLexerWarnings();
        mergeNewParserWarnings();

        // Finalize parser → document AST list
        List<AstNode> docsList = parser.finish();

        // Wrap in {"docs": [...]} per spec
        AstMap wrapper = new AstMap();
        wrapper.put("docs", new AstList(docsList));

        return new LoadResult(wrapper, new ArrayList<>(allWarnings));
    }

    /** Accumulated warnings so far (useful for progress reporting). */
    public List<Warning> getWarnings() {
        return new ArrayList<>(allWarnings);
    }

    private void mergeNewLexerWarnings() {
        var w = lexer.getWarnings();
        if (w.size() > lexerWarningCount) {
            allWarnings.addAll(w.subList(lexerWarningCount, w.size()));
            lexerWarningCount = w.size();
        }
    }

    private void mergeNewParserWarnings() {
        var w = parser.getWarnings();
        if (w.size() > parserWarningCount) {
            allWarnings.addAll(w.subList(parserWarningCount, w.size()));
            parserWarningCount = w.size();
        }
    }
}
