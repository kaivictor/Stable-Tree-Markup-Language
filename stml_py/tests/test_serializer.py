"""Unit tests for the STML serializer (dumps and to_json)."""

import json
import stml


class TestSerializer:
    """Serializer unit tests."""

    # ---- dumps() ----

    def test_serialize_null(self):
        assert stml.dumps(None) == "null\n"

    def test_serialize_string(self):
        assert stml.dumps("hello") == '"hello"\n'

    def test_serialize_string_with_quote(self):
        result = stml.dumps('he"llo')
        assert result == '"he\\"llo"\n'

    def test_serialize_simple_map(self):
        ast = {"docs": [{"key": "value"}]}
        result = stml.dumps(ast)
        assert '"key":' in result
        assert '"value"' in result

    def test_serialize_nested_map(self):
        ast = {"docs": [{"a": {"b": "c"}}]}
        result = stml.dumps(ast)
        assert '"a":' in result
        assert '"b":' in result
        assert '"c"' in result

    def test_serialize_list(self):
        ast = {"docs": [{"items": ["a", "b", "c"]}]}
        result = stml.dumps(ast)
        assert '- "a"' in result
        assert '- "b"' in result
        assert '- "c"' in result

    def test_serialize_inline_list_all_scalars(self):
        ast = {"docs": [{"nums": ["1", "2", "3"]}]}
        result = stml.dumps(ast)
        # All-scalar lists may be serialized inline
        assert "1" in result

    # ---- to_json() ----

    def test_to_json_null(self):
        assert stml.to_json(None) == "null\n"

    def test_to_json_string(self):
        assert stml.to_json("hello") == '"hello"\n'

    def test_to_json_map(self):
        ast = {"key": "value"}
        result = json.loads(stml.to_json(ast))
        assert result == {"key": "value"}

    def test_to_json_nested(self):
        ast = {"a": {"b": [1, 2, 3]}}
        result = json.loads(stml.to_json(ast))
        assert result == {"a": {"b": ["1", "2", "3"]}}

    def test_to_json_null_in_ast(self):
        ast = {"key": None}
        result = json.loads(stml.to_json(ast))
        assert result == {"key": None}

    # ---- Roundtrip: STML → AST → STML → AST ----

    def test_roundtrip_simple(self):
        text = '"key": "value"\n'
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2

    def test_roundtrip_nested(self):
        text = '"a":\n  "b":\n    "c": "d"\n'
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2

    def test_roundtrip_with_list(self):
        text = '"list":\n  - "x"\n  - "y"\n'
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2

    def test_roundtrip_null(self):
        text = '"a": null\n'
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2

    def test_roundtrip_empty(self):
        text = ""
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2

    def test_roundtrip_multi_key_sequence(self):
        text = '"items":\n  - "k1": "v1"\n    "k2": "v2"\n'
        ast1, _ = stml.loads(text)
        stml1 = stml.dumps(ast1)
        ast2, _ = stml.loads(stml1)
        assert ast1 == ast2


class TestTokenizeRoundtrip:
    """tokenize() → parse() roundtrip."""

    def test_tokenize_parse_roundtrip(self):
        text = '"key": "value"\n'
        tokens, _ = stml.tokenize(text)
        ast, _ = stml.parse(tokens)
        docs = ast.get("docs", [])
        assert docs[0]["key"] == "value"
