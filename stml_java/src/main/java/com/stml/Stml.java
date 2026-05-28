package com.stml;

import com.stml.ast.*;
import com.stml.diagnostics.Warning;
import com.stml.lexer.STMLLexer;
import com.stml.lexer.Token;
import com.stml.parser.STMLParser;
import com.stml.serializer.Serializer;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

/**
 * STML — Stable Tree Markup Language Java Library.
 *
 * Public API:
 *   loads(text)    → LoadResult       parse STML string
 *   load(filename) → LoadResult       parse STML file
 *   dumps(ast)     → String           AST → canonical STML
 *   toJson(ast)    → String           AST → JSON string
 *   tokenize(text) → TokenizeResult   lex only (debug)
 *   parse(tokens)  → ParseResult      parse only (debug)
 */
public final class Stml {

    private Stml() {}

    // =========================================================================
    // loads — parse STML text
    // =========================================================================

    /** Parse STML text → (AST, warnings). AST is always {"docs": [{...}, ...]}. */
    public static LoadResult loads(String text) {
        STMLLexer lexer = new STMLLexer(text);
        var lexResult = lexer.tokenize();

        STMLParser parser = new STMLParser(lexResult.tokens());
        var parseResult = parser.parse();

        List<Warning> allWarnings = new ArrayList<>();
        allWarnings.addAll(lexResult.warnings());
        allWarnings.addAll(parseResult.warnings());

        // Wrap in {"docs": [{...}, ...]} per the spec
        AstMap wrapper = new AstMap();
        wrapper.put("docs", new AstList(parseResult.docs()));
        return new LoadResult(wrapper, allWarnings);
    }

    // =========================================================================
    // load — parse an STML file
    // =========================================================================

    /** Read and parse an STML file → (AST, warnings). */
    public static LoadResult load(String filename) throws IOException {
        String text = Files.readString(Path.of(filename));
        return loads(text);
    }

    // =========================================================================
    // Serialization
    // =========================================================================

    /** Convert docs-wrapped AST to canonical STML text. */
    public static String dumps(AstNode node) {
        return Serializer.dumps(node);
    }

    /** Convert an AST node to JSON text. */
    public static String toJson(AstNode node) {
        return Serializer.toJson(node);
    }

    // =========================================================================
    // Debug utilities
    // =========================================================================

    /** Lex only — returns (tokens, warnings). Useful for debugging. */
    public static STMLLexer.TokenizeResult tokenize(String text) {
        STMLLexer lexer = new STMLLexer(text);
        return lexer.tokenize();
    }

    /** Parse only — returns (ast, warnings) from a pre-lexed token list. */
    public static STMLParser.ParseResult parse(List<Token> tokens) {
        STMLParser parser = new STMLParser(tokens);
        return parser.parse();
    }
}
