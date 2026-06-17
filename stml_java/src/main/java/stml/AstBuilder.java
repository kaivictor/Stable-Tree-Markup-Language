package stml;

import java.util.ArrayList;
import java.util.List;

public class AstBuilder {
    private List<Warning> warnings = new ArrayList<>();

    public List<Warning> warnings() { return warnings; }

    public AstNode build(List<Line> lines) {
        if (lines.isEmpty()) return new AstNode();

        boolean allDash = true;
        for (Line line : lines) {
            if (!line.isDash()) { allDash = false; break; }
        }

        if (allDash) return buildSequence(lines);

        boolean firstIsDash = !lines.isEmpty() && lines.get(0).isDash();
        if (firstIsDash) return buildSequence(lines);

        return new AstNode(buildMapping(lines));
    }

    private List<AstNode.Pair> buildMapping(List<Line> lines) {
        List<AstNode.Pair> map = new ArrayList<>();
        int i = 0;

        while (i < lines.size()) {
            Line line = lines.get(i);
            if (!(line.kind == Line.Kind.KEY_VAL || line.kind == Line.Kind.BARE_KEY)) {
                i++;
                continue;
            }

            String key = line.key;
            AstNode val = new AstNode();

            if (line.hasInlineValue()) {
                val = AstNode.deepClone(line.inlineValue);
                map.add(new AstNode.Pair(key, val));
                if (line.hasChildren()) {
                    absorbChildrenAsSiblings(map, line.children);
                }
                i++;
                continue;
            } else if (line.hasChildren()) {
                val = build(line.children);
                i++;
                if (val.isList()) {
                    int j = i;
                    while (j < lines.size() && lines.get(j).isDash()) j++;
                    if (j > i) {
                        List<Line> dashLines = new ArrayList<>(lines.subList(i, j));
                        AstNode extra = build(dashLines);
                        if (extra.isList()) {
                            List<AstNode> valList = val.asList();
                            List<AstNode> extraList = extra.asList();
                            valList.addAll(extraList);
                        } else {
                            val.asList().add(extra);
                        }
                        i = j;
                    }
                }
            } else {
                int j = i + 1;
                while (j < lines.size() && lines.get(j).isDash()) j++;
                if (j > i + 1) {
                    List<Line> dashLines = new ArrayList<>(lines.subList(i + 1, j));
                    val = build(dashLines);
                    i = j;
                } else {
                    i++;
                }
            }

            map.add(new AstNode.Pair(key, val));
        }

        return map;
    }

    private AstNode buildSequence(List<Line> lines) {
        boolean isComplex = false;
        boolean hasNonDash = false;
        for (Line line : lines) {
            if (line.kind == Line.Kind.DASH_KEY_VAL) isComplex = true;
            if (!line.isDash()) hasNonDash = true;
        }

        if (isComplex || hasNonDash) {
            return new AstNode(buildComplexSequence(lines));
        } else {
            return new AstNode(buildSimpleSequence(lines));
        }
    }

    private List<AstNode> buildSimpleSequence(List<Line> lines) {
        List<AstNode> seq = new ArrayList<>();

        for (Line line : lines) {
            if (!line.isDash()) continue;

            if (line.kind == Line.Kind.DASH_EMPTY) {
                if (line.hasChildren()) {
                    seq.add(new AstNode());
                    AstNode child = build(line.children);
                    if (child.isList()) {
                        seq.addAll(child.asList());
                    } else {
                        seq.add(child);
                    }
                } else {
                    seq.add(new AstNode());
                }
            } else if (line.kind == Line.Kind.DASH_SCALAR) {
                if (line.hasChildren()) {
                    List<Line> dashKids = new ArrayList<>(), nonDashKids = new ArrayList<>();
                    for (Line c : line.children) {
                        if (c.isDash()) dashKids.add(c);
                        else nonDashKids.add(c);
                    }
                    if (!nonDashKids.isEmpty()) {
                        warnings.add(new Warning(line.lineNo, 1,
                                "Dash scalar entry has unexpected child content, ignoring children"));
                    }
                    seq.add(AstNode.deepClone(line.inlineValue));
                    if (!dashKids.isEmpty()) {
                        AstNode extra = buildSequence(dashKids);
                        if (extra.isList()) seq.addAll(extra.asList());
                        else seq.add(extra);
                    }
                } else {
                    seq.add(AstNode.deepClone(line.inlineValue));
                }
            }
        }

        return seq;
    }

