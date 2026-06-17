package stml;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public class STML {

    public static class LoadResult {
        public final AstNode ast;
        public final List<Warning> warnings;
        public LoadResult(AstNode ast, List<Warning> warnings) {
            this.ast = ast;
            this.warnings = warnings;
        }
    }

    public static LoadResult loads(String text) {
        STMLLexer lexer = new STMLLexer(text);
        STMLLexer.TokenizeResult lexResult = lexer.tokenize();
        List<Token> tokens = lexResult.tokens;
        List<Warning> lexWarnings = lexResult.warnings;

        LineTreeBuilder treeBuilder = new LineTreeBuilder();
        treeBuilder.process(tokens);

        AstBuilder astBuilder = new AstBuilder();
        List<AstNode> docsList = new ArrayList<>();
        for (List<Line> docLines : treeBuilder.documents()) {
            docsList.add(astBuilder.build(docLines));
        }

        List<Warning> allWarnings = new ArrayList<>();
        allWarnings.addAll(lexWarnings);
        allWarnings.addAll(treeBuilder.warnings());
        allWarnings.addAll(astBuilder.warnings());

        List<AstNode.Pair> wrapper = new ArrayList<>();
        wrapper.add(new AstNode.Pair("docs", new AstNode(docsList)));
        return new LoadResult(new AstNode(wrapper), allWarnings);
    }

    public static LoadResult load(String filename) throws IOException {
        String text = Files.readString(Path.of(filename));
        return loads(text);
    }

    public static String dumps(AstNode ast) {
        return Serializer.dumps(ast);
    }

    public static String toJson(AstNode ast) {
        return Serializer.toJson(ast);
    }

    public static STMLLexer.TokenizeResult tokenize(String text) {
        STMLLexer lexer = new STMLLexer(text);
        return lexer.tokenize();
    }

    public static class ParseResult {
        public final List<AstNode> astList;
        public final List<Warning> warnings;
        public ParseResult(List<AstNode> astList, List<Warning> warnings) {
            this.astList = astList;
            this.warnings = warnings;
        }
    }

    public static ParseResult parse(List<Token> tokens) {
        LineTreeBuilder treeBuilder = new LineTreeBuilder();
        treeBuilder.process(tokens);

        AstBuilder astBuilder = new AstBuilder();
        List<AstNode> docsList = new ArrayList<>();
        for (List<Line> docLines : treeBuilder.documents()) {
            docsList.add(astBuilder.build(docLines));
        }

        List<Warning> allWarnings = new ArrayList<>();
        allWarnings.addAll(treeBuilder.warnings());
        allWarnings.addAll(astBuilder.warnings());

        return new ParseResult(docsList, allWarnings);
    }
}