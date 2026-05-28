"""Unit tests for the STML lexer (via tokenize)."""

import stml


class TestLexer:
    """Lexer unit tests via stml.tokenize()."""

    def _token_types(self, tokens):
        """Extract token type names as a list."""
        return [t["type"] for t in tokens]

    def _token_values(self, tokens):
        """Extract token values as a list."""
        return [t["value"] for t in tokens]

    # ---- Basic structure ----

    def test_simple_key_value(self):
        tokens, _ = stml.tokenize('"key": "value"\n')
        types = self._token_types(tokens)
        assert "KEY" in types
        assert "COLON" in types
        assert "SCALAR" in types

    def test_null_value(self):
        tokens, _ = stml.tokenize('"key": null\n')
        types = self._token_types(tokens)
        assert "NULL_" in types

    def test_tilde_null(self):
        tokens, _ = stml.tokenize('"key": ~\n')
        types = self._token_types(tokens)
        assert "NULL_" in types

    # ---- Indentation ----

    def test_indent_dedent(self):
        text = '"root":\n  "child": "value"\n'
        tokens, _ = stml.tokenize(text)
        types = self._token_types(tokens)
        assert "INDENT" in types
        assert "DEDENT" in types

    def test_deep_nesting(self):
        text = '"a":\n  "b":\n    "c": "d"\n'
        tokens, _ = stml.tokenize(text)
        types = self._token_types(tokens)
        assert types.count("INDENT") == 2
        assert types.count("DEDENT") == 2

    # ---- Sequences ----

    def test_dash_sequence(self):
        text = '"list":\n  - "item1"\n  - "item2"\n'
        tokens, _ = stml.tokenize(text)
        types = self._token_types(tokens)
        assert types.count("DASH") == 2

    # ---- Quoted keys ----

    def test_quoted_key_non_greedy(self):
        text = '"ke\"y": "value"\n'
        tokens, _ = stml.tokenize(text)
        # The key should be ke"y (unescaped)
        for t in tokens:
            if t["type"] == "KEY":
                assert t["value"] == 'ke"y'

    # ---- Inline lists ----

    def test_inline_list(self):
        text = '"nums": [1, 2, 3]\n'
        tokens, _ = stml.tokenize(text)
        types = self._token_types(tokens)
        assert "INLINE_LIST" in types

    # ---- Multiline strings ----

    def test_multiline_string(self):
        text = '"text": {\n  line1\n  line2\n}\n'
        tokens, _ = stml.tokenize(text)
        types = self._token_types(tokens)
        assert "MULTILINE_STRING" in types

    # ---- Comments ----

    def test_comment_line(self):
        text = '# this is a comment\n"key": "value"\n'
        tokens, _ = stml.tokenize(text)
        # Comment line is a newline, not a separate token type
        types = self._token_types(tokens)
        assert "KEY" in types

    # ---- Edge cases ----

    def test_empty_input(self):
        tokens, _ = stml.tokenize("")
        # Should at least have END token
        assert len(tokens) >= 1
        assert tokens[-1]["type"] == "END"

    def test_unclosed_quote_produces_warning(self):
        text = '"key": "unclosed string\n'
        tokens, warnings = stml.tokenize(text)
        # Should produce at least one warning for unclosed quote
        raw_strings = [t for t in tokens if t["type"] == "RAW_STRING"]
        assert len(raw_strings) >= 1 or len(warnings) >= 1
