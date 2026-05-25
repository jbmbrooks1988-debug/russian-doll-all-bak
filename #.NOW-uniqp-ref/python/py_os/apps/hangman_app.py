"""
Hangman Game Application for PyOS
A classic word-guessing game
"""

from PySide6.QtWidgets import QVBoxLayout, QWidget, QLabel, QLineEdit, QPushButton, QHBoxLayout, QGridLayout
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont, QColor

import random

from apps.base_app import BaseApplication


class HangmanWidget(QWidget):
    def __init__(self):
        super().__init__()
        self.setup_ui()
        self.reset_game()
        
    def setup_ui(self):
        """Setup the Hangman game UI"""
        layout = QVBoxLayout(self)
        layout.setSpacing(15)
        
        # Title
        title_label = QLabel("HANGMAN GAME")
        title_label.setAlignment(Qt.AlignCenter)
        title_label.setStyleSheet("font-size: 24px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Word display
        self.word_label = QLabel("")
        self.word_label.setAlignment(Qt.AlignCenter)
        self.word_label.setStyleSheet("font-size: 28px; font-weight: bold; color: #fff; margin: 20px;")
        layout.addWidget(self.word_label)
        
        # Gallows display
        self.gallows_label = QLabel("")
        self.gallows_label.setAlignment(Qt.AlignCenter)
        self.gallows_label.setStyleSheet("font-family: 'Courier New'; font-size: 14px; color: #aaa;")
        layout.addWidget(self.gallows_label)
        
        # Incorrect guesses display
        self.incorrect_label = QLabel("Incorrect guesses: ")
        self.incorrect_label.setAlignment(Qt.AlignCenter)
        self.incorrect_label.setStyleSheet("font-size: 16px; color: #ff6666;")
        layout.addWidget(self.incorrect_label)
        
        # Input section
        input_layout = QHBoxLayout()
        
        self.letter_input = QLineEdit()
        self.letter_input.setMaxLength(1)
        self.letter_input.setStyleSheet("""
            QLineEdit {
                font-size: 18px;
                padding: 8px;
                border: 2px solid #555;
                border-radius: 5px;
                background-color: #3a3a3a;
                color: white;
            }
        """)
        self.letter_input.setPlaceholderText("Enter a letter...")
        input_layout.addWidget(self.letter_input)
        
        self.guess_button = QPushButton("Guess")
        self.guess_button.setStyleSheet("""
            QPushButton {
                background-color: #4a90d9;
                color: white;
                border: none;
                padding: 10px 20px;
                font-size: 16px;
                border-radius: 5px;
            }
            QPushButton:hover {
                background-color: #5a9fe9;
            }
            QPushButton:pressed {
                background-color: #3a80c9;
            }
        """)
        input_layout.addWidget(self.guess_button)
        
        layout.addLayout(input_layout)
        
        # Message display
        self.message_label = QLabel("Guess a letter to start the game!")
        self.message_label.setAlignment(Qt.AlignCenter)
        self.message_label.setStyleSheet("font-size: 16px; color: #aaa;")
        layout.addWidget(self.message_label)
        
        # Reset button
        self.reset_button = QPushButton("New Game")
        self.reset_button.setStyleSheet("""
            QPushButton {
                background-color: #7a6da5;
                color: white;
                border: none;
                padding: 10px;
                font-size: 14px;
                border-radius: 5px;
            }
            QPushButton:hover {
                background-color: #8a7db5;
            }
            QPushButton:pressed {
                background-color: #6a5d95;
            }
        """)
        layout.addWidget(self.reset_button)
        
        # Connect signals
        self.guess_button.clicked.connect(self.make_guess)
        self.letter_input.returnPressed.connect(self.make_guess)
        self.reset_button.clicked.connect(self.reset_game)
        
        # Initialize game
        self.reset_game()
    
    def reset_game(self):
        """Reset the game to initial state"""
        # List of words to choose from
        self.words = [
            "PYTHON", "COMPUTER", "PROGRAMMING", "HANGMAN", "DEVELOPER",
            "ALGORITHM", "FUNCTION", "VARIABLE", "STRING", "INTEGER",
            "BOOLEAN", "LOOP", "CONDITION", "CLASS", "OBJECT"
        ]
        
        # Select a random word
        self.target_word = random.choice(self.words)
        self.guessed_letters = set()
        self.incorrect_guesses = 0
        self.max_incorrect = 6
        
        # Update displays
        self.update_word_display()
        self.update_gallows()
        self.incorrect_label.setText("Incorrect guesses: ")
        self.message_label.setText("Guess a letter to start the game!")
        self.letter_input.clear()
        self.letter_input.setEnabled(True)
        self.guess_button.setEnabled(True)
    
    def update_word_display(self):
        """Update the display showing the current state of the word"""
        display = ""
        for letter in self.target_word:
            if letter in self.guessed_letters:
                display += letter + " "
            else:
                display += "_ "
        self.word_label.setText(display.strip())
    
    def update_gallows(self):
        """Update the gallows display based on incorrect guesses"""
        stages = [
            """
               ------
               |    |
               |
               |
               |
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |
               |
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |    |
               |
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |   /|
               |
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |   /|\\
               |
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |   /|\\
               |   /
               |
            --------
            """,
            """
               ------
               |    |
               |    O
               |   /|\\
               |   / \\
               |
            --------
            """
        ]
        
        self.gallows_label.setText(stages[self.incorrect_guesses])
    
    def make_guess(self):
        """Process a letter guess"""
        letter = self.letter_input.text().strip().upper()
        self.letter_input.clear()
        
        if not letter:
            self.message_label.setText("Please enter a letter!")
            return
        
        if len(letter) != 1 or not letter.isalpha():
            self.message_label.setText("Please enter a single letter!")
            return
        
        if letter in self.guessed_letters:
            self.message_label.setText(f"You already guessed '{letter}'!")
            return
        
        self.guessed_letters.add(letter)
        
        if letter in self.target_word:
            self.message_label.setText(f"Good guess! '{letter}' is in the word.")
            self.update_word_display()
            
            # Check for win
            if all(c in self.guessed_letters for c in self.target_word):
                self.message_label.setText(f"Congratulations! You won! The word was '{self.target_word}'")
                self.letter_input.setEnabled(False)
                self.guess_button.setEnabled(False)
        else:
            self.incorrect_guesses += 1
            self.message_label.setText(f"Sorry, '{letter}' is not in the word.")
            
            # Update incorrect guesses display
            incorrect_list = [l for l in sorted(self.guessed_letters) if l not in self.target_word]
            self.incorrect_label.setText(f"Incorrect guesses: {', '.join(incorrect_list)}")
            
            self.update_gallows()
            
            # Check for loss
            if self.incorrect_guesses >= self.max_incorrect:
                self.message_label.setText(f"Game Over! The word was '{self.target_word}'")
                self.letter_input.setEnabled(False)
                self.guess_button.setEnabled(False)


class HangmanApp(BaseApplication):
    def setup_ui(self):
        """Setup the Hangman application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        # Create the hangman game widget
        self.hangman_widget = HangmanWidget()
        layout.addWidget(self.hangman_widget)
        
        # Add stretch to push content up
        layout.addStretch()