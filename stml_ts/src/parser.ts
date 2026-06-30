/// STML Parser — converts token stream to AST
/// Ported from stml_cpp/parser/line_tree_builder.cpp and ast_builder.cpp

import { nullNode, stringNode, listNode, mapNode, clone, AstNode } from './ast';
import { TokenType, Token, Warning } from './lexer';

// ============================================================
// Line — intermediate representation
// ============================================================
export enum LineKind {
  DASH_EMPTY   = 'DASH_EMPTY',
  DASH_SCALAR  = 'DASH_SCALAR',
  DASH_KEY_VAL = 'DASH_KEY_VAL',
  KEY_VAL      = 'KEY_VAL',
  BARE_KEY     = 'BARE_KEY',
}

export class Line {
  kind: LineKind;
  key: string;
  inlineValue: AstNode;
  indent: number;
  lineNo: number;
  children: Line[];

  constructor(kind: LineKind = LineKind.BARE_KEY) {
    this.kind = kind;
    this.key = '';
    this.inlineValue = nullNode();
    this.indent = 0;
    this.lineNo = 0;
    this.children = [];
  }

  isDash(): boolean {
    return this.kind === LineKind.DASH_EMPTY
        || this.kind === LineKind.DASH_SCALAR
        || this.kind === LineKind.DASH_KEY_VAL;
  }

  hasInlineValue(): boolean { return !this.inlineValue.isNull(); }
  hasChildren(): boolean    { return this.children.length > 0; }
}

// ============================================================
// LineTreeBuilder — Phase 1: Token stream → Line tree
// ============================================================
export class LineTreeBuilder {
  docs: Line[][] = [];
  private blocks: Line[][] = [[]];
  private indentStack: number[] = [0];
  private pending: Line | null = null;
  private hasPending: boolean = false;
  private pendingHasDash: boolean = false;
  private pendingHasColon: boolean = false;
  private pendingHasKey: boolean = false;
  private hasPushedDoc: boolean = false;
  private hasContent: boolean = false;
  warnings: Warning[] = [];

  reset(): void {
    this.docs = [];
    this.blocks = [[]];
    this.indentStack = [0];
    this.pending = null;
    this.hasPending = false;
    this.pendingHasDash = false;
    this.pendingHasColon = false;
    this.pendingHasKey = false;
    this.hasPushedDoc = false;
    this.hasContent = false;
    this.warnings = [];
  }

  process(tokens: Token[]): void {
    for (const t of tokens) {
      this.processToken(t);
    }
  }

  processToken(token: Token): void {
    switch (token.type) {
      case TokenType.INDENT: {
        this._finalizeLine();
        const delta = parseInt(token.value as string, 10);
        const absoluteIndent = this.indentStack[this.indentStack.length - 1] + delta;
        this.indentStack.push(absoluteIndent);
        this.blocks.push([]);
        break;
      }
      case TokenType.DEDENT:
        this._finalizeLine();
        this._closeIndentLevel();
        if (this.indentStack.length > 1) {
          this.indentStack.pop();
        }
        break;

      case TokenType.DASH:
        this._finalizeLine();
        this._startNewLine(LineKind.DASH_EMPTY);
        this.pendingHasDash = true;
        this.pendingHasColon = false;
        this.pendingHasKey = false;
        this.pending!.indent = this.indentStack[this.indentStack.length - 1];
        this.pending!.lineNo = token.line;
        break;

      case TokenType.KEY:
        if (this.pendingHasDash && this.pending!.kind === LineKind.DASH_EMPTY) {
          this.pending!.kind = LineKind.DASH_KEY_VAL;
          this.pending!.key = token.value as string;
          this.pendingHasKey = true;
          this.pendingHasColon = false;
        } else {
          this._finalizeLine();
          this._startNewLine(LineKind.KEY_VAL);
          this.pending!.key = token.value as string;
          this.pendingHasKey = true;
          this.pendingHasColon = false;
          this.pendingHasDash = false;
          this.pending!.indent = this.indentStack[this.indentStack.length - 1];
          this.pending!.lineNo = token.line;
        }
        break;

      case TokenType.COLON:
        this.pendingHasColon = true;
        break;

      case TokenType.SCALAR:
      case TokenType.NULL_:
      case TokenType.INLINE_LIST:
      case TokenType.MULTILINE_STRING:
      case TokenType.RAW_STRING:
        if (this.pendingHasDash && !this.pendingHasColon && !this.pendingHasKey) {
          this.pending!.kind = LineKind.DASH_SCALAR;
          this.pending!.inlineValue = this._tokenToAstValue(token);
        } else if (this.pendingHasColon) {
          this.pending!.inlineValue = this._tokenToAstValue(token);
        }
        break;

      case TokenType.BARE_KEY:
        if (this.pendingHasDash && this.pending!.kind === LineKind.DASH_EMPTY) {
          this.pending!.kind = LineKind.DASH_SCALAR;
          this.pending!.inlineValue = stringNode(token.value as string);
        } else {
          this._finalizeLine();
          this._startNewLine(LineKind.BARE_KEY);
          this.pending!.key = token.value as string;
          this.pendingHasKey = true;
          this.pendingHasDash = false;
          this.pendingHasColon = false;
          this.pending!.indent = this.indentStack[this.indentStack.length - 1];
          this.pending!.lineNo = token.line;
        }
        break;

      case TokenType.NEWLINE:
        this._finalizeLine();
        break;

      case TokenType.DOC_SEPARATOR:
        this._finalizeLine();
        while (this.blocks.length > 1) {
          this._closeIndentLevel();
        }
        if (this.blocks.length > 0) {
          this.docs.push(this.blocks[0]);
          this.hasPushedDoc = true;
          this.blocks[0] = [];
          this.hasContent = false;
          this.indentStack = [0];
        }
        break;

      case TokenType.END:
        this._finalizeLine();
        while (this.blocks.length > 1) {
          this._closeIndentLevel();
        }
        if (this.blocks.length > 0) {
          if (this.hasContent || this.hasPushedDoc) {
            this.docs.push(this.blocks[0]);
          }
        }
        break;
    }
  }

