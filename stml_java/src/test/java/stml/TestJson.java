package stml;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public class TestJson {
    private String text;
    private int pos;
    private int len;

    public TestJson(String text) {
        this.text = text;
        this.pos = 0;
        this.len = text.length();
    }

    public AstNode parseValue() {
        skipWs();
        if (pos >= len) throw new RuntimeException("Unexpected EOF");
        char ch = text.charAt(pos);
        if (ch == '"') return parseString();
        if (ch == '{') return parseObject();
        if (ch == '[') return parseArray();
        if (ch == 'n') { expect("null"); return new AstNode(); }
        if (ch == 't' || ch == 'f') return parseString();
        if (ch == '-' || (ch >= '0' && ch <= '9')) return parseNumber();
        throw new RuntimeException("Unexpected char: " + ch);
    }

    private void skipWs() {
        while (pos < len) {
            char ch = text.charAt(pos);
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') pos++;
            else break;
        }
    }

    private AstNode parseString() {
        pos++; // skip opening "
        StringBuilder result = new StringBuilder();
        while (pos < len) {
            char ch = text.charAt(pos++);
            if (ch == '"') return new AstNode(result.toString());
            if (ch == '\\') {
                if (pos >= len) throw new RuntimeException("Unexpected EOF in escape");
                char esc = text.charAt(pos++);
                switch (esc) {
                    case '"': result.append('"'); break;
                    case '\\': result.append('\\'); break;
                    case '/': result.append('/'); break;
                    case 'n': result.append('\n'); break;
                    case 'r': result.append('\r'); break;
                    case 't': result.append('\t'); break;
                    case 'b': result.append('\b'); break;
                    case 'f': result.append('\f'); break;
                    case 'u': {
                        if (pos + 4 > len) throw new RuntimeException("Unexpected EOF in \\u");
                        String hex = text.substring(pos, pos + 4);
                        pos += 4;
                        int codepoint = Integer.parseInt(hex, 16);
                        result.append((char) codepoint);
                        break;
                    }
                    default: result.append('\\'); result.append(esc); break;
                }
            } else {
                result.append(ch);
            }
        }
        throw new RuntimeException("Unclosed string");
    }

    private AstNode parseObject() {
        pos++; // skip {
        List<AstNode.Pair> map = new ArrayList<>();
        skipWs();
        if (pos < len && text.charAt(pos) == '}') {
            pos++;
            return new AstNode(map, AstNode.Kind.MAP);
        }
        while (true) {
            skipWs();
            if (text.charAt(pos) != '"') throw new RuntimeException("Expected string key");
            AstNode keyNode = parseString();
            String key = keyNode.asString();
            skipWs();
            if (text.charAt(pos++) != ':') throw new RuntimeException("Expected ':'");
            skipWs();
            AstNode val = parseValue();
            map.add(new AstNode.Pair(key, val));
            skipWs();
            if (pos < len && text.charAt(pos) == ',') { pos++; continue; }
            if (text.charAt(pos) == '}') { pos++; break; }
            throw new RuntimeException("Expected ',' or '}' in object");
        }
        return new AstNode(map);
    }

    private AstNode parseArray() {
        pos++; // skip [
        List<AstNode> list = new ArrayList<>();
        skipWs();
        if (pos < len && text.charAt(pos) == ']') {
            pos++;
            return new AstNode(list, AstNode.Kind.LIST);
        }
        while (true) {
            skipWs();
            list.add(parseValue());
            skipWs();
            if (pos < len && text.charAt(pos) == ',') { pos++; continue; }
            if (text.charAt(pos) == ']') { pos++; break; }
            throw new RuntimeException("Expected ',' or ']' in array");
        }
        return new AstNode(list);
    }

    private AstNode parseNumber() {
        int start = pos;
        if (text.charAt(pos) == '-') pos++;
        while (pos < len && Character.isDigit(text.charAt(pos))) pos++;
        if (pos < len && text.charAt(pos) == '.') {
            pos++;
            while (pos < len && Character.isDigit(text.charAt(pos))) pos++;
        }
        if (pos < len && (text.charAt(pos) == 'e' || text.charAt(pos) == 'E')) {
            pos++;
            if (pos < len && (text.charAt(pos) == '+' || text.charAt(pos) == '-')) pos++;
            while (pos < len && Character.isDigit(text.charAt(pos))) pos++;
        }
        return new AstNode(text.substring(start, pos));
    }

    private void expect(String word) {
        for (int i = 0; i < word.length(); i++) {
            if (pos >= len || text.charAt(pos++) != word.charAt(i))
                throw new RuntimeException("Expected " + word);
        }
    }

    public static AstNode parse(String jsonText) {
        return new TestJson(jsonText).parseValue();
    }

    public static AstNode load(String filepath) throws IOException {
        String text = Files.readString(Path.of(filepath));
        return parse(text);
    }
}