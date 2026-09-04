#!/usr/bin/env node
import { GoogleGenerativeAI } from '@google/generative-ai';
import fs from 'fs';
import path from 'path';
import { exec } from 'child_process';
import readline from 'readline';
import chalk from 'chalk';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const MODEL = 'gemini-2.5-flash';

const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);
const model = genAI.getGenerativeModel({ model: MODEL });

let YOLO_MODE = false;
let messages = [];
let cwd = process.cwd();

// Global variables for completion cycling
let lastCompletions = [];
let completionIndex = 0;

// ==================== IMPROVED PATH AUTOCOMPLETER ====================
function getCompletions(line) {
  const match = line.match(/@([^\s]*)$/);
  if (!match) {
    lastCompletions = [];
    return [[], line];
  }

  const partial = match[1];
  let dirToScan = cwd;
  let prefix = partial;

  if (partial.includes('/')) {
    const lastSlash = partial.lastIndexOf('/');
    const dirPart = partial.substring(0, lastSlash + 1);
    prefix = partial.substring(lastSlash + 1);
    try {
      dirToScan = path.resolve(cwd, dirPart);
    } catch (_) {
      lastCompletions = [];
      return [[], line];
    }
  }

  try {
    const entries = fs.readdirSync(dirToScan, { withFileTypes: true })
      .filter(entry => entry.name.toLowerCase().startsWith(prefix.toLowerCase()));

    lastCompletions = entries.map(entry => {
      const base = partial.substring(0, partial.lastIndexOf('/') + 1);
      return entry.isDirectory()
        ? `@${base}${entry.name}/`
        : `@${base}${entry.name}`;
    });

    completionIndex = 0;
    return [lastCompletions, line];
  } catch (err) {
    lastCompletions = [];
    return [[], line];
  }
}

// ==================== TOOLS ====================
async function web_search(query) {
  try {
    const res = await fetch(`https://api.duckduckgo.com/?q=${encodeURIComponent(query)}&format=json`);
    const data = await res.json();
    return data.RelatedTopics?.slice(0, 5)
      .map(t => `Title: ${t.Text}\nURL: ${t.FirstURL}`)
      .join('\n\n') || "No results.";
  } catch (e) {
    return `Search Error: ${e.message}`;
  }
}

async function write_file(filename, content) {
  try {
    await fs.promises.writeFile(filename, content, 'utf8');
    return `Done: Wrote to ${filename}`;
  } catch (e) { return `Error: ${e.message}`; }
}

async function read_file(filename) {
  try {
    return await fs.promises.readFile(filename, 'utf8');
  } catch (e) { return `Error: ${e.message}`; }
}

async function edit_file(filename, search_block, replace_block) {
  try {
    let content = await fs.promises.readFile(filename, 'utf8');
    if (!content.includes(search_block.trim())) {
      return "Error: Search block not found. Ensure exact match.";
    }
    content = content.replace(search_block.trim(), replace_block.trim());
    await fs.promises.writeFile(filename, content, 'utf8');
    return `Successfully edited ${filename}.`;
  } catch (e) { return `Error: ${e.message}`; }
}

async function search_in_files(query, file_pattern = "*") {
  const results = [];
  async function walk(dir) {
    const entries = await fs.promises.readdir(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) await walk(fullPath);
      else if (entry.isFile()) {
        if (file_pattern === "*" || entry.name.match(new RegExp(file_pattern.replace(/\*/g, '.*')))) {
          try {
            const content = await fs.promises.readFile(fullPath, 'utf8');
            content.split('\n').forEach((line, i) => {
              if (line.includes(query)) results.push(`${fullPath} [Line ${i+1}]: ${line.trim()}`);
            });
          } catch (_) {}
        }
      }
    }
  }
  await walk('.');
  return results.slice(0, 20).join('\n') || "No matches found.";
}

