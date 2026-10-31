package com.schullars.desktopctl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

final class CommandTokenizer {
    private CommandTokenizer() {
    }

    static TokenizationResult tokenize(String commandLine) {
        if (commandLine == null || commandLine.trim().isEmpty()) {
            return TokenizationResult.failure("The command line is empty.");
        }

        List<String> tokens = new ArrayList<>();
        StringBuilder token = new StringBuilder();
        boolean tokenStarted = false;
        char quote = 0;
        boolean escaping = false;

        for (int index = 0; index < commandLine.length(); ++index) {
            char character = commandLine.charAt(index);
            if (escaping) {
                token.append(character);
                tokenStarted = true;
                escaping = false;
                continue;
            }
            if (quote == '\'') {
                if (character == '\'') {
                    quote = 0;
                } else {
                    token.append(character);
                }
                tokenStarted = true;
                continue;
            }
            if (quote == '"') {
                if (character == '"') {
                    quote = 0;
                } else if (character == '\\') {
                    escaping = true;
                } else {
                    token.append(character);
                }
                tokenStarted = true;
                continue;
            }
            if (character == '\\') {
                escaping = true;
                tokenStarted = true;
                continue;
            }
            if (character == '\'' || character == '"') {
                quote = character;
                tokenStarted = true;
                continue;
            }
            if (Character.isWhitespace(character)) {
                if (tokenStarted) {
                    tokens.add(token.toString());
                    token.setLength(0);
                    tokenStarted = false;
                }
                continue;
            }
            if (isShellOperator(character)) {
                return TokenizationResult.failure(
                        "Shell operators are not supported in DesktopCTL virtual commands.");
            }
            token.append(character);
            tokenStarted = true;
        }

        if (escaping) {
            return TokenizationResult.failure("The command line ends with an incomplete escape.");
        }
        if (quote != 0) {
            return TokenizationResult.failure("The command line contains an unterminated quote.");
        }
        if (tokenStarted) {
            tokens.add(token.toString());
        }
        if (tokens.isEmpty()) {
            return TokenizationResult.failure("The command line is empty.");
        }
        return TokenizationResult.success(tokens);
    }

    private static boolean isShellOperator(char character) {
        return character == ';'
                || character == '|'
                || character == '&'
                || character == '<'
                || character == '>'
                || character == '`';
    }

    static final class TokenizationResult {
        private final List<String> tokens;
        private final String error;

        private TokenizationResult(List<String> tokens, String error) {
            this.tokens = tokens;
            this.error = error;
        }

        static TokenizationResult success(List<String> tokens) {
            return new TokenizationResult(
                    Collections.unmodifiableList(new ArrayList<>(tokens)),
                    "");
        }

        static TokenizationResult failure(String error) {
            return new TokenizationResult(Collections.<String>emptyList(), error);
        }

        boolean isSuccessful() {
            return error.isEmpty();
        }

        List<String> getTokens() {
            return tokens;
        }

        String getError() {
            return error;
        }
    }
}
