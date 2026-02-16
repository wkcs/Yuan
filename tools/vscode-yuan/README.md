# Yuan VS Code Extension

This extension adds basic editor support for the Yuan language (`.yu`) in VS Code.

## Features

- Syntax highlighting for:
  - Keywords, primitive types, operators
  - `@builtin` identifiers (for example `@import`)
  - Numbers (binary/octal/hex/float/integer)
  - Strings, raw strings (`r###"..."###`), multiline strings (`"""..."""`), char literals
  - Line comments (`//`, `///`) and block comments (`/* ... */`)
- Language configuration:
  - Comment toggling
  - Bracket matching
  - Auto-closing and surrounding pairs
- Common Yuan snippets (`func`, `main`, `struct`, `enum`, `impl`, `match`, `for`, `errblk`)

## Development

1. Open this folder in VS Code:
   - `tools/vscode-yuan`
2. Press `F5` to run an Extension Development Host.
3. Open any `.yu` file in the new window to test highlighting and snippets.

## Package as VSIX

```bash
cd tools/vscode-yuan
npm install -g @vscode/vsce
vsce package
```

The command outputs a `.vsix` file that can be installed in VS Code.
