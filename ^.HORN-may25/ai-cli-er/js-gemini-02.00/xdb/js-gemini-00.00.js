#!/usr/bin/env node
import { GoogleGenerativeAI } from '@google/generative-ai';
import fs from 'fs/promises';
import path from 'path';
import { exec } from 'child_process';
import readline from 'readline';
import chalk from 'chalk';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const MODEL = 'gemini-2.5-flash';
const CONTEXT_LIMIT = 1048576;

const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);
const model = genAI.getGenerativeModel({ model: MODEL });

let YOLO_MODE = false;
let messages = [];
let cwd = process.cwd();

// --- Tools ---
async function web_search(query) {
  try {
    const response = await fetch(`https://api.duckduckgo.com/?q=${encodeURIComponent(query)}&format=json&no_html=1&skip_disambig=1`);
    const data = await response.json();
    return data.RelatedTopics?.slice(0, 5).map(t => 
      `Title: ${t.Text}\nURL: ${t.FirstURL}\n`
    ).join('\n') || "No results found.";
  } catch (e) {
    return `Search Error: ${e.message}`;
  }
}

async function write_file(filename, content) {
  try {
    await fs.writeFile(filename, content, 'utf8');
    return `Done: Wrote to ${filename}`;
  } catch (e) {
    return `Error: ${e.message}`;
  }
}

async function read_file(filename) {
  try {
    return await fs.readFile(filename, 'utf8');
  } catch (e) {
    return `Error: ${e.message}`;
  }
}

async function edit_file(filename, search_block, replace_block) {
  try {
    let content = await fs.readFile(filename, 'utf8');
    if (!content.includes(search_block.trim())) {
      return "Error: Search block not found. Ensure exact match.";
    }
    content = content.replace(search_block.trim(), replace_block.trim());
    await fs.writeFile(filename, content, 'utf8');
    return `Successfully edited ${filename}.`;
  } catch (e) {
    return `Error: ${e.message}`;
  }
}

async function search_in_files(query, file_pattern = "*") {
  const results = [];
  async function walk(dir) {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await walk(fullPath);
      } else if (entry.isFile() && matchPattern(entry.name, file_pattern)) {
        try {
          const content = await fs.readFile(fullPath, 'utf8');
          const lines = content.split('\n');
          lines.forEach((line, i) => {
            if (line.includes(query)) {
              results.push(`${fullPath} [Line ${i+1}]: ${line.trim()}`);
            }
          });
        } catch (_) {}
      }
    }
  }
  await walk('.');
  return results.slice(0, 20).join('\n') || "No matches found.";
}

function matchPattern(filename, pattern) {
  if (pattern === "*") return true;
  return filename.endsWith(pattern.replace("*", ""));
}

async function run_command(command) {
  if (!YOLO_MODE) {
    console.log(chalk.magenta(`\n⚠️  [PERMISSION REQUEST] Agent wants to run:`));
    console.log(chalk.yellow(`   ${command}`));
    const answer = await question(chalk.yellow('Execute? (y/n): '));
    if (answer.toLowerCase() !== 'y') return "Permission Denied.";
  }

  return new Promise((resolve) => {
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
  } catch (e) {
    return `Error: ${e.message}`;
  }
}

async function list_dir(dir = ".") {
  try {
    const items = await fs.readdir(dir);
    return items.join('\n') || "Empty.";
  } catch (e) {
    return `Error: ${e.message}`;
  }
}

// --- Tool Registry ---
const tools = {
  web_search, write_file, read_file, edit_file, search_in_files,
  run_command, change_directory, list_dir
};

// Tool definitions for Gemini
const toolDeclarations = [
  { name: "web_search", description: "Search the internet", parameters: { type: "OBJECT", properties: { query: { type: "STRING" } }, required: ["query"] }},
  { name: "write_file", description: "Write/create a file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" }, content: { type: "STRING" } }, required: ["filename", "content"] }},
  { name: "read_file", description: "Read a file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" } }, required: ["filename"] }},
  { name: "edit_file", description: "Search and replace in file", parameters: { type: "OBJECT", properties: { filename: { type: "STRING" }, search_block: { type: "STRING" }, replace_block: { type: "STRING" } }, required: ["filename", "search_block", "replace_block"] }},
  { name: "search_in_files", description: "Search across files", parameters: { type: "OBJECT", properties: { query: { type: "STRING" }, file_pattern: { type: "STRING" } }, required: ["query"] }},
  { name: "run_command", description: "Run shell command", parameters: { type: "OBJECT", properties: { command: { type: "STRING" } }, required: ["command"] }},
  { name: "change_directory", description: "Change working directory", parameters: { type: "OBJECT", properties: { path: { type: "STRING" } }, required: ["path"] }},
  { name: "list_dir", description: "List directory contents", parameters: { type: "OBJECT", properties: { path: { type: "STRING" } } }},
];

