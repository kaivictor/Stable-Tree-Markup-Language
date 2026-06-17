package stml;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

public class STMLStreamer {
    private StringBuilder buffer = new StringBuilder();
    private boolean finished = false;
    private List<Warning> warnings = new ArrayList<>();
    private Consumer<AstNode> docCallback;
    private List<AstNode> storedDocs = new ArrayList<>();

    public void onDocument(Consumer<AstNode> cb) {
        docCallback = cb;
    }

    public void feed(String chunk) {
        if (finished) return;
        buffer.append(chunk);
        processCompleteDocuments();
    }

    public void finish() {
        if (finished) return;
        finished = true;

        String bufStr = buffer.toString();
        if (!bufStr.isEmpty() || warnings.isEmpty()) {
            STML.LoadResult result = STML.loads(bufStr);
            warnings.addAll(result.warnings);
            if (result.ast.isMap()) {
                AstNode docs = AstNode.mapFind(result.ast.asMap(), "docs");
                if (docs != null && docs.isList()) {
                    for (AstNode doc : docs.asList()) {
                        if (docCallback != null) docCallback.accept(doc);
                    }
                }
            }
        }
        buffer.setLength(0);
    }

    public STML.LoadResult finalizeStream() {
        List<AstNode> docs = new ArrayList<>(storedDocs);
        Consumer<AstNode> prevCb = docCallback;
        docCallback = docs::add;
        finish();
        docCallback = prevCb;
        return new STML.LoadResult(new AstNode(new ArrayList<>(List.of(new AstNode.Pair("docs", new AstNode(docs))))), new ArrayList<>(warnings));
    }

    public List<Warning> warnings() { return warnings; }

    private void processCompleteDocuments() {
        while (true) {
            String buf = buffer.toString();
            int sepPos = findDocumentSeparator(buf);
            if (sepPos == -1) break;

            String docText = sepPos > 0 ? buf.substring(0, sepPos) : "";

            int consumeEnd = sepPos + 3;
            if (consumeEnd < buf.length() && buf.charAt(consumeEnd) == '\r') consumeEnd++;
            if (consumeEnd < buf.length() && buf.charAt(consumeEnd) == '\n') consumeEnd++;
            buffer.delete(0, consumeEnd);

            if (!docText.isEmpty() || sepPos > 0) {
                STML.LoadResult result = STML.loads(docText);
                warnings.addAll(result.warnings);
                if (result.ast.isMap()) {
                    AstNode docs = AstNode.mapFind(result.ast.asMap(), "docs");
                    if (docs != null && docs.isList()) {
                        for (AstNode doc : docs.asList()) {
                            storedDocs.add(doc);
                            if (docCallback != null) docCallback.accept(doc);
                        }
                    }
                }
            }
        }
    }

    private static int findDocumentSeparator(String buf) {
        int pos = 0;
        int multilineDepth = 0;

        while (pos < buf.length()) {
            char ch = buf.charAt(pos);

            // Track multiline braces (unescaped, unquoted)
            if (ch == '\\' && pos + 1 < buf.length()) {
                pos += 2; // skip escaped char
                continue;
            }
            if (ch == '{') {
                multilineDepth++;
            } else if (ch == '}') {
                if (multilineDepth > 0) multilineDepth--;
            } else if (pos == 0 && buf.length() >= 4
                    && buf.charAt(0) == '-' && buf.charAt(1) == '-' && buf.charAt(2) == '-'
                    && buf.charAt(3) == '\n' && multilineDepth == 0) {
                return 0;
            }

            if (ch == '\n') {
                int next = pos + 1;
                if (next + 3 <= buf.length()
                        && buf.charAt(next) == '-' && buf.charAt(next + 1) == '-'
                        && buf.charAt(next + 2) == '-'
                        && multilineDepth == 0) {
                    if (next + 3 == buf.length() || buf.charAt(next + 3) == '\n'
                            || buf.charAt(next + 3) == '\r') {
                        return next;
                    }
                }
            }
            pos++;
        }

        return -1;
    }

    public static STML.LoadResult streamParse(String text) {
        STMLStreamer streamer = new STMLStreamer();
        streamer.feed(text);
        return streamer.finalizeStream();
    }
}