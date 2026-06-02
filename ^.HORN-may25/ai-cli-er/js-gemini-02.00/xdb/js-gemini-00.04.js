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
const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);
const model = genAI.getGenerativeModel({ model: MODEL });

let YOLO_MODE = false;
let messages = [];
let cwd = process.cwd();

// ==================== BETTER PATH COMPLETER ====================
function getCompletions(line) {
  const match = line.match(/@([^\s@]*)$/);
  if (!match) return [[], line];

  const partial = match[1];
  let dirToScan = cwd;
  let prefix = partial;

  if (partial.includes('/')) {
    const lastSlash = partial.lastIndexOf('/');
    const dirPart = partial.substring(0, lastSlash + 1);
    prefix = partial.substring(lastSlash + 1);
    dirToScan = path.resolve(cwd, dirPart);
  }

  try {
    const entries = fs.readdirSync(dirToScan, { withFileTypes: true });
    const completions = entries
      .filter(entry => entry.name.toLowerCase().startsWith(prefix.toLowerCase()))
      .map(entry => {
        const base = partial.substring(0, partial.lastIndexOf('/') + 1) || '';
        const fullPath = `@${base}${entry.name}`;
        return entry.isDirectory() ? fullPath + '/' : fullPath;
      });

    return [completions, line];
  } catch {
    return [[], line];
  }
}

// ==================== TOOLS ====================
async function web_search(query) {
  try {
    const res = await fetch(`https://api.duckduckgo.com/?q=${encodeURIComponent(query)}&format=json&no_html=1`);
    const data = await res.json();
    return data.RelatedTopics?.slice(0, 5).map(t => `Title: ${t.Text}\nURL: ${t.FirstURL}`).join('\n\n') || "No results";
  } catch (e) { return `Search Error: ${e.message}`; }
}

async function write_file(filename, content) {
  try { await fs.writeFile(filename, content, 'utf8'); return `Wrote ${filename}`; } 
  catch (e) { return `Error: ${e.message}`; }
}

async function read_file(filename) {
  try { return await fs.readFile(filename, 'utf8'); } 
  catch (e) { return `Error: ${e.message}`; }
}

async function edit_file(filename, search_block, replace_block) {
  try {
    let content = await fs.readFile(filename, 'utf8');
    if (!content.includes(search_block.trim())) return "Error: Search block not found";
    content = content.replace(search_block.trim(), replace_block.trim());
    await fs.writeFile(filename, content, 'utf8');
    return `Edited ${filename}`;
  } catch (e) { return `Error: ${e.message}`; }
}

async function search_in_files(query, file_pattern = "*") {
  // Simplified version
  const results = [];
  async function walk(dir) {
    const items = await fs.readdir(dir, { withFileTypes: true });
    for (const item of items) {
      const full = path.join(dir, item.name);
      if (item.isDirectory()) await walk(full);
      else if (item.isFile()) {
        try {
          const data = await fs.readFile(full, 'utf8');
          if (data.includes(query)) results.push(`${full}: contains "${query}"`);
        } catch {}
      }
    }
  }
  await walk('.');
  return results.slice(0, 15).join('\n') || "No matches";
}

async function run_command(command) {
  if (!YOLO_MODE) {
    const answer = await new Promise(r => rl.question(chalk.yellow(`\nRun command? ${chalk.white(command)} (y/n): `), r));
    if (answer.toLowerCase() !== 'y') return "Permission denied.";
  }
  return new Promise(r => exec(command, { cwd }, (_, out, err) => r(`STDOUT:\n${out}\nSTDERR:\n${err}`)));
}

async function change_directory(p) {
  try { process.chdir(p); cwd = process.cwd(); return `Now in: ${cwd}`; } 
  catch (e) { return `Error: ${e.message}`; }
}

async function list_dir(d = ".") {
  try { return (await fs.readdir(d)).join('\n'); } 
  catch (e) { return `Error: ${e.message}`; }
}

const tools = { web_search, write_file, read_file, edit_file, search_in_files, run_command, change_directory, list_dir };

const toolDeclarations = [ /* Add all your tool declarations here - same as previous messages */ ];

const SYSTEM_PROMPT = "You are Aida, expert coding agent. Always explore the local files first.";

// ==================== CLI SETUP ====================
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  completer: getCompletions,
  prompt: chalk.bold('>> '),
  terminal: true
});

rl.on('SIGINT', () => process.exit(0));

function printToolbar() {
  console.log(chalk.gray(`\n📂 ${path.basename(cwd)} | Mode: ${YOLO_MODE ? 'YOLO' : 'SAFE'}`));
}

// ==================== MAIN ====================
async function main() {
  console.log(chalk.blue.bold('\nAida Streaming Terminal'));
  console.log(chalk.gray('• Type @ then press Tab (or Tab twice) for path completion\n'));

  while (true) {
    printToolbar();
    const userInput = await new Promise(res => rl.question(chalk.bold('>> '), res));

    const input = userInput.trim();
    if (!input) continue;
    if (['exit', 'quit'].includes(input.toLowerCase())) break;

    if (input === '/clear') { messages = []; console.log(chalk.gray('Memory cleared')); continue; }
    if (input === '/yolo') { YOLO_MODE = !YOLO_MODE; console.log(chalk.gray(`YOLO Mode ${YOLO_MODE ? 'ON' : 'OFF'}`)); continue; }
    if (input === '/help') { console.log(chalk.cyan('\nCommands: /clear, /yolo, /scan, /help, exit')); continue; }

    messages.push({ role: "user", parts: [{ text: input }] });

    // Generation + Tool loop (same as before)
    let loop = true;
    while (loop) {
      try {
        const result = await model.generateContentStream({
          contents: messages,
          systemInstruction: SYSTEM_PROMPT,
          tools: [{ functionDeclarations: toolDeclarations }]
        });

        let accumulated = '';
        let calls = [];

        for await (const chunk of result.stream) {
          const t = chunk.text();
          if (t) {
            accumulated += t;
            process.stdout.write(t);
          }
          if (chunk.functionCalls()) calls.push(...chunk.functionCalls());
        }
        console.log();

        if (accumulated) messages.push({ role: "model", parts: [{ text: accumulated }] });

        if (calls.length) {
          const toolParts = [];
          for (const call of calls) {
            const name = call.name;
            const args = call.args || {};
            console.log(chalk.green(`  • Executing ${name}`));
            const res = await tools[name]?.(...Object.values(args)) || "Tool not found";
            toolParts.push({ functionResponse: { name, response: { result: res } } });
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
