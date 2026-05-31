"""Quick check of test 2 diff points."""
import json
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'stml_py', 'src'))
import stml

TestData = os.path.join(os.path.dirname(__file__), 'TestData')

for n in [1, 2, 5]:
    print(f"=== Test {n} ===")
    text = open(os.path.join(TestData, f'test{n}.stml'), 'r', encoding='utf-8').read()
    ast, warnings = stml.loads(text)
    actual = json.loads(stml.to_json(ast))
    expected = json.load(open(os.path.join(TestData, f'test{n}_expected.json'), 'r', encoding='utf-8'))

    # Find and print diffs
    def find_diffs(a, e, path=""):
        diffs = []
        if type(a) != type(e):
            diffs.append(f"{path}: TYPE {type(a).__name__} vs {type(e).__name__}")
            diffs.append(f"  a={json.dumps(a, ensure_ascii=False)[:80]}")
            diffs.append(f"  e={json.dumps(e, ensure_ascii=False)[:80]}")
        elif isinstance(a, dict):
            for k in set(list(a.keys()) + list(e.keys())):
                sub = find_diffs(a.get(k), e.get(k), f"{path}.{k}")
                diffs.extend(sub)
                if len(diffs) > 20:
                    return diffs
        elif isinstance(a, list):
            if len(a) != len(e):
                diffs.append(f"{path}: len {len(a)} vs {len(e)}")
            for i in range(min(len(a), len(e))):
                sub = find_diffs(a[i], e[i], f"{path}[{i}]")
                diffs.extend(sub)
                if len(diffs) > 20:
                    return diffs
            if len(a) > len(e):
                for i in range(len(e), len(a)):
                    diffs.append(f"{path}[{i}]: EXTRA {json.dumps(a[i], ensure_ascii=False)[:60]}")
            elif len(e) > len(a):
                for i in range(len(a), len(e)):
                    diffs.append(f"{path}[{i}]: MISSING {json.dumps(e[i], ensure_ascii=False)[:60]}")
        elif a != e:
            diffs.append(f"{path}: {json.dumps(a, ensure_ascii=False)[:40]} vs {json.dumps(e, ensure_ascii=False)[:40]}")
        return diffs

    diffs = find_diffs(actual, expected, f"test{n}")
    for d in diffs[:15]:
        print(f"  {d}")
    if len(diffs) > 15:
        print(f"  ... ({len(diffs)} total diffs)")
    print()

