/// \file Lexer.h
/// \brief Lexical analyzer interface.

#ifndef YUAN_LEXER_LEXER_H
#define YUAN_LEXER_LEXER_H

#include "yuan/Lexer/Token.h"
#include "yuan/Basic/SourceLocation.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include <deque>

namespace yuan {

class DiagnosticEngine;

/// \brief Lexical analyzer for Yuan source code.
///
/// The Lexer class tokenizes Yuan source code into a stream of tokens.
/// It supports lookahead through the peek() methods and maintains
/// accurate source location information for each token.
class Lexer {
public:
    /// \brief Construct a lexer for a specific file.
    /// \param sm Source manager for location services.
    /// \param diag Diagnostic engine for error reporting.
    /// \param fileID The file to tokenize.
    Lexer(SourceManager& sm, DiagnosticEngine& diag, 
          SourceManager::FileID fileID);
    
    /// \brief Get the next token from the input stream.
    /// \return The next token.
    Token lex();
    
    /// \brief Look at the next token without consuming it.
    /// \return The next token.
    Token peek();
    
    /// \brief Look at the nth token ahead without consuming any tokens.
    /// \param n The number of tokens to look ahead (0 = current, 1 = next, etc.).
    /// \return The nth token ahead.
    Token peek(unsigned n);
    
    /// \brief Get the current source location.
    /// \return The current position in the source.
    SourceLocation getCurrentLocation() const;

    /// \brief Check if two locations are on different lines.
    /// \return True if line numbers differ.
    bool isNewLineBetween(SourceLocation left, SourceLocation right) const;
    
    /// \brief Check if we've reached the end of the file.
    /// \return True if at end of file.
    bool isAtEnd() const;

    /// \brief Split a '>>' token into two '>' tokens.
    /// This is used by the parser when parsing nested generic types like Map<T, Vec<U>>.
    /// After calling this method, the next call to lex() will return '>'.
    void splitGreaterGreater();

private:
    SourceManager& SM;
    DiagnosticEngine& Diag;
    SourceManager::FileID FileID;
    
    const char* BufferStart;    ///< Start of the input buffer
    const char* BufferEnd;      ///< End of the input buffer
    const char* CurPtr;         ///< Current position in the buffer
    
    /// \brief Lookahead token cache for peek() operations.
    std::deque<Token> LookaheadTokens;
    
    /// \brief The actual lexing implementation.
    /// \return The next token from the input.
    Token lexImpl();
    
    /// \brief Lex an identifier or keyword.
    /// \return The identifier or keyword token.
    Token lexIdentifier();
    
    /// \brief Lex a numeric literal (integer or float).
    /// \return The numeric literal token.
    Token lexNumber();
    
    /// \brief Lex a string literal.
    /// \return The string literal token.
    Token lexString();
    
    /// \brief Lex a raw string literal (r"..." or r###"..."###).
    /// \return The raw string literal token.
    Token lexRawString();
    
    /// \brief Lex a multiline string literal ("""...""").
    /// \return The multiline string literal token.
    Token lexMultilineString();
    
    /// \brief Lex a character literal.
    /// \return The character literal token.
    Token lexChar();
    
    /// \brief Lex an operator or punctuation.
    /// \return The operator/punctuation token.
    Token lexOperator();
    
    /// \brief Skip whitespace characters.
    void skipWhitespace();
    
    /// \brief Skip a line comment (// ...).
    void skipLineComment();
    
    /// \brief Skip a block comment (/* ... */).
    void skipBlockComment();
    
    /// \brief Check if a character can start an identifier.
    /// \param c The character to check.
    /// \return True if the character can start an identifier.
    bool isIdentifierStart(char c) const;
    
    /// \brief Check if a character can continue an identifier.
    /// \param c The character to check.
    /// \return True if the character can continue an identifier.
    bool isIdentifierContinue(char c) const;
    
    /// \brief Check if a character is a decimal digit.
    /// \param c The character to check.
    /// \return True if the character is a decimal digit.
    bool isDigit(char c) const;
    
    /// \brief Check if a character is a hexadecimal digit.
    /// \param c The character to check.
    /// \return True if the character is a hexadecimal digit.
    bool isHexDigit(char c) const;
    
    /// \brief Peek at the current character without consuming it.
    /// \return The current character, or '\0' if at end.
    char peekChar() const;
    
    /// \brief Peek at the nth character ahead without consuming any.
    /// \param n The number of characters to look ahead.
    /// \return The nth character ahead, or '\0' if past end.
    char peekChar(unsigned n) const;
    
    /// \brief Consume and return the current character.
    /// \return The consumed character, or '\0' if at end.
    char consumeChar();
    
    /// \brief Get the current source location.
    /// \return The current location in the source.
    SourceLocation getLocation() const;
    
    /// \brief Report a lexical error.
    /// \param id The diagnostic ID.
    /// \param loc The location of the error.
    void reportError(DiagID id, SourceLocation loc);
    
    /// \brief Report a lexical error with arguments.
    /// \param id The diagnostic ID.
    /// \param loc The location of the error.
    /// \param arg The argument for the error message.
    void reportError(DiagID id, SourceLocation loc, const std::string& arg);
    
    /// \brief Process an escape sequence in a string or character literal.
    /// \param startLoc The location where the escape sequence starts.
    /// \param escapeChar The character after the backslash.
    /// \return True if the escape sequence is valid, false otherwise.
    bool processEscapeSequence(SourceLocation startLoc, char escapeChar);
    
    /// \brief Decode a UTF-8 sequence into a Unicode codepoint.
    /// \param start Pointer to the start of the UTF-8 sequence.
    /// \param end Pointer to the end of the buffer.
    /// \param codepoint Output parameter for the decoded codepoint.
    /// \param bytesConsumed Output parameter for the number of bytes consumed.
    /// \return True if decoding was successful.
    bool decodeUTF8(const char* start, const char* end, uint32_t& codepoint, int& bytesConsumed) const;
    
    /// \brief Check if a Unicode codepoint can start an identifier.
    /// \param codepoint The Unicode codepoint to check.
    /// \return True if the codepoint can start an identifier.
    bool isUnicodeIdentifierStart(uint32_t codepoint) const;
    
    /// \brief Check if a Unicode codepoint can continue an identifier.
    /// \param codepoint The Unicode codepoint to check.
    /// \return True if the codepoint can continue an identifier.
    bool isUnicodeIdentifierContinue(uint32_t codepoint) const;

    /// \brief Attach pending doc comment to a token if present.
    Token attachDocComment(Token token);

    /// \brief Pending doc comment (collected from ///).
    std::string PendingDocComment;
};

} // namespace yuan

#endif // YUAN_LEXER_LEXER_H
