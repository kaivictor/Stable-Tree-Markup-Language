"""Unit tests for the STML parser (via loads)."""

import pytest
import stml


def _docs(ast):
    """Extract the 'docs' list from a loads() result."""
    return ast.get("docs", [])


class TestParser:
    """Parser unit tests via stml.loads()."""

    def test_simple_key_value(self):
        ast, _ = stml.loads('"key": "value"\n')
        docs = _docs(ast)
        assert docs[0]["key"] == "value"

    def test_null_value(self):
        ast, _ = stml.loads('"key": null\n')
        docs = _docs(ast)
        assert docs[0]["key"] is None

    def test_tilde_null(self):
        ast, _ = stml.loads('"key": ~\n')
        docs = _docs(ast)
        assert docs[0]["key"] is None

    def test_nested_mapping(self):
        text = '"a":\n  "b":\n    "c": "d"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["a"]["b"]["c"] == "d"

    def test_sequence(self):
        text = '"list":\n  - "a"\n  - "b"\n  - "c"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["list"] == ["a", "b", "c"]

    def test_sequence_with_mapping_items(self):
        text = '"items":\n  - "name": "Alice"\n  - "name": "Bob"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 2
        assert items[0]["name"] == "Alice"
        assert items[1]["name"] == "Bob"

    def test_inline_list(self):
        text = '"nums": [1, 2, 3]\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["nums"] == ["1", "2", "3"]

    def test_inline_list_with_null(self):
        text = '"vals": [a, ~, null]\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["vals"] == ["a", None, None]

    def test_multi_document(self):
        text = '"key": "a"\n---\n"key": "b"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["key"] == "a"
        assert docs[1]["key"] == "b"

    def test_empty_document(self):
        # Lone separator → leading + trailing null docs
        ast, _ = stml.loads("---\n")
        docs = _docs(ast)
        assert docs == [None, None]

    def test_double_separator_null_doc(self):
        # Two consecutive separators create null documents between/before/after
        ast, _ = stml.loads("---\n---\nkey: value\n")
        docs = _docs(ast)
        assert len(docs) == 3
        assert docs[0] is None
        assert docs[1] is None
        assert docs[2] == {"key": "value"}

    def test_empty_input(self):
        ast, _ = stml.loads("")
        docs = _docs(ast)
        assert docs == []

    def test_multiline_string(self):
        text = '"text": {\n  hello world\n  line 2\n}\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        # Multiline strings preserve indentation of content
        assert docs[0]["text"] == "  hello world\n  line 2"

    def test_bare_key(self):
        text = "just_a_key\n"
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        assert docs[0]["just_a_key"] is None

    def test_warnings_produced(self):
        """Unclosed quotes should produce warnings."""
        text = '"key": "unclosed\n'
        _, warnings = stml.loads(text)
        assert len(warnings) >= 1
        assert hasattr(warnings[0], "message")
        assert hasattr(warnings[0], "line")
        assert hasattr(warnings[0], "column")

    def test_parse_error_structural_conflict(self):
        """Unsupported structural conflicts should raise ParseError."""
        # Mixing mapping and sequence at the same level
        text = '- "item1"\n"key": "value"\n'
        with pytest.raises(stml.ParseError) as exc_info:
            stml.loads(text)
        e = exc_info.value
        assert isinstance(e, RuntimeError)  # ParseError inherits RuntimeError
        assert hasattr(e, "line")
        assert hasattr(e, "column")
        assert hasattr(e, "message")
        assert e.line >= 1
        assert len(e.message) > 0

    # ---- Multi-key mapping in sequence ----

    def test_multi_key_sequence_inline(self):
        """Multi-key mapping with inline values in a sequence entry."""
        text = '"items":\n  - "key1": "value1"\n    "key2": "value2"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 1
        assert items[0] == {"key1": "value1", "key2": "value2"}

    def test_multi_key_sequence_mixed(self):
        """Multi-key: first inline, second bare (null)."""
        text = '"items":\n  - "key1": "value1"\n    "key2"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 1
        assert items[0] == {"key1": "value1", "key2": None}

    def test_multi_key_sequence_all_null(self):
        """Multi-key: all keys without values."""
        text = '"items":\n  - "key1":\n    "key2":\n    "key3":\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 1
        assert items[0] == {"key1": None, "key2": None, "key3": None}

    def test_multi_key_sequence_three_siblings(self):
        """Three sibling keys with inline values."""
        text = '"items":\n  - "k1": "v1"\n    "k2": "v2"\n    "k3": "v3"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 1
        assert items[0] == {"k1": "v1", "k2": "v2", "k3": "v3"}

    def test_multi_key_sequence_with_block(self):
        """Multi-key: sibling with a block value."""
        text = '"items":\n  - "key1": "value1"\n    "key2":\n      "nested": "val"\n'
        ast, _ = stml.loads(text)
        docs = _docs(ast)
        items = docs[0]["items"]
        assert len(items) == 1
        assert items[0]["key1"] == "value1"
        assert items[0]["key2"] == {"nested": "val"}
