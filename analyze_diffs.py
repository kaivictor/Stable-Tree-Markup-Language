"""Analyze differences between parser output and expected JSON for each test."""
import json
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'stml_cpp', 'build'))
TestData = os.path.join(os.path.dirname(__file__), 'TestData')

import stml

def load_expected_json(n):
    path = os.path.join(TestData, f'test{n}_expected.json')
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

def load_stml_text(n):
    path = os.path.join(TestData, f'test{n}.stml')
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()

def get_parser_output(n):
    text = load_stml_text(n)
    ast, warnings = stml.loads(text)
    # ast is a dict with "docs" key
    actual_json_str = stml.to_json(ast)
    return json.loads(actual_json_str)

def compare_dicts(actual, expected, path=""):
    """Recursively compare two JSON structures and yield differences."""
    if type(actual) != type(expected):
        yield f"{path}: TYPE mismatch - actual={type(actual).__name__}, expected={type(expected).__name__}"
        yield f"  actual:   {json.dumps(actual, ensure_ascii=False)}"
        yield f"  expected: {json.dumps(expected, ensure_ascii=False)}"
        return

    if isinstance(actual, dict):
        act_keys = list(actual.keys())
        exp_keys = list(expected.keys())

        for k in act_keys:
            if k not in expected:
                yield f"{path}.{k}: EXTRA key in actual (not in expected)"
        for k in exp_keys:
            if k not in actual:
                yield f"{path}.{k}: MISSING key in actual (present in expected)"

        for k in act_keys:
            if k in expected:
                yield from compare_dicts(actual[k], expected[k], f"{path}.{k}")

    elif isinstance(actual, list):
        if len(actual) != len(expected):
            yield f"{path}: LENGTH mismatch - actual={len(actual)}, expected={len(expected)}"

        for i in range(min(len(actual), len(expected))):
            yield from compare_dicts(actual[i], expected[i], f"{path}[{i}]")

        if len(actual) > len(expected):
            for i in range(len(expected), len(actual)):
                yield f"{path}[{i}]: EXTRA in actual: {json.dumps(actual[i], ensure_ascii=False)}"
        elif len(expected) > len(actual):
            for i in range(len(actual), len(expected)):
                yield f"{path}[{i}]: MISSING in actual (expected: {json.dumps(expected[i], ensure_ascii=False)})"

    elif actual != expected:
        yield f"{path}: VALUE mismatch"
        yield f"  actual:   {json.dumps(actual, ensure_ascii=False)}"
        yield f"  expected: {json.dumps(expected, ensure_ascii=False)}"


def print_json_brief(data, indent=0, max_width=100):
    """Print a compact summary of JSON structure, showing keys and value types."""
    prefix = "  " * indent
    if isinstance(data, dict):
        for k, v in data.items():
            key_str = json.dumps(k, ensure_ascii=False)
            if isinstance(v, dict):
                print(f"{prefix}{key_str}: {{...}} ({len(v)} keys)")
                print_json_brief(v, indent + 1)
            elif isinstance(v, list):
                print(f"{prefix}{key_str}: [...] ({len(v)} items)")
                print_json_brief(v, indent + 1)
            elif v is None:
                print(f"{prefix}{key_str}: null")
            else:
                val_str = json.dumps(v, ensure_ascii=False)
                if len(val_str) > max_width:
                    val_str = val_str[:max_width] + "..."
                print(f"{prefix}{key_str}: {val_str}")
    elif isinstance(data, list):
        for i, item in enumerate(data):
            if isinstance(item, dict):
                print(f"{prefix}[{i}]: {{...}} ({len(item)} keys)")
                print_json_brief(item, indent + 1)
            elif isinstance(item, list):
                print(f"{prefix}[{i}]: [...] ({len(item)} items)")
                print_json_brief(item, indent + 1)
            elif item is None:
                print(f"{prefix}[{i}]: null")
            else:
                val_str = json.dumps(item, ensure_ascii=False)
                if len(val_str) > max_width:
                    val_str = val_str[:max_width] + "..."
                print(f"{prefix}[{i}]: {val_str}")
    else:
        print(f"{prefix}{json.dumps(data, ensure_ascii=False)}")


if __name__ == '__main__':
    # Ensure UTF-8 output
    sys.stdout.reconfigure(encoding='utf-8')

    failing = [2, 3, 4, 5, 12]

    for n in failing:
        print("=" * 80)
        print(f"Test {n}")
        print("=" * 80)

        try:
            actual = get_parser_output(n)
            expected = load_expected_json(n)

            diffs = list(compare_dicts(actual, expected, f"test{n}"))

            if not diffs:
                print("  NO DIFFERENCES - PASS")
                continue

            print(f"\n  Found {len(diffs)} differences:\n")
            for d in diffs:
                print(f"    {d}")

            print(f"\n  --- ACTUAL structure ---")
            print_json_brief(actual, indent=2)

            print(f"\n  --- EXPECTED structure ---")
            print_json_brief(expected, indent=2)

        except Exception as e:
            print(f"  ERROR: {e}")
            import traceback
            traceback.print_exc()

        print()

