/// STML AST — Abstract Syntax Tree types for JavaScript
///
/// AstNode types: null, string, list, map
/// AstMap is an ordered array of [key, value] pairs
/// AstList is an ordered array of AstNode

const TYPE_NULL   = 'null';
const TYPE_STRING = 'string';
const TYPE_LIST   = 'list';
const TYPE_MAP    = 'map';

class AstNode {
  constructor(type, value) {
    this._type = type;
    this._value = value;
  }

  // ---- type queries ----
  isNull()   { return this._type === TYPE_NULL; }
  isString() { return this._type === TYPE_STRING; }
  isList()   { return this._type === TYPE_LIST; }
  isMap()    { return this._type === TYPE_MAP; }

  // ---- accessors ----
  asString() { return this.isString() ? this._value : null; }
  asList()   { return this.isList() ? this._value : null; }
  asMap()    { return this.isMap() ? this._value : null; }
}

// ---- factory functions ----
function nullNode()           { return new AstNode(TYPE_NULL, null); }
function stringNode(s)        { return new AstNode(TYPE_STRING, s); }
function listNode(lst)        { return new AstNode(TYPE_LIST, lst); }
function mapNode(map)         { return new AstNode(TYPE_MAP, map); }

// ---- map helpers ----
function mapFind(map, key) {
  for (const [k, v] of map) {
    if (k === key) return v;
  }
  return null;
}

function mapSet(map, key, value) {
  for (let i = 0; i < map.length; i++) {
    if (map[i][0] === key) {
      map[i][1] = value;
      return;
    }
  }
  map.push([key, value]);
}

// ---- deep clone ----
function clone(node) {
  if (node.isNull()) return nullNode();
  if (node.isString()) return stringNode(node.asString());
  if (node.isList()) {
    return listNode(node.asList().map(item => clone(item)));
  }
  if (node.isMap()) {
    return mapNode(node.asMap().map(([k, v]) => [k, clone(v)]));
  }
  return nullNode();
}

// ---- deep equality ----
function astEqual(a, b) {
  if (a.isNull() && b.isNull()) return true;
  if (a.isString() && b.isString()) return a.asString() === b.asString();
  if (a.isList() && b.isList()) {
    const la = a.asList(), lb = b.asList();
    if (la.length !== lb.length) return false;
    for (let i = 0; i < la.length; i++) {
      if (!astEqual(la[i], lb[i])) return false;
    }
    return true;
  }
  if (a.isMap() && b.isMap()) {
    const ma = a.asMap(), mb = b.asMap();
    if (ma.length !== mb.length) return false;
    for (let i = 0; i < ma.length; i++) {
      if (ma[i][0] !== mb[i][0]) return false;
      if (!astEqual(ma[i][1], mb[i][1])) return false;
    }
    return true;
  }
  return false;
}

// ---- walk ----
function walk(node, visitor, path = []) {
  if (node.isNull() || node.isString()) {
    visitor([...path], node);
  } else if (node.isList()) {
    node.asList().forEach((item, i) => {
      path.push(String(i));
      walk(item, visitor, path);
      path.pop();
    });
  } else if (node.isMap()) {
    node.asMap().forEach(([k, v]) => {
      path.push(k);
      walk(v, visitor, path);
      path.pop();
    });
  }
}

module.exports = {
  AstNode,
  nullNode, stringNode, listNode, mapNode,
  mapFind, mapSet,
  clone, astEqual, walk,
};