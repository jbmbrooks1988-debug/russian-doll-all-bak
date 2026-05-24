/**
 * parser.js - The PMO OS Parser (The "Theater")
 * Strictly emulates chtpm_parser.c text-only composition.
 * 
 * C-PARITY FIXES:
 * - Menu handling: only <menu> is interactive, children hidden until opened
 * - Navigation: proper [>] vs [ ] focus indicators
 * - href support: page transitions
 * - INTERACT mode: all keys to module, ESC exits
 * - Variable loading: from state.txt and view.txt
 */

class CHTPMParser {
    constructor() {
        this.vars = {};
        this.focusIndex = 0;
        this.activeIndex = -1; // -1 = navigation mode, >=0 = active element
        this.navBuffer = "";
        this.clearNavOnNext = false;
        this.elements = [];
        this.currentLayout = 'pieces/chtpm/layouts/os.chtpm';
        this.menuChildFocus = {}; // Track focus within open menus
        this.interactiveCount = 0;
    }

    setVars(newVars) {
        this.vars = { ...this.vars, ...newVars };
    }

    substitute(text) {
        if (!text) return "";
        return text.replace(/\${([^}]+)}/g, (match, varName) => {
            return this.vars[varName] !== undefined ? this.vars[varName] : match;
        });
    }

    /**
     * Load variables from piece state files (mirrors C's load_vars())
     */
    async loadVars() {
        // 1. Load Defaults from PDL (DNA)
        try {
            const dnaRes = await fetch(`api/bridge.php?action=read&path=pieces/apps/fuzzpet_app/fuzzpet/fuzzpet.pdl`);
            const dnaData = await dnaRes.json();
            if (dnaData.content) {
                dnaData.content.split('\n').forEach(line => {
                    if (line.startsWith('STATE')) {
                        const parts = line.split('|');
                        if (parts.length >= 3) {
                            const key = parts[1].trim();
                            const val = parts[2].trim();
                            this.vars[`fuzzpet_${key}`] = val;
                        }
                    }
                });
            }
        } catch (e) { /* ignore */ }

        // 2. Load fuzzpet state from Mirror (state.txt)
        try {
            const mirrorRes = await fetch(`api/bridge.php?action=read&path=pieces/apps/fuzzpet_app/fuzzpet/state.txt`);
            const mirrorData = await mirrorRes.json();
            if (mirrorData.content) {
                mirrorData.content.split('\n').forEach(line => {
                    const eq = line.indexOf('=');
                    if (eq > 0) {
                        const key = line.substring(0, eq).trim();
                        const val = line.substring(eq + 1).trim();
                        this.vars[`fuzzpet_${key}`] = val;
                    }
                });
            }
        } catch (e) { /* ignore */ }

        // 3. Load clock state
        try {
            const clockRes = await fetch(`api/bridge.php?action=read&path=pieces/apps/fuzzpet_app/clock/state.txt`);
            const clockData = await clockRes.json();
            if (clockData.content) {
                clockData.content.split('\n').forEach(line => {
                    line = line.trim();
                    if (line.startsWith('turn')) this.vars['clock_turn'] = line.split(' ')[1];
                    if (line.startsWith('time')) this.vars['clock_time'] = line.split(' ')[1];
                    // Also handle key=value format if it exists
                    const eq = line.indexOf('=');
                    if (eq > 0) {
                        const key = line.substring(0, eq).trim();
                        const val = line.substring(eq + 1).trim();
                        this.vars[`clock_${key}`] = val;
                    }
                });
            }
        } catch (e) { /* ignore */ }

        // 4. Load display state
        try {
            const displayRes = await fetch(`api/bridge.php?action=read&path=pieces/display/state.txt`);
            const displayData = await displayRes.json();
            if (displayData.content) {
                displayData.content.split('\n').forEach(line => {
                    const eq = line.indexOf('=');
                    if (eq > 0) {
                        const key = line.substring(0, eq).trim();
                        const val = line.substring(eq + 1).trim();
                        this.vars[`display_${key}`] = val;
                    }
                });
            }
        } catch (e) { /* ignore */ }

        // 5. Load module's sovereign view (view.txt) as ${game_map}
        try {
            const viewRes = await fetch(`api/bridge.php?action=read&path=pieces/apps/fuzzpet_app/fuzzpet/view.txt`);
            const viewData = await viewRes.json();
            if (viewData.content) {
                this.vars['game_map'] = viewData.content;
            }
        } catch (e) { /* ignore */ }
    }

    /**
     * Parse layout and build element tree (mirrors C's parse_chtm())
     */
    parseLayout(layoutContent) {
        this.elements = [];
        this.interactiveCount = 0;
        const stack = [];

        // Remove comments
        let cleanContent = layoutContent.replace(/<!--[\s\S]*?-->/g, '');
        
        // Split by tags but keep the tags
        const parts = cleanContent.split(/(<[^>]+>)/g);

        for (let part of parts) {
            if (!part.trim()) continue;
            
            if (part.startsWith('<') && !part.startsWith('</')) {
                // Opening tag
                const tagNameMatch = part.match(/<([^\s>/]+)/);
                if (!tagNameMatch) continue;
                const tagName = tagNameMatch[1].toLowerCase();

                if (tagName === 'panel') {
                    stack.push(-1);
                    continue;
                }

                // Extract attributes
                const labelMatch = part.match(/label="([^"]*)"/);
                const label = labelMatch ? labelMatch[1] : "";
                const hrefMatch = part.match(/href="([^"]*)"/);
                const href = hrefMatch ? hrefMatch[1] : "";
                const onClickMatch = part.match(/onClick="([^"]*)"/);
                const onClick = onClickMatch ? onClickMatch[1] : "";
                const visibilityMatch = part.match(/visibility="([^"]*)"/);
                // C-PARITY: Default visibility is false (0) to match C's memset(el, 0) behavior
                const visibility = visibilityMatch ? (visibilityMatch[1] !== "0") : false;

                const element = {
                    type: tagName,
                    label: label,
                    href: href,
                    onClick: onClick,
                    parentIndex: stack.length > 0 ? stack[stack.length - 1] : -1,
                    interactiveIndex: 0,
                    children: [],
                    visibility: visibility,
                    input_buffer: ""
                };

                // Count interactive elements (C-PARITY: All interactive elements get a global index)
                if (this.isInteractive(element)) {
                    this.interactiveCount++;
                    element.interactiveIndex = this.interactiveCount;
                }

                // Add to parent's children list
                if (element.parentIndex >= 0 && this.elements[element.parentIndex]) {
                    this.elements[element.parentIndex].children.push(this.elements.length);
                }

                this.elements.push(element);

                if (tagName === 'menu' || tagName === 'panel') {
                    stack.push(this.elements.length - 1);
                }
            } else if (part.startsWith('</')) {
                // Closing tag
                const tagNameMatch = part.match(/<\/([^\s>]+)/);
                if (tagNameMatch) {
                    const closeTag = tagNameMatch[1].toLowerCase();
                    if (closeTag === 'menu' || closeTag === 'panel' || closeTag === 'button' || closeTag === 'link' || closeTag === 'text') {
                        if (stack.length > 0) stack.pop();
                    }
                }
            } else if (part.startsWith('<br')) {
                // br tag - add as element
                const element = {
                    type: 'br',
                    label: '',
                    href: '',
                    onClick: '',
                    parentIndex: stack.length > 0 ? stack[stack.length - 1] : -1,
                    interactiveIndex: 0,
                    children: [],
                    visibility: true
                };
                if (element.parentIndex >= 0 && this.elements[element.parentIndex]) {
                    this.elements[element.parentIndex].children.push(this.elements.length);
                }
                this.elements.push(element);
            } else {
                // Text content between tags
                const trimmed = part;
                if (trimmed) {
                    const element = {
                        type: 'text',
                        label: trimmed,
                        href: '',
                        onClick: '',
                        parentIndex: stack.length > 0 ? stack[stack.length - 1] : -1,
                        interactiveIndex: 0,
                        children: [],
                        visibility: true
                    };
                    if (element.parentIndex >= 0 && this.elements[element.parentIndex]) {
                        this.elements[element.parentIndex].children.push(this.elements.length);
                    }
                    this.elements.push(element);
                }
            }
        }
    }

    /**
     * Check if element is interactive (mirrors C's is_interactive())
     */
    isInteractive(el) {
        return el && (el.type === 'button' || el.type === 'link' || el.type === 'cli_io' || el.type === 'menu');
    }

    /**
     * Check if element is navigable (mirrors C's is_navigable())
     */
    isNavigable(idx) {
        if (idx < 0 || idx >= this.elements.length) return false;
        return this.isInteractive(this.elements[idx]);
    }

    /**
     * Initialize focus to first navigable element
     */
    initializeFocus() {
        if (this.elements.length > 0 && !this.isNavigable(this.focusIndex)) {
            for (let i = 0; i < this.elements.length; i++) {
                if (this.isNavigable(i)) {
                    this.focusIndex = i;
                    break;
                }
            }
        }
    }

    /**
     * Render a single element to the frame string
     */
    renderElement(idx, frameParts) {
        if (idx < 0 || idx >= this.elements.length) return;
        const el = this.elements[idx];

        // Visibility check (C-PARITY)
        if (el.parentIndex !== -1) {
            const p = this.elements[el.parentIndex];
            if (p.type === 'menu' && !p.visibility && this.activeIndex !== el.parentIndex) return;
        }

        // Substitute variables in label
        let substituted = this.substitute(el.label);

        if (el.type === 'text') {
            frameParts.push(substituted);
        } else if (el.type === 'br') {
            frameParts.push('\n');
        } else if (this.isInteractive(el)) {
            // Determine focus state
            let isFocused = (this.activeIndex === -1 && this.focusIndex === idx);
            
            // Check if this is a child of an open menu
            if (el.parentIndex >= 0) {
                const parent = this.elements[el.parentIndex];
                if (parent && parent.type === 'menu' && this.activeIndex === el.parentIndex) {
                    isFocused = false; 
                    const menuFocus = this.menuChildFocus[el.parentIndex] || 0;
                    if (parent.children[menuFocus] === idx) {
                        isFocused = true;
                    }
                }
            }

            let indicator;
            if (idx === this.activeIndex) {
                indicator = "[^]";
            } else if (isFocused) {
                indicator = "[>]";
            } else {
                indicator = "[ ]";
            }

            let line;
            if (el.type === 'cli_io') {
                line = `${indicator} ${el.interactiveIndex}. ${substituted}${el.input_buffer}_ `;
            } else {
                line = `${indicator} ${el.interactiveIndex}. [${substituted}] `;
            }

            // Indent children
            if (el.parentIndex >= 0) {
                line = "  " + line;
            }

            frameParts.push(line);
        }

        // Render children for menu (when open or visible) or panel
        if ((el.type === 'menu' || el.type === 'panel') && (el.parentIndex === -1 || el.visibility || this.activeIndex === idx)) {
            for (const childIdx of el.children) {
                this.renderElement(childIdx, frameParts);
            }
        }
    }

    /**
     * Compose the full frame (mirrors C's compose_frame())
     */
    async composeFrame(layoutContent) {
        await this.loadVars();
        this.parseLayout(layoutContent);
        this.initializeFocus();

        const frameParts = [];
        for (let i = 0; i < this.elements.length; i++) {
            if (this.elements[i].parentIndex === -1) {
                this.renderElement(i, frameParts);
            }
        }

        let output = frameParts.join('');
        
        // Add footer
        output += '\n╚═══════════════════════════════════════════════════════════╝\n';
        
        if (this.activeIndex === -1) {
            output += `Nav > ${this.navBuffer}_`;
        } else {
            const el = this.elements[this.activeIndex];
            if (el.onClick === "INTERACT") {
                output += `Active [^]: ${this.navBuffer} (ESC to exit)`;
            } else {
                output += `Active [^]: ${el.label} (ESC to exit)`;
            }
        }
        output += '\n';

        if (this.clearNavOnNext) {
            this.navBuffer = "";
            this.clearNavOnNext = false;
        }

        return output;
    }

    /**
     * Handle navigation key (mirrors C's process_key())
     */
    async navigate(key) {
        const ARROW_UP = 1002, ARROW_DOWN = 1003, ARROW_LEFT = 1000, ARROW_RIGHT = 1001;
        const ESC_KEY = 27;

        if (this.activeIndex === -1) {
            if (key === ARROW_UP || key === 119 || key === 'w' || key === 'W') {
                let prev = this.focusIndex;
                do { this.focusIndex--; } while (this.focusIndex >= 0 && !this.isNavigable(this.focusIndex));
                if (this.focusIndex < 0) this.focusIndex = prev;
                this.navBuffer = 'w';
                this.clearNavOnNext = true;
                return true;
            } else if (key === ARROW_DOWN || key === 115 || key === 's' || key === 'S') {
                let prev = this.focusIndex;
                do { this.focusIndex++; } while (this.focusIndex < this.elements.length && !this.isNavigable(this.focusIndex));
                if (this.focusIndex >= this.elements.length) this.focusIndex = prev;
                this.navBuffer = 's';
                this.clearNavOnNext = true;
                return true;
            } else if (key === ARROW_LEFT || key === 97 || key === 'a' || key === 'A') {
                this.navBuffer = 'a'; this.clearNavOnNext = true; return true;
            } else if (key === ARROW_RIGHT || key === 100 || key === 'd' || key === 'D') {
                this.navBuffer = 'd'; this.clearNavOnNext = true; return true;
            } else if (key >= 48 && key <= 57) { // Number keys 0-9
                const digit = String.fromCharCode(key);
                if (this.navBuffer.length >= 10 || isNaN(this.navBuffer)) this.navBuffer = "";
                this.navBuffer += digit;
                
                const targetNum = parseInt(this.navBuffer);
                let found = false;
                for (let i = 0; i < this.elements.length; i++) {
                    if (this.elements[i].interactiveIndex === targetNum) {
                        this.focusIndex = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Try single digit jump if combined fails
                    const single = parseInt(digit);
                    for (let i = 0; i < this.elements.length; i++) {
                        if (this.elements[i].interactiveIndex === single) {
                            this.focusIndex = i;
                            this.navBuffer = digit;
                            break;
                        }
                    }
                }
                return true;
            } else if (key === 10 || key === 13) { // Enter
                if (this.elements.length > 0 && this.focusIndex < this.elements.length) {
                    const el = this.elements[this.focusIndex];
                    this.navBuffer = '';
                    
                    if (el.href && el.href.length > 0) {
                        this.currentLayout = el.href;
                        this.focusIndex = 0;
                        this.activeIndex = -1;
                        this.menuChildFocus = {};
                        return 'layout_change';
                    } else if (el.type === 'menu') {
                        this.activeIndex = this.focusIndex;
                        this.menuChildFocus[this.focusIndex] = 0;
                        return true;
                    } else {
                        this.activeIndex = this.focusIndex;
                        if (el.type !== 'cli_io' && el.onClick !== 'INTERACT') {
                            this.activeIndex = -1;
                        }
                        return el.onClick || true;
                    }
                }
            } else if (key === 127 || key === 8) { // Backspace
                if (this.navBuffer.length > 0) this.navBuffer = this.navBuffer.slice(0, -1);
                return true;
            } else if (key === 'q' || key === 'Q' || key === 113) {
                return 'quit';
            }
        } else {
            // ACTIVE MODE
            const el = this.elements[this.activeIndex];
            if (key === ESC_KEY) {
                this.activeIndex = -1;
                this.navBuffer = '';
                return 'exit_interact';
            }
            
            if (el.type === 'menu') {
                if (key === ARROW_UP || key === 119 || key === 'w' || key === 'W') {
                    let focus = this.menuChildFocus[this.activeIndex] || 0;
                    if (focus > 0) this.menuChildFocus[this.activeIndex] = focus - 1;
                    return true;
                } else if (key === ARROW_DOWN || key === 115 || key === 's' || key === 'S') {
                    let focus = this.menuChildFocus[this.activeIndex] || 0;
                    if (focus < el.children.length - 1) this.menuChildFocus[this.activeIndex] = focus + 1;
                    return true;
                } else if (key === 10 || key === 13) {
                    let focus = this.menuChildFocus[this.activeIndex] || 0;
                    const childIdx = el.children[focus];
                    const child = this.elements[childIdx];
                    if (child.href) {
                        this.currentLayout = child.href;
                        this.focusIndex = 0;
                        this.activeIndex = -1;
                        return 'layout_change';
                    }
                    return child.onClick || true;
                }
            } else if (el.type === 'cli_io') {
                if (key === 10 || key === 13) {
                    const val = el.input_buffer;
                    el.input_buffer = "";
                    this.activeIndex = -1;
                    return `CLI:${val}`;
                } else if (key === 127 || key === 8) {
                    el.input_buffer = el.input_buffer.slice(0, -1);
                    return true;
                } else if (key >= 32 && key <= 126) {
                    el.input_buffer += String.fromCharCode(key);
                    return true;
                }
            } else if (el.onClick === 'INTERACT') {
                if (key >= 32 && key <= 126) {
                    this.navBuffer = String.fromCharCode(key);
                } else {
                    this.navBuffer = '[Key]';
                }
                this.clearNavOnNext = true;
                return key; // Forward key code
            }
        }
        return false;
    }

    /**
     * Handle key in INTERACT mode (mirrors C's process_key() for INTERACT)
     */
    async handleInteractKey(key) {
        const ESC_KEY = 27;

        if (this.activeIndex < 0) return null;
        const el = this.elements[this.activeIndex];

        if (key === ESC_KEY) {
            this.activeIndex = -1;
            this.navBuffer = '';
            return 'exit_interact';
        }

        if (el.onClick === 'INTERACT') {
            // Forward all keys to module
            if (key >= 32 && key <= 126) {
                this.navBuffer = String.fromCharCode(key);
            } else {
                this.navBuffer = '[Key]';
            }
            this.clearNavOnNext = true;
            return key; // Return key code to forward to module
        }

        return null;
    }

    /**
     * Get current layout path
     */
    getCurrentLayout() {
        return this.currentLayout;
    }

    /**
     * Set layout (for href navigation)
     */
    setLayout(path) {
        this.currentLayout = path;
        this.focusIndex = 0;
        this.activeIndex = -1;
        this.menuChildFocus = {};
    }

    /**
     * Get active element's onClick command
     */
    getActiveOnClick() {
        if (this.activeIndex >= 0 && this.activeIndex < this.elements.length) {
            return this.elements[this.activeIndex].onClick;
        }
        return null;
    }
}
