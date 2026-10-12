"""Reject NOTPORTED calls introduced only by registered preprocessing.

This is a conservative source guard, not a complete reachability analysis. Even
conditionally implemented compatibility calls (e.g. geninterrupt) count as stubs.
"""
from collections import Counter
from pathlib import Path
import re
import subprocess
import sys

compiler, directory = sys.argv[1:]
root = Path(directory)
# Discover remaining compatibility/assembly stubs rather than hard-code their names.
functions = re.compile(r'^\w[^\n;{}]*?\b(\w+)\s*\([^{};]*\)\s*\{', re.M)

def bodies(text):
    # Ignore braces in comments/strings, and handle nested blocks in new stubs.
    text = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', ' ', text, flags=re.S)
    for signature in functions.finditer(text):
        depth, end = 1, signature.end()
        while depth and end < len(text):
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        assert depth == 0, 'Unbalanced stub definition'
        yield signature[1], text[signature.end():end - 1]

stubs = {'WL_NotPorted'}
for path in (root / 'port').glob('*.c'):
    for name, body in bodies(path.read_text()):
        if re.search(r'\b(?:NOTPORTED|PORT_ASM)\s*\(', body):
            stubs.add(name)
assert {'geninterrupt', '_dos_gettime', 'SDL_SetDS'} <= stubs
calls = re.compile(r'\b(' + '|'.join(map(re.escape, sorted(stubs))) + r')\s*\(')
problems = []
for source in sorted((root / 'WOLFSRC').glob('*.C')):
    counts = []
    for defines in ([], ['-DWOLF3D_REGISTERED']):
        text = subprocess.check_output([compiler, '-E', '-P', '-x', 'c', '-std=gnu99',
                                       '-I' + str(root / 'port'), '-I' + str(root / 'WOLFSRC'),
                                       '-I' + str(root / 'audio'),
                                       *defines, str(source)], text=True)
        # Common declarations cancel out. Compare each stub separately so removal
        # of one stub cannot mask introduction of another.
        counts.append(Counter(calls.findall(text)))
    extra = counts[1] - counts[0]
    if extra:
        problems.append(f'{source.name}: registered-only stub references {dict(extra)}')
assert not problems, '\n'.join(problems)
print(f'PASS: no registered-only NOTPORTED paths in {len(list((root / "WOLFSRC").glob("*.C")))} sources')
