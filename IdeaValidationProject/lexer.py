"""STML Lexer: character stream → Token list + Warning list.

All deterministic recovery rules live here.  The lexer is designed
so that the resulting token stream is fully determined for any input.
No C++-incompatible features (generators, duck typing, GC-dependent patterns).
"""

from typing import Any, List, Optional, Tuple

from .token_types import Token, TokenType, Warning


class STMLLexer:
    """Converts STML text into a token stream with source-location tracking.

    Architecture:
        tokenize() is the single entry point.  It reads all lines, manages
        an indentation stack, and dispatches each content line to specialised
        handlers (sequence-entry / mapping-entry / etc.).

    C++ mapping:
        - `self.lines`    → `std::vector<std::string>`
        - `self.tokens`   → `std::vector<Token>`
        - `self.warnings` → `std::vector<Warning>`
        - `indent_stack`  → `std::vector<int>`
    """

    def __init__(self, text: str):
        # Normalise line endings, split, preserve raw lines for multiline
        normalised = text.replace('\r\n', '\n').replace('\r', '\n')
        self.lines: List[str] = normalised.split('\n')
        self.line_count: int = len(self.lines)

        # Accumulators (C++: vectors)
        self.tokens: List[Token] = []
        self.warnings: List[Warning] = []

        # Indentation stack: starts with implicit root at indent 0
        self.indent_stack: List[int] = [0]

        # Current processing position in self.lines
        self._line_idx: int = 0

        # Deferred DEDENT target: set by multiline-string parsing when the
        # closing '}' line is consumed.  The main loop emits the DEDENT(s)
        # at the correct position (between lines, not mid-line).
        self._deferred_dedent_to: Optional[int] = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def tokenize(self) -> Tuple[List[Token], List[Warning]]:
        """Run the full lexer and return (tokens, warnings)."""
        self.tokens = []
        self.warnings = []
        self.indent_stack = [0]
        self._line_idx = 0
        self._deferred_dedent_to = None

        while self._line_idx < self.line_count:
            line = self.lines[self._line_idx]
            indent = self._calc_indent(line)
            content = line[indent:]

            # Skip empty lines and whole-line comments (transparent to indent)
            if self._is_blank_or_comment(content):
                self._line_idx += 1
                continue

            # Document separator (indent must be 0)
            if indent == 0 and content.strip() == '---':
                self._emit_dedents_to(0)
                self._add_token(TokenType.DOC_SEPARATOR, '---',
                                self._line_idx + 1, indent + 1)
                self._line_idx += 1
                continue

            # Process indentation changes before the content of this line
            self._process_indent(indent)

            # Dispatch on line type
            # `---` at non-root level is plain content, not a sequence entry
            if content.strip() == '---' and indent > 0:
                self._lex_mapping_line(content, indent)
            elif content.startswith('-'):
                self._lex_sequence_line(content, indent)
            else:
                self._lex_mapping_line(content, indent)

            self._line_idx += 1

            # Emit deferred DEDENTs from a multiline-string closure.
            # The closing '}' line was consumed by _read_multiline without
            # _process_indent seeing it, so we must emit DEDENTs here
            # (between lines, the correct position for indent tokens).
            if self._deferred_dedent_to is not None:
                target = self._deferred_dedent_to
                self._deferred_dedent_to = None
                self._emit_dedents_to(target)

        # EOF: pop remaining indentation levels → DEDENTs
        self._emit_dedents_to(0)
        self._add_token(TokenType.EOF, None, self.line_count, 1)

        return self.tokens, self.warnings

    # ------------------------------------------------------------------
    # Indentation helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _calc_indent(line: str) -> int:
        """Count leading spaces (U+0020).  Tabs and other chars stop counting."""
        n = 0
        for ch in line:
            if ch == ' ':
                n += 1
            else:
                break
        return n

    @staticmethod
    def _is_blank_or_comment(content: str) -> bool:
        """True when *content* is empty or a whole-line comment."""
        stripped = content.lstrip(' ')
        return stripped == '' or stripped.startswith('#')

    def _process_indent(self, indent: int) -> None:
        """Compare *indent* with the stack and emit INDENT / DEDENT tokens.

        When indent changes but stays above the parent, we emit a DEDENT
        to the previous level followed by an INDENT to the new level
        (the spec allows entries at different indentations within the same block).
        """
        top = self.indent_stack[-1]
        if indent > top:
            self.indent_stack.append(indent)
            self._add_token(TokenType.INDENT, indent - top,
                            self._line_idx + 1, 1)
        elif indent < top:
            self._emit_dedents_to(indent)
            # If indent > new top, emit INDENT to re-enter at the new level
            if indent > self.indent_stack[-1]:
                diff = indent - self.indent_stack[-1]
                self.indent_stack.append(indent)
                self._add_token(TokenType.INDENT, diff,
                                self._line_idx + 1, 1)
        # indent == top → nothing to do

    def _emit_dedents_to(self, target: int) -> None:
        """Pop stack until top <= *target*, emitting a DEDENT for each pop."""
        while self.indent_stack[-1] > target:
            self.indent_stack.pop()
            self._add_token(TokenType.DEDENT, None,
                            self._line_idx + 1, 1)

    # ------------------------------------------------------------------
    # Token factory
    # ------------------------------------------------------------------

    def _add_token(self, ttype: TokenType, value: Any,
                   line: int, column: int) -> None:
        self.tokens.append(Token(ttype, value, line, column))

    def _add_warning(self, line: int, column: int, message: str) -> None:
        self.warnings.append(Warning(line, column, message))

    def _col(self, indent: int, offset_in_content: int) -> int:
        """Convert a 0-based offset within *content* to 1-based column."""
        return indent + offset_in_content + 1

    # ==================================================================
    # Line-level dispatch
    # ==================================================================

    def _lex_sequence_line(self, content: str, indent: int) -> None:
        """Process a line starting with '-'.

        Emits: DASH [value-or-key-tokens...]  NEWLINE
        """
        line_no = self._line_idx + 1
        self._add_token(TokenType.DASH, None, line_no,
                        self._col(indent, 0))

        rest = content[1:]                          # strip '-'
        if rest.startswith(' '):
            rest = rest[1:]                          # strip optional space

        if rest == '':
            # Empty sequence entry – parser decides block vs null
            self._add_token(TokenType.NEWLINE, None, line_no,
                            self._col(indent, len(content)))
            return

        # Sequence entries scan for inline mapping colon (§13.2.4)
        self._lex_value_or_key_on_line(rest, indent, scan_colon=True)
        self._add_token(TokenType.NEWLINE, None, line_no,
                        self._col(indent, len(content)))

    def _lex_mapping_line(self, content: str, indent: int) -> None:
        """Process a mapping line (not starting with '-').

        Emits one of:
            KEY COLON [value-tokens] NEWLINE
            BARE_KEY NEWLINE
        """
        line_no = self._line_idx + 1

        # (1) Try quoted-key mode  (§6.2)
        if content.startswith('"'):
            key, rest, ok, colon_pos = self._try_quoted_key(content, indent)
            if ok:
                self._add_token(TokenType.KEY, key, line_no,
                                self._col(indent, 0))
                self._add_token(TokenType.COLON, None, line_no,
                                self._col(indent, colon_pos))
                self._lex_remainder_after_colon(rest, indent, line_no)
                self._add_token(TokenType.NEWLINE, None, line_no,
                                self._col(indent, len(content)))
                return

        # (2) Scan for structural colon  (§5)
        colon = self._find_unquoted_colon(content)

        if colon == -1:
            # No structural colon → may be bare-key or null-scalar
            stripped = content.strip()
            if stripped in ('null', '~'):
                # Entire line is null → NULL token (not a key named "null")
                self._add_token(TokenType.NULL, None, line_no,
                                self._col(indent, 0))
                self._add_token(TokenType.NEWLINE, None, line_no,
                                self._col(indent, len(content)))
                return
            key = self._bare_key_from_content(content)
            self._add_token(TokenType.BARE_KEY, key, line_no,
                            self._col(indent, 0))
            self._add_token(TokenType.NEWLINE, None, line_no,
                            self._col(indent, len(content)))
            return

        if colon == 0:
            # Colon at position 0 → empty key → treat as BARE_KEY
            # Process rest as value then reconstruct the bare-key text
            key = self._reconstruct_empty_key(content)
            self._add_token(TokenType.BARE_KEY, key, line_no,
                            self._col(indent, 0))
            self._add_token(TokenType.NEWLINE, None, line_no,
                            self._col(indent, len(content)))
            return

        # (3) Normal key: content before colon
        key = content[:colon]                       # preserve trailing spaces (§9.1)
        rest = content[colon + 1:]

        self._add_token(TokenType.KEY, key, line_no,
                        self._col(indent, 0))
        self._add_token(TokenType.COLON, None, line_no,
                        self._col(indent, colon))
        self._lex_remainder_after_colon(rest, indent, line_no)
        self._add_token(TokenType.NEWLINE, None, line_no,
                        self._col(indent, len(content)))

    # ==================================================================
    # Value parsing (inline content after COLON or DASH)
    # ==================================================================

    def _lex_remainder_after_colon(self, rest: str, indent: int,
                                   line_no: int) -> None:
        """Parse the value portion after ':' in a mapping entry.

        Mapping values do NOT recursively scan for colons – the value
        is treated as a plain scalar (or quoted value / inline list / multiline).
        """
        if rest.startswith(' ') or rest.startswith('\t'):
            rest = rest[1:]                          # consume optional single space or tab

        if rest == '':
            return  # no inline value – parser checks for INDENT (block) or null

        self._lex_value_or_key_on_line(rest, indent, scan_colon=False)

    def _lex_value_or_key_on_line(self, text: str, indent: int,
                                  scan_colon: bool = False) -> None:
        """Parse inline content as a value.

        When *scan_colon* is True (sequence entries only), additionally scan
        for inline-mapping KEY:COLON patterns.  When False (mapping values),
        treat the text as a plain scalar.
        """
        line_no = self._line_idx + 1

        # Multiline string trigger  (§11)
        if text.strip() == '{' and self._line_idx + 1 < self.line_count:
            ml_value, consumed, close_indent = self._read_multiline(indent)
            self._add_token(TokenType.MULTILINE_STRING, ml_value,
                            line_no, self._col(indent, indent))
            self._line_idx = consumed - 1  # -1 because caller will +=1
            # Defer DEDENT emission to the main loop — emitting tokens here
            # would place them mid-line in the token stream, confusing the parser.
            self._deferred_dedent_to = close_indent
            return

        # Inline list  (§10)
        # Only triggers when the line contains BOTH '[' and ']'.
        # A lone '[' (without matching ']' on the same line) is just text.
        if text.startswith('[') and text.rstrip().endswith(']'):
            elems, ok, col = self._parse_inline_list(text, indent)
            if ok:
                self._add_token(TokenType.INLINE_LIST, elems,
                                line_no, self._col(indent, 0))
                return
            # Malformed inline list → keep as raw string
            self._add_warning(line_no, col or self._col(indent, 0),
                              "内联列表缺少结尾 ']'，降级为原始字符串")
            self._add_token(TokenType.RAW_STRING, text,
                            line_no, self._col(indent, 0))
            return

        # Value starts with '[' but lacks ']' on the same line → malformed
        if text.startswith('['):
            self._add_warning(line_no, self._col(indent, 0),
                              "内联列表缺少结尾 ']'，降级为原始字符串")
            self._add_token(TokenType.RAW_STRING, text,
                            line_no, self._col(indent, 0))
            return

        # Quoted content → for sequence entries, try quoted-key first (§13.2.4);
        # otherwise parse as greedy quoted value (§6.3).
        if text.startswith('"'):
            if scan_colon:
                # Non-greedy: try "key": value pattern inside sequence entry
                key, rest, ok, colon_pos = self._try_quoted_key(text, indent)
                if ok:
                    self._add_token(TokenType.KEY, key, line_no,
                                    self._col(indent, 0))
                    self._add_token(TokenType.COLON, None, line_no,
                                    self._col(indent, colon_pos))
                    consumed = len(text) - len(rest)
                    if rest.startswith(' '):
                        rest = rest[1:]
                    self._lex_value_or_key_on_line(rest,
                                                   indent + consumed,
                                                   scan_colon=True)
                    return
                # Non-greedy key failed → fall through to greedy value
            # Greedy quoted value
            val, end, ok = self._parse_quoted_value(text, 0, indent)
            if ok:
                self._add_token(TokenType.SCALAR, val,
                                line_no, self._col(indent, 0))
            else:
                self._add_warning(line_no, self._col(indent, 0),
                                  "引号未闭合，降级为原始字符串")
                self._add_token(TokenType.RAW_STRING, text,
                                line_no, self._col(indent, 0))
            return

        # Unquoted value → for sequence entries, check for inline mapping key:value
        if scan_colon:
            colon = self._find_unquoted_colon(text)
            if colon >= 0:
                # Inline mapping: "key: value" inside a sequence entry
                inline_key = text[:colon]
                inline_rest = text[colon + 1:]
                if inline_rest.startswith(' '):
                    inline_rest = inline_rest[1:]
                self._add_token(TokenType.KEY, inline_key,
                                line_no, self._col(indent, 0))
                self._add_token(TokenType.COLON, None,
                                line_no, self._col(indent, len(inline_key)))
                self._lex_value_or_key_on_line(inline_rest,
                                               indent + len(inline_key) + 1,
                                               scan_colon=True)
                return

        # Plain scalar  (§12)
        self._lex_scalar_or_null(text, indent)

    def _lex_scalar_or_null(self, text: str, indent: int) -> None:
        """Strip, test for null/~, emit SCALAR or NULL.

        Empty string → emit nothing (parser infers null from NEWLINE).
        """
        line_no = self._line_idx + 1
        # Only strip spaces (U+0020) — tabs are content characters.
        # STML uses spaces for indentation, so leading/trailing spaces
        # around a scalar are semantically meaningless.
        stripped = text.strip(' ')
        if stripped == '':
            return   # empty → no token; parser handles via NEWLINE or context
        # For null/~ check, strip all whitespace (including tabs).
        if stripped.strip() in ('null', '~'):
            self._add_token(TokenType.NULL, None,
                            line_no, self._col(indent, 0))
        else:
            self._add_token(TokenType.SCALAR, stripped,
                            line_no, self._col(indent, 0))

    # ==================================================================
    # Quoted-key attempt  (§6.2) – non-greedy
    # ==================================================================

    def _try_quoted_key(self, content: str, indent: int) -> Tuple[str, str, bool, int]:
        """Try to parse *content* as a quoted key.

        Returns (key, rest, ok, colon_pos).  *ok* is True when a quoted key was
        successfully extracted (non-greedy, closing-quote immediately
        followed by ':').  *colon_pos* is the 0-based position of ':' in content.
        """
        end = self._find_first_unescaped_quote(content, 1)
        if end == -1:
            return "", "", False, -1
        # Must be immediately followed by ':'  (no space allowed)
        if end + 1 >= len(content) or content[end + 1] != ':':
            return "", "", False, -1
        raw_key = content[1:end]
        key = self._unescape(raw_key, indent + 1)
        rest = content[end + 2:]                    # after quote + ':'
        return key, rest, True, end + 1             # colon_pos = end + 1

    # ==================================================================
    # Structural colon scan  (§5)
    # ==================================================================

    def _find_unquoted_colon(self, s: str) -> int:
        """Return 0-based position of the first ':' not inside quotes,
        or -1 when none exists.

        State machine: \\ skips 2 chars, \" flips in_quote.
        """
        in_quote = False
        had_close = False
        i = 0
        n = len(s)
        while i < n:
            ch = s[i]
            if ch == '\\':
                i += 2
                continue
            if ch == '"':
                if in_quote:
                    had_close = True
                in_quote = not in_quote
            elif ch == ':' and not in_quote:
                return i
            i += 1

        # Unclosed quote: when a quote was *never* closed the opening '"'
        # is probably a key-name character, not a string delimiter.
        if in_quote and not had_close:
            return s.find(':')
        return -1

    # ==================================================================
    # Quote matching  (§6.1)
    # ==================================================================

    @staticmethod
    def _find_first_unescaped_quote(s: str, start: int) -> int:
        """Non-greedy: return position of first unescaped '\"' starting at *start*."""
        i = start
        n = len(s)
        while i < n:
            if s[i] == '\\':
                i += 2
                continue
            if s[i] == '"':
                return i
            i += 1
        return -1

    @staticmethod
    def _find_last_unescaped_quote(s: str, start: int) -> int:
        """Greedy: return position of last unescaped '\"' starting at *start*."""
        last = -1
        i = start
        n = len(s)
        while i < n:
            if s[i] == '\\':
                i += 2
                continue
            if s[i] == '"':
                last = i
            i += 1
        return last

    # ==================================================================
    # Escape processing  (§6.1)
    # ==================================================================

    def _unescape(self, s: str, col_offset: int = 0) -> str:
        """Process escape sequences in *s*.  Illegal escapes are kept as-is
        and generate warnings."""
        result: List[str] = []
        i = 0
        n = len(s)
        while i < n:
            if s[i] == '\\' and i + 1 < n:
                nxt = s[i + 1]
                if nxt == '"':
                    result.append('"')
                elif nxt == '\\':
                    result.append('\\')
                elif nxt == 'n':
                    result.append('\n')
                elif nxt == 't':
                    result.append('\t')
                else:
                    # Illegal escape – keep as-is  (§14)
                    result.append('\\')
                    result.append(nxt)
                    self._add_warning(
                        self._line_idx + 1,
                        col_offset + i + 1,
                        f"非法转义序列 '\\{nxt}'，保留原样"
                    )
                i += 2
            else:
                result.append(s[i])
                i += 1
        return ''.join(result)

    # ==================================================================
    # Quoted value  (§6.3) – greedy
    # ==================================================================

    def _parse_quoted_value(self, s: str, start: int, indent: int
                            ) -> Tuple[str, int, bool]:
        """Parse a quoted string value using GREEDY matching.

        Returns (value, end_pos, ok).  *ok* is False when no closing quote
        is found (caller emits RAW_STRING + warning).
        """
        end = self._find_last_unescaped_quote(s, start + 1)
        if end == -1:
            return "", start, False
        raw = s[start + 1:end]
        val = self._unescape(raw, indent + start + 1)
        return val, end + 1, True

    # ==================================================================
    # Inline list  (§10)
    # ==================================================================

    def _parse_inline_list(self, s: str, indent: int
                           ) -> Tuple[List[Any], bool, int]:
        """Parse [elem1, elem2, ...].

        Returns (elements, ok, error_column).  *ok* is False when the
        closing ']' is missing (caller emits RAW_STRING + warning).
        """
        # Find the matching ']' – the LAST character of *s* after stripping
        # is expected to be ']'.  If not, it's malformed.
        if not s.endswith(']'):
            # Try to find ']' in the string
            close_pos = s.find(']')
            if close_pos == -1:
                return [], False, indent + len(s)
            # If found but there's content after it, it's still malformed
            # We'll accept it if the rest after ']' is just whitespace
            after = s[close_pos + 1:].strip()
            if after:
                return [], False, indent + close_pos + 1 + len(after)
            # Accept: content only after ']' is whitespace

        # Find matching bracket
        close_pos = s.rfind(']')
        if close_pos == -1:
            return [], False, indent + len(s)

        inner = s[1:close_pos]
        elements: List[Any] = []
        i = 0
        n = len(inner)

        while i < n:
            # Skip leading spaces
            while i < n and inner[i] == ' ':
                i += 1
            if i >= n:
                break

            # Quoted element (non-greedy matching for list elements)
            if inner[i] == '"':
                eq = self._find_first_unescaped_quote(inner, i + 1)
                if eq != -1:
                    raw_elem = inner[i + 1:eq]
                    elements.append(self._unescape(raw_elem, indent + i + 2))
                    i = eq + 1
                else:
                    # Unclosed quote in element – take rest as raw
                    elements.append(inner[i:].strip())
                    i = n
                # Skip comma and spaces
                while i < n and inner[i] in (',', ' '):
                    i += 1
            else:
                # Unquoted element – stops at ',' or end
                start = i
                while i < n and inner[i] != ',':
                    i += 1
                elem = inner[start:i].strip()
                if elem in ('null', '~'):
                    elements.append(None)
                elif elem == '':
                    elements.append(None)                # empty → null
                else:
                    elements.append(elem)
                # Skip comma and spaces
                while i < n and inner[i] in (',', ' '):
                    i += 1

        # Trailing comma → extra null element
        if inner.rstrip().endswith(','):
            elements.append(None)

        return elements, True, 0

    # ==================================================================
    # Multiline string  (§11)
    # ==================================================================

    def _read_multiline(self, key_indent: int) -> Tuple[str, int, int]:
        """Read raw lines until a standalone '}' line with indent <= *key_indent*,
        or EOF.

        Returns (content, next_line_idx, close_indent).  EOF auto-closes (§14).

        *close_indent* is the indent of the closing ``}`` line (or *key_indent*
        for EOF auto-close).  The caller MUST emit DEDENT tokens for this indent
        change *at the position between lines* (in the main lexer loop), NOT
        inline during value-lexing.

        IMPORTANT: ``---`` at any indentation is ALWAYS treated as multiline
        content.  Once inside a ``{...}`` block, only a standalone ``}`` at the
        correct indentation (or EOF) closes it.
        """
        parts: List[str] = []
        i = self._line_idx + 1                          # start after '{' line
        close_indent = key_indent                        # default (EOF case)

        while i < self.line_count:
            line = self.lines[i]
            content = line.lstrip(' ') if line else ''
            line_indent = len(line) - len(content) if line else 0
            stripped = content.strip()

            # Standalone '}' at correct indentation → close
            if stripped == '}' and line_indent <= key_indent:
                close_indent = line_indent
                i += 1                                  # consume closing '}'
                break

            # Preserve the raw line (including original indentation)
            parts.append(line)
            i += 1
        else:
            # EOF reached – auto-close  (§14)
            self._add_warning(
                i, 1,
                "多行字符串未找到闭合 '}'，已由 EOF 自动闭合"
            )

        return '\n'.join(parts), i, close_indent

    # ==================================================================
    # Bare-key helpers
    # ==================================================================

    @staticmethod
    def _bare_key_from_content(content: str) -> str:
        """Extract bare-key text from *content*.

        When the entire line is surrounded by double quotes, the quotes
        are stripped (they were not matched as a quoted key, so they are
        decorative).
        """
        stripped = content.rstrip()
        if len(stripped) >= 2 and stripped[0] == '"' and stripped[-1] == '"':
            return stripped[1:-1] + content[len(stripped):]
        return content.rstrip()

    def _reconstruct_empty_key(self, content: str) -> str:
        """Reconstruct a bare key when colon is at position 0.

        Format: ':' [space] value
        The value part is parsed as a normal value (quotes stripped), then
        the key is reconstructed as ':' + processed_value.
        """
        rest = content[1:]                              # after ':'
        had_space = rest.startswith(' ')
        if had_space:
            rest = rest[1:]

        # Parse *rest* as a value to strip quotes
        if rest.startswith('"'):
            val, _, ok = self._parse_quoted_value(rest, 0, 0)
            if ok:
                processed = val
            else:
                processed = rest
        else:
            processed = rest.strip()

        if had_space:
            return ':' + ' ' + processed
        return ':' + processed
