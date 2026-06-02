#!/usr/bin/env node
import { GoogleGenerativeAI } from '@google/generative-ai';
import fs from 'fs/promises';
import path from 'path';
import { exec } from 'child_process';
import chalk from 'chalk';
import Enquirer from 'enquirer';

const { prompt } = Enquirer;

const MODEL = 'gemini-2.5-flash';
const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);
const model = genAI.getGenerativeModel({ model: MODEL });

let YOLO_MODE = false;
let messages = [];
let cwd = process.cwd();

// ==================== PATH AUTOCOMPLETE ====================
async function pathCompleter(input) {
  if (!input.startsWith('@')) return [];

  const partial = input.slice(1); // remove @
  let dirToScan = cwd;
  let prefix = partial;

  if (partial.includes('/')) {
    const lastSlash = partial.lastIndexOf('/');
    const dirPart = partial.substring(0, lastSlash + 1);
    prefix = partial.substring(lastSlash + 1);
    dirToScan = path.resolve(cwd, dirPart);
  }

  try {
    const entries = await fs.readdir(dirToScan, { withFileTypes: true });
    return entries
      .filter(e => e.name.toLowerCase().startsWith(prefix.toLowerCase()))
      .map(entry => {
        const base = partial.substring(0, partial.lastIndexOf('/') + 1);
        const full = `@${base}${entry.name}`;
        return entry.isDirectory() ? full + '/' : full;
      });
  } catch {
    return [];
  }
}

// ==================== TOOLS (shortened for brevity) ====================
async function list_dir(dir = ".") {
  try {
    const items = await fs.readdir(dir);
    return items.join('\n') || "Empty.";
  } catch (e) { return `Error: ${e.message}`; }
}

async function read_file(filename) { /* ... same as before */ }
async function write_file(filename, content) { /* ... */ }
async function edit_file(filename, search_block, replace_block) { /* ... */ }
async function search_in_files(query, file_pattern = "*") { /* ... */ }
async function run_command(command) { /* ... permission logic ... */ }
async function change_directory(newPath) { /* ... */ }
async function web_search(query) { /* ... */ }

const tools = { web_search, write_file, read_file, edit_file, search_in_files, run_command, change_directory, list_dir };

const toolDeclarations = [ /* same tool definitions as previous version */ ];

const SYSTEM_PROMPT = "You are Aida, an expert technical coding agent..."; // keep your original

// ==================== MAIN LOOP ====================
async function main() {
  console.log(chalk.blue.bold('\nAida Streaming Terminal'));
  console.log(chalk.gray('Type @ to trigger smart path autocomplete\n'));

  while (true) {
    console.log(chalk.gray(`📂 ${path.basename(cwd)} | Mode: ${YOLO_MODE ? 'YOLO' : 'SAFE'}`));

    const { input } = await prompt({
      type: 'autocomplete',
      name: 'input',
      message: '>>',
      limit: 12,
      choices: [],
      suggest: async (input) => {
        if (input.startsWith('@')) {
          return await pathCompleter(input);
        }
        return [];
      }
    });

    const userInput = input.trim();
    if (!userInput) continue;
    if (['exit', 'quit'].includes(userInput.toLowerCase())) break;

    // Handle commands
    if (userInput === '/clear') { messages = []; console.log(chalk.gray('Memory cleared.')); continue; }
    if (userInput === '/yolo') { YOLO_MODE = !YOLO_MODE; console.log(chalk.gray(`YOLO: ${YOLO_MODE ? 'ON' : 'OFF'}`)); continue; }
    if (userInput === '/help') { console.log(chalk.cyan('\nCommands: /clear /yolo /scan /help exit')); continue; }
    if (userInput === '/scan') { /* add scan logic */ continue; }

    // Normal message
    messages.push({ role: "user", parts: [{ text: userInput }] });

    // ... (rest of your generation + tool calling loop stays the same)
    let loop = true;
    while (loop) {
      try {
        const result = await model.generateContentStream({
          contents: messages,
          systemInstruction: SYSTEM_PROMPT,
          tools: [{ functionDeclarations: toolDeclarations }]
        });

        let text = '';
        let functionCalls = [];

        for await (const chunk of result.stream) {
          const chunkText = chunk.text();
          if (chunkText) {
            text += chunkText;
            process.stdout.write(chunkText);
          }
          if (chunk.functionCalls()) functionCalls.push(...chunk.functionCalls());
        }
        console.log();

        if (text) messages.push({ role: "model", parts: [{ text }] });

        if (functionCalls.length > 0) {
          // Tool execution logic (same as before)
          const toolParts = [];
          for (const call of functionCalls) {
            const name = call.name;
            const args = call.args || {};
            console.log(chalk.green(`  • ${name}`));
            const resultText = await tools[name](...Object.values(args));
            toolParts.push({ functionResponse: { name, response: { result: resultText } } });
          }
          messages.push({ role: "function", parts: toolParts });
          continue;
        }
        loop = false;
      } catch (err) {
        console.error(chalk.red('Error:', err.message));
        break;
      }
    }
  }
}

main().catch(console.error);
