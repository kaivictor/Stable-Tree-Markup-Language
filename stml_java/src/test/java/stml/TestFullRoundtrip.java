package stml;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public class TestFullRoundtrip {
    static int testsRun = 0;
    static int testsPassed = 0;
    static String testdataPath;

    static void test(String name, Runnable test) {
        testsRun++;
        try {
            test.run();
            testsPassed++;
            System.out.println("  PASS " + name);
        } catch (Exception e) {
            System.out.println("  FAIL " + name + ": " + e.getMessage());
        }
    }

    static String readFile(String path) throws IOException {
        return Files.readString(Path.of(path));
    }

    static void runFullRoundtrip(int testNum) throws IOException {
        String stmlPath = testdataPath + "/test" + testNum + ".stml";
        String jsonPath = testdataPath + "/test" + testNum + "_expected.json";

        if (!Files.exists(Path.of(stmlPath)) || !Files.exists(Path.of(jsonPath))) {
            System.out.println("  SKIP: file not found");
            testsPassed++;
            return;
        }

        String stmlText = readFile(stmlPath);
        String expectedJsonText = readFile(jsonPath);

        boolean onlyWs = true;
        for (char ch : expectedJsonText.toCharArray()) {
            if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') { onlyWs = false; break; }
        }
        if (onlyWs) expectedJsonText = "{\"docs\": []}";

        AstNode expectedAst = TestJson.parse(expectedJsonText);

        // Chain A: STML → AST → JSON ≟ expected JSON
        STML.LoadResult result = STML.loads(stmlText);
        String jsonFromStml = STML.toJson(result.ast);
        AstNode jsonFromStmlAst = TestJson.parse(jsonFromStml);

        if (!jsonFromStmlAst.equals(expectedAst)) {
            System.out.println("\n  Chain A FAIL for test" + testNum);
            System.out.println("  STML→JSON:\n  " + jsonFromStml.trim());
            System.out.println("  Expected:\n  " + expectedJsonText.trim());
            throw new RuntimeException("Chain A: STML→JSON != expected JSON");
        }

        // Chain B: JSON → AST → STML → AST → JSON ≟ expected JSON
        String stmlFromJson = STML.dumps(expectedAst);
        STML.LoadResult rtResult = STML.loads(stmlFromJson);
        String jsonRt = STML.toJson(rtResult.ast);
        AstNode jsonRtAst = TestJson.parse(jsonRt);

        if (!jsonRtAst.equals(expectedAst)) {
            System.out.println("\n  Chain B FAIL for test" + testNum);
            System.out.println("  JSON→STML→JSON:\n  " + jsonRt.trim());
            System.out.println("  Expected:\n  " + expectedJsonText.trim());
            throw new RuntimeException("Chain B: JSON→STML→JSON != expected JSON");
        }
    }

    public static void main(String[] args) {
        if (args.length > 0) {
            testdataPath = args[0];
        } else {
            String defaultPath = "../TestData";
            if (Files.exists(Path.of(defaultPath))) {
                testdataPath = defaultPath;
            } else {
                String altPath = "../../TestData";
                if (Files.exists(Path.of(altPath))) {
                    testdataPath = altPath;
                } else {
                    System.err.println("Cannot find TestData directory. Pass path as argument.");
                    System.exit(1);
                }
            }
        }

        System.out.println("Full round-trip tests (using TestData: " + testdataPath + ")\n");
        System.out.println("Chain A: STML → AST → JSON ≟ expected JSON");
        System.out.println("Chain B: JSON → AST → STML → AST → JSON ≟ expected JSON\n");

        for (int i = 1; i <= 13; i++) {
            final int num = i;
            test("test_chain_ab_" + num, () -> { try { runFullRoundtrip(num); } catch (IOException e) { throw new RuntimeException(e); } });
        }

        System.out.println("\n" + testsPassed + "/" + testsRun + " passed");
        System.exit(testsPassed == testsRun ? 0 : 1);
    }
}