    private List<AstNode> buildComplexSequence(List<Line> lines) {
        List<AstNode> seq = new ArrayList<>();
        int i = 0;

        while (i < lines.size()) {
            if (!lines.get(i).isDash()) {
                boolean anyDashLeft = false;
                for (int k = i + 1; k < lines.size(); k++) {
                    if (lines.get(k).isDash()) { anyDashLeft = true; break; }
                }
                if (!anyDashLeft) break;
                i++;
                continue;
            }

            List<AstNode.Pair> entry = new ArrayList<>();
            Line dashLine = lines.get(i);

            if (dashLine.kind == Line.Kind.DASH_KEY_VAL) {
                AstNode val = dashLine.hasInlineValue() ? AstNode.deepClone(dashLine.inlineValue) : new AstNode();
                entry.add(new AstNode.Pair(dashLine.key, val));

                if (!dashLine.hasInlineValue() && dashLine.hasChildren()) {
                    entry.set(entry.size() - 1, new AstNode.Pair(dashLine.key, build(dashLine.children)));
                } else if (dashLine.hasInlineValue() && dashLine.hasChildren()) {
                    List<Line> nonDashKids = new ArrayList<>(), dashKids = new ArrayList<>();
                    for (Line c : dashLine.children) {
                        if (c.isDash()) dashKids.add(c);
                        else nonDashKids.add(c);
                    }
                    if (!nonDashKids.isEmpty()) {
                        absorbChildrenAsSiblings(entry, nonDashKids);
                    }
                    seq.add(new AstNode(new ArrayList<>(entry)));
                    if (!dashKids.isEmpty()) {
                        AstNode extra = buildSequence(dashKids);
                        if (extra.isList()) seq.addAll(extra.asList());
                        else seq.add(extra);
                    }
                    i++;
                    continue;
                }
            } else if (dashLine.kind == Line.Kind.DASH_SCALAR) {
                String key = dashLine.inlineValue.asString() != null ? dashLine.inlineValue.asString() : "";
                entry.add(new AstNode.Pair(key, new AstNode()));

                if (dashLine.hasChildren()) {
                    List<Line> nonDashKids = new ArrayList<>(), dashKids = new ArrayList<>();
                    for (Line c : dashLine.children) {
                        if (c.isDash()) dashKids.add(c);
                        else nonDashKids.add(c);
                    }
                    if (!nonDashKids.isEmpty()) {
                        absorbChildrenAsSiblings(entry, nonDashKids);
                    }
                    if (!dashKids.isEmpty()) {
                        seq.add(new AstNode(new ArrayList<>(entry)));
                        AstNode extra = buildSequence(dashKids);
                        if (extra.isList()) seq.addAll(extra.asList());
                        else seq.add(extra);
                        i++;
                        continue;
                    }
                }
            } else if (dashLine.kind == Line.Kind.DASH_EMPTY) {
                if (dashLine.hasChildren()) {
                    AstNode child = build(dashLine.children);
                    if (child.isMap()) {
                        entry.addAll(child.asMap());
                    } else if (child.isList()) {
                        entry.add(new AstNode.Pair("", child));
                    } else {
                        entry.add(new AstNode.Pair("", child));
                    }
                } else {
                    seq.add(new AstNode());
                    i++;
                    continue;
                }
            }

            // Collect sibling keys
            int j = i + 1;
            while (j < lines.size() && !lines.get(j).isDash()) {
                Line sibling = lines.get(j);
                if (sibling.kind == Line.Kind.KEY_VAL || sibling.kind == Line.Kind.BARE_KEY) {
                    String key = sibling.key;
                    AstNode val = new AstNode();

                    if (sibling.hasInlineValue()) {
                        val = AstNode.deepClone(sibling.inlineValue);
                        if (sibling.hasChildren()) {
                            absorbChildrenAsSiblings(entry, sibling.children);
                        }
                    } else if (sibling.hasChildren()) {
                        val = build(sibling.children);
                    }

                    entry.add(new AstNode.Pair(key, val));
                }
                j++;
            }

            seq.add(new AstNode(new ArrayList<>(entry)));
            i = j;
        }

        // Handle trailing non-dash lines
        if (i < lines.size()) {
            List<Line> trailing = new ArrayList<>(lines.subList(i, lines.size()));
            AstNode extra = build(trailing);
            if (extra.isMap()) {
                seq.add(extra);
            } else if (extra.isList()) {
                seq.addAll(extra.asList());
            }
        }

        return seq;
    }

    private void absorbChildrenAsSiblings(List<AstNode.Pair> map, List<Line> children) {
        for (Line child : children) {
            if (child.isDash()) {
                warnings.add(new Warning(child.lineNo, 1,
                        "Dash entry found in irregular indent context, skipping"));
                continue;
            }

            if (child.kind == Line.Kind.KEY_VAL || child.kind == Line.Kind.BARE_KEY) {
                String key = child.key;
                AstNode val = new AstNode();

                if (child.hasInlineValue()) {
                    val = AstNode.deepClone(child.inlineValue);
                } else if (child.hasChildren()) {
                    val = build(child.children);
                }

                map.add(new AstNode.Pair(key, val));
            }
        }
    }
}