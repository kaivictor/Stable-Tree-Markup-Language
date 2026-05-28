"""Generate and verify 测试N标准文档.stml from TestData/testN.stml.

Regenerates the canonical STML output using the fixed serializer,
then verifies round-trip idempotency: STML → AST → STML → AST.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from stml_py_ import loads, dumps

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TESTDATA_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), 'TestData')


def _deep_compare(a, b, path=''):
    """Return list of difference descriptions, empty when equal."""
    if type(a) != type(b):
        return [f'Type mismatch at {path}: {type(a).__name__} vs {type(b).__name__}']
    if isinstance(a, dict):
        diffs = []
        all_keys = set(a.keys()) | set(b.keys())
        for k in sorted(all_keys, key=str):
            if k not in a:
                diffs.append(f'Missing key {path}.{repr(k)} in actual')
            elif k not in b:
                diffs.append(f'Extra key {path}.{repr(k)} in actual')
            else:
                diffs.extend(_deep_compare(a[k], b[k], f'{path}.{repr(k)}'))
        return diffs
    elif isinstance(a, list):
        diffs = []
        if len(a) != len(b):
            diffs.append(f'List length mismatch at {path}: {len(a)} vs {len(b)}')
        for i in range(min(len(a), len(b))):
            diffs.extend(_deep_compare(a[i], b[i], f'{path}[{i}]'))
        return diffs
    elif a != b:
        return [f'Value mismatch at {path}: {repr(a)[:100]} vs {repr(b)[:100]}']
    return []


def main():
    errors = []

    for n in range(1, 8):
        input_path = os.path.join(TESTDATA_DIR, f'test{n}.stml')
        output_path = os.path.join(TESTDATA_DIR, f'测试{n}标准文档.stml')

        # 1. Parse original STML
        with open(input_path, 'r', encoding='utf-8') as f:
            text = f.read()
        ast1, warnings = loads(text)

        # 2. Serialize to canonical STML
        stml_output = dumps(ast1)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(stml_output)

        # 3. Round-trip verify: STML → AST → STML → AST
        ast2, _ = loads(stml_output)
        diffs = _deep_compare(ast1, ast2)

        doc_count = len(ast1.get('docs', {}))
        status = 'OK' if not diffs else 'FAIL'
        print(f'{status} test{n}: {doc_count} docs, {len(warnings)} warnings')

        if diffs:
            for d in diffs[:5]:
                print(f'     {d}')
            errors.append(n)

    if errors:
        print(f'\nFAIL: tests {errors} round-trip mismatch')
        sys.exit(1)
    else:
        print('\nAll tests passed.')


if __name__ == '__main__':
    main()