async function run_command(command) {
  if (!YOLO_MODE) {
    console.log(chalk.magenta("\n⚠️  [PERMISSION REQUEST]"));
    console.log(chalk.yellow(`Command: ${command}`));
    const ans = await new Promise(r => rl.question(chalk.yellow("Execute? (y/n): "), r));
    if (ans.toLowerCase() !== 'y') return "Permission Denied by user.";
  }

  return new Promise(resolve => {
    exec(command, { cwd }, (error, stdout, stderr) => {
      resolve(`STDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
    });
  });
}

async function change_directory(newPath) {
  try {
    process.chdir(newPath);
    cwd = process.cwd();
    return `Changed to: ${cwd}`;
  } catch (e) { return `Error: ${e.message}`; }
}

async function list_dir(dir = ".") {
  try {
    const items = await fs.promises.readdir(dir);
    return items.join('\n') || "Empty directory.";
  } catch (e) { return `Error: ${e.message}`; }
}

const tools = { web_search, write_file, read_file, edit_file, search_in_files, run_command, change_directory, list_dir };

const toolDeclarations = [
  { name: "web_search", description: "Search the internet", parameters: { type: "OBJECT", properties: { query: { type: "STRING" } }, required: ["query"] }},
  { name: "write_file", description: "Write/create a file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" }, content: { type: "STRING" } }, required: ["filename", "content"] }},
  { name: "read_file", description: "Read a file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" } }, required: ["filename"] }},
  { name: "edit_file", description: "Search & replace in file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" }, search_block: { type: "STRING" }, replace_block: { type: "STRING" } }, required: ["filename", "search_block", "replace_block"] }},
  { name: "search_in_files", description: "Search content in local files", parameters: { type: "OBJECT", properties: { query: { type: "STRING" }, file_pattern: { type: "STRING" } }, required: ["query"] }},
  { name: "run_command", description: "Run shell command", parameters: { type: "OBJECT", properties: { command: { type: "STRING" } }, required: ["command"] }},
  { name: "change_directory", description: "Change working directory", parameters: { type: "OBJECT", properties: { path: { type: "STRING" } }, required: ["path"] }},
  { name: "list_dir", description: "List directory contents", parameters: { type: "OBJECT", properties: { path: { type: "STRING" } }}},
];

const SYSTEM_PROMPT = "You are Aida, an expert technical coding agent. Always scan the current directory first using list_dir, search_in_files or read_file when working with code.";

// ==================== CLI SETUP ====================
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  completer: getCompletions,
  prompt: chalk.bold('>> '),
});

rl.on('SIGINT', () => {
  console.log(chalk.red('\n\nGoodbye!'));
  process.exit(0);
});

// Tab cycling support
rl.input.on('keypress', (char, key) => {
  if (key && key.name === 'tab' && lastCompletions.length > 1) {
    completionIndex = (completionIndex + 1) % lastCompletions.length;
    const currentLine = rl.line;
    const match = currentLine.match(/@([^\s]*)$/);

    if (match) {
      const before = currentLine.substring(0, match.index);
      rl.write(null, { ctrl: true, name: 'u' }); // Clear current line
      rl.write(before + lastCompletions[completionIndex]);
    }
  }
});

function printToolbar() {
  const contextPct = Math.round((JSON.stringify(messages).length / 1048576) * 100);
  console.log(chalk.gray(`\n📂 ${path.basename(cwd)} | Context: ${contextPct}% | Mode: ${YOLO_MODE ? chalk.red('YOLO') : chalk.green('SAFE')}`));
}

// ==================== MAIN LOOP ====================
async function main() {
  console.log(chalk.blue.bold('\nAida Streaming Terminal Active'));
  console.log(chalk.gray('• Type @ then press Tab (multiple times to cycle)'));
  console.log(chalk.gray('• Commands: /clear, /yolo, /scan, /help, exit\n'));

  while (true) {
    printToolbar();
    const userInput = await new Promise(resolve => rl.question(chalk.bold('>> '), resolve));

    const input = userInput.trim();
    if (!input) continue;

    if (['exit', 'quit'].includes(input.toLowerCase())) break;
    if (input === '/clear') { messages = []; console.log(chalk.gray('Memory cleared.')); continue; }
    if (input === '/yolo') { YOLO_MODE = !YOLO_MODE; console.log(chalk.gray(`YOLO Mode: ${YOLO_MODE ? 'ON' : 'OFF'}`)); continue; }
    if (input === '/help') {
      console.log(chalk.cyan('\nCommands: /clear, /yolo, /scan, /help, exit'));
      continue;
    }
    if (input === '/scan') {
      try {
        const items = await fs.promises.readdir('.');
        const snapshot = items.map(item => fs.statSync(item).isDirectory() ? `📁 ${item}/` : `📄 ${item}`);
        const str = snapshot.join('\n');
        console.log(chalk.gray(str));
        messages.push({ role: "user", parts: [{ text: `System Notification: Directory snapshot:\n${str}` }] });
      } catch (e) {}
      continue;
    }

    messages.push({ role: "user", parts: [{ text: input }] });

    let loop = true;
    while (loop) {
      try {
        const result = await model.generateContentStream({
          contents: messages,
          systemInstruction: SYSTEM_PROMPT,
          tools: [{ functionDeclarations: toolDeclarations }]
        });

        let accumulatedText = '';
        let functionCalls = [];

        for await (const chunk of result.stream) {
          const text = chunk.text();
          if (text) {
            accumulatedText += text;
            process.stdout.write(text);
          }

          const calls = chunk.functionCalls();
          if (calls?.length) functionCalls.push(...calls);
        }

        console.log();

        if (accumulatedText) {
          messages.push({ role: "model", parts: [{ text: accumulatedText }] });
        }

        if (functionCalls.length > 0) {
          const toolParts = [];
          for (const call of functionCalls) {
            const toolName = call.name;
            const args = call.args || {};

            console.log(chalk.green(`  • Executing: ${toolName}`));
            Object.entries(args).forEach(([k, v]) => {
              const val = String(v).length > 60 ? String(v).slice(0, 57) + '...' : v;
              console.log(chalk.gray(`    └ ${k}: ${val}`));
            });

            if (tools[toolName]) {
              const resultText = await tools[toolName](...Object.values(args));
              toolParts.push({ functionResponse: { name: toolName, response: { result: resultText } } });
            }
          }
          messages.push({ role: "function", parts: toolParts });
          continue;
        }

        loop = false;
      } catch (err) {
        console.error(chalk.red(`\nError: ${err.message}`));
        break;
      }
    }
  }

  rl.close();
}

main().catch(console.error);
