import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    const config = vscode.workspace.getConfiguration('yuan');
    const serverPath = config.get<string>('languageServerPath') || 'yuan-lsp';

    const serverOptions: ServerOptions = {
        run:   { command: serverPath },
        debug: { command: serverPath }
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'yuan' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.yu')
        },
        outputChannelName: 'Yuan Language Server',
    };

    client = new LanguageClient(
        'yuanLanguageServer',
        'Yuan Language Server',
        serverOptions,
        clientOptions
    );

    client.start().catch((err: Error) => {
        vscode.window.showErrorMessage(
            `Yuan Language Server failed to start: ${err.message}. ` +
            `Check that yuan-lsp is in your PATH or set yuan.languageServerPath in settings.`
        );
    });
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