  private _finalizeLine(): void {
    if (!this.hasPending || !this.pending) return;

    if (this.pending.indent === 0 && this.blocks.length > 1) {
      this.pending.indent = this.indentStack[this.indentStack.length - 1];
    }

    if (!this.pending.isDash() && this.pending.kind === LineKind.BARE_KEY
        && this.blocks[this.blocks.length - 1].length > 0) {
      const block = this.blocks[this.blocks.length - 1];
      const blockHasDash = block.some(l => l.isDash());
      if (blockHasDash) {
        let lastDash: Line | null = null;
        for (let i = block.length - 1; i >= 0; i--) {
          if (block[i].isDash()) { lastDash = block[i]; break; }
        }
        if (lastDash) {
          lastDash.children.push(this.pending);
          this.pending = null;
          this.hasPending = false;
          this.pendingHasDash = false;
          this.pendingHasColon = false;
          this.pendingHasKey = false;
          return;
        }
      }
    }

    this.blocks[this.blocks.length - 1].push(this.pending);

    if (this.blocks.length <= 1) {
      this.hasContent = true;
    }

    this.pending = null;
    this.hasPending = false;
    this.pendingHasDash = false;
    this.pendingHasColon = false;
    this.pendingHasKey = false;
  }

  private _startNewLine(kind: LineKind): void {
    this.pending = new Line(kind);
    this.hasPending = true;
  }

  private _closeIndentLevel(): void {
    if (this.blocks.length <= 1) return;

    const child = this.blocks.pop()!;
    if (child.length === 0) return;

    const parent = this.blocks[this.blocks.length - 1];
    if (parent.length === 0) {
      this.blocks[this.blocks.length - 1] = child;
      return;
    }

    const parentHasDash = parent.some(l => l.isDash());
    const parentLast = parent[parent.length - 1];

    let childAllDash = true;
    let childHasKey = false;
    for (const line of child) {
      if (!line.isDash()) childAllDash = false;
      if (line.kind === LineKind.KEY_VAL || line.kind === LineKind.BARE_KEY) childHasKey = true;
    }

    const isOpen = (l: Line): boolean => {
      if (l.kind !== LineKind.KEY_VAL
          && l.kind !== LineKind.DASH_KEY_VAL
          && l.kind !== LineKind.BARE_KEY) return false;
      if (l.hasInlineValue()) return false;
      let hasK = false, hasD = false;
      for (const c of l.children) {
        if (c.isDash()) hasD = true;
        else hasK = true;
      }
      return !(hasK && hasD);
    };

    // Rule 1
    if (isOpen(parentLast)) {
      let parentChildrenAllDash = true;
      for (const c of parentLast.children) {
        if (!c.isDash()) { parentChildrenAllDash = false; break; }
      }
      const compatible = parentLast.children.length === 0
        || (parentChildrenAllDash && childAllDash)
        || (!parentChildrenAllDash && childHasKey);
      if (compatible) {
        for (const line of child) {
          parentLast.children.push(line);
        }
        return;
      }
    }

    // Rule 2
    if (parentHasDash && childHasKey) {
      let lastDash: Line | null = null;
      for (let i = parent.length - 1; i >= 0; i--) {
        if (parent[i].isDash()) { lastDash = parent[i]; break; }
      }
      if (lastDash) {
        for (const line of child) {
          lastDash.children.push(line);
        }
      }
      return;
    }

    // Rule 3
    if (childAllDash) {
      for (const line of child) {
        parent.push(line);
      }
      return;
    }

    // Rule 4
    if (!childAllDash && !parentHasDash) {
      for (const line of child) {
        parent.push(line);
      }
      return;
    }

    // Default: true nesting
    parentLast.children = child;
  }

