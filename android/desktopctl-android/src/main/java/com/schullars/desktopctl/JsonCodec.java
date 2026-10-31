package com.schullars.desktopctl;

final class JsonCodec {
    private JsonCodec() {
    }

    static String quote(String value) {
        String safeValue = value == null ? "" : value;
        StringBuilder result = new StringBuilder(safeValue.length() + 2);
        result.append('"');
        for (int index = 0; index < safeValue.length(); ++index) {
            char character = safeValue.charAt(index);
            switch (character) {
                case '"':
                    result.append("\\\"");
                    break;
                case '\\':
                    result.append("\\\\");
                    break;
                case '\b':
                    result.append("\\b");
                    break;
                case '\f':
                    result.append("\\f");
                    break;
                case '\n':
                    result.append("\\n");
                    break;
                case '\r':
                    result.append("\\r");
                    break;
                case '\t':
                    result.append("\\t");
                    break;
                default:
                    if (character < 0x20) {
                        result.append(String.format("\\u%04x", (int) character));
                    } else {
                        result.append(character);
                    }
                    break;
            }
        }
        result.append('"');
        return result.toString();
    }
}
