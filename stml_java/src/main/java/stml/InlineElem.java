package stml;

import java.util.Optional;

public class InlineElem {
    public final Optional<String> value;

    public InlineElem(String value) {
        this.value = Optional.ofNullable(value);
    }

    public static InlineElem of(String value) { return new InlineElem(value); }
    public static InlineElem nullElem() { return new InlineElem(null); }

    public boolean isNull() { return !value.isPresent(); }
    public String get() { return value.orElse(null); }
}