  private _tokenToAstValue(t: Token): AstNode {
    switch (t.type) {
      case TokenType.NULL_:
        return nullNode();
      case TokenType.SCALAR:
      case TokenType.MULTILINE_STRING:
      case TokenType.RAW_STRING:
        return stringNode(t.value as string);
      case TokenType.INLINE_LIST: {
        const list: AstNode[] = [];
        for (const elem of (t.value as (string | null)[])) {
          if (elem !== null && elem !== undefined) {
            list.push(stringNode(elem));
          } else {
            list.push(nullNode());
          }
        }
        return listNode(list);
      }
      default:
        return nullNode();
    }
  }
}

// ============================================================
// AstBuilder — Phase 2: Line tree → AST
// ============================================================
export class AstBuilder {
  warnings_: Warning[] = [];

  buildAll(docLines: Line[][]): AstNode[] {
    return docLines.map(lines => this.build(lines));
  }

  build(lines: Line[]): AstNode {
    if (lines.length === 0) return nullNode();

    let allDash = true;
    for (const line of lines) {
      if (!line.isDash()) { allDash = false; break; }
    }

    if (allDash) {
      return this._buildSequence(lines);
    }

    const firstIsDash = lines.length > 0 && lines[0].isDash();
    if (firstIsDash) {
      return this._buildSequence(lines);
    }

    return mapNode(this._buildMapping(lines));
  }

  private _buildMapping(lines: Line[]): Array<[string, AstNode]> {
    const map: Array<[string, AstNode]> = [];
    let i = 0;

    while (i < lines.length) {
      const line = lines[i];
      if (!(line.kind === LineKind.KEY_VAL || line.kind === LineKind.BARE_KEY)) {
        i++;
        continue;
      }

      const key = line.key;
      let val: AstNode = nullNode();

      if (line.hasInlineValue()) {
        val = clone(line.inlineValue);
        map.push([key, val]);
        if (line.hasChildren()) {
          this._absorbChildrenAsSiblings(map, line.children);
        }
        i++;
        continue;
      } else if (line.hasChildren()) {
        val = this.build(line.children);
        i++;
        if (val.isList()) {
          let j = i;
          while (j < lines.length && lines[j].isDash()) j++;
          if (j > i) {
            const dashLines = lines.slice(i, j);
            const extra = this.build(dashLines);
            if (extra.isList()) {
              const valList = val.asList()!;
              const extraList = extra.asList()!;
              for (const item of extraList) {
                valList.push(item);
              }
            } else {
              val.asList()!.push(extra);
            }
            i = j;
          }
        }
      } else {
        let j = i + 1;
        while (j < lines.length && lines[j].isDash()) j++;
        if (j > i + 1) {
          const dashLines = lines.slice(i + 1, j);
          val = this.build(dashLines);
          i = j;
        } else {
          i++;
        }
      }

      map.push([key, val]);
    }

    return map;
  }

  private _buildSequence(lines: Line[]): AstNode {
    let isComplex = false;
    let hasNonDash = false;
    for (const line of lines) {
      if (line.kind === LineKind.DASH_KEY_VAL) isComplex = true;
      if (!line.isDash()) hasNonDash = true;
    }

    if (isComplex || hasNonDash) {
      return listNode(this._buildComplexSequence(lines));
    } else {
      return listNode(this._buildSimpleSequence(lines));
    }
  }

  private _buildSimpleSequence(lines: Line[]): AstNode[] {
    const seq: AstNode[] = [];

    for (const line of lines) {
      if (!line.isDash()) continue;

      if (line.kind === LineKind.DASH_EMPTY) {
        if (line.hasChildren()) {
          seq.push(nullNode());
          const child = this.build(line.children);
          if (child.isList()) {
            for (const item of child.asList()!) {
              seq.push(item);
            }
          } else {
            seq.push(child);
          }
        } else {
          seq.push(nullNode());
        }
      } else if (line.kind === LineKind.DASH_SCALAR) {
        if (line.hasChildren()) {
          const dashKids: Line[] = [];
          const nonDashKids: Line[] = [];
          for (const c of line.children) {
            if (c.isDash()) dashKids.push(c);
            else nonDashKids.push(c);
          }
          if (nonDashKids.length > 0) {
            this.warnings_.push(new Warning(line.lineNo, 1,
              'Dash scalar entry has unexpected child content, ignoring children'));
          }
          seq.push(clone(line.inlineValue));
          if (dashKids.length > 0) {
            const extra = this._buildSequence(dashKids);
            if (extra.isList()) {
              for (const item of extra.asList()!) {
                seq.push(item);
              }
            } else {
              seq.push(extra);
            }
          }
        } else {
          seq.push(clone(line.inlineValue));
        }
      }
    }

    return seq;
  }

