// @ts-check
'use strict';

// ---------------------------------------------------------------------------
// String / comment stripper
// ---------------------------------------------------------------------------

/**
 * Strip string literals and line comments from a single line, replacing their
 * content with spaces so structural characters at real positions are preserved.
 *
 * Handles:
 *   @text"..."  — interpolated string
 *   "..."       — plain string
 *   (( ... ))   — return spec: (( preceded by ) is NOT a comment even if -- inside
 *   --          — line comment (only outside strings and return specs)
 *
 * @param {string} line
 * @returns {string}
 */
function stripStringsAndComments(line) {
    let out = '';
    let i = 0;
    const n = line.length;
    let returnSpecDepth = 0;

    while (i < n) {
        const ch = line[i];

        // (( return spec open — only when preceded by ) in the output so far
        // e.g. "fun foo() (( type ))" but NOT ".on_click((c = any)"
        if (ch === '(' && i + 1 < n && line[i + 1] === '(') {
            const prevSig = out.trimEnd();
            const prevChar = prevSig[prevSig.length - 1] || '';
            if (prevChar === ')') {
                out += '((';
                returnSpecDepth++;
                i += 2;
                continue;
            }
        }

        // )) return spec close
        if (ch === ')' && i + 1 < n && line[i + 1] === ')' && returnSpecDepth > 0) {
            out += '))';
            returnSpecDepth--;
            i += 2;
            continue;
        }

        // @" interpolated string
        if (ch === '@' && i + 1 < n && line[i + 1] === '"') {
            out += '  ';
            i += 2;
            while (i < n && line[i] !== '"') {
                if (line[i] === '\\') { out += '  '; i += 2; continue; }
                out += ' ';
                i++;
            }
            if (i < n) { out += ' '; i++; }
        }
        // " plain string
        else if (ch === '"') {
            out += ' ';
            i++;
            while (i < n && line[i] !== '"') {
                if (line[i] === '\\') { out += '  '; i += 2; continue; }
                out += ' ';
                i++;
            }
            if (i < n) { out += ' '; i++; }
        }
        // -- line comment (but NOT inside a return spec)
        else if (ch === '-' && i + 1 < n && line[i + 1] === '-' && returnSpecDepth === 0) {
            break;
        }
        else {
            out += ch;
            i++;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Line classifier
// ---------------------------------------------------------------------------

/**
 * @typedef {{
 *   trimmed:           string,
 *   stripped:          string,
 *   isBlank:           boolean,
 *   isComment:         boolean,
 *   startsWithDot:     boolean,
 *   startsWithClose:   boolean,
 *   closeChar:         string | null,
 *   netParens:         number,
 *   netBraces:         number,
 *   netBrackets:       number,
 * }} LineInfo
 */

/**
 * Classify a single raw source line.
 * @param {string} rawLine
 * @returns {LineInfo}
 */
function classifyLine(rawLine) {
    const trimmed = rawLine.trim();
    const stripped = stripStringsAndComments(trimmed).trim();

    const isBlank = trimmed.length === 0;
    const isComment = trimmed.startsWith('--');

    let netParens = 0, netBraces = 0, netBrackets = 0;
    for (const ch of stripped) {
        if      (ch === '(') netParens++;
        else if (ch === ')') netParens--;
        else if (ch === '{') netBraces++;
        else if (ch === '}') netBraces--;
        else if (ch === '[') netBrackets++;
        else if (ch === ']') netBrackets--;
    }

    const startsWithDot = stripped.startsWith('.');

    const firstChar = stripped[0] || '';
    const startsWithClose = firstChar === '}' || firstChar === ')' || firstChar === ']';
    const closeChar = startsWithClose ? firstChar : null;

    return {
        trimmed,
        stripped,
        isBlank,
        isComment,
        startsWithDot,
        startsWithClose,
        closeChar,
        netParens,
        netBraces,
        netBrackets,
    };
}

// ---------------------------------------------------------------------------
// Indent engine
// ---------------------------------------------------------------------------

/**
 * @typedef {{ level: number, type: 'root' | 'brace' | 'bracket' | 'paren' }} StackEntry
 */

/**
 * Format a Yis source file.
 * @param {string} text
 * @param {{ tabSize: number, insertSpaces: boolean }} options
 * @returns {string}
 */
function formatYis(text, options) {
    const tabWidth = (options && options.tabSize) || 4;
    const useTabs = options && !options.insertSpaces;

    const hadCR = text.includes('\r\n');
    const lines = text.replace(/\r\n/g, '\n').split('\n');
    const trailingNewline = text.endsWith('\n') || text.endsWith('\r\n');

    /** @type {StackEntry[]} */
    const stack = [{ level: 0, type: 'root' }];
    const top = () => stack[stack.length - 1];
    const popStack = () => { if (stack.length > 1) stack.pop(); };

    /** @param {number} level */
    const makeIndent = (level) =>
        useTabs ? '\t'.repeat(level / tabWidth) : ' '.repeat(level);

    const out = [];

    // Trim trailing empty lines
    let lastNonEmpty = lines.length - 1;
    while (lastNonEmpty > 0 && lines[lastNonEmpty].trim() === '') lastNonEmpty--;

    for (let li = 0; li <= lastNonEmpty; li++) {
        const rawTrimmed = lines[li].trim();

        if (rawTrimmed.length === 0) {
            // Collapse consecutive blank lines to a single blank
            if (out.length > 0 && out[out.length - 1] === '') continue;
            out.push('');
            continue;
        }

        const info = classifyLine(rawTrimmed);
        const strippedLine = stripStringsAndComments(rawTrimmed).trimEnd();
        const lastSig = strippedLine[strippedLine.length - 1] || '';

        // ---- Step 1: Pop leading close chars --------------------------------
        // The FIRST close char determines this line's emit level.
        // Any subsequent close chars (})), }), etc.) pop their contexts after emit.
        const closeRun = info.startsWithClose
            ? (rawTrimmed.match(/^[}\])]+/) || [''])[0]
            : '';

        if (closeRun.length > 0) {
            const firstCh = closeRun[0];
            if (firstCh === '}') {
                while (stack.length > 1 && top().type !== 'brace') popStack();
                if (stack.length > 1) popStack();
            } else if (firstCh === ']') {
                while (stack.length > 1 && top().type !== 'bracket') popStack();
                if (stack.length > 1) popStack();
            } else if (firstCh === ')') {
                if (stack.length > 1 && top().type === 'paren') popStack();
            }
        }

        // ---- Step 2: Emit ---------------------------------------------------
        const thisLevel = top().level;
        out.push(makeIndent(thisLevel) + rawTrimmed);

        // ---- Step 3: Pop remaining close chars (positions 1+) ---------------
        for (let ci = 1; ci < closeRun.length; ci++) {
            const ch = closeRun[ci];
            if (ch === ')') {
                if (stack.length > 1 && top().type === 'paren') popStack();
            } else if (ch === ']') {
                while (stack.length > 1 && top().type !== 'bracket') popStack();
                if (stack.length > 1) popStack();
            } else if (ch === '}') {
                while (stack.length > 1 && top().type !== 'brace') popStack();
                if (stack.length > 1) popStack();
            }
        }

        // ---- Step 4: Push openers -------------------------------------------
        if (lastSig === '{') {
            stack.push({ level: thisLevel + tabWidth, type: 'brace' });
        } else if (lastSig === '[' && info.netBrackets > 0) {
            stack.push({ level: thisLevel + tabWidth, type: 'bracket' });
        } else if (info.netParens > 0) {
            stack.push({ level: thisLevel + tabWidth, type: 'paren' });
        }
    }

    const joined = out.join('\n');
    return trailingNewline ? joined + '\n' : joined;
}

module.exports = { formatYis, classifyLine, stripStringsAndComments };
