import os
import sys
import subprocess

# macOS Font Paths
LATIN_FONT = "/System/Library/Fonts/Supplemental/Arial.ttf"
LATIN_FONT_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
CJK_FONT = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"

def test_fonts():
    try:
        from fpdf import FPDF
    except ImportError:
        print("fpdf2 is not installed. Attempting to install...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "fpdf2"])
        from fpdf import FPDF

    pdf = FPDF()
    
    # Check if fonts exist
    for f in [LATIN_FONT, LATIN_FONT_BOLD, CJK_FONT]:
        if not os.path.exists(f):
            print(f"CRITICAL ERROR: Font not found: {f}")
            return False

    pdf.add_font("Main", "", LATIN_FONT)
    pdf.add_font("Main", "B", LATIN_FONT_BOLD)
    pdf.add_font("CJK", "", CJK_FONT)
    pdf.set_fallback_fonts(["CJK"])
    
    pdf.add_page()
    pdf.set_font("Main", "B", 16)
    pdf.cell(0, 10, "TPMOS Font Test v6.00")
    pdf.ln(10)
    
    pdf.set_font("Main", "", 12)
    pdf.cell(0, 10, "Testing Latin and Emojis (Fallback):")
    pdf.ln(10)
    
    test_text = "Hello World! [B 书] [GRAD 学] 🚀 🧱 🧘‍♂️"
    # Note: fpdf2 uses the emoji mapping or CJK fallback
    # The original script has an EMOJI_MAP. We should test that too.
    
    EMOJI_MAP = {"📚": "[B 书]", "🎓": "[GRAD 学]", "🚀": "[LAUNCH 发]", "🧱": "[PIECE 砖]", "🧘‍♂️": "[ZEN 禅]"}
    def clean(text):
        for k, v in EMOJI_MAP.items():
            text = text.replace(k, v)
        return text

    pdf.multi_cell(0, 10, clean(test_text))
    pdf.ln(10)
    
    pdf.cell(0, 10, "Testing Raw CJK:")
    pdf.ln(10)
    pdf.multi_cell(0, 10, "基于件的系统指南 - Sovereign Computing")
    
    output = "font_test_v6.pdf"
    pdf.output(output)
    print(f"Test PDF generated: {output}")
    return True

if __name__ == "__main__":
    if test_fonts():
        sys.exit(0)
    else:
        sys.exit(1)
