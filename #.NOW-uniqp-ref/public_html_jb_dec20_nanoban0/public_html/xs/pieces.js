<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Welcome to jbmbrooks.com</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin-top: 50px; 
            background-color: #1a1a1a; /* Darker black background as fallback */
            color: #e0e0e0; /* Light grey text for body */
            background-size: cover; /* Ensure background image covers entire body */
            background-position: center; /* Center the background image */
            background-repeat: no-repeat; /* Prevent image repetition */
            min-height: 100vh; /* Ensure body covers viewport height */
            display: flex; /* Flexbox to push footer to bottom */
            flex-direction: column; /* Stack children vertically */
        }
        .main-content {
            flex: 1; /* Push footer to bottom by filling available space */
        }
        h1 { 
            color: #00ff00; /* Neon green for h1 */
        }
        h2 { 
            color: #ff6200; /* Orange for h2 */
        }
        .text-container {
            display: inline-block; /* Fit content width */
            padding: 15px 25px; /* Padding for background area */
            background-color: rgba(0, 0, 0, 0.55); /* 55% translucent dark black */
            border-radius: 8px; /* Rounded corners for aesthetics */
            margin-bottom: 20px; /* Space below text container */
        }
        .login-container {
            margin: 20px auto;
            padding: 20px;
            background-color: rgba(0, 0, 0, 0.35); /* 35% translucent dark black */
            border-radius: 8px;
            width: 300px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.5);
        }
        .login-container input[type="text"],
        .login-container input[type="password"] {
            width: 90%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #e0e0e0;
            border-radius: 4px;
            background-color: #555; /* Darker input background */
            color: #e0e0e0; /* Light grey text for input */
        }
        .login-container input[type="submit"] {
            padding: 10px 20px;
            background-color: #3282e6; /* Matching link hover color */
            color: #ffffff; /* White text for button */
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .login-container input[type="submit"]:hover {
            background-color: #2567b3; /* Darker blue on hover */
        }
        .signup-container {
            display: inline-block; /* Fit content width */
            margin: 10px auto;
            padding: 10px;
            background-color: rgba(0, 0, 0, 0.55); /* 55% translucent dark black */
            border-radius: 8px; /* Match login button styling */
        }
        .signup-container a {
            display: inline-block; /* Treat link as block for button styling */
            padding: 10px 20px; /* Match login submit button */
            background-color: #3282e6; /* Same blue as login button */
            color: #ffffff; /* White text for button */
            border: none;
            border-radius: 4px;
            text-decoration: none; /* No underline */
            cursor: pointer;
        }
        .signup-container a:hover {
            background-color: #2567b3; /* Darker blue on hover */
            color: #ffffff; /* Keep white text on hover */
        }
        #visitor-counter {
            font-size: 18px;
            color: #e0e0e0; /* Light grey text for counter */
        }
        .footer {
            margin-top: 40px; /* Space above footer */
            padding-bottom: 20px; /* Padding to keep counter at foot */
        }
        .pieces-container {
            position: fixed; /* Fix at bottom */
            bottom: 60px; /* Above footer to avoid overlap with visitor counter */
            left: 50%; /* Center horizontally */
            transform: translateX(-50%); /* Offset for centering */
            display: flex; /* Horizontal alignment */
            gap: 10px; /* Space between images */
            z-index: 1000; /* Ensure images are above background */
        }
        .piece-img {
            width: 42px;
            height: 42px;
            cursor: move; /* Indicate draggability */
            position: absolute; /* Allow dragging to any position */
            user-select: none; /* Prevent text selection during drag */
        }
        #debug {
            position: fixed;
            top: 10px;
            left: 10px;
            color: #ff0000; /* Red for visibility */
            font-size: 14px;
            z-index: 2000;
        }
    </style>
</head>
<body>
    <div id="debug"></div>
    <div class="main-content">
        <div class="text-container">
            <h1>JBMBrooks.com</h1>
            <h2>My Network Portfolio Suite</h2>
        </div>
        <div class="login-container">
            <form action="login.php" method="post">
                <input type="text" name="username" placeholder="Username" required>
                <input type="password" name="password" placeholder="Password" required>
                <input type="submit" value="Login">
            </form>
        </div>
        <div class="signup-container">
            <a href="signup.php">Signup</a>
        </div>
    </div>
    <div class="pieces-container" id="pieces-container"></div>
    <div class="footer">
        <div id="visitor-counter"></div>
    </div>
    <script src="counter.php"></script>
    <script src="pieces.js"></script>
    <script>
        // Override toggle: 0 = time-based, 1 = day, 2 = night
        const overrideToggle = 0;
        const debugDiv = document.getElementById('debug');

        // Function to set background based on time or override
        function setBackgroundImage() {
            let backgroundImage = '';
            if (overrideToggle === 1) {
                backgroundImage = 'url("ss.flip_day.png")'; // Fixed potential typo
                debugDiv.innerText = 'Background: Forced day (ss.flip_day.png)';
            } else if (overrideToggle === 2) {
                backgroundImage = 'url("ss.flip_nite.png")'; // Fixed potential typo
                debugDiv.innerText = 'Background: Forced night (ss.flip_nite.png)';
            } else {
                const now = new Date();
                const hour = now.toLocaleString('en-US', { 
                    timeZone: 'America/Los_Angeles', 
                    hour: 'numeric', 
                    hour12: false 
                });
                const hourNum = parseInt(hour);
                backgroundImage = (hourNum >= 6 && hourNum < 18) 
                    ? 'url("ss.flip_day.png")' // Fixed potential typo
                    : 'url("ss.flip_nite.png")'; // Fixed potential typo
                debugDiv.innerText = `Background: Time-based, hour=${hourNum}, using ${backgroundImage}`;
            }
            document.body.style.backgroundImage = backgroundImage;
            // Test if image loads
            const img = new Image();
            img.src = backgroundImage.replace('url("', '').replace('")', '');
            img.onload = () => debugDiv.innerText += '\nImage loaded successfully';
            img.onerror = () => debugDiv.innerText += '\nError: Image failed to load';
        }

        // Run on page load
        setBackgroundImage();
    </script>
</body>
</html>