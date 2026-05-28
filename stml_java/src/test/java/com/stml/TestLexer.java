package com.stml;

import com.stml.ast.*;
import com.stml.lexer.STMLLexer;
import com.stml.lexer.TokenType;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class TestLexer {

    @Test
    void testEmptyInput() {
        var result = Stml.tokenize("");
        assertFalse(result.tokens().isEmpty());
        // Should produce at least END
        assertTrue(result.tokens().stream().anyMatch(t -> t.type() == TokenType.END));
    }

    @Test
    void testSimpleKeyValue() {
        var result = Stml.tokenize("key: value");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.KEY));
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.COLON));
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.SCALAR));
    }

    @Test
    void testQuotedKey() {
        var result = Stml.tokenize("\"my key\": value");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.KEY));
    }

    @Test
    void testNullValue() {
        var result = Stml.tokenize("key: null");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.NULL_));
    }

    @Test
    void testTildeNullValue() {
        var result = Stml.tokenize("key: ~");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.NULL_));
    }

    @Test
    void testBareKey() {
        var result = Stml.tokenize("just_a_key");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.BARE_KEY));
    }

    @Test
    void testSequenceEntry() {
        var result = Stml.tokenize("- item1");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.DASH));
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.SCALAR));
    }

    @Test
    void testInlineList() {
        var result = Stml.tokenize("key: [a, b, c]");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.INLINE_LIST));
    }

    @Test
    void testDocSeparator() {
        var result = Stml.tokenize("---\nkey: value\n---\nkey2: value2");
        var tokens = result.tokens();
        long docSepCount = tokens.stream().filter(t -> t.type() == TokenType.DOC_SEPARATOR).count();
        assertEquals(2, docSepCount);
    }

    @Test
    void testIndentDedent() {
        var result = Stml.tokenize("key:\n  subkey: value");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.INDENT));
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.DEDENT));
    }

    @Test
    void testMultilineString() {
        var result = Stml.tokenize("key: {\n  line1\n  line2\n}");
        var tokens = result.tokens();
        assertTrue(tokens.stream().anyMatch(t -> t.type() == TokenType.MULTILINE_STRING));
    }

    @Test
    void testIteratorNotNull() {
        var result = Stml.tokenize("key: value");
        assertNotNull(result.tokens());
        assertNotNull(result.warnings());
    }
}
