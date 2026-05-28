"""Round-trip regression tests using shared TestData (test1-11.stml).

test6 contains unsupported structural conflicts → ParseError.
test10 has an empty expected JSON (empty input → {"docs": []}).

Validation chains:
  A: STML → loads() → to_json() → compare with expected JSON
  B: expected JSON → dumps() → loads() → to_json() → compare with expected JSON
"""

import json
import os
import pytest
import stml

TESTDATA_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "TestData")
TEST_COUNT = 12

# test6 triggers ParseError; test10 has an empty expected JSON.
PARSE_ERROR_TESTS = {6}
EMPTY_EXPECTED_TESTS = {10}


def read_file(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def load_expected_json(n):
    """Load expected JSON, handling empty files (→ {"docs": []})."""
    expected_path = os.path.join(TESTDATA_DIR, f"test{n}_expected.json")
    raw = read_file(expected_path)
    if n in EMPTY_EXPECTED_TESTS:
        raw = raw.strip()
        if not raw:
            raw = '{"docs": []}'
    return json.loads(raw)


def json_normalize(obj):
    """Parse JSON string and return the Python object for deep comparison."""
    return json.loads(obj)


class TestRegression:
    """Test all 11 test datasets for STML→AST→JSON correctness."""

    @pytest.mark.parametrize("n", range(1, TEST_COUNT + 1))
    def test_parse_to_json(self, n):
        """Chain A: STML text → loads() → to_json() ≡ expected JSON."""
        stml_path = os.path.join(TESTDATA_DIR, f"test{n}.stml")
        stml_text = read_file(stml_path)

        if n in PARSE_ERROR_TESTS:
            with pytest.raises(stml.ParseError):
                stml.loads(stml_text)
            return

        expected = load_expected_json(n)
        ast, warnings = stml.loads(stml_text)

        # Convert AST to JSON string then parse for deep comparison
        actual_json = stml.to_json(ast)
        actual = json_normalize(actual_json)

        assert actual == expected, (
            f"test{n}: AST→JSON mismatch.\n"
            f"Expected keys at root: {sorted(expected.keys())}\n"
            f"Actual keys at root:   {sorted(actual.keys())}"
        )

    @pytest.mark.parametrize("n", range(1, TEST_COUNT + 1))
    def test_roundtrip_json_via_stml(self, n):
        """Chain B: JSON → dumps() → loads() → to_json() ≡ expected JSON."""
        if n in PARSE_ERROR_TESTS:
            pytest.skip("test6 has no expected JSON (ParseError case)")

        expected = load_expected_json(n)

        # JSON AST → STML text
        stml_text = stml.dumps(expected)

        # STML text → AST
        ast2, _ = stml.loads(stml_text)

        # AST → JSON
        actual_json = stml.to_json(ast2)
        actual = json_normalize(actual_json)

        assert actual == expected, (
            f"test{n}: Roundtrip mismatch.\n"
            f"JSON → STML → JSON did not preserve the AST."
        )

    @pytest.mark.parametrize("n", range(1, TEST_COUNT + 1))
    def test_roundtrip_stml_idempotent(self, n):
        """STML → loads() → dumps() → loads() → dumps() is idempotent."""
        stml_path = os.path.join(TESTDATA_DIR, f"test{n}.stml")
        stml_text = read_file(stml_path)

        if n in PARSE_ERROR_TESTS:
            with pytest.raises(stml.ParseError):
                stml.loads(stml_text)
            return

        ast1, _ = stml.loads(stml_text)
        stml1 = stml.dumps(ast1)

        ast2, _ = stml.loads(stml1)
        stml2 = stml.dumps(ast2)

        assert stml1 == stml2, (
            f"test{n}: STML roundtrip not idempotent.\n"
            f"First  dumps(): {stml1[:200]}...\n"
            f"Second dumps(): {stml2[:200]}..."
        )
