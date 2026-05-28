package com.stml.lexer;

/**
 * An element of an inline list.
 * StringElem = a string value, NullElem = a null element.
 */
public sealed interface InlineElem {
    record StringElem(String value) implements InlineElem {}
    enum NullElem implements InlineElem { INSTANCE }
}