  private _buildComplexSequence(lines: Line[]): AstNode[] {
    const seq: AstNode[] = [];
    let i = 0;

    while (i < lines.length) {
      if (!lines[i].isDash()) {
        let anyDashLeft = false;
        for (let k = i + 1; k < lines.length; k++) {
          if (lines[k].isDash()) { anyDashLeft = true; break; }
        }
        if (!anyDashLeft) break;
        i++;
        continue;
      }

      const entry: Array<[string, AstNode]> = [];
      const dashLine = lines[i];

      if (dashLine.kind === LineKind.DASH_KEY_VAL) {
        let val = dashLine.hasInlineValue() ? clone(dashLine.inlineValue) : nullNode();
        entry.push([dashLine.key, val]);

        if (!dashLine.hasInlineValue() && dashLine.hasChildren()) {
          entry[entry.length - 1][1] = this.build(dashLine.children);
        } else if (dashLine.hasInlineValue() && dashLine.hasChildren()) {
          const nonDashKids: Line[] = [];
          const dashKids: Line[] = [];
          for (const c of dashLine.children) {
            if (c.isDash()) dashKids.push(c);
            else nonDashKids.push(c);
          }
          if (nonDashKids.length > 0) {
            this._absorbChildrenAsSiblings(entry, nonDashKids);
          }
          seq.push(mapNode(entry));
          if (dashKids.length > 0) {
            const extra = this._buildSequence(dashKids);
            if (extra.isList()) {
              for (const item of extra.asList()!) {
                seq.push(item);
              }
            } else {
              seq.push(extra);
            }
          }
          i++;
          continue;
        }
      } else if (dashLine.kind === LineKind.DASH_SCALAR) {
        const key = dashLine.inlineValue.isString() ? dashLine.inlineValue.asString()! : '';
        entry.push([key, nullNode()]);

        if (dashLine.hasChildren()) {
          const nonDashKids: Line[] = [];
          const dashKids: Line[] = [];
          for (const c of dashLine.children) {
            if (c.isDash()) dashKids.push(c);
            else nonDashKids.push(c);
          }
          if (nonDashKids.length > 0) {
            this._absorbChildrenAsSiblings(entry, nonDashKids);
          }
          if (dashKids.length > 0) {
            seq.push(mapNode(entry));
            const extra = this._buildSequence(dashKids);
            if (extra.isList()) {
              for (const item of extra.asList()!) {
                seq.push(item);
              }
            } else {
              seq.push(extra);
            }
            i++;
            continue;
          }
        }
      } else if (dashLine.kind === LineKind.DASH_EMPTY) {
        if (dashLine.hasChildren()) {
          const child = this.build(dashLine.children);
          if (child.isMap()) {
            for (const [k, v] of child.asMap()!) {
              entry.push([k, v]);
            }
          } else if (child.isList()) {
            entry.push(['', child]);
          } else {
            entry.push(['', child]);
          }
        } else {
          seq.push(nullNode());
          i++;
          continue;
        }
      }

      // Collect sibling keys
      let j = i + 1;
      while (j < lines.length && !lines[j].isDash()) {
        const sibling = lines[j];
        if (sibling.kind === LineKind.KEY_VAL || sibling.kind === LineKind.BARE_KEY) {
          const key = sibling.key;
          let val: AstNode = nullNode();

          if (sibling.hasInlineValue()) {
            val = clone(sibling.inlineValue);
            if (sibling.hasChildren()) {
              this._absorbChildrenAsSiblings(entry, sibling.children);
            }
          } else if (sibling.hasChildren()) {
            val = this.build(sibling.children);
          }

          entry.push([key, val]);
        }
        j++;
      }

      seq.push(mapNode(entry));
      i = j;
    }

    if (i < lines.length) {
      const trailing = lines.slice(i);
      const extra = this.build(trailing);
      if (extra.isMap()) {
        seq.push(extra);
      } else if (extra.isList()) {
        for (const item of extra.asList()!) {
          seq.push(item);
        }
      }
    }

    return seq;
  }

  private _absorbChildrenAsSiblings(map: Array<[string, AstNode]>, children: Line[]): void {
    for (const child of children) {
      if (child.isDash()) {
        this.warnings_.push(new Warning(child.lineNo, 1,
          'Dash entry found in irregular indent context, skipping'));
        continue;
      }

      if (child.kind === LineKind.KEY_VAL || child.kind === LineKind.BARE_KEY) {
        const key = child.key;
        let val: AstNode = nullNode();

        if (child.hasInlineValue()) {
          val = clone(child.inlineValue);
        } else if (child.hasChildren()) {
          val = this.build(child.children);
        }

        map.push([key, val]);
      }
    }
  }
}