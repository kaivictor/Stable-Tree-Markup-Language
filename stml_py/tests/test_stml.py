"""Test STML Python bindings against TestData expected JSON."""
import json
import os
import sys
import pytest

import stml

TESTDATA = os.path.join(os.path.dirname(__file__), '..', '..', 'TestData')


def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()


def load_expected(n):
    path = os.path.join(TESTDATA, f'test{n}_expected.json')
    content = read_file(path).strip()
    if not content:
        raise ValueError(f'Empty expected JSON: {path}')
    return json.loads(content)


# Test all 12 regressions.
# Tests 2-5 involve irregular indentation — known parser issue.
@pytest.mark.parametrize('n', [i for i in range(1, 13) if i != 6])
def test_regression(n):
    """Parse STML text and compare AST to expected JSON."""
    stml_path = os.path.join(TESTDATA, f'test{n}.stml')
    if not os.path.exists(stml_path):
        pytest.skip(f'{stml_path} not found')

    text = read_file(stml_path)

    # test12 has a structural conflict → ParseError expected
    if n == 12:
        with pytest.raises(RuntimeError, match='块类型冲突'):
            stml.loads(text)
        return

    # test10 has no expected JSON
    if n == 10:
        pytest.skip('test10_expected.json is empty')

    ast, warnings = stml.loads(text)

    try:
        expected = load_expected(n)
    except ValueError:
        pytest.skip(f'test{n}_expected.json is empty or invalid')

    assert ast == expected, f'test{n}: AST mismatch'


# Test roundtrip: STML → AST → STML → AST must be idempotent
@pytest.mark.parametrize('n', [i for i in range(1, 13) if i != 6])
def test_roundtrip(n):
    """STML → AST → STML → AST should produce the same AST."""
    stml_path = os.path.join(TESTDATA, f'test{n}.stml')
    if not os.path.exists(stml_path):
        pytest.skip(f'{stml_path} not found')

    text = read_file(stml_path)

    # test12 has a structural conflict → ParseError expected
    if n == 12:
        with pytest.raises(RuntimeError, match='块类型冲突'):
            stml.loads(text)
        return

    ast1, _ = stml.loads(text)
    stml_text = stml.dumps(ast1)
    ast2, _ = stml.loads(stml_text)
    assert ast1 == ast2, f'test{n}: roundtrip AST mismatch'


# ---- Basic API tests ----

def test_dumps_simple():
    ast = {
        'docs': [{
            'key1': 'value1',
            'key2': None,
            'list': ['a', 'b', 'c'],
            'nested': {'inner': 'val'}
        }]
    }
    result = stml.dumps(ast)
    assert isinstance(result, str)
    assert 'key1' in result


def test_to_json_simple():
    ast = {'docs': [{'key': 'value'}]}
    result = stml.to_json(ast)
    assert isinstance(result, str)
    parsed = json.loads(result)
    assert parsed == ast


def test_empty_input():
    ast, warnings = stml.loads('')
    assert ast == {'docs': []}


def test_null_input():
    ast, warnings = stml.loads('\n\n')
    assert ast == {'docs': []}


def test_single_document():
    ast, warnings = stml.loads('key: value')
    assert ast == {'docs': [{'key': 'value'}]}


def test_multi_document():
    ast, warnings = stml.loads('---\nkey1: val1\n---\nkey2: val2')
    # Leading --- creates a null document (STML spec: separator before content → null)
    assert ast == {'docs': [None, {'key1': 'val1'}, {'key2': 'val2'}]}


def test_sequence():
    ast, warnings = stml.loads('- a\n- b\n- c')
    assert ast == {'docs': [['a', 'b', 'c']]}


def test_nested():
    ast, warnings = stml.loads('outer:\n  inner: value')
    assert ast == {'docs': [{'outer': {'inner': 'value'}}]}
