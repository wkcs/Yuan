import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    // Determine the path to the yuan-lsp executable
    // Typically it will be in the system PATH or configured via settings
    const config = vscode.workspace.getConfiguration('yuan');
    const serverPath = config.get<string>('languageServerPath') || 'yuan-lsp';

    const serverOptions: ServerOptions = {
        run: { command: serverPath },
        debug: { command: serverPath }
    };

    const clientOptions: LanguageClientOptions = {
        // Register the server for yuan documents
        documentSelector: [{ scheme: 'file', language: 'yuan' }],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
        }
    };

    client = new LanguageClient(
        'yuanLanguageServer',
        'Yuan Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client
    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
