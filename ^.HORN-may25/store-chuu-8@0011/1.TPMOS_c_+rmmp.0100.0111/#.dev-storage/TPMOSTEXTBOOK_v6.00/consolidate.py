import os

output_file = "TPMOS_TEXTBOOK_v6.00_CONSOLIDATED.md"
chapters = [
    "CH1_PHILOSOPHY.md", "CH2_FILE_SYSTEM.md", "CH3_PIPELINE.md",
    "CH4_DEVELOPMENT.md", "CH5_SYSTEM_APPS.md", "CH6_PAL.md",
    "CH7_FUZZ_OP_OP_ED.md", "CH8_GL_OS.md", "CH9_TESTING.md",
    "CH10_FUTURE_HORIZONS.md", "CH11_RECURSIVE_FORGE.md", "CH12_SIMULATION_THEATER.md",
    "CH13_BUSINESS_STRATEGY.md", "CH14_SOUL_PEN.md", "CH15_CROSS_PLATFORM.md",
    "CH16_PITFALLS_DEBUGGING.md", "CH17_EXO_SOVEREIGNTY.md", "CH18_DYNAMIC_TRAIT_MENUS.md",
    "CH19_THEATER.md", "CH20_OPENGL_SHELL.md", "CH21_P2P_NET.md",
    "CH22_AI_BRAIN.md", "CH23_FINANCIAL_FORGE.md", "CH24_PROJECT_CATALOG.md",
    "CH25_MARKETING.md"
]

with open(output_file, "w", encoding="utf-8") as outfile:
    with open("INDEX.md", "r", encoding="utf-8") as index:
        outfile.write(index.read())
        outfile.write("

--- CONSOLIDATED CONTENT ---

")
    
    for chapter in chapters:
        if os.path.exists(chapter):
            outfile.write("

##################################################
")
            outfile.write("### CHAPTER: " + chapter + "
")
            outfile.write("##################################################

")
            with open(chapter, "r", encoding="utf-8") as chapfile:
                outfile.write(chapfile.read())
        else:
            print("Warning: " + chapter + " not found.")

print("Consolidation complete.")
