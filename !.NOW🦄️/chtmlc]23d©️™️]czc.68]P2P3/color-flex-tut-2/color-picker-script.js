const colors = [
    [255, 0, 0],    // Red
    [0, 255, 0],    // Green
    [0, 0, 255],    // Blue
    [255, 255, 0],  // Yellow
    [0, 255, 255],  // Cyan
    [255, 0, 255],  // Magenta
    [255, 255, 255],// White
    [0, 0, 0],      // Black
    [128, 0, 0],    // Maroon
    [0, 128, 0],    // Dark Green
    [0, 0, 128],    // Navy
    [128, 128, 0],  // Olive
    [128, 0, 128],  // Purple
    [0, 128, 128],  // Teal
    [192, 192, 192],// Silver
    [128, 128, 128],// Gray
    [255, 165, 0],  // Orange
    [255, 192, 203],// Pink
];
const colorNames = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Black", "Maroon", "Dark Green", "Navy", "Olive", "Purple", "Teal", "Silver", "Gray", "Orange", "Pink"];

let selectedColor = 0;

function setupColorPalette() {
    const colorPalette = document.getElementById('color-palette');
    colors.forEach((color, i) => {
        const item = document.createElement('div');
        item.className = 'palette-item';
        item.style.backgroundColor = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
        item.dataset.idx = i;
        item.addEventListener('click', () => {
            selectedColor = i;
            updateSelections();
            
            // Log the selected color for tutorial purposes
            console.log(`Color selected: ${colorNames[i]} - RGB(${color[0]}, ${color[1]}, ${color[2]})`);
        });
        colorPalette.appendChild(item);
    });
    
    updateSelections();
}

function updateSelections() {
    document.querySelectorAll('#color-palette .palette-item').forEach((item, i) => {
        item.classList.toggle('selected', i === selectedColor);
    });
    document.getElementById('status').textContent = `Selected Color: ${colorNames[selectedColor]} - RGB(${colors[selectedColor][0]}, ${colors[selectedColor][1]}, ${colors[selectedColor][2]})`;
}

// Initialize the color palette when the page loads
document.addEventListener('DOMContentLoaded', setupColorPalette);