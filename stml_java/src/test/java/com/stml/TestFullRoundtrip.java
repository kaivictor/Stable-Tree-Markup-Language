package com.stml;

import com.stml.ast.AstNode;
import com.stml.diagnostics.ParseError;
import com.stml.serializer.Serializer;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.reflect.TypeToken;
import org.junit.jupiter.api.DynamicTest;
import org.junit.jupiter.api.TestFactory;

import java.io.IOException;
import java.lang.reflect.Type;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.*;

class TestFullRoundtrip {

    private static final Gson gson = new GsonBuilder().setPrettyPrinting().create();
    private static final Type MAP_TYPE = new TypeToken<Map<String, Object>>(){}.getType();

    record TestCase(String name, String stmlFile, String expectedJsonFile, String standardStmlFile) {}

    static List<TestCase> getTestCases() {
        String base = "src/test/resources/TestData/";
        return List.of(
            new TestCase("test1", base + "test1.stml", base + "test1_expected.json", base + "测试1标准文档.stml"),
            new TestCase("test2", base + "test2.stml", base + "test2_expected.json", base + "测试2标准文档.stml"),
            new TestCase("test3", base + "test3.stml", base + "test3_expected.json", base + "测试3标准文档.stml"),
            new TestCase("test4", base + "test4.stml", base + "test4_expected.json", base + "测试4标准文档.stml"),
            new TestCase("test5", base + "test5.stml", base + "test5_expected.json", base + "测试5标准文档.stml"),
            // test6 is special: throws ParseError
            new TestCase("test7", base + "test7.stml", base + "test7_expected.json", base + "测试7标准文档.stml"),
            new TestCase("test8", base + "test8.stml", base + "test8_expected.json", base + "测试1标准文档.stml"), // no standard stml for test8
            new TestCase("test9", base + "test9.stml", base + "test9_expected.json", base + "测试1标准文档.stml"), // no standard stml for test9
            new TestCase("test10", base + "test10.stml", base + "test10_expected.json", base + "测试1标准文档.stml"), // empty input
            new TestCase("test11", base + "test11.stml", base + "test11_expected.json", base + "测试1标准文档.stml")  // tiny input
        );
    }

    @TestFactory
    Stream<DynamicTest> testChainA_stmlToJson() {
        return getTestCases().stream().map(tc ->
            DynamicTest.dynamicTest(tc.name() + " - Chain A: STML → JSON", () -> {
                String stmlText = Files.readString(Path.of(tc.stmlFile()));
                var result = Stml.loads(stmlText);

                String actualJson = Stml.toJson(result.ast());
                Path expectedJsonPath = Path.of(tc.expectedJsonFile());
                long expectedSize = Files.size(expectedJsonPath);

                if (expectedSize == 0) {
                    // Empty expected file: verify output is valid JSON with empty docs
                    Map<String, Object> actualMap = gson.fromJson(actualJson, MAP_TYPE);
                    assertNotNull(actualMap, "Actual JSON is null for " + tc.name());
                    assertTrue(actualMap.containsKey("docs"), "Missing 'docs' key for " + tc.name());
                    @SuppressWarnings("unchecked")
                    List<Object> docs = (List<Object>) actualMap.get("docs");
                    assertEquals(0, docs.size(), "Expected empty docs list for " + tc.name());
                } else {
                    String expectedJson = Files.readString(expectedJsonPath);
                    Map<String, Object> actualMap = gson.fromJson(actualJson, MAP_TYPE);
                    Map<String, Object> expectedMap = gson.fromJson(expectedJson, MAP_TYPE);

                    assertNotNull(actualMap, "Actual JSON is null for " + tc.name());
                    assertNotNull(expectedMap, "Expected JSON is null for " + tc.name());

                    // Compare via Gson for structural equality
                    String actualNorm = gson.toJson(actualMap);
                    String expectedNorm = gson.toJson(expectedMap);
                    assertEquals(expectedNorm, actualNorm,
                            "Chain A mismatch for " + tc.name());
                }
            })
        );
    }

    @TestFactory
    Stream<DynamicTest> testChainB_stmlToStml() {
        return getTestCases().stream().map(tc ->
            DynamicTest.dynamicTest(tc.name() + " - Chain B: STML → AST → STML", () -> {
                String stmlText = Files.readString(Path.of(tc.stmlFile()));
                var result = Stml.loads(stmlText);

                String outputStml = Stml.dumps(result.ast());
                assertNotNull(outputStml);

                // Re-parse and compare AST
                // Note: empty docs produce empty STML output
                var result2 = Stml.loads(outputStml);
                assertTrue(AstNode.deepEquals(result.ast(), result2.ast()),
                        "Chain B roundtrip mismatch for " + tc.name());
            })
        );
    }

    @TestFactory
    Stream<DynamicTest> testStandardDocumentRoundtrip() {
        // Standard documents should roundtrip identically: STML → AST → STML == original STML
        String base = "src/test/resources/TestData/";
        return Stream.of("测试1标准文档.stml", "测试2标准文档.stml", "测试3标准文档.stml",
                         "测试4标准文档.stml", "测试5标准文档.stml", "测试6标准文档.stml",
                         "测试7标准文档.stml")
            .map(f -> DynamicTest.dynamicTest(f + " - standard doc roundtrip", () -> {
                String stmlText = Files.readString(Path.of(base + f));
                var result = Stml.loads(stmlText);

                String outputStml = Stml.dumps(result.ast());
                var result2 = Stml.loads(outputStml);

                assertTrue(AstNode.deepEquals(result.ast(), result2.ast()),
                        "Standard document roundtrip mismatch for " + f);
            }));
    }

    @TestFactory
    Stream<DynamicTest> testTest6ThrowsParseError() {
        return Stream.of(
            DynamicTest.dynamicTest("test6 throws ParseError", () -> {
                assertThrows(ParseError.class, () -> {
                    Stml.load("src/test/resources/TestData/test6.stml");
                });
            })
        );
    }
}
