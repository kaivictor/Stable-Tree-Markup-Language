/// STML AST — Abstract Syntax Tree types for TypeScript

export type AstNodeType = 'null' | 'string' | 'list' | 'map';

/** An ordered key-value pair array. */
export type AstMap = Array<[string, AstNode]>;

/** An ordered list of AST nodes. */
export type AstList = AstNode[];

/** The core AST node — a discriminated union. */
export class AstNode {
  private _type: AstNodeType;
  private _value: null | string | AstNode[] | Array<[string, AstNode]>;

  constructor(type: AstNodeType, value: null | string | AstNode[] | Array<[string, AstNode]>) {
    this._type = type;
    this._value = value;
  }

  isNull(): boolean   { return this._type === 'null'; }
  isString(): boolean { return this._type === 'string'; }
  isList(): boolean   { return this._type === 'list'; }
  isMap(): boolean    { return this._type === 'map'; }

  asString(): string | null { return this.isString() ? (this._value as string) : null; }
  asList(): AstNode[] | null { return this.isList() ? (this._value as AstNode[]) : null; }
  asMap(): Array<[string, AstNode]> | null { return this.isMap() ? (this._value as Array<[string, AstNode]>) : null; }
}

// ---- Factory functions ----
export function nullNode(): AstNode           { return new AstNode('null', null); }
export function stringNode(s: string): AstNode { return new AstNode('string', s); }
export function listNode(lst: AstNode[]): AstNode { return new AstNode('list', lst); }
export function mapNode(map: Array<[string, AstNode]>): AstNode { return new AstNode('map', map); }

// ---- Map helpers ----
export function mapFind(map: Array<[string, AstNode]>, key: string): AstNode | null {
  for (const [k, v] of map) {
    if (k === key) return v;
  }
  return null;
}

export function mapSet(map: Array<[string, AstNode]>, key: string, value: AstNode): void {
  for (let i = 0; i < map.length; i++) {
    if (map[i][0] === key) {
      map[i][1] = value;
      return;
    }
  }
  map.push([key, value]);
}

// ---- Deep clone ----
export function clone(node: AstNode): AstNode {
  if (node.isNull()) return nullNode();
  if (node.isString()) return stringNode(node.asString()!);
  if (node.isList()) {
    return listNode(node.asList()!.map(item => clone(item)));
  }
  if (node.isMap()) {
    return mapNode(node.asMap()!.map(([k, v]) => [k, clone(v)]));
  }
  return nullNode();
}

// ---- Deep equality ----
export function astEqual(a: AstNode, b: AstNode): boolean {
  if (a.isNull() && b.isNull()) return true;
  if (a.isString() && b.isString()) return a.asString() === b.asString();
  if (a.isList() && b.isList()) {
    const la = a.asList()!, lb = b.asList()!;
    if (la.length !== lb.length) return false;
    for (let i = 0; i < la.length; i++) {
      if (!astEqual(la[i], lb[i])) return false;
    }
    return true;
  }
  if (a.isMap() && b.isMap()) {
    const ma = a.asMap()!, mb = b.asMap()!;
    if (ma.length !== mb.length) return false;
    for (let i = 0; i < ma.length; i++) {
      if (ma[i][0] !== mb[i][0]) return false;
      if (!astEqual(ma[i][1], mb[i][1])) return false;
    }
    return true;
  }
  return false;
}

// ---- Walk ----
export type AstVisitor = (path: string[], node: AstNode) => void;

export function walk(node: AstNode, visitor: AstVisitor, path: string[] = []): void {
  if (node.isNull() || node.isString()) {
    visitor([...path], node);
  } else if (node.isList()) {
    node.asList()!.forEach((item, i) => {
      path.push(String(i));
      walk(item, visitor, path);
      path.pop();
    });
  } else if (node.isMap()) {
    node.asMap()!.forEach(([k, v]) => {
      path.push(k);
      walk(v, visitor, path);
      path.pop();
    });
  }
}