// --- System Prompt ---
const SYSTEM_PROMPT = `You are Aida, an expert technical coding agent.
When asked about code, architecture, or documentation:
1. ALWAYS start by scanning the current directory using list_dir, search_in_files, or read_file.
2. Base your answers on the local codebase first.
3. Use edit_file with exact matches when editing.
Keep responses concise and tool-efficient.`;

// --- CLI Setup ---
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  completer: (line) => {
    // Simple @file completion
    const m = line.match(/@(\S*)$/);
    if (m) {
      // Could implement proper path completion here
    }
    return [[], line];
  }
});

const question = (q) => new Promise(resolve => rl.question(q, resolve));

function printToolbar() {
  const contextPct = Math.round((JSON.stringify(messages).length / CONTEXT_LIMIT) * 100);
  console.log(chalk.gray(`\n📂 ${path.basename(cwd)} | Context: ${contextPct}% | Mode: ${YOLO_MODE ? chalk.red('YOLO') : chalk.green('SAFE')}`));
}

// --- Main Loop ---
async function main() {
  console.log(chalk.blue('Aida Streaming Terminal Active. Type /help for commands. [Ctrl+C] to stop generation.'));

  while (true) {
    printToolbar();
    const userInput = await question(chalk.bold('>> '));

    if (!userInput.trim()) continue;
    if (['exit', 'quit'].includes(userInput.toLowerCase())) break;

    // Commands
    if (userInput === '/clear') {
      messages = [];
      console.log(chalk.gray('Memory cleared.'));
      continue;
    }
    if (userInput === '/yolo') {
      YOLO_MODE = !YOLO_MODE;
      console.log(chalk.gray(`YOLO Mode: ${YOLO_MODE ? 'ON' : 'OFF'}`));
      continue;
    }
    if (userInput === '/scan') {
      const items = await fs.readdir('.');
      const snapshot = items.map(item => {
        return fs.stat(item).then(stat => stat.isDirectory() ? `📁 ${item}/` : `📄 ${item}`);
      });
      const snapshotStr = (await Promise.all(snapshot)).join('\n');
      console.log(chalk.gray(snapshotStr));
      messages.push({ role: "user", parts: [{ text: `System Notification: Directory snapshot:\n${snapshotStr}` }] });
      continue;
    }

    messages.push({ role: "user", parts: [{ text: userInput }] });

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

          if (chunk.functionCalls()) {
            functionCalls.push(...chunk.functionCalls());
          }
        }

        console.log(); // newline

        // Add model response to history
        if (accumulatedText) {
          messages.push({ role: "model", parts: [{ text: accumulatedText }] });
        }

        // Handle tool calls
        if (functionCalls.length > 0) {
          const toolParts = [];

          for (const call of functionCalls) {
            const toolName = call.name;
            const args = call.args || {};

            console.log(chalk.green(`  • Executing: ${toolName}`));
            Object.entries(args).forEach(([k, v]) => {
              const display = String(v).length > 60 ? String(v).slice(0, 57) + '...' : v;
              console.log(chalk.gray(`    └ ${k}: ${display}`));
            });

            if (tools[toolName]) {
              const resultText = await tools[toolName](...Object.values(args));
              toolParts.push({
                functionResponse: {
                  name: toolName,
                  response: { result: resultText }
                }
              });
            }
          }

          messages.push({ role: "function", parts: toolParts });
          // Continue loop to let model respond to tool results
          continue;
        }

        loop = false; // No more tool calls

      } catch (err) {
        console.error(chalk.red(`\nError: ${err.message}`));
        if (err.message.includes('quota') || err.message.includes('rate')) {
          console.log(chalk.yellow('Rate limit hit.'));
        }
        break;
      }
    }
  }

  rl.close();
}

main().catch(console.error);
