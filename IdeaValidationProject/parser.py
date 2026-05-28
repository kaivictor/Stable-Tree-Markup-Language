"""STML Parser: Token stream → AST + Warning list.

Consumes the token list produced by STMLLexer and builds the AST
(Python dict/list/str/None).  All structural decisions (indentation,
block type, complex-sequence mode) are driven by the token types.

C++ compatible: index-pointer traversal, explicit stacks, no generators.
"""

from typing import Any, Dict, List, Optional, Tuple

from .token_types import Token, TokenType, Warning


# Token types that represent a value (may appear after COLON or DASH)
_VALUE_TOKENS = frozenset({
    TokenType.SCALAR, TokenType.NULL, TokenType.RAW_STRING,
    TokenType.INLINE_LIST, TokenType.MULTILINE_STRING,
})


class STMLParser:
    """Recursive-descent parser operating on a flat token list.

    INDENT / DEDENT tokens drive block nesting.  The parser never
    computes indentation values — it trusts the lexer's INDENT/DEDENT
    pairing.

    Two sequence-parsing modes:
      nested=True  – stop at the first DEDENT (block value under INDENT)
      nested=False – treat INDENT/DEDENT as internal structure;
                     stop at non-DASH content token (root or same-indent)
    """

    def __init__(self, tokens: List[Token]):
        self.tokens: List[Token] = tokens
        self.n: int = len(tokens)
        self.pos: int = 0
        self.warnings: List[Warning] = []

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def parse(self) -> Tuple[Dict[str, Any], List[Warning]]:
        """Parse the full token stream.

        Returns ({"docs": {"doc1": ..., ...}}, warnings).
        Always wraps, even for single-document files.
        """
        self.pos = 0
        self.warnings = []
        docs: Dict[str, Any] = {}
        doc_index = 1

        while self.pos < self.n:
            self._skip_newlines()

            if self.pos >= self.n:
                break

            ttype = self._peek_type()

            # Stop at EOF
            if ttype == TokenType.EOF:
                break

            # Consume trailing DEDENTs (remaining indent stack after last doc)
            if ttype == TokenType.DEDENT:
                self.pos += 1
                continue

            # Consume DOC_SEPARATOR between documents
            if ttype == TokenType.DOC_SEPARATOR:
                self.pos += 1
                self._skip_newlines()
                # Consecutive separators (with optional blank lines between)
                # create a null document (§4).
                if self.pos < self.n and self._peek_type() == TokenType.DOC_SEPARATOR:
                    docs[f'doc{doc_index}'] = None
                    doc_index += 1
                continue

            # Parse one document
            node, self.pos = self._parse_document(self.pos)
            docs[f'doc{doc_index}'] = node
            doc_index += 1

        if not docs:
            docs['doc1'] = None

        return {'docs': docs}, self.warnings

    # ------------------------------------------------------------------
    # Document-level
    # ------------------------------------------------------------------

    def _parse_document(self, pos: int) -> Tuple[Any, int]:
        """Parse a single document: dispatch to mapping or sequence."""
        pos = self._skip_newlines_from(pos)

        if pos >= self.n or self.tokens[pos].type in (TokenType.EOF,):
            return None, pos

        # Skip any leading DEDENTs (from previous document's indent stack)
        while pos < self.n and self.tokens[pos].type == TokenType.DEDENT:
            pos += 1

        if pos >= self.n or self.tokens[pos].type in (TokenType.EOF,):
            return None, pos

        # Handle root-level INDENT (first line may be indented)
        root_indented = False
        if self.tokens[pos].type == TokenType.INDENT:
            root_indented = True
            pos += 1

        node, pos = self._parse_node(pos)

        # Consume root INDENT's matching DEDENT(s)
        while root_indented and pos < self.n and self.tokens[pos].type == TokenType.DEDENT:
            pos += 1

        return node, pos

    # ------------------------------------------------------------------
    # Node dispatch
    # ------------------------------------------------------------------

    def _parse_node(self, pos: int) -> Tuple[Any, int]:
        """Dispatch on the current token type."""
        pos = self._skip_newlines_from(pos)

        if pos >= self.n:
            return None, pos

        t = self.tokens[pos]

        if t.type == TokenType.DASH:
            return self._parse_sequence(pos, nested=False)

        if t.type == TokenType.NULL:
            # Top-level null scalar (e.g. a document containing just "null")
            return None, pos + 1

        if t.type in (TokenType.KEY, TokenType.BARE_KEY):
            return self._parse_mapping(pos)

        # DEDENT / EOF / DOC_SEPARATOR / anything else → no node here
        return None, pos

    # ------------------------------------------------------------------
    # Mapping  (§9)
    # ------------------------------------------------------------------

    def _parse_mapping(self, pos: int) -> Tuple[Dict[str, Any], int]:
        """Parse a mapping block.

        Stops at DEDENT (with internal INDENT/DEDENT nesting counted),
        EOF, DOC_SEPARATOR, or block-type conflict (DASH as a new entry).
        """
        mapping: Dict[str, Any] = {}
        indent_nesting: int = 0   # balances internal INDENT/DEDENT pairs

        while pos < self.n:
            ttype = self._peek_type_at(pos)

            # Internal INDENT – increase nesting, skip
            if ttype == TokenType.INDENT:
                indent_nesting += 1
                pos += 1
                continue

            # DEDENT – decrease internal nesting, or check for re-indent
            if ttype == TokenType.DEDENT:
                if indent_nesting > 0:
                    indent_nesting -= 1
                    pos += 1
                    continue
                # DEDENT + INDENT at the same block level = entries at
                # a different (but still valid) indentation. Skip both.
                next_p = pos + 1
                while next_p < self.n and self.tokens[next_p].type == TokenType.NEWLINE:
                    next_p += 1
                if self._peek_type_at(next_p) == TokenType.INDENT:
                    pos = next_p + 1                # skip DEDENT, NEWLINEs, INDENT
                    continue
                break

            # Other block boundaries
            if ttype in (TokenType.EOF, TokenType.DOC_SEPARATOR):
                break

            if ttype == TokenType.NEWLINE:
                pos += 1
                continue

            # ----- Block type conflict: sequence item as a new entry -----
            if ttype == TokenType.DASH:
                self._add_warning_at(pos,
                                     "块类型冲突：映射块中出现序列条目 '-'，终止当前映射")
                break

            # ----- BARE_KEY -----
            if ttype == TokenType.BARE_KEY:
                key = self.tokens[pos].value
                pos += 1
                pos = self._consume_newline(pos)
                mapping[key] = None
                continue

            # ----- KEY : value -----
            if ttype != TokenType.KEY:
                pos += 1                              # skip unexpected
                continue

            key = self.tokens[pos].value
            pos += 1                                  # consume KEY

            # Expect COLON
            colon_ok = False
            if pos < self.n and self.tokens[pos].type == TokenType.COLON:
                colon_ok = True
                pos += 1                              # consume COLON

            if not colon_ok:
                # Key without colon → treat as bare
                mapping[key] = None
                pos = self._consume_newline(pos)
                continue

            # ---- Value after COLON ----
            nxt = self._peek_type_at(pos)

            if nxt in _VALUE_TOKENS:
                # Inline value
                val, pos = self._consume_value(pos)
                mapping[key] = val
                pos = self._consume_newline(pos)

            elif nxt == TokenType.NEWLINE:
                # Block value or null
                nl_pos = pos + 1                     # after NEWLINE
                nxt2 = self._peek_type_at(nl_pos)

                if nxt2 == TokenType.INDENT:
                    # Explicit INDENT block
                    pos = nl_pos + 1                  # consume NEWLINE + INDENT
                    val, pos = self._parse_node(pos)
                    if pos < self.n and self.tokens[pos].type == TokenType.DEDENT:
                        pos += 1                      # consume matching DEDENT
                    # Extend list-values with same-level sequence (§7)
                    if isinstance(val, list) and self._peek_type_at(pos) == TokenType.DASH:
                        seq, pos = self._parse_sequence(pos, nested=False)
                        val.extend(seq)
                    mapping[key] = val

                elif nxt2 == TokenType.DASH:
                    # Same-indent sequence as block value (§7)
                    pos = nl_pos                     # consume NEWLINE
                    val, pos = self._parse_sequence(pos, nested=False)
                    mapping[key] = val

                else:
                    # No block value → null
                    mapping[key] = None
                    pos = nl_pos

            elif nxt == TokenType.DASH:
                # Same-indent DASH immediately after COLON (no NEWLINE)
                # This shouldn't normally happen, but handle gracefully
                val, pos = self._parse_sequence(pos, nested=False)
                mapping[key] = val

            else:
                # DEDENT, EOF, or other → null
                mapping[key] = None

        return mapping, pos

    # ------------------------------------------------------------------
    # Sequence  (§13)
    # ------------------------------------------------------------------

    def _parse_sequence(self, pos: int, nested: bool) -> Tuple[List[Any], int]:
        """Parse a sequence block.

        *nested* controls the stop condition:
          True  – inside an INDENT block; stop at first DEDENT.
          False – root-level or same-indent; INDENT/DEDENT are internal
                  structure of complex items.  Stop at non-DASH content
                  token (KEY, BARE_KEY, DOC_SEPARATOR, EOF, or DEDENT
                  when internal depth ≤ 0).
        """
        complex_mode = self._is_complex_sequence(pos, nested)

        seq: List[Any] = []
        internal_depth = 0    # tracks INDENT/DEDENT for root-level sequences

        while pos < self.n:
            ttype = self._peek_type_at(pos)

            # ---- boundaries ----
            if ttype == TokenType.EOF or ttype == TokenType.DOC_SEPARATOR:
                break

            if ttype == TokenType.DEDENT:
                if nested:
                    # Check for re-indent (DEDENT + INDENT = same block)
                    next_p = pos + 1
                    while next_p < self.n and self.tokens[next_p].type == TokenType.NEWLINE:
                        next_p += 1
                    if self._peek_type_at(next_p) == TokenType.INDENT:
                        pos = next_p + 1               # skip DEDENT, NEWLINEs, INDENT
                        continue
                    break                               # first DEDENT ends nested seq
                if internal_depth > 0:
                    internal_depth -= 1
                    pos += 1
                    continue
                # Root-level: check for re-indent
                next_p = pos + 1
                while next_p < self.n and self.tokens[next_p].type == TokenType.NEWLINE:
                    next_p += 1
                if self._peek_type_at(next_p) == TokenType.INDENT:
                    pos = next_p + 1
                    continue
                break                            # parent-level DEDENT

            if ttype == TokenType.NEWLINE:
                pos += 1
                continue

            if ttype == TokenType.INDENT:
                if nested:
                    # INDENT inside a nested sequence shouldn't happen –
                    # the INDENT block was consumed as a sub-block value
                    pos += 1
                    continue
                internal_depth += 1
                pos += 1
                continue

            # ---- block type conflict / end of sequence ----
            if ttype != TokenType.DASH:
                if nested:
                    # Inside INDENT block: non-DASH is a real conflict
                    self._add_warning_at(pos,
                                         "块类型冲突：序列块中出现非 '-' 条目，终止当前序列")
                # Root-level: non-DASH (KEY/BARE_KEY/etc) naturally ends the sequence
                break

            # ---- one sequence entry ----
            item, pos = self._parse_sequence_item(pos, complex_mode)
            seq.append(item)

        return seq, pos

    def _parse_sequence_item(self, pos: int, complex_mode: bool
                             ) -> Tuple[Any, int]:
        """Parse a single DASH entry.

        Complex mode:
          DASH + KEY ...       → {key: value}     (inline mapping)
          DASH + SCALAR        → {scalar: null}
          DASH + NULL          → None
          DASH + [] INDENT     → {"": block}
          DASH + MULTILINE_STR → {text: null}
        Simple mode:
          DASH + SCALAR        → scalar
          DASH + NULL          → None
          DASH + [] INDENT     → block value (direct)
          DASH + INLINE_LIST   → list
        """
        # Expect DASH
        if pos >= self.n or self.tokens[pos].type != TokenType.DASH:
            return None, pos
        pos += 1                                       # consume DASH

        nxt = self._peek_type_at(pos)

        # ---- empty entry (DASH, then NEWLINE or nothing) ----
        if nxt == TokenType.NEWLINE:
            newline_pos = pos + 1
            nxt2 = self._peek_type_at(newline_pos)
            if nxt2 == TokenType.INDENT:
                # Empty entry with sub-block
                pos = newline_pos + 1                   # consume NEWLINE + INDENT
                block_val, pos = self._parse_node(pos)
                if pos < self.n and self.tokens[pos].type == TokenType.DEDENT:
                    pos += 1
                if complex_mode:
                    return {"": block_val}, pos
                else:
                    return block_val, pos
            # Empty entry, no sub-block → null
            return None, newline_pos

        # ---- NULL ----
        if nxt == TokenType.NULL:
            _, pos = self._consume_value(pos)
            pos = self._consume_newline(pos)
            return None, pos

        # ---- INLINE_LIST ----
        if nxt == TokenType.INLINE_LIST:
            val, pos = self._consume_value(pos)
            pos = self._consume_newline(pos)
            return val, pos

        # ---- KEY → inline mapping  (e.g. "- key: value") ----
        if nxt == TokenType.KEY:
            key = self.tokens[pos].value
            pos += 1                                   # consume KEY

            if pos < self.n and self.tokens[pos].type == TokenType.COLON:
                pos += 1                               # consume COLON

            after = self._peek_type_at(pos)

            if after in _VALUE_TOKENS:
                # Inline value: "- key: value"
                val, pos = self._consume_value(pos)
                pos = self._consume_newline(pos)
                return {key: val}, pos

            if after == TokenType.NEWLINE:
                newline_pos = pos + 1
                after2 = self._peek_type_at(newline_pos)
                if after2 == TokenType.INDENT:
                    # Block value
                    pos = newline_pos + 1              # consume NEWLINE + INDENT
                    block_val, pos = self._parse_node(pos)
                    if pos < self.n and self.tokens[pos].type == TokenType.DEDENT:
                        pos += 1
                    return {key: block_val}, pos
                elif after2 == TokenType.DASH:
                    # Same-indent sequence as block value (§7)
                    pos = newline_pos
                    block_val, pos = self._parse_sequence(pos, nested=False)
                    return {key: block_val}, pos
                else:
                    # No block, no inline → null
                    return {key: None}, newline_pos

            # DEDENT, EOF, etc.
            return {key: None}, pos

        # ---- SCALAR, RAW_STRING ----
        if nxt in (TokenType.SCALAR, TokenType.RAW_STRING):
            val, pos = self._consume_value(pos)
            pos = self._consume_newline(pos)
            if complex_mode:
                return {val: None}, pos
            else:
                return val, pos

        # ---- MULTILINE_STRING ----
        if nxt == TokenType.MULTILINE_STRING:
            val, pos = self._consume_value(pos)
            pos = self._consume_newline(pos)
            if complex_mode:
                return {val: None}, pos
            else:
                return val, pos

        # Fallback
        return None, pos

    # ------------------------------------------------------------------
    # Complex-sequence pre-scan  (§13.3)
    # ------------------------------------------------------------------

    def _is_complex_sequence(self, start_pos: int, nested: bool) -> bool:
        """Look ahead in the token stream for any DASH entry that has
        a KEY (colon on the source line) or is empty with a sub-block
        (NEWLINE INDENT).

        *nested* works the same as in _parse_sequence.
        """
        pos = start_pos
        depth = 0        # INDENT/DEDENT nesting for root sequences

        while pos < self.n:
            t = self.tokens[pos]

            if t.type == TokenType.INDENT:
                if nested:
                    pos += 1
                    continue
                depth += 1
                pos += 1
                continue

            if t.type == TokenType.DEDENT:
                if nested:
                    break
                if depth > 0:
                    depth -= 1
                    pos += 1
                    continue
                break

            if t.type in (TokenType.EOF, TokenType.DOC_SEPARATOR):
                break

            # Non-DASH content tokens end the sequence (same as _parse_sequence)
            if not nested and t.type in (TokenType.KEY, TokenType.BARE_KEY):
                break

            if t.type == TokenType.DASH:
                # Examine the token immediately after this DASH
                # (skip any NEWLINEs between DASH and the content)
                scan = pos + 1
                while scan < self.n:
                    st = self.tokens[scan]
                    if st.type == TokenType.NEWLINE:
                        scan += 1
                        continue
                    if st.type == TokenType.KEY:
                        return True               # DASH + KEY → inline mapping
                    if st.type == TokenType.INDENT:
                        return True               # DASH + INDENT → sub-block
                    break                         # DASH + SCALAR/NULL/etc
                pos = scan                         # advance past this entry
                continue

            # Skip any other token (KEY, SCALAR, etc. inside an entry)
            pos += 1

        return False

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _peek_type(self) -> Optional[TokenType]:
        """Token type at current self.pos, or None."""
        if self.pos >= self.n:
            return None
        return self.tokens[self.pos].type

    def _peek_type_at(self, pos: int) -> Optional[TokenType]:
        """Token type at *pos*, or None."""
        if pos >= self.n:
            return None
        return self.tokens[pos].type

    def _skip_newlines(self) -> None:
        """Advance self.pos past NEWLINE tokens."""
        while self.pos < self.n and self.tokens[self.pos].type == TokenType.NEWLINE:
            self.pos += 1

    def _skip_newlines_from(self, pos: int) -> int:
        """Return position after skipping NEWLINEs."""
        while pos < self.n and self.tokens[pos].type == TokenType.NEWLINE:
            pos += 1
        return pos

    def _consume_newline(self, pos: int) -> int:
        """Consume a NEWLINE if present at *pos*."""
        if pos < self.n and self.tokens[pos].type == TokenType.NEWLINE:
            return pos + 1
        return pos

    def _consume_value(self, pos: int) -> Tuple[Any, int]:
        """Consume a value token and return (value, new_pos)."""
        if pos >= self.n:
            return None, pos
        t = self.tokens[pos]
        if t.type == TokenType.SCALAR:
            return t.value, pos + 1
        if t.type == TokenType.NULL:
            return None, pos + 1
        if t.type == TokenType.RAW_STRING:
            return t.value, pos + 1
        if t.type == TokenType.INLINE_LIST:
            return t.value, pos + 1
        if t.type == TokenType.MULTILINE_STRING:
            return t.value, pos + 1
        return None, pos

    def _add_warning_at(self, pos: int, message: str) -> None:
        """Record a parser-level warning with source location."""
        if pos < self.n:
            t = self.tokens[pos]
            self.warnings.append(Warning(t.line, t.column, message))
        else:
            self.warnings.append(Warning(0, 0, message))
