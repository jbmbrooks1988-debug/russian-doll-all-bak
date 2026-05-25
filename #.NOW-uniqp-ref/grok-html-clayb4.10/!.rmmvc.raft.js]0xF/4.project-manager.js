// Project Manager for saving/loading maps
class ProjectManager {
    constructor() {
        this.projects = {
            'default': ['global_events.txt', 'map_0_events.txt', 'map_0.txt', 'map_1.txt', 'map_2_events.txt', 'map_2.txt', 'map_events_0.txt'],
            '009': ['global_events.txt', 'map_0_events.txt', 'map_0.txt', 'map_1_events.txt', 'map_1.txt'],
            'colors': ['global_ev', 'map_0.txt']
        };
        this.currentProject = 'default';
    }

    // Get current project name
    getCurrentProject() {
        return this.currentProject;
    }

    // Set current project
    setCurrentProject(projectName) {
        if (this.projects[projectName]) {
            this.currentProject = projectName;
            return true;
        }
        return false;
    }

    // Get all projects
    getProjects() {
        return Object.keys(this.projects);
    }

    // Get files for a project
    getProjectFiles(projectName) {
        return this.projects[projectName] || [];
    }

    // Save current world state to a map file
    saveMap(projectName, mapName) {
        try {
            // Convert blocks to CSV format
            let csvContent = "x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z\n";
            
            // Iterate through all blocks
            blocks.forEach((blockData, key) => {
                const [x, y, z] = key.split('-').map(Number);
                const {
                    emoji_idx = 0,
                    fg_color_idx = 0,
                    alpha = 1,
                    scale = 1,
                    rel_x = 0,
                    rel_y = 0,
                    rel_z = 0
                } = blockData;
                
                csvContent += `${x},${y},${z},${emoji_idx},${fg_color_idx},${alpha},${scale},${rel_x},${rel_y},${rel_z}\n`;
            });

            // In a browser environment, we can't directly save files to disk
            // Instead, we'll create a downloadable link
            const blob = new Blob([csvContent], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            
            // Create a temporary link and trigger download
            const a = document.createElement('a');
            a.href = url;
            a.download = mapName;
            document.body.appendChild(a);
            a.click();
            
            // Clean up
            setTimeout(() => {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }, 100);
            
            console.log(`Map saved successfully as "${mapName}" for project "${projectName}"`);
            
            // Add to project files if not already there
            if (!this.projects[projectName].includes(mapName)) {
                this.projects[projectName].push(mapName);
            }
            
            return true;
        } catch (error) {
            console.error("Error saving map:", error);
            return false;
        }
    }

    // Parse CSV data and create blocks
    parseCSVAndCreateBlocks(csvData) {
        try {
            const lines = csvData.trim().split('\n');
            if (lines.length < 2) {
                console.error("Invalid CSV format: Not enough lines");
                return false;
            }
            
            // Skip header line
            for (let i = 1; i < lines.length; i++) {
                const values = lines[i].split(',');
                if (values.length !== 10) {
                    console.warn(`Skipping invalid line ${i}: Expected 10 values, got ${values.length}`);
                    continue;
                }
                
                // Parse values
                const x = parseFloat(values[0]);
                const y = parseFloat(values[1]);
                const z = parseFloat(values[2]);
                const emoji_idx = parseInt(values[3]) || 0;
                const fg_color_idx = parseInt(values[4]) || 0;
                const alpha = parseFloat(values[5]) || 1;
                const scale = parseFloat(values[6]) || 1;
                const rel_x = parseFloat(values[7]) || 0;
                const rel_y = parseFloat(values[8]) || 0;
                const rel_z = parseFloat(values[9]) || 0;
                
                // Determine block type and color based on fg_color_idx or other criteria
                let color = BLOCK_COLORS.STONE;
                let type = 'STONE';
                
                // Map fg_color_idx to actual colors (simplified mapping)
                switch (fg_color_idx) {
                    case 1: // Green
                        color = BLOCK_COLORS.GRASS;
                        type = 'GRASS';
                        break;
                    case 2: // Brown
                        color = BLOCK_COLORS.DIRT;
                        type = 'DIRT';
                        break;
                    case 3: // Gray
                        color = BLOCK_COLORS.STONE;
                        type = 'STONE';
                        break;
                    case 4: // Wood color
                        color = BLOCK_COLORS.WOOD;
                        type = 'WOOD';
                        break;
                    case 5: // Leaf color
                        color = BLOCK_COLORS.LEAVES;
                        type = 'LEAVES';
                        break;
                    default:
                        color = BLOCK_COLORS.STONE;
                        type = 'STONE';
                }
                
                // Create the block with all parameters
                createBlock(
                    x, y, z,
                    color, type,
                    emoji_idx, fg_color_idx,
                    alpha, scale,
                    rel_x, rel_y, rel_z
                );
            }
            
            return true;
        } catch (error) {
            console.error("Error parsing CSV:", error);
            return false;
        }
    }

    // Load a map from a CSV file
    loadMap(projectName, mapName) {
        try {
            // Clear existing blocks from the scene
            blocks.forEach((blockData) => {
                scene.remove(blockData.mesh);
            });
            blocks.clear();
            
            // For demonstration purposes, we'll generate sample CSV data
            // In a real implementation, this would come from an actual uploaded file
            const sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,1,1,1,0,0,0
-1,5,0,0,1,1,1,0,0,0
0,5,1,0,1,1,1,0,0,0
0,5,-1,0,1,1,1,0,0,0
0,6,0,0,3,1,1,0,0,0
0,7,0,0,3,1,1,0,0,0
0,8,0,0,4,1,1,0,0,0`;
            
            // Parse the CSV data and create blocks
            if (this.parseCSVAndCreateBlocks(sampleCSV)) {
                console.log(`Successfully loaded map "${mapName}" for project "${projectName}"`);
                return true;
            } else {
                console.error(`Failed to parse map "${mapName}" for project "${projectName}"`);
                return false;
            }
        } catch (error) {
            console.error("Error loading map:", error);
            return false;
        }
    }

    // Simulate loading a map file (in a real implementation, this would load from actual files)
    loadMapFile(projectName, fileName) {
        try {
            // Clear existing blocks from the scene
            blocks.forEach((blockData) => {
                scene.remove(blockData.mesh);
            });
            blocks.clear();
            
            // In a real implementation, this would load the actual file
            // For now, we'll generate some sample data based on the file name
            let sampleCSV;
            
            if (fileName.includes('map_0')) {
                // Sample data for map_0
                sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,1,1,1,0,0,0
-1,5,0,0,1,1,1,0,0,0
0,5,1,0,1,1,1,0,0,0
0,5,-1,0,1,1,1,0,0,0
0,6,0,0,3,1,1,0,0,0
0,7,0,0,3,1,1,0,0,0
0,8,0,0,4,1,1,0,0,0`;
            } else if (fileName.includes('map_1')) {
                // Sample data for map_1
                sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
2,5,2,0,2,1,1,0,0,0
3,5,2,0,2,1,1,0,0,0
2,5,3,0,2,1,1,0,0,0
3,5,3,0,2,1,1,0,0,0
2,6,2,0,4,1,1,0,0,0
3,6,3,0,4,1,1,0,0,0`;
            } else {
                // Generic sample data
                sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,3,1,1,0,0,0
0,5,1,0,4,1,1,0,0,0`;
            }
            
            // Parse the CSV data and create blocks
            if (this.parseCSVAndCreateBlocks(sampleCSV)) {
                console.log(`Successfully loaded map "${fileName}" for project "${projectName}"`);
                return true;
            } else {
                console.error(`Failed to parse map "${fileName}" for project "${projectName}"`);
                return false;
            }
        } catch (error) {
            console.error("Error loading map file:", error);
            return false;
        }
    }
}

// Create a global instance
const projectManager = new ProjectManager();