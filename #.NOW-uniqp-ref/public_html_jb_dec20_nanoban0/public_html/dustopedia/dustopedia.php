<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dustopedia - jbmbrooks.com</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background-color: #1a1a1a;
            color: #e0e0e0;
            background-size: cover;
            background-position: center;
            background-repeat: no-repeat;
            min-height: 100vh;
            display: flex;
            margin: 0;
            padding: 0;
            overflow: auto; /* Page always scrolls */
        }
        .sidebar {
            width: 80px;
            background-color: rgba(0, 0, 0, 0.8);
            padding: 20px 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            position: fixed;
            height: 100vh;
            z-index: 100;
            transition: transform 0.3s ease;
        }
        .sidebar-icon {
            font-size: 32px;
            margin: 20px 0;
            cursor: pointer;
            color: #e0e0e0;
            text-decoration: none;
            display: block;
            transition: color 0.3s;
        }
        .sidebar-icon:hover {
            color: #00ff00;
        }
        .main-content {
            margin-left: 80px;
            flex: 1;
            display: flex;
            flex-direction: column;
            justify-content: flex-start;
            align-items: center;
            padding: 20px;
            padding-bottom: 80px; /* Add space for footer */
            transition: margin-left 0.3s ease;
        }
        .header {
            background-color: rgba(0, 0, 0, 0.3);
            border-radius: 8px;
            padding: 15px 20px;
            margin-bottom: 20px;
            width: 100%;
            max-width: 800px;
            text-align: center;
            border: 1px solid #555;
        }
        .header h2 {
            color: #00ff00;
            margin: 0 0 10px 0;
            font-size: 18px;
        }
        .search-form {
            display: flex;
            justify-content: center;
            gap: 10px;
        }
        .search-input {
            flex: 1;
            padding: 10px;
            border: 1px solid #e0e0e0;
            border-radius: 4px;
            background-color: #555;
            color: #e0e0e0;
            max-width: 400px;
        }
        .search-input::placeholder {
            color: #888;
        }
        .search-button {
            padding: 10px 20px;
            background-color: #3282e6;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .search-button:hover {
            background-color: #2567b3;
        }
        .download-button {
            display: block;
            width: 100%;
            max-width: 400px;
            margin: 15px auto 0;
            padding: 12px;
            background-color: #00ff00;
            color: #000000;
            text-align: center;
            text-decoration: none;
            border-radius: 4px;
            font-weight: bold;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .download-button:hover {
            background-color: #00cc00;
        }
        .cards-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            max-width: 800px;
            width: 100%;
        }
        .wiki-card {
            background-color: rgba(0, 0, 0, 0.65);
            border-radius: 8px;
            border: 1px solid #555;
            overflow: hidden;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .wiki-card:hover {
            background-color: rgba(0, 0, 0, 0.8);
        }
        .wiki-summary {
            padding: 15px 20px;
            margin: 0;
            background-color: rgba(0, 0, 0, 0.2);
            font-weight: bold;
            color: #00ff00;
            border-bottom: 1px solid #555;
        }
        .wiki-details {
            max-height: 0;
            overflow: hidden;
            transition: max-height 0.3s ease-out;
            background-color: rgba(0, 0, 0, 0.4);
        }
        .wiki-card.expanded .wiki-details {
            max-height: none; /* Let content flow fully */
            padding: 20px;
            overflow-y: visible; /* No internal scroll; page handles it */
        }
        .wiki-content {
            font-size: 14px;
            line-height: 1.5;
            white-space: pre-line;
            font-family: 'Courier New', monospace;
        }
        .wiki-content img {
            max-width: 100%;
            height: auto;
            display: block;
            margin: 20px auto;
            border-radius: 8px;
            border: 1px solid #555;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
        }
        .footer {
            position: fixed;
            bottom: 0;
            left: 80px;
            right: 0;
            text-align: center;
            padding: 10px;
            background-color: rgba(0, 0, 0, 0.5);
            font-size: 12px;
            transition: left 0.3s ease;
        }
        .contact-section {
            margin-bottom: 10px;
        }
        .contact-section h4 {
            color: #00ff00;
            margin: 0 0 10px 0;
            font-size: 14px;
        }
        .contact-buttons {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-bottom: 10px;
            flex-wrap: wrap;
        }
        .contact-icon {
            font-size: 24px;
            color: #e0e0e0;
            text-decoration: none;
            transition: color 0.3s;
        }
        .contact-icon:hover {
            color: #00ff00;
        }

        /* Mobile Responsiveness */
        @media (max-width: 768px) {
            .sidebar {
                transform: translateX(-100%);
            }
            .main-content {
                margin-left: 0;
                padding: 10px;
            }
            .header {
                padding: 10px 15px;
                margin-bottom: 15px;
            }
            .header h2 {
                font-size: 16px;
            }
            .search-form {
                flex-direction: column;
                align-items: center;
                gap: 10px;
            }
            .search-input {
                max-width: 100%;
                width: 100%;
            }
            .search-button {
                width: 100%;
                max-width: 200px;
            }
            .download-button {
                max-width: 100%;
            }
            .cards-container {
                grid-template-columns: 1fr;
                gap: 15px;
                max-width: 100%;
            }
            .wiki-card {
                width: 100%;
            }
            .wiki-details {
                padding: 15px;
            }
            .wiki-content {
                font-size: 13px;
            }
            .wiki-content img {
                margin: 15px auto;
            }
            .footer {
                left: 0;
                padding: 10px 15px;
                font-size: 11px;
            }
            .contact-buttons {
                gap: 15px;
            }
            .contact-icon {
                font-size: 20px;
            }
        }

        @media (max-width: 480px) {
            .cards-container {
                gap: 10px;
            }
            .wiki-details {
                padding: 12px;
            }
            .wiki-content {
                font-size: 12px;
            }
            .wiki-content img {
                margin: 10px auto;
            }
            .contact-buttons {
                gap: 10px;
            }
            .contact-icon {
                font-size: 18px;
            }
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <a href="page1.html" class="sidebar-icon" title="Home">🏠</a>
        <a href="page2.html" class="sidebar-icon" title="Profile">👤</a>
        <a href="page3.html" class="sidebar-icon" title="Settings">⚙️</a>
        <a href="page4.html" class="sidebar-icon" title="Help">❓</a>
        <a href="page5.html" class="sidebar-icon" title="About">ℹ️</a>
    </div>
    <div class="main-content">
        <div class="header">
            <h2>Dustopedia - jbmbrooks.com</h2>
            <form class="search-form" action="search.html" method="get">
                <input type="text" class="search-input" name="q" placeholder="Search...">
                <button type="submit" class="search-button">Search</button>
            </form>
            <a href="test.zip" download class="download-button">Download Unix Desktop Application</a>
        </div>
        <div class="cards-container">
            <div class="wiki-card" onclick="toggleWikiCard(this)">
                <div class="wiki-summary">CARCAZOD (Homo sapiens carcharias-arthropoda)</div>
                <div class="wiki-details">
                    <div class="wiki-content" id="carcazod-content">
                        <?php
                        $filePath = 'carcazod.txt';
                        if (file_exists($filePath)) {
                            echo htmlspecialchars(file_get_contents($filePath), ENT_QUOTES, 'UTF-8');
                        } else {
                            echo 'Error: carcazod.txt not found. Please upload it to the same directory as this PHP file.';
                        }
                        
                        // Dynamically load images at the bottom
                        $image1Path = 'carcazod0000.jpg';
                        if (file_exists($image1Path)) {
                            echo '<img src="' . htmlspecialchars($image1Path) . '" alt="Carcazod Image" />';
                        }
                        
                        $image2Path = 'karkshark0000.jpg';
                        if (file_exists($image2Path)) {
                            echo '<img src="' . htmlspecialchars($image2Path) . '" alt="Karkshark Image" />';
                        }
                        ?>
                    </div>
                </div>
            </div>
            <!-- Add more wiki cards here as needed -->
        </div>
    </div>
    <div class="footer">
        <div class="contact-section">
            <h4>Contact</h4>
            <div class="contact-buttons">
                <a href="mailto:info@jbmbrooks.com" class="contact-icon" title="Email">📧</a>
                <a href="https://youtube.com/@jbmbrooks" class="contact-icon" title="YouTube">📺</a>
                <a href="https://x.com/jbmbrooks" class="contact-icon" title="X.com">🐦</a>
                <a href="mailto:support@jbmbrooks.com" class="contact-icon" title="Support Email">📧</a>
            </div>
        </div>
        <p>Dustopedia - JBMBrooks © 2025</p>
    </div>
    <script>
        // Override toggle: 0 = time-based, 1 = day, 2 = night
        const overrideToggle = 0;

        // Function to set background based on time or override
        function setBackgroundImage() {
            let backgroundImage = '';
            if (overrideToggle === 1) {
                backgroundImage = 'url("../ss.flip]day.png")';
            } else if (overrideToggle === 2) {
                backgroundImage = 'url("../ss.flip]nite.png")';
            } else {
                const now = new Date();
                const hour = now.toLocaleString('en-US', { 
                    timeZone: 'America/Los_Angeles', 
                    hour: 'numeric', 
                    hour12: false 
                });
                const hourNum = parseInt(hour);
                backgroundImage = (hourNum >= 6 && hourNum < 18) 
                    ? 'url("../ss.flip]day.png")' 
                    : 'url("../ss.flip]nite.png")';
            }
            document.body.style.backgroundImage = backgroundImage;
        }

        // Run on page load
        setBackgroundImage();

        function navigateTo(page) {
            window.location.href = page;
        }

        // Toggle function for wiki cards (page scrolls, no internal)
        function toggleWikiCard(card) {
            card.classList.toggle('expanded');
            // Optional: Scroll to top of card for better UX
            if (card.classList.contains('expanded')) {
                card.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }
        }

        // Simple mobile sidebar toggle (optional enhancement)
        function toggleSidebar() {
            const sidebar = document.querySelector('.sidebar');
            const mainContent = document.querySelector('.main-content');
            const footer = document.querySelector('.footer');
            sidebar.classList.toggle('visible');
            if (sidebar.classList.contains('visible')) {
                mainContent.style.marginLeft = '80px';
                footer.style.left = '80px';
            } else {
                mainContent.style.marginLeft = '0';
                footer.style.left = '0';
            }
        }

        // Add a menu button for mobile if needed (uncomment and add button in HTML)
        // document.addEventListener('DOMContentLoaded', () => {
        //     const menuBtn = document.createElement('button');
        //     menuBtn.innerHTML = '☰';
        //     menuBtn.style.position = 'fixed';
        //     menuBtn.style.top = '10px';
        //     menuBtn.style.left = '10px';
        //     menuBtn.style.zIndex = '101';
        //     menuBtn.style.background = 'rgba(0,0,0,0.5)';
        //     menuBtn.style.color = '#e0e0e0';
        //     menuBtn.style.border = 'none';
        //     menuBtn.style.padding = '10px';
        //     menuBtn.onclick = toggleSidebar;
        //     document.body.appendChild(menuBtn);
        // });
    </script>
</body>
</html>