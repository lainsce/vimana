// @ts-check
'use strict';

const vscode = require('vscode');
const { formatYis } = require('./formatter');

/**
 * @param {vscode.ExtensionContext} _context
 */
function activate(_context) {
    vscode.languages.registerDocumentFormattingEditProvider('yis', {
        /**
         * @param {vscode.TextDocument} document
         * @param {vscode.FormattingOptions} options
         * @returns {vscode.TextEdit[]}
         */
        provideDocumentFormattingEdits(document, options) {
            const text = document.getText();
            const formatted = formatYis(text, options);
            if (formatted === text) return [];
            const full = new vscode.Range(
                document.positionAt(0),
                document.positionAt(text.length)
            );
            return [vscode.TextEdit.replace(full, formatted)];
        }
    });
}

function deactivate() {}

module.exports = { activate, deactivate };
