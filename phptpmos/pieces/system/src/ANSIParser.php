<?php
/**
 * ANSI Parser - Convert ANSI escape codes to structured data
 * 
 * Converts:
 * - \x1b[31m (red) → color: 'red'
 * - \x1b[1m (bold) → bold: true
 * - \x1b[2J (clear) → action: 'clear'
 * 
 * Output: ['lines' => [...], 'colors' => [...], 'styles' => [...]]
 */

class ANSIParser {
    
    private $defaultFg = '#aaaaaa';
    private $defaultBg = '#000000';
    
    // ANSI color palette
    private $colors = [
        30 => '#000000', // black
        31 => '#ff0000', // red
        32 => '#00ff00', // green
        33 => '#ffff00', // yellow
        34 => '#0000ff', // blue
        35 => '#ff00ff', // magenta
        36 => '#00ffff', // cyan
        37 => '#ffffff', // white
        90 => '#808080', // bright black
        91 => '#ff6060', // bright red
        92 => '#60ff60', // bright green
        93 => '#ffff60', // bright yellow
        94 => '#6060ff', // bright blue
        95 => '#ff60ff', // bright magenta
        96 => '#60ffff', // bright cyan
        97 => '#ffffff', // bright white
    ];
    
    /**
     * Parse ANSI escape sequence
     */
    public function parse($text) {
        $lines = explode("\n", $text);
        $result = [
            'lines' => [],
            'colors' => [],
            'styles' => [],
        ];
        
        $currentFg = $this->defaultFg;
        $currentBg = $this->defaultBg;
        $currentBold = false;
        $currentDim = false;
        
        foreach ($lines as $lineNum => $line) {
            $parsed = $this->parseLine($line);
            
            $result['lines'][] = $parsed['text'];
            $result['colors'][$lineNum] = $parsed['colors'];
            $result['styles'][$lineNum] = $parsed['styles'];
        }
        
        return $result;
    }
    
    /**
     * Parse single line for ANSI codes
     */
    private function parseLine($line) {
        $text = '';
        $colors = [];
        $styles = [];
        $charPos = 0;
        
        $fg = $this->defaultFg;
        $bg = $this->defaultBg;
        $bold = false;
        $dim = false;
        
        // Remove ANSI codes and track positions
        $pattern = '/\x1b\[([0-9;]*)(m|H|J|K|A|B|C|D)/';
        
        while (preg_match($pattern, $line, $matches, PREG_OFFSET_CAPTURE)) {
            // Add text before the code
            $beforeCode = substr($line, 0, $matches[0][1]);
            $text .= $beforeCode;
            
            // Track color/style changes
            $codes = explode(';', $matches[1][0]);
            foreach ($codes as $code) {
                $code = (int)$code;
                
                if ($code === 0) {
                    // Reset
                    $fg = $this->defaultFg;
                    $bg = $this->defaultBg;
                    $bold = false;
                    $dim = false;
                } elseif ($code === 1) {
                    $bold = true;
                } elseif ($code === 2) {
                    $dim = true;
                } elseif ($code >= 30 && $code <= 37) {
                    $fg = $this->colors[$code] ?? $this->defaultFg;
                } elseif ($code >= 40 && $code <= 47) {
                    $bg = $this->colors[$code - 10] ?? $this->defaultBg;
                } elseif ($code >= 90 && $code <= 97) {
                    $fg = $this->colors[$code] ?? $this->defaultFg;
                }
            }
            
            // Store colors for this character range
            $rangeEnd = strlen($text);
            for ($i = $charPos; $i < $rangeEnd; $i++) {
                $colors[$i] = ['fg' => $fg, 'bg' => $bg];
                $styles[$i] = ['bold' => $bold, 'dim' => $dim];
            }
            $charPos = $rangeEnd;
            
            // Remove processed code and continue
            $line = substr($line, $matches[0][1] + strlen($matches[0][0]));
        }
        
        // Add remaining text
        $text .= $line;
        for ($i = $charPos; $i < strlen($text); $i++) {
            $colors[$i] = ['fg' => $fg, 'bg' => $bg];
            $styles[$i] = ['bold' => $bold, 'dim' => $dim];
        }
        
        return [
            'text' => $text,
            'colors' => $colors,
            'styles' => $styles,
        ];
    }
}

?>
