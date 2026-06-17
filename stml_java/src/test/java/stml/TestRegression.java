package stml;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class TestRegression {
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

    static void runRegression(int testNum) throws IOException {
        String stmlFile = testdataPath + "/test" + testNum + ".stml";
        String jsonFile = testdataPath + "/test" + testNum + "_expected.json";

        if (!Files.exists(Path.of(stmlFile))) {
            System.out.println("  SKIP test" + testNum + ": file not found");
            testsPassed++;
            return;
        }

        STML.LoadResult result = STML.load(stmlFile);
        AstNode expected = TestJson.load(jsonFile);

        if (!result.ast.equals(expected)) {
            System.out.println("\n  STML output (JSON):\n" + STML.toJson(result.ast));
            System.out.println("  Expected (JSON):\n" + STML.toJson(expected));

            List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
            List<AstNode> expDocs = AstNode.mapFind(expected.asMap(), "docs").asList();

            if (docs.size() != expDocs.size()) {
                System.out.println("  Doc count differs: " + docs.size() + " vs " + expDocs.size());
            }
            int maxDocs = Math.min(docs.size(), expDocs.size());
            for (int i = 0; i < maxDocs; i++) {
                if (!docs.get(i).equals(expDocs.get(i))) {
                    System.out.println("  Doc " + i + " differs");
                }
            }
            throw new RuntimeException("AST mismatch for test" + testNum);
        }
    }

    static void test_roundtrip_regression(int testNum) throws IOException {
        String stmlFile = testdataPath + "/test" + testNum + ".stml";
        if (!Files.exists(Path.of(stmlFile))) return;

        String text = Files.readString(Path.of(stmlFile));
        AstNode ast1 = STML.loads(text).ast;
        String out1 = STML.dumps(ast1);
        AstNode ast2 = STML.loads(out1).ast;

        if (!ast1.equals(ast2)) {
            System.out.println("\n  Original JSON:\n" + STML.toJson(ast1));
            System.out.println("  Roundtrip JSON:\n" + STML.toJson(ast2));
            throw new RuntimeException("Round-trip mismatch for test" + testNum);
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

        System.out.println("Using TestData at: " + testdataPath + "\n");

        System.out.println("Regression tests (against expected JSON):");
        for (int i = 1; i <= 13; i++) {
            final int num = i;
            test("test_regression_" + num, () -> { try { runRegression(num); } catch (IOException e) { throw new RuntimeException(e); } });
        }

        System.out.println("\nRound-trip regression tests:");
        for (int i = 1; i <= 13; i++) {
            final int num = i;
            test("test_roundtrip_" + num, () -> { try { test_roundtrip_regression(num); } catch (IOException e) { throw new RuntimeException(e); } });
        }

        System.out.println("\n" + testsPassed + "/" + testsRun + " passed");
        System.exit(testsPassed == testsRun ? 0 : 1);
    